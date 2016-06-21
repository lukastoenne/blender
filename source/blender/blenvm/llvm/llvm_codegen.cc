/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenvm/llvm/llvm_codegen.cc
 *  \ingroup llvm
 */

#include <cctype>
#include <cstdio>
#include <set>
#include <sstream>

#include "node_graph.h"

#include "llvm_codegen.h"
#include "llvm_engine.h"
#include "llvm_function.h"
#include "llvm_headers.h"
#include "llvm_modules.h"
#include "llvm_types.h"

#include "util_opcode.h"

#include "modules.h"

namespace blenvm {

/* llvm::ostream that writes to a FILE. */
class file_ostream : public llvm::raw_ostream {
	FILE *file;
	
	/* write_impl - See raw_ostream::write_impl. */
	void write_impl(const char *ptr, size_t size) override {
		fwrite(ptr, sizeof(char), size, file);
	}
	
	/* current_pos - Return the current position within the stream, not
	 * counting the bytes currently in the buffer.
	 */
	uint64_t current_pos() const override { return ftell(file); }
	
public:
	explicit file_ostream(FILE *f) : file(f) {}
	~file_ostream() {
		fflush(file);
	}
};

class debug_assembly_annotation_writer : public llvm::AssemblyAnnotationWriter
{
	/* add implementation here if needed */
};

/* ------------------------------------------------------------------------- */

/* replace non-alphanumeric chars with underscore */
static string sanitize_name(const string &name)
{
	string s = name;
	for (string::iterator it = s.begin(); it != s.end(); ++it) {
		char &c = *it;
		if (c != '_' && !isalnum(c))
			c = '_';
	}
	return s;
}

/* forward declaration */
static llvm::Function *declare_graph_function(
        llvm::LLVMContext &context,
        llvm::Module *mod,
        const string &name,
        const NodeGraph *graph,
        bool use_globals);

static void define_node_function(
        llvm::LLVMContext &context,
        llvm::Module *mod,
        OpCode op,
        const string &nodetype_name);


llvm::Module *LLVMCodeGenerator::m_nodes_module = NULL;

LLVMCodeGenerator::LLVMCodeGenerator(int opt_level) :
    m_opt_level(opt_level),
    m_function_address(0),
    m_module(NULL),
    m_function(NULL),
    m_block(NULL)
{
	using namespace llvm;
	
	BLI_assert(m_nodes_module != NULL);
}

LLVMCodeGenerator::~LLVMCodeGenerator()
{
	using namespace llvm;
	
	destroy_module();
}

void LLVMCodeGenerator::define_nodes_module(llvm::LLVMContext &context)
{
	using namespace llvm;
	
	m_nodes_module = new llvm::Module("texture_nodes", context);
	
#define DEF_OPCODE(op) \
	define_node_function(context, m_nodes_module, OP_##op, STRINGIFY(op));
	
	BVM_DEFINE_OPCODES
	
#undef DEF_OPCODE
}

llvm::LLVMContext &LLVMCodeGenerator::context() const
{
	return llvm::getGlobalContext();
}

ValueHandle LLVMCodeGenerator::get_handle(const DualValue &value)
{
	return value.value();
}

void LLVMCodeGenerator::create_module(const string &name)
{
	using namespace llvm;
	
	std::string error;
	
	BLI_assert(m_nodes_module != NULL);
	
	/* create an empty module */
	m_module = new llvm::Module(name, context());
	
	/* link the node functions module, so we can call those functions */
	Linker::LinkModules(m_module, m_nodes_module, Linker::LinkerMode::PreserveSource, &error);
	
	verifyModule(*m_module, &outs());
}

void LLVMCodeGenerator::destroy_module()
{
	if (m_module != NULL) {
		delete m_module;
		m_module = NULL;
	}
}

void LLVMCodeGenerator::finalize_function()
{
	using namespace llvm;
	
	llvm_optimize_module(m_module, m_opt_level);
	llvm_optimize_function(m_function, m_opt_level);
	
	verifyFunction(*m_function, &outs());
	verifyModule(*m_module, &outs());
	
	/* Note: Adding module to exec engine before creating the function prevents compilation! */
	llvm_execution_engine()->addModule(m_module);
	llvm_execution_engine()->generateCodeForModule(m_module);
	m_function_address = llvm_execution_engine()->getFunctionAddress(m_function->getName());
	BLI_assert(m_function_address != 0);
	
	llvm_execution_engine()->removeModule(m_module);
	destroy_module();
	
	m_function = NULL;
}

void LLVMCodeGenerator::debug_function(FILE *file)
{
	using namespace llvm;
	
	llvm_optimize_module(m_module, m_opt_level);
	llvm_optimize_function(m_function, m_opt_level);
	
	verifyFunction(*m_function, &outs());
	verifyModule(*m_module, &outs());
	
	file_ostream stream(file);
	debug_assembly_annotation_writer aaw;
	m_module->print(stream, &aaw);
	
	destroy_module();
}

void LLVMCodeGenerator::node_graph_begin(const string &name, const NodeGraph *graph, bool use_globals)
{
	using namespace llvm;
	
	create_module(name);
	
	m_function = declare_graph_function(context(), m_module, name, graph, use_globals);
	
	/* map function arguments */
	{
		size_t num_inputs = graph->inputs.size();
		size_t num_outputs = graph->outputs.size();
		Function::ArgumentListType::iterator arg_it = m_function->arg_begin();
		m_globals_ptr = arg_it++; /* globals, passed to functions which need it */
		for (int i = 0; i < num_outputs; ++i)
			m_output_args.push_back(arg_it++);
		for (int i = 0; i < num_inputs; ++i)
			m_input_args.push_back(arg_it++);
	}
	
	m_block = BasicBlock::Create(context(), "entry", m_function);
}

void LLVMCodeGenerator::node_graph_end()
{
	using namespace llvm;
	
	BLI_assert(m_function != NULL);
	
	IRBuilder<> builder(context());
	builder.SetInsertPoint(m_block);
	
	builder.CreateRetVoid();
	
	m_values.clear();
}

void LLVMCodeGenerator::store_return_value(size_t output_index, const TypeSpec *typespec, ValueHandle handle)
{
	using namespace llvm;
	
	IRBuilder<> builder(context());
	builder.SetInsertPoint(m_block);
	
	Argument *arg = m_output_args[output_index];
	Value *value_ptr = builder.CreateStructGEP(arg, 0, sanitize_name(arg->getName().str() + "_V"));
	Value *dx_ptr = builder.CreateStructGEP(arg, 1, sanitize_name(arg->getName().str() + "_DX"));
	Value *dy_ptr = builder.CreateStructGEP(arg, 2, sanitize_name(arg->getName().str() + "_DY"));
	
	DualValue dual = m_values.at(handle);
	bvm_llvm_copy_value(context(), m_block, value_ptr, dual.value(), typespec);
	bvm_llvm_copy_value(context(), m_block, dx_ptr, dual.dx(), typespec);
	bvm_llvm_copy_value(context(), m_block, dy_ptr, dual.dy(), typespec);
}

ValueHandle LLVMCodeGenerator::map_argument(size_t input_index, const TypeSpec *typespec)
{
	using namespace llvm;
	
	IRBuilder<> builder(context());
	builder.SetInsertPoint(m_block);
	
	Argument *arg = m_input_args[input_index];
	DualValue dval;
	if (bvm_type_has_dual_value(typespec)) {
		/* argument is a struct, use GEP instructions to get the individual elements */
		dval = DualValue(builder.CreateStructGEP(arg, 0, sanitize_name(arg->getName().str() + "_V")),
		                 builder.CreateStructGEP(arg, 1, sanitize_name(arg->getName().str() + "_DX")),
		                 builder.CreateStructGEP(arg, 2, sanitize_name(arg->getName().str() + "_DY")));
	}
	else {
		dval = DualValue(arg, NULL, NULL);
	}
	
	ValueHandle handle = get_handle(dval);
	bool ok = m_values.insert(HandleValueMap::value_type(handle, dval)).second;
	BLI_assert(ok && "Could not insert value!");
	UNUSED_VARS(ok);
	
	return handle;
}

ValueHandle LLVMCodeGenerator::alloc_node_value(const TypeSpec *typespec, const string &name)
{
	using namespace llvm;
	
	IRBuilder<> builder(context());
	builder.SetInsertPoint(m_block);
	
	Type *type = bvm_get_llvm_type(context(), typespec, false);
	BLI_assert(type != NULL);
	
	DualValue dval(builder.CreateAlloca(type, NULL, sanitize_name(name + "_V")),
	               builder.CreateAlloca(type, NULL, sanitize_name(name + "_DX")),
	               builder.CreateAlloca(type, NULL, sanitize_name(name + "_DY")));
	
	ValueHandle handle = get_handle(dval);
	bool ok = m_values.insert(HandleValueMap::value_type(handle, dval)).second;
	BLI_assert(ok && "Could not insert value!");
	UNUSED_VARS(ok);
	
	return handle;
}

ValueHandle LLVMCodeGenerator::create_constant(const TypeSpec *UNUSED(typespec), const NodeConstant *node_value)
{
	using namespace llvm;
	
	IRBuilder<> builder(context());
	builder.SetInsertPoint(m_block);
	
	/* create storage for the global value */
	Constant *cvalue = bvm_create_llvm_constant(context(), node_value);
	
	AllocaInst *pvalue = builder.CreateAlloca(cvalue->getType());
	/* XXX this may not work for larger aggregate types (matrix44) !! */
	builder.CreateStore(cvalue, pvalue);
	
	DualValue dval(pvalue);
	
	ValueHandle handle = get_handle(dval);
	bool ok = m_values.insert(HandleValueMap::value_type(handle, dval)).second;
	BLI_assert(ok && "Could not insert value!");
	UNUSED_VARS(ok);
	
	return handle;
}

void LLVMCodeGenerator::eval_node(const NodeType *nodetype,
                                  ArrayRef<ValueHandle> input_args,
                                  ArrayRef<ValueHandle> output_args)
{
	using namespace llvm;
	
	IRBuilder<> builder(context());
	builder.SetInsertPoint(m_block);
	
	/* call evaluation function */
	Function *evalfunc = m_module->getFunction(nodetype->name());
	BLI_assert(evalfunc != NULL && "Could not find node function!");
	
	std::vector<Value*> evalargs;
	
	if (nodetype->use_globals()) {
		evalargs.push_back(m_globals_ptr);
	}
	
	for (int i = 0; i < nodetype->num_outputs(); ++i) {
		const NodeOutput *output = nodetype->find_output(i);
		const TypeSpec *typespec = output->typedesc.get_typespec();
		const DualValue &dval = m_values.at(output_args[i]);
		
		if (bvm_type_has_dual_value(typespec)) {
			evalargs.push_back(dval.value());
			evalargs.push_back(dval.dx());
			evalargs.push_back(dval.dy());
		}
		else {
			evalargs.push_back(dval.value());
		}
		
	}
	for (int i = 0; i < nodetype->num_inputs(); ++i) {
		const NodeInput *input = nodetype->find_input(i);
		const TypeSpec *typespec = input->typedesc.get_typespec();
		bool is_constant = (input->value_type == INPUT_CONSTANT);
		const DualValue &dval = m_values.at(input_args[i]);
		
		if (!is_constant && bvm_type_has_dual_value(typespec)) {
			if (typespec->is_aggregate() || typespec->is_structure()) {
				evalargs.push_back(dval.value());
				evalargs.push_back(dval.dx());
				evalargs.push_back(dval.dy());
			}
			else {
				/* pass-by-value for non-aggregate types */
				evalargs.push_back(builder.CreateLoad(dval.value()));
				evalargs.push_back(builder.CreateLoad(dval.dx()));
				evalargs.push_back(builder.CreateLoad(dval.dy()));
			}
		}
		else {
			if (typespec->is_aggregate() || typespec->is_structure()) {
				evalargs.push_back(dval.value());
			}
			else {
				/* pass-by-value for non-aggregate types */
				evalargs.push_back(builder.CreateLoad(dval.value()));
			}
		}
	}
	
	CallInst *call = builder.CreateCall(evalfunc, evalargs);
	UNUSED_VARS(call);
}

/* ------------------------------------------------------------------------- */

static void append_graph_input_args(llvm::LLVMContext &context,
                                   std::vector<llvm::Type*> &arg_types,
                                   std::vector<string> &arg_names,
                                   const NodeGraph::Input *input)
{
	using namespace llvm;
	
	const TypeSpec *spec = input->typedesc.get_typespec();
	llvm::Type *type = bvm_get_llvm_type(context, spec, true);
	if (bvm_type_has_dual_value(spec))
		type = type->getPointerTo();
	
	arg_types.push_back(type);
	arg_names.push_back(input->name);
}

static void append_graph_output_args(llvm::LLVMContext &context,
                                    std::vector<llvm::Type*> &arg_types,
                                    std::vector<string> &arg_names,
                                    const NodeGraph::Output *output)
{
	using namespace llvm;
	
	const TypeSpec *spec = output->typedesc.get_typespec();
	Type *type = bvm_get_llvm_type(context, spec, true);
	/* return argument is a pointer */
	type = type->getPointerTo();
	
	arg_types.push_back(type);
	arg_names.push_back(output->name);
}

static llvm::Function *declare_graph_function(
        llvm::LLVMContext &context,
        llvm::Module *mod,
        const string &name,
        const NodeGraph *graph,
        bool use_globals)
{
	using namespace llvm;
	
	std::vector<Type *> arg_types;
	std::vector<string> arg_names;
	
	if (use_globals) {
		Type *t_globals = llvm::TypeBuilder<void*, false>::get(context);
		arg_types.push_back(t_globals);
		arg_names.push_back("globals");
	}
	
	for (int i = 0; i < graph->outputs.size(); ++i) {
		const NodeGraph::Output *output = graph->get_output(i);
		append_graph_output_args(context, arg_types, arg_names, output);
		
	}
	for (int i = 0; i < graph->inputs.size(); ++i) {
		const NodeGraph::Input *input = graph->get_input(i);
		append_graph_input_args(context, arg_types, arg_names, input);
	}
	
	FunctionType *functype = FunctionType::get(TypeBuilder<void, true>::get(context), arg_types, false);
	Function *func = Function::Create(functype, Function::ExternalLinkage, name, mod);
	
	Function::arg_iterator arg_it = func->arg_begin();
	for (int i = 0; arg_it != func->arg_end(); ++arg_it, ++i) {
		arg_it->setName(sanitize_name(arg_names[i]));
	}
	
	return func;
}

static void append_node_input_args(llvm::LLVMContext &context,
                                   std::vector<llvm::Type*> &arg_types,
                                   std::vector<string> &arg_names,
                                   const NodeInput *input)
{
	using namespace llvm;
	
	const TypeSpec *spec = input->typedesc.get_typespec();
	bool is_constant = (input->value_type == INPUT_CONSTANT);
	Type *type = bvm_get_llvm_type(context, spec, false);
	/* pass-by-reference for aggregate types */
	if (spec->is_aggregate() || spec->is_structure())
		type = type->getPointerTo();
	
	if (!is_constant && bvm_type_has_dual_value(spec)) {
		arg_types.push_back(type);
		arg_names.push_back("V_" + input->name);
		
		/* two derivatives */
		arg_types.push_back(type);
		arg_names.push_back("DX_" + input->name);
		
		arg_types.push_back(type);
		arg_names.push_back("DY_" + input->name);
	}
	else {
		arg_types.push_back(type);
		arg_names.push_back(input->name);
	}
}

static void append_node_output_args(llvm::LLVMContext &context,
                                    std::vector<llvm::Type*> &arg_types,
                                    std::vector<string> &arg_names,
                                    const NodeOutput *output)
{
	using namespace llvm;
	
	const TypeSpec *spec = output->typedesc.get_typespec();
	Type *type = bvm_get_llvm_type(context, spec, false);
	/* return argument is a pointer */
	type = type->getPointerTo();
	
	if (bvm_type_has_dual_value(spec)) {
		arg_types.push_back(type);
		arg_names.push_back("V_" + output->name);
		
		/* two derivatives */
		arg_types.push_back(type);
		arg_names.push_back("DX_" + output->name);
		
		arg_types.push_back(type);
		arg_names.push_back("DY_" + output->name);
	}
	else {
		arg_types.push_back(type);
		arg_names.push_back(output->name);
	}
}

static llvm::Function *declare_node_function(
        llvm::LLVMContext &context,
        llvm::Module *mod,
        const NodeType *nodetype)
{
	using namespace llvm;
	
	std::vector<Type *> arg_types;
	std::vector<string> arg_names;
	
	if (nodetype->use_globals()) {
		Type *t_globals = llvm::TypeBuilder<void*, false>::get(context);
		arg_types.push_back(t_globals);
		arg_names.push_back("globals");
	}
	
	for (int i = 0; i < nodetype->num_outputs(); ++i) {
		const NodeOutput *output = nodetype->find_output(i);
		append_node_output_args(context, arg_types, arg_names, output);
		
	}
	for (int i = 0; i < nodetype->num_inputs(); ++i) {
		const NodeInput *input = nodetype->find_input(i);
		append_node_input_args(context, arg_types, arg_names, input);
	}
	
	FunctionType *functype = FunctionType::get(TypeBuilder<void, true>::get(context), arg_types, false);
	Function *func = Function::Create(functype, Function::ExternalLinkage, nodetype->name(), mod);
	
	Function::arg_iterator arg_it = func->arg_begin();
	for (int i = 0; arg_it != func->arg_end(); ++arg_it, ++i) {
		arg_it->setName(sanitize_name(arg_names[i]));
	}
	
	return func;
}

static void append_elementary_input_args(llvm::LLVMContext &context,
                                         std::vector<llvm::Type*> &arg_types,
                                         std::vector<string> &arg_names,
                                         const NodeInput *input, bool with_derivs)
{
	using namespace llvm;
	
	const TypeSpec *spec = input->typedesc.get_typespec();
	bool is_constant = (input->value_type == INPUT_CONSTANT);
	Type *type = bvm_get_llvm_type(context, spec, false);
	/* pass-by-reference for aggregate types */
	if (spec->is_aggregate() || spec->is_structure())
		type = type->getPointerTo();
	
	if (!is_constant && with_derivs && bvm_type_has_dual_value(spec)) {
		arg_types.push_back(type);
		arg_names.push_back("V_" + input->name);
		
		/* partial derivative */
		arg_types.push_back(type);
		arg_names.push_back("D_" + input->name);
	}
	else {
		arg_types.push_back(type);
		arg_names.push_back(input->name);
	}
}

static void append_elementary_output_args(llvm::LLVMContext &context,
                                          std::vector<llvm::Type*> &arg_types,
                                          std::vector<string> &arg_names,
                                          const NodeOutput *output)
{
	using namespace llvm;
	
	const TypeSpec *spec = output->typedesc.get_typespec();
	Type *type = bvm_get_llvm_type(context, spec, false);
	/* return argument is a pointer */
	type = type->getPointerTo();
	
	arg_types.push_back(type);
	arg_names.push_back(output->name);
}

static llvm::Function *declare_elementary_node_function(
        llvm::LLVMContext &context,
        llvm::Module *mod,
        const NodeType *nodetype,
        const string &name,
        bool with_input_derivs)
{
	using namespace llvm;
	
	std::vector<Type *> arg_types;
	std::vector<string> arg_names;
	
	if (nodetype->use_globals()) {
		Type *t_globals = llvm::TypeBuilder<void*, false>::get(context);
		arg_types.push_back(t_globals);
		arg_names.push_back("globals");
	}
	
	for (int i = 0; i < nodetype->num_outputs(); ++i) {
		const NodeOutput *output = nodetype->find_output(i);
		append_elementary_output_args(context, arg_types, arg_names, output);
		
	}
	for (int i = 0; i < nodetype->num_inputs(); ++i) {
		const NodeInput *input = nodetype->find_input(i);
		append_elementary_input_args(context, arg_types, arg_names, input, with_input_derivs);
	}
	
	FunctionType *functype = FunctionType::get(TypeBuilder<void, true>::get(context), arg_types, false);
	Function *func = Function::Create(functype, Function::ExternalLinkage, name, mod);
	
	Function::arg_iterator arg_it = func->arg_begin();
	for (int i = 0; arg_it != func->arg_end(); ++arg_it, ++i) {
		arg_it->setName(sanitize_name(arg_names[i]));
	}
	
	return func;
}

static void define_elementary_functions(
        llvm::LLVMContext &context,
        llvm::Module *mod,
        OpCode op,
        const NodeType *nodetype)
{
	using namespace llvm;
	
	/* declare functions */
	Function *value_func = NULL, *deriv_func = NULL;
	
	if (llvm_has_external_impl_value(op)) {
		value_func = declare_elementary_node_function(
		                 context, mod, nodetype,
		                 bvm_value_function_name(nodetype->name()), false);
	}
	
	if (llvm_has_external_impl_deriv(op)) {
		deriv_func = declare_elementary_node_function(
		                 context, mod, nodetype,
		                 bvm_deriv_function_name(nodetype->name()), true);
	}
	
	UNUSED_VARS(value_func, deriv_func);
}

static void define_dual_function_wrapper(
        llvm::LLVMContext &context,
        llvm::Module *mod,
        llvm::Function *func,
        OpCode UNUSED(op),
        const NodeType *nodetype)
{
	using namespace llvm;
	
	/* get evaluation function(s) */
	Function *value_func = mod->getFunction(bvm_value_function_name(nodetype->name()));
	BLI_assert(value_func != NULL && "Could not find node function!");
	
	Function *deriv_func = mod->getFunction(bvm_deriv_function_name(nodetype->name()));
	
	BasicBlock *block = BasicBlock::Create(context, "entry", func);
	IRBuilder<> builder(context);
	builder.SetInsertPoint(block);
	
	/* collect arguments for calling internal elementary functions */
	/* arguments for calculating main value and partial derivatives */
	std::vector<Value*> call_args_value, call_args_dx, call_args_dy;
	
	Function::arg_iterator arg_it = func->arg_begin();
	
	if (nodetype->use_globals()) {
		Value *globals = arg_it++;
		call_args_value.push_back(globals);
		call_args_dx.push_back(globals);
		call_args_dy.push_back(globals);
	}
	
	/* output arguments */
	for (int i = 0; i < nodetype->num_outputs(); ++i) {
		const NodeOutput *output = nodetype->find_output(i);
		
		Value *val, *dx, *dy;
		if (bvm_type_has_dual_value(output->typedesc.get_typespec())) {
			val = arg_it++;
			dx = arg_it++;
			dy = arg_it++;
		}
		else {
			val = arg_it++;
			dx = NULL;
			dy = NULL;
		}
		
		call_args_value.push_back(val);
		if (dx != NULL)
			call_args_dx.push_back(dx);
		if (dy != NULL)
			call_args_dy.push_back(dy);
	}
	/* input arguments */
	for (int i = 0; i < nodetype->num_inputs(); ++i) {
		const NodeInput *input = nodetype->find_input(i);
		const TypeSpec *typespec = input->typedesc.get_typespec();
		
		Value *val, *dx, *dy;
		if (input->value_type != INPUT_CONSTANT && bvm_type_has_dual_value(typespec)) {
			val = arg_it++;
			dx = arg_it++;
			dy = arg_it++;
		}
		else {
			val = arg_it++;
			dx = NULL;
			dy = NULL;
		}
		
		call_args_value.push_back(val);
		
		/* derivative functions take input value as well as its derivative */
		call_args_dx.push_back(val);
		if (dx != NULL)
			call_args_dx.push_back(dx);
		
		call_args_dy.push_back(val);
		if (dy != NULL)
			call_args_dy.push_back(dy);
	}
	
	BLI_assert(arg_it == func->arg_end() && "Did not use all the function arguments!");
	
	/* calculate value */
	builder.CreateCall(value_func, call_args_value);
	
	if (deriv_func) {
		builder.CreateCall(deriv_func, call_args_dx);
		builder.CreateCall(deriv_func, call_args_dy);
	}
	else {
		/* zero the derivatives */
		for (int i = 0; i < nodetype->num_outputs(); ++i) {
			const NodeOutput *output = nodetype->find_output(i);
			const TypeSpec *typespec = output->typedesc.get_typespec();
			
			if (bvm_type_has_dual_value(typespec)) {
				int arg_i = nodetype->use_globals() ? i + 1 : i;
				bvm_llvm_set_zero(context, block, call_args_dx[arg_i], typespec);
				bvm_llvm_set_zero(context, block, call_args_dy[arg_i], typespec);
			}
		}
	}
	
	builder.CreateRetVoid();
}

static void define_node_function(
        llvm::LLVMContext &context,
        llvm::Module *mod,
        OpCode op,
        const string &nodetype_name)
{
	using namespace llvm;
	
	const NodeType *nodetype = NodeGraph::find_node_type(nodetype_name);
	if (nodetype == NULL)
		return;
	
	/* wrapper function */
	Function *func = declare_node_function(context, mod, nodetype);
	if (func == NULL)
		return;
	
	switch (op) {
		/* special cases */
		case OP_GET_DERIVATIVE_FLOAT:
			def_node_GET_DERIVATIVE_FLOAT(context, func);
			break;
		case OP_GET_DERIVATIVE_FLOAT3:
			def_node_GET_DERIVATIVE_FLOAT3(context, func);
			break;
		case OP_GET_DERIVATIVE_FLOAT4:
			def_node_GET_DERIVATIVE_FLOAT4(context, func);
			break;
		
		case OP_VALUE_FLOAT:
			def_node_VALUE_FLOAT(context, func);
			break;
		case OP_VALUE_INT:
			def_node_VALUE_INT(context, func);
			break;
		case OP_VALUE_FLOAT3:
			def_node_VALUE_FLOAT3(context, func);
			break;
		case OP_VALUE_FLOAT4:
			def_node_VALUE_FLOAT4(context, func);
			break;
		case OP_VALUE_MATRIX44:
			def_node_VALUE_MATRIX44(context, func);
			break;
			
		default:
			define_elementary_functions(context, mod, op, nodetype);
			define_dual_function_wrapper(context, mod, func, op, nodetype);
			break;
	}
}

} /* namespace blenvm */

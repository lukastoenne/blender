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

LLVMCompilerBase::LLVMCompilerBase() :
    m_module(NULL)
{
}

LLVMCompilerBase::~LLVMCompilerBase()
{
}

llvm::LLVMContext &LLVMCompilerBase::context() const
{
	return llvm::getGlobalContext();
}

void LLVMCompilerBase::create_module(const string &name)
{
	using namespace llvm;
	
	std::string error;
	
	/* make sure the node base functions are defined */
	if (get_nodes_module() == NULL) {
		define_nodes_module();
	}
	
	/* create an empty module */
	m_module = new llvm::Module(name, context());
	
	/* link the node functions module, so we can call those functions */
	Linker::LinkModules(module(), get_nodes_module(), Linker::LinkerMode::PreserveSource, &error);
	
	verifyModule(*module(), &outs());
}

void LLVMCompilerBase::destroy_module()
{
	delete m_module;
	m_module = NULL;
}

void LLVMCompilerBase::codegen_node(llvm::BasicBlock *block,
                                    const NodeInstance *node)
{
	switch (node->type->kind()) {
		case NODE_TYPE_FUNCTION:
		case NODE_TYPE_KERNEL:
			expand_function_node(block, node);
			break;
		case NODE_TYPE_PASS:
			expand_pass_node(block, node);
			break;
		case NODE_TYPE_ARG:
			expand_argument_node(block, node);
			break;
	}
}

/* Compile nodes as a simple expression.
 * Every node can be treated as a single statement. Each node is translated
 * into a function call, with regular value arguments. The resulting value is
 * assigned to a variable and can be used for subsequent node function calls.
 */
llvm::BasicBlock *LLVMCompilerBase::codegen_function_body_expression(const NodeGraph &graph, llvm::Function *func)
{
	using namespace llvm;
	
	IRBuilder<> builder(context());
	
	BasicBlock *block = BasicBlock::Create(context(), "entry", func);
	builder.SetInsertPoint(block);
	
	int num_inputs = graph.inputs.size();
	int num_outputs = graph.outputs.size();
	
	Argument *retarg = func->getArgumentList().begin();
	{
		Function::ArgumentListType::iterator it = retarg;
		for (int i = 0; i < num_outputs; ++i) {
			++it; /* skip output arguments */
		}
		for (int i = 0; i < num_inputs; ++i) {
			const NodeGraph::Input &input = graph.inputs[i];
			Argument *arg = &(*it++);
			
			map_argument(block, input.key, arg);
		}
	}
	
	OrderedNodeSet nodes;
	for (NodeGraph::NodeInstanceMap::const_iterator it = graph.nodes.begin(); it != graph.nodes.end(); ++it)
		nodes.insert(it->second);
	
	for (OrderedNodeSet::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
		const NodeInstance &node = **it;
		
		codegen_node(block, &node);
	}
	
	{
		Function::ArgumentListType::iterator it = func->getArgumentList().begin();
		for (int i = 0; i < num_outputs; ++i) {
			const NodeGraph::Output &output = graph.outputs[i];
			Argument *arg = &(*it++);
			
			store_return_value(block, output.key, arg);
		}
	}
	
	builder.CreateRetVoid();
	
	m_output_values.clear();
	
	return block;
}

llvm::Function *LLVMCompilerBase::codegen_node_function(const string &name, const NodeGraph &graph)
{
	using namespace llvm;
	
	std::vector<llvm::Type*> input_types, output_types;
	for (int i = 0; i < graph.inputs.size(); ++i) {
		const NodeGraph::Input &input = graph.inputs[i];
		const TypeSpec *typespec = input.typedesc.get_typespec();
		Type *type = get_value_type(typespec, false);
		if (use_argument_pointer(typespec, false))
			type = type->getPointerTo();
		input_types.push_back(type);
	}
	for (int i = 0; i < graph.outputs.size(); ++i) {
		const NodeGraph::Output &output = graph.outputs[i];
		const TypeSpec *typespec = output.typedesc.get_typespec();
		Type *type = get_value_type(typespec, false);
		output_types.push_back(type);
	}
	FunctionType *functype = get_node_function_type(input_types, output_types);
	
	Function *func = Function::Create(functype, Function::ExternalLinkage, name, module());
	
	BLI_assert(func->getArgumentList().size() == graph.inputs.size() + graph.outputs.size() &&
	           "Error: Function has wrong number of arguments for node tree\n");
	
	codegen_function_body_expression(graph, func);
	
	return func;
}

void LLVMCompilerBase::optimize_function(llvm::Function *func, int opt_level)
{
	using namespace llvm;
	using legacy::FunctionPassManager;
	using legacy::PassManager;
	
	FunctionPassManager FPM(module());
	PassManager MPM;
	
#if 0
	/* Set up the optimizer pipeline.
	 * Start with registering info about how the
	 * target lays out data structures.
	 */
	FPM.add(new DataLayoutPass(*llvm_execution_engine()->getDataLayout()));
	/* Provide basic AliasAnalysis support for GVN. */
	FPM.add(createBasicAliasAnalysisPass());
	/* Do simple "peephole" optimizations and bit-twiddling optzns. */
	FPM.add(createInstructionCombiningPass());
	/* Reassociate expressions. */
	FPM.add(createReassociatePass());
	/* Eliminate Common SubExpressions. */
	FPM.add(createGVNPass());
	/* Simplify the control flow graph (deleting unreachable blocks, etc). */
	FPM.add(createCFGSimplificationPass());
	
	FPM.doInitialization();
#endif
	
	PassManagerBuilder builder;
	builder.OptLevel = opt_level;
	
	builder.populateModulePassManager(MPM);
	if (opt_level > 1) {
		/* Inline small functions */
		MPM.add(createFunctionInliningPass());
	}
	
	if (opt_level > 1) {
		/* Optimize memcpy intrinsics */
		FPM.add(createMemCpyOptPass());
	}
	builder.populateFunctionPassManager(FPM);
	
	MPM.run(*module());
	FPM.run(*func);
}

void LLVMCompilerBase::map_argument(llvm::BasicBlock *block, const OutputKey &output, llvm::Argument *arg)
{
	m_output_values[output] = arg;
	UNUSED_VARS(block);
}

void LLVMCompilerBase::store_return_value(llvm::BasicBlock *block, const OutputKey &output, llvm::Value *arg)
{
	using namespace llvm;
	
	IRBuilder<> builder(context());
	builder.SetInsertPoint(block);
	
	Value *value = m_output_values.at(output);
	Value *rvalue = builder.CreateLoad(value);
	builder.CreateStore(rvalue, arg);
}

void LLVMCompilerBase::expand_pass_node(llvm::BasicBlock *block, const NodeInstance *node)
{
	using namespace llvm;
	
	IRBuilder<> builder(context());
	builder.SetInsertPoint(block);
	
	BLI_assert(node->num_inputs() == 1);
	BLI_assert(node->num_outputs() == 1);
	
	ConstInputKey input = node->input(0);
	ConstOutputKey output = node->output(0);
	BLI_assert(input.value_type() == INPUT_EXPRESSION);
	
	Value *value = m_output_values.at(input.link());
	bool ok = m_output_values.insert(OutputValueMap::value_type(output, value)).second;
	BLI_assert(ok && "Value for node output already defined!");
	UNUSED_VARS(ok);
}

void LLVMCompilerBase::expand_argument_node(llvm::BasicBlock *block, const NodeInstance *node)
{
	using namespace llvm;
	/* input arguments are mapped in advance */
	BLI_assert(m_output_values.find(node->output(0)) != m_output_values.end() &&
	           "Input argument value node mapped!");
	UNUSED_VARS(block, node);
}

void LLVMCompilerBase::expand_function_node(llvm::BasicBlock *block, const NodeInstance *node)
{
	using namespace llvm;
	
	IRBuilder<> builder(context());
	builder.SetInsertPoint(block);
	
	/* get evaluation function */
	const std::string &evalname = node->type->name();
	Function *evalfunc = llvm_find_external_function(module(), evalname);
	BLI_assert(evalfunc != NULL && "Could not find node function!");
	
	/* function call arguments */
	std::vector<Value *> args;
	
	for (int i = 0; i < node->num_outputs(); ++i) {
		ConstOutputKey output = node->output(i);
		const TypeSpec *typespec = output.socket->typedesc.get_typespec();
		Type *type = get_value_type(typespec, false);
		BLI_assert(type != NULL);
		Value *value = builder.CreateAlloca(type);
		
		args.push_back(value);
		
		/* use as node output values */
		bool ok = m_output_values.insert(OutputValueMap::value_type(output, value)).second;
		BLI_assert(ok && "Value for node output already defined!");
		UNUSED_VARS(ok);
	}
	
	/* set input arguments */
	for (int i = 0; i < node->num_inputs(); ++i) {
		ConstInputKey input = node->input(i);
		const TypeSpec *typespec = input.socket->typedesc.get_typespec();
		bool is_constant = (input.value_type() == INPUT_CONSTANT);
		
		switch (input.value_type()) {
			case INPUT_CONSTANT: {
				/* create storage for the global value */
				Constant *cvalue = create_node_value_constant(input.value());
				
				Value *value;
				if (use_argument_pointer(typespec, is_constant)) {
					AllocaInst *pvalue = builder.CreateAlloca(cvalue->getType());
					builder.CreateStore(cvalue, pvalue);
					value = pvalue;
				}
				else {
					value = cvalue;
				}
				
				args.push_back(value);
				break;
			}
			case INPUT_EXPRESSION: {
				Value *pvalue = m_output_values.at(input.link());
				Value *value;
				if (use_argument_pointer(typespec, is_constant)) {
					value = pvalue;
				}
				else {
					value = builder.CreateLoad(pvalue);
				}
				
				args.push_back(value);
				break;
			}
			case INPUT_VARIABLE: {
				/* TODO */
				BLI_assert(false && "Variable inputs not supported yet!");
				break;
			}
		}
	}
	
	CallInst *call = builder.CreateCall(evalfunc, args);
	UNUSED_VARS(call);
}

llvm::FunctionType *LLVMCompilerBase::get_node_function_type(const std::vector<llvm::Type*> &inputs,
                                                             const std::vector<llvm::Type*> &outputs)
{
	using namespace llvm;
	
	std::vector<llvm::Type*> arg_types;
	for (int i = 0; i < outputs.size(); ++i) {
		Type *value_type = outputs[i];
		/* use a pointer to store output values */
		arg_types.push_back(value_type->getPointerTo());
	}
	arg_types.insert(arg_types.end(), inputs.begin(), inputs.end());
	
	return FunctionType::get(TypeBuilder<void, true>::get(context()), arg_types, false);
}
static void define_function_OP_VALUE_SINGLE(llvm::LLVMContext &context, llvm::BasicBlock *block,
                                            llvm::Value *result, llvm::Value *value)
{
	using namespace llvm;
	
	IRBuilder<> builder(context);
	builder.SetInsertPoint(block);
	
	builder.CreateStore(value, result);
	
	builder.CreateRetVoid();
}

static void define_function_OP_VALUE_AGGREGATE(llvm::LLVMContext &context, llvm::BasicBlock *block,
                                               llvm::Value *result, llvm::Value *value, size_t size)
{
	using namespace llvm;
	
	IRBuilder<> builder(context);
	builder.SetInsertPoint(block);
	
	Value *size_v = ConstantInt::get(context, APInt(32, size));
	builder.CreateMemCpy(result, value, size_v, 0);
	
	builder.CreateRetVoid();
}

llvm::Function *LLVMCompilerBase::declare_node_function(llvm::Module *mod, const NodeType *nodetype)
{
	using namespace llvm;
	
	std::vector<Type *> input_types, output_types;
	for (int i = 0; i < nodetype->num_inputs(); ++i) {
		const NodeInput *input = nodetype->find_input(i);
		const TypeSpec *typespec = input->typedesc.get_typespec();
		bool is_constant = (input->value_type == INPUT_CONSTANT);
		
		Type *type = get_value_type(typespec, is_constant);
		if (type == NULL)
			break;
		if (use_argument_pointer(typespec, is_constant))
			type = type->getPointerTo();
		input_types.push_back(type);
	}
	for (int i = 0; i < nodetype->num_outputs(); ++i) {
		const NodeOutput *output = nodetype->find_output(i);
		const TypeSpec *typespec = output->typedesc.get_typespec();
		Type *type = get_value_type(typespec, false);
		if (type == NULL)
			break;
		output_types.push_back(type);
	}
	if (input_types.size() != nodetype->num_inputs() ||
	    output_types.size() != nodetype->num_outputs()) {
		/* some arguments could not be handled */
		return NULL;
	}
	
	FunctionType *functype = get_node_function_type(input_types, output_types);
	
	Function *func = Function::Create(functype, Function::ExternalLinkage, nodetype->name(), mod);
	
//	printf("Declared function for node type '%s':\n", nodetype->name().c_str());
//	func->dump();
	
	return func;
}

/* ------------------------------------------------------------------------- */

llvm::Module *LLVMSimpleCompilerImpl::m_nodes_module = NULL;

llvm::Type *LLVMSimpleCompilerImpl::get_value_type(const TypeSpec *spec, bool UNUSED(is_constant))
{
	return bvm_get_llvm_type(context(), spec, false);
}

bool LLVMSimpleCompilerImpl::use_argument_pointer(const TypeSpec *typespec, bool UNUSED(is_constant))
{
	using namespace llvm;
	
	if (typespec->is_structure()) {
		/* pass by reference */
		return true;
	}
	else {
		switch (typespec->base_type()) {
			case BVM_FLOAT:
			case BVM_INT:
				/* pass by value */
				return false;
			case BVM_FLOAT3:
			case BVM_FLOAT4:
			case BVM_MATRIX44:
				/* pass by reference */
				return true;
				
			case BVM_STRING:
			case BVM_RNAPOINTER:
			case BVM_MESH:
			case BVM_DUPLIS:
				/* TODO */
				break;
		}
	}
	
	return false;
}

llvm::Constant *LLVMSimpleCompilerImpl::create_node_value_constant(const NodeValue *node_value)
{
	return bvm_create_llvm_constant(context(), node_value);
}

bool LLVMSimpleCompilerImpl::set_node_function_impl(OpCode op, const NodeType *UNUSED(nodetype),
                                                    llvm::Function *func)
{
	using namespace llvm;
	
	std::vector<Value*> args;
	args.reserve(func->arg_size());
	for (Function::arg_iterator a = func->arg_begin(); a != func->arg_end(); ++a)
		args.push_back(a);
	
	switch (op) {
		case OP_VALUE_FLOAT:
		case OP_VALUE_INT: {
			BasicBlock *block = BasicBlock::Create(context(), "entry", func);
			define_function_OP_VALUE_SINGLE(context(), block, args[0], args[1]);
			return true;
		}
		case OP_VALUE_FLOAT3: {
			BasicBlock *block = BasicBlock::Create(context(), "entry", func);
			define_function_OP_VALUE_AGGREGATE(context(), block, args[0], args[1], sizeof(float3));
			return true;
		}
		case OP_VALUE_FLOAT4: {
			BasicBlock *block = BasicBlock::Create(context(), "entry", func);
			define_function_OP_VALUE_AGGREGATE(context(), block, args[0], args[1], sizeof(float4));
			return true;
		}
		case OP_VALUE_MATRIX44: {
			BasicBlock *block = BasicBlock::Create(context(), "entry", func);
			define_function_OP_VALUE_AGGREGATE(context(), block, args[0], args[1], sizeof(matrix44));
			return true;
		}
		
		default:
			return false;
	}
}

void LLVMSimpleCompilerImpl::define_nodes_module()
{
	using namespace llvm;
	
	Module *mod = new llvm::Module("simple_nodes", context());
	
#define DEF_OPCODE(op) \
	{ \
		const NodeType *nodetype = NodeGraph::find_node_type(STRINGIFY(op)); \
		if (nodetype != NULL) { \
			Function *func = declare_node_function(mod, nodetype); \
			if (func != NULL) { \
				bool has_impl = set_node_function_impl(OP_##op, nodetype, func); \
				if (!has_impl) { \
					/* use C callback as implementation of the function */ \
					void *cb_ptr = modules::get_node_impl_value<OP_##op>(); \
					BLI_assert(cb_ptr != NULL && "No implementation for OpCode!"); \
					llvm_execution_engine()->addGlobalMapping(func, cb_ptr); \
				} \
			} \
		} \
	}
	
	BVM_DEFINE_OPCODES
	
#undef DEF_OPCODE
	
	llvm_execution_engine()->addModule(mod);
	
	m_nodes_module = mod;
}

/* ------------------------------------------------------------------------- */

llvm::Module *LLVMTextureCompilerImpl::m_nodes_module = NULL;

llvm::Type *LLVMTextureCompilerImpl::get_value_type(const TypeSpec *spec, bool is_constant)
{
	return bvm_get_llvm_type(context(), spec, !is_constant);
}

bool LLVMTextureCompilerImpl::use_argument_pointer(const TypeSpec *typespec, bool is_constant)
{
	using namespace llvm;
	
	if (typespec->is_structure()) {
		/* pass by reference */
		return true;
	}
	else if (!is_constant && bvm_type_has_dual_value(typespec)) {
		return true;
	}
	else {
		switch (typespec->base_type()) {
			case BVM_FLOAT:
			case BVM_INT:
				/* pass by value */
				return false;
			case BVM_FLOAT3:
			case BVM_FLOAT4:
			case BVM_MATRIX44:
				/* pass by reference */
				return true;
				
			case BVM_STRING:
			case BVM_RNAPOINTER:
			case BVM_MESH:
			case BVM_DUPLIS:
				/* TODO */
				break;
		}
	}
	
	return false;
}

bool LLVMTextureCompilerImpl::use_elementary_argument_pointer(const TypeSpec *typespec)
{
	using namespace llvm;
	
	if (typespec->is_structure()) {
		/* pass by reference */
		return true;
	}
	else {
		switch (typespec->base_type()) {
			case BVM_FLOAT:
			case BVM_INT:
				/* pass by value */
				return false;
			case BVM_FLOAT3:
			case BVM_FLOAT4:
			case BVM_MATRIX44:
				/* pass by reference */
				return true;
				
			case BVM_STRING:
			case BVM_RNAPOINTER:
			case BVM_MESH:
			case BVM_DUPLIS:
				/* TODO */
				break;
		}
	}
	
	return false;
}

llvm::Constant *LLVMTextureCompilerImpl::create_node_value_constant(const NodeValue *node_value)
{
	return bvm_create_llvm_constant(context(), node_value);
}

llvm::Function *LLVMTextureCompilerImpl::declare_elementary_node_function(llvm::Module *mod, const NodeType *nodetype, const string &name)
{
	using namespace llvm;
	
	std::vector<Type *> input_types, output_types;
	for (int i = 0; i < nodetype->num_inputs(); ++i) {
		const NodeInput *input = nodetype->find_input(i);
		const TypeSpec *typespec = input->typedesc.get_typespec();
		Type *type = bvm_get_llvm_type(context(), typespec, false);
		if (type == NULL)
			break;
		if (use_elementary_argument_pointer(typespec))
			type = type->getPointerTo();
		input_types.push_back(type);
	}
	for (int i = 0; i < nodetype->num_outputs(); ++i) {
		const NodeOutput *output = nodetype->find_output(i);
		const TypeSpec *typespec = output->typedesc.get_typespec();
		Type *type = bvm_get_llvm_type(context(), typespec, false);
		if (type == NULL)
			break;
		output_types.push_back(type);
	}
	if (input_types.size() != nodetype->num_inputs() ||
	    output_types.size() != nodetype->num_outputs()) {
		/* some arguments could not be handled */
		return NULL;
	}
	
	FunctionType *functype = get_node_function_type(input_types, output_types);
	
	Function *func = Function::Create(functype, Function::ExternalLinkage, name, mod);
	return func;
}

bool LLVMTextureCompilerImpl::set_elementary_node_function_impl(OpCode op, const NodeType *UNUSED(nodetype),
                                                                llvm::Function *func, int deriv)
{
	using namespace llvm;
	
	std::vector<Value*> args;
	args.reserve(func->arg_size());
	for (Function::arg_iterator a = func->arg_begin(); a != func->arg_end(); ++a)
		args.push_back(a);
	
	switch (op) {
		case OP_VALUE_FLOAT:
		case OP_VALUE_INT: {
			BasicBlock *block = BasicBlock::Create(context(), "entry", func);
			define_function_OP_VALUE_SINGLE(context(), block, args[0], args[1]);
			return true;
		}
		case OP_VALUE_FLOAT3: {
			BasicBlock *block = BasicBlock::Create(context(), "entry", func);
			define_function_OP_VALUE_AGGREGATE(context(), block, args[0], args[1], sizeof(float3));
			return true;
		}
		case OP_VALUE_FLOAT4: {
			BasicBlock *block = BasicBlock::Create(context(), "entry", func);
			define_function_OP_VALUE_AGGREGATE(context(), block, args[0], args[1], sizeof(float4));
			return true;
		}
		case OP_VALUE_MATRIX44: {
			BasicBlock *block = BasicBlock::Create(context(), "entry", func);
			define_function_OP_VALUE_AGGREGATE(context(), block, args[0], args[1], sizeof(matrix44));
			return true;
		}
		
		default:
			return false;
	}
}

template <OpCode op>
static void define_elementary_functions(LLVMTextureCompilerImpl &C, llvm::Module *mod, const string &nodetype_name)
{
	using namespace llvm;
	
	const NodeType *nodetype = NodeGraph::find_node_type(nodetype_name);
	if (nodetype == NULL)
		return;
	
	Function *value_func = C.declare_elementary_node_function(mod, nodetype,
	                                                          llvm_value_function_name(nodetype->name()));
	/* -1 means the base version, 0..n define the derivative of the n-th argument, if defined */
	bool has_impl = C.set_elementary_node_function_impl(op, nodetype, value_func, -1);
	UNUSED_VARS(has_impl);
	
#if 0 /* TODO only do the value function for now */
	/* 0..n define the derivative of the n-th argument, if it is a dependent variable */
	for (int arg_n = -1; arg_n < nodetype->num_inputs(); ++arg_n) {
		Function *deriv_func = C.declare_elementary_node_function(mod, nodetype,
		                                                          llvm_deriv_function_name(nodetype->name(), arg_n));
		bool has_impl = C.set_elementary_node_function_impl(op, nodetype, deriv_func, arg_n);
		UNUSED_VARS(has_impl);
	}
#endif
}

void LLVMTextureCompilerImpl::define_dual_function_wrapper(llvm::Module *mod, const string &nodetype_name)
{
	using namespace llvm;
	
	const NodeType *nodetype = NodeGraph::find_node_type(nodetype_name);
	if (nodetype == NULL)
		return;
	
	/* get evaluation function(s) */
	Function *value_func = llvm_find_external_function(mod, llvm_value_function_name(nodetype->name()));
	BLI_assert(value_func != NULL && "Could not find node function!");
	
	Function *func = declare_node_function(mod, nodetype);
	if (func == NULL)
		return;
	
	BasicBlock *block = BasicBlock::Create(context(), "entry", func);
	IRBuilder<> builder(context());
	builder.SetInsertPoint(block);
	
	/* collect arguments for calling internal elementary functions */
	/* value and derivative components of input/output duals */
	std::vector<Value*> in_value, in_dx, in_dy;
	std::vector<Value*> out_value, out_dx, out_dy; 
	/* arguments for calculating main value and partial derivatives */
	std::vector<Value*> call_args;
	
	Function::arg_iterator arg_it = func->arg_begin();
	/* output arguments */
	for (int i = 0; i < nodetype->num_outputs(); ++i, ++arg_it) {
		Argument *arg = &(*arg_it);
		const NodeOutput *output = nodetype->find_output(i);
		
		Value *val, *dx, *dy;
		if (bvm_type_has_dual_value(output->typedesc.get_typespec())) {
			val = builder.CreateStructGEP(arg, 0);
			dx = builder.CreateStructGEP(arg, 1);
			dy = builder.CreateStructGEP(arg, 2);
		}
		else {
			val = arg;
			dx = dy = NULL;
		}
		
		out_value.push_back(val);
		out_dx.push_back(dx);
		out_dy.push_back(dy);
		call_args.push_back(val);
	}
	/* input arguments */
	for (int i = 0; i < nodetype->num_inputs(); ++i, ++arg_it) {
		Argument *arg = &(*arg_it);
		const NodeInput *input = nodetype->find_input(i);
		const TypeSpec *typespec = input->typedesc.get_typespec();
		
		Value *val, *dx, *dy;
		if (input->value_type != INPUT_CONSTANT && bvm_type_has_dual_value(typespec)) {
			val = builder.CreateStructGEP(arg, 0);
			dx = builder.CreateStructGEP(arg, 1);
			dy = builder.CreateStructGEP(arg, 2);
			
			if (!use_elementary_argument_pointer(typespec)) {
				val = builder.CreateLoad(val);
				dx = builder.CreateLoad(dx);
				dy = builder.CreateLoad(dy);
			}
		}
		else {
			val = arg;
			dx = dy = NULL;
		}
		
		in_value.push_back(val);
		in_dx.push_back(dx);
		in_dy.push_back(dy);
		call_args.push_back(val);
	}
	
	/* call primary value function */
	builder.CreateCall(value_func, call_args);
	
	builder.CreateRetVoid();
}

void LLVMTextureCompilerImpl::define_nodes_module()
{
	using namespace llvm;
	
	Module *mod = new llvm::Module("texture_nodes", context());
	
#define DEF_OPCODE(op) \
	define_elementary_functions<OP_##op>(*this, mod, STRINGIFY(op));
	
	BVM_DEFINE_OPCODES
	
#undef DEF_OPCODE
	
	/* link base functions into the module */
//	std::string error;
//	Linker::LinkModules(mod, mod_basefuncs, Linker::LinkerMode::PreserveSource, &error);
	
#define DEF_OPCODE(op) \
	define_dual_function_wrapper(mod, STRINGIFY(op));
	
	BVM_DEFINE_OPCODES
	
#undef DEF_OPCODE
	
//	llvm_execution_engine()->addModule(mod_basefuncs);
//	llvm_execution_engine()->addModule(mod);
	
	m_nodes_module = mod;
}

/* ------------------------------------------------------------------------- */

FunctionLLVM *LLVMCompiler::compile_function(const string &name, const NodeGraph &graph, int opt_level)
{
	using namespace llvm;
	
	create_module(name);
	
	Function *func = codegen_node_function(name, graph);
	BLI_assert(module()->getFunction(name) && "Function not registered in module!");
	BLI_assert(func != NULL && "codegen_node_function returned NULL!");
	
	BLI_assert(opt_level >= 0 && opt_level <= 3 && "Invalid optimization level (must be between 0 and 3)");
	optimize_function(func, opt_level);
	
#if 0
	printf("=== NODE FUNCTION ===\n");
	fflush(stdout);
//	func->dump();
	module()->dump();
	printf("=====================\n");
	fflush(stdout);
#endif
	
	verifyFunction(*func, &outs());
	verifyModule(*module(), &outs());
	
	/* Note: Adding module to exec engine before creating the function prevents compilation! */
	llvm_execution_engine()->addModule(module());
	llvm_execution_engine()->generateCodeForModule(module());
	uint64_t address = llvm_execution_engine()->getFunctionAddress(name);
	BLI_assert(address != 0);
	
	llvm_execution_engine()->removeModule(module());
	destroy_module();
	
	FunctionLLVM *fn = new FunctionLLVM(address);
	return fn;
}

/* ------------------------------------------------------------------------- */

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

void DebugLLVMCompiler::compile_function(const string &name, const NodeGraph &graph, int opt_level, FILE *file)
{
	using namespace llvm;
	
	create_module(name);
	
	Function *func = codegen_node_function(name, graph);
	BLI_assert(module()->getFunction(name) && "Function not registered in module!");
	BLI_assert(func != NULL && "codegen_node_function returned NULL!");
	
	BLI_assert(opt_level >= 0 && opt_level <= 3 && "Invalid optimization level (must be between 0 and 3)");
	optimize_function(func, opt_level);
	
	file_ostream stream(file);
	debug_assembly_annotation_writer aaw;
	module()->print(stream, &aaw);
	
	destroy_module();
}

} /* namespace blenvm */

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

/** \file blender/blenvm/llvm/llvm_compiler.cc
 *  \ingroup llvm
 */

#include <cstdio>
#include <set>
#include <sstream>

#include "node_graph.h"

#include "llvm_compiler.h"
#include "llvm_engine.h"
#include "llvm_function.h"
#include "llvm_headers.h"
#include "llvm_modules.h"
#include "llvm_types.h"

#include "util_opcode.h"

#include "modules.h"

namespace blenvm {

Scope::Scope(Scope *parent) :
    parent(parent)
{
}

bool Scope::has_node(const NodeInstance *node) const
{
	/* XXX this is not ideal, but we can expect all outputs
	 * to be mapped once a node is added.
	 */
	ConstOutputKey key(node, node->type->find_output(0));
	return has_value(key);
}

bool Scope::has_value(const ConstOutputKey &key) const
{
	const Scope *scope = this;
	while (scope) {
		SocketValueMap::const_iterator it = scope->values.find(key);
		if (it != scope->values.end()) {
			return true;
		}
		
		scope = scope->parent;
	}
	return false;
}

ValueHandle Scope::find_value(const ConstOutputKey &key) const
{
	const Scope *scope = this;
	while (scope) {
		SocketValueMap::const_iterator it = scope->values.find(key);
		if (it != scope->values.end()) {
			return it->second;
		}
		
		scope = scope->parent;
	}
	return ValueHandle(0);
}

void Scope::set_value(const ConstOutputKey &key, ValueHandle value)
{
	bool ok = values.insert(SocketValueMap::value_type(key, value)).second;
	BLI_assert(ok && "Could not insert socket value!");
	UNUSED_VARS(ok);
}

/* ------------------------------------------------------------------------- */

LLVMCompilerBase::LLVMCompilerBase() :
    m_module(NULL),
    m_globals_ptr(NULL)
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

/* Compile nodes as a simple expression.
 * Every node can be treated as a single statement. Each node is translated
 * into a function call, with regular value arguments. The resulting value is
 * assigned to a variable and can be used for subsequent node function calls.
 */
llvm::BasicBlock *LLVMCompilerBase::codegen_function_body_expression(const NodeGraph &graph, llvm::Function *func)
{
	using namespace llvm;
	
	node_graph_begin();
	/* cache function arguments */
	int num_inputs = graph.inputs.size();
	int num_outputs = graph.outputs.size();
	std::vector<Argument*> input_args(num_inputs), output_args(num_outputs);
	{
		Function::ArgumentListType::iterator arg_it = func->arg_begin();
		m_globals_ptr = arg_it++; /* globals, passed to functions which need it */
		for (int i = 0; i < num_outputs; ++i)
			output_args[i] = arg_it++;
		for (int i = 0; i < num_inputs; ++i)
			input_args[i] = arg_it++;
	}
	
	IRBuilder<> builder(context());
	
	BasicBlock *block = BasicBlock::Create(context(), "entry", func);
	builder.SetInsertPoint(block);
	
	for (int i = 0; i < num_inputs; ++i) {
		const NodeGraph::Input &input = graph.inputs[i];
		const TypeSpec *typespec = input.typedesc.get_typespec();
		
		if (input.key) {
			ValueHandle handle = map_argument(block, typespec, input_args[i]);
			m_argument_values.insert(ArgumentValueMap::value_type(input.key, handle));
		}
	}
	
	Scope scope_main(NULL);
	
	for (int i = 0; i < num_outputs; ++i) {
		const NodeGraph::Output &output = graph.outputs[i];
		const TypeSpec *typespec = output.typedesc.get_typespec();
		
		expand_node(block, output.key.node, scope_main);
		ValueHandle value = scope_main.find_value(output.key);
		
		store_return_value(block, typespec, value, output_args[i]);
	}
	
	builder.CreateRetVoid();
	
	node_graph_end();
	m_globals_ptr = NULL;
	
	return block;
}

llvm::Function *LLVMCompilerBase::codegen_node_function(const string &name, const NodeGraph &graph)
{
	using namespace llvm;
	
	FunctionParameterList input_types, output_types;
	for (int i = 0; i < graph.inputs.size(); ++i) {
		const NodeGraph::Input &input = graph.inputs[i];
		Type *type = get_argument_type(input.typedesc.get_typespec());
		input_types.push_back(FunctionParameter(type, input.name));
	}
	for (int i = 0; i < graph.outputs.size(); ++i) {
		const NodeGraph::Output &output = graph.outputs[i];
		Type *type = get_return_type(output.typedesc.get_typespec());
		output_types.push_back(FunctionParameter(type, output.name));
	}
	
	Function *func = declare_function(module(), name, input_types, output_types, true);
	BLI_assert(func->getArgumentList().size() == 1 + graph.inputs.size() + graph.outputs.size() &&
	           "Error: Function has wrong number of arguments for node tree\n");
	
	codegen_function_body_expression(graph, func);
	
	return func;
}

void LLVMCompilerBase::expand_node(llvm::BasicBlock *block, const NodeInstance *node, Scope &scope)
{
	if (scope.has_node(node))
		return;
	
	switch (node->type->kind()) {
		case NODE_TYPE_FUNCTION:
		case NODE_TYPE_KERNEL:
			expand_expression_node(block, node, scope);
			break;
		case NODE_TYPE_PASS:
			expand_pass_node(block, node, scope);
			break;
		case NODE_TYPE_ARG:
			expand_argument_node(block, node, scope);
			break;
	}
}

void LLVMCompilerBase::expand_pass_node(llvm::BasicBlock *block, const NodeInstance *node, Scope &scope)
{
	using namespace llvm;
	
	BLI_assert(node->num_inputs() == 1);
	BLI_assert(node->num_outputs() == 1);
	
	ConstInputKey input = node->input(0);
	BLI_assert(input.value_type() == INPUT_EXPRESSION);
	
	expand_node(block, input.link().node, scope);
}

void LLVMCompilerBase::expand_argument_node(llvm::BasicBlock *block, const NodeInstance *node, Scope &scope)
{
	using namespace llvm;
	
	BLI_assert(node->num_outputs() == 1);
	
	ConstOutputKey output = node->output(0);
	scope.set_value(output, m_argument_values.at(output));
	
	UNUSED_VARS(block);
}

void LLVMCompilerBase::expand_expression_node(llvm::BasicBlock *block, const NodeInstance *node, Scope &scope)
{
	using namespace llvm;
	
	IRBuilder<> builder(context());
	builder.SetInsertPoint(block);
	
	/* get evaluation function */
	Function *evalfunc = module()->getFunction(node->type->name());
	BLI_assert(evalfunc != NULL && "Could not find node function!");
	
	/* function call arguments */
	std::vector<Value *> args;
	
	if (node->type->use_globals()) {
		args.push_back(m_globals_ptr);
	}
	
	for (int i = 0; i < node->num_outputs(); ++i) {
		ConstOutputKey output = node->output(i);
		const TypeSpec *typespec = output.socket->typedesc.get_typespec();
		
		ValueHandle value = alloc_node_value(block, typespec);
		append_output_arguments(args, typespec, value);
		
		scope.set_value(output, value);
	}
	
	/* set input arguments */
	for (int i = 0; i < node->num_inputs(); ++i) {
		ConstInputKey input = node->input(i);
		const TypeSpec *typespec = input.socket->typedesc.get_typespec();
		
		switch (input.value_type()) {
			case INPUT_CONSTANT: {
				append_input_constant(block, args, typespec, input.value());
				break;
			}
			case INPUT_EXPRESSION: {
				expand_node(block, input.link().node, scope);
				
				ValueHandle link_value = scope.find_value(input.link());
				append_input_value(block, args, typespec, link_value);
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

llvm::Function *LLVMCompilerBase::declare_function(llvm::Module *mod, const string &name,
                                                   const FunctionParameterList &input_types,
                                                   const FunctionParameterList &output_types,
                                                   bool use_globals)
{
	using namespace llvm;
	
	std::vector<llvm::Type*> arg_types;
	
	if (use_globals) {
		Type *t_globals = llvm::TypeBuilder<void*, false>::get(context());
		arg_types.push_back(t_globals);
	}
	
	for (int i = 0; i < output_types.size(); ++i) {
		Type *type = output_types[i].type;
		/* use a pointer to store output values */
		arg_types.push_back(type->getPointerTo());
	}
	for (int i = 0; i < input_types.size(); ++i) {
		Type *type = input_types[i].type;
		arg_types.push_back(type);
	}
	
	FunctionType *functype = FunctionType::get(TypeBuilder<void, true>::get(context()), arg_types, false);
	Function *func = Function::Create(functype, Function::ExternalLinkage, name, mod);
	
	Function::arg_iterator it = func->arg_begin();
	if (use_globals) {
		(it++)->setName("globals");
	}
	for (size_t i = 0; i < output_types.size(); ++i) {
		(it++)->setName(output_types[i].name);
	}
	for (size_t i = 0; i < input_types.size(); ++i) {
		(it++)->setName(input_types[i].name);
	}
	
	return func;
}

llvm::Function *LLVMCompilerBase::declare_node_function(llvm::Module *mod, const NodeType *nodetype)
{
	using namespace llvm;
	
	FunctionParameterList input_types, output_types;
	for (int i = 0; i < nodetype->num_inputs(); ++i) {
		const NodeInput *input = nodetype->find_input(i);
		append_input_types(input_types, input);
	}
	for (int i = 0; i < nodetype->num_outputs(); ++i) {
		const NodeOutput *output = nodetype->find_output(i);
		append_output_types(output_types, output);
	}
	
	return declare_function(mod, nodetype->name(), input_types, output_types, nodetype->use_globals());
}

/* ------------------------------------------------------------------------- */

FunctionLLVM *LLVMCompilerBase::compile_function(const string &name, const NodeGraph &graph, int opt_level)
{
	using namespace llvm;
	
	create_module(name);
	
	Function *func = codegen_node_function(name, graph);
	BLI_assert(module()->getFunction(name) && "Function not registered in module!");
	BLI_assert(func != NULL && "codegen_node_function returned NULL!");
	
	llvm_optimize_module(module(), opt_level);
	llvm_optimize_function(func, opt_level);
	
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

void LLVMCompilerBase::debug_function(const string &name, const NodeGraph &graph, int opt_level, FILE *file)
{
	using namespace llvm;
	
	create_module(name);
	
	Function *func = codegen_node_function(name, graph);
	BLI_assert(module()->getFunction(name) && "Function not registered in module!");
	BLI_assert(func != NULL && "codegen_node_function returned NULL!");
	
	llvm_optimize_module(module(), opt_level);
	llvm_optimize_function(func, opt_level);
	
	file_ostream stream(file);
	debug_assembly_annotation_writer aaw;
	module()->print(stream, &aaw);
	
	destroy_module();
}

} /* namespace blenvm */

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

/** \file llvm_codegen.cc
 *  \ingroup llvm
 */

#include <set>

#include "nodegraph.h"

#include "llvm_codegen.h"
#include "llvm_engine.h"
#include "llvm_function.h"
#include "llvm_headers.h"

namespace blenvm {

LLVMCompiler::LLVMCompiler()
{
}

LLVMCompiler::~LLVMCompiler()
{
}

llvm::Type *LLVMCompiler::codegen_typedesc(TypeDesc *td)
{
	using namespace llvm;
	
	// TODO
	return NULL;
}

llvm::FunctionType *LLVMCompiler::codegen_node_function_type(const NodeGraph &graph)
{
	using namespace llvm;
	
	LLVMContext &context = getGlobalContext();
	
	std::vector<Type *> input_types, output_types;
	
#if 0
	for (int i = 0; i < graph.outputs.size(); ++i) {
#if 0 // TODO
		const NodeGraph::Output &output = graph.outputs[i];
		Type *type = bjit_get_socket_llvm_type(output.type, context, module);
		output_types.push_back(type);
#endif
	}
	StructType *return_type = StructType::get(context, output_types);
	
	input_types.push_back(PointerType::get(return_type, 0));
	for (int i = 0; i < graph.inputs.size(); ++i) {
#if 0 // TODO
		const NodeGraph::Input &input = graph.inputs[i];
		Type *type = bjit_get_socket_llvm_type(input.type, context, module);
		type = PointerType::get(type, 0);
		input_types.push_back(type);
#endif
	}
#endif
	
	return FunctionType::get(TypeBuilder<void, true>::get(context), input_types, false);
}

llvm::Function *LLVMCompiler::codegen_node_function(const string &name, const NodeGraph &graph, llvm::Module *module)
{
	using namespace llvm;
	
	LLVMContext &context = getGlobalContext();
	IRBuilder<> builder(context);
	
	FunctionType *functype = codegen_node_function_type(graph);
//	Function *func = Function::Create(functype, Function::ExternalLinkage, name, module);
	Function *func = llvm::cast<Function>(module->getOrInsertFunction(name, functype));
#if 0
	Argument *retarg = func->getArgumentList().begin();
	retarg->addAttr(AttributeSet::get(context, AttributeSet::ReturnIndex, Attribute::StructRet));
	
	int num_inputs = graph.inputs.size();
	int num_outputs = graph.outputs.size();
	
	if (func->getArgumentList().size() != num_inputs + 1) {
		printf("Error: Function has wrong number of arguments for node tree\n");
		return func;
	}
	
	Function::ArgumentListType::iterator it = func->getArgumentList().begin();
//	++it; /* skip return arg */
	for (int i = 0; i < num_inputs; ++i) {
//		Argument *arg = &(*it++);
//		const NodeGraph::Input &input = graph.inputs[i];
		
//		graph.set_input_argument(input.name, arg);
	}
#endif
	
	BasicBlock *entry = BasicBlock::Create(context, "entry", func);
	builder.SetInsertPoint(entry);
	
#if 0 // TODO
	NodeRefList sorted_nodes = toposort_nodes(graph);
	for (NodeRefList::iterator it = sorted_nodes.begin(); it != sorted_nodes.end(); ++it) {
		NodeInstance *node = *it;
		
		CallInst *call = codegen_node_function_call(builder, module, node);
		if (!call)
			continue;
	}
#endif
	
#if 0
	for (int i = 0; i < num_outputs; ++i) {
		Value *retptr = builder.CreateStructGEP(retarg, i);
		
#if 0 // TODO
		const NodeGraph::Output &output = graph.outputs[i];
		Value *value = NULL;
		if (output->link_node && output->link_socket) {
			value = output->link_node->find_output_value(output->link_socket->name);
		}
		else {
			value = output->default_value;
		}
		BLI_assert(value);
		
		Value *retval = builder.CreateLoad(value);
		builder.CreateStore(retval, retptr);
#endif
	}
#endif
	
	builder.CreateRetVoid();
	
	return func;
}

FunctionLLVM *LLVMCompiler::compile_function(const string &name, const NodeGraph &graph)
{
	using namespace llvm;
	
	Module *module = new Module(name, getGlobalContext());
	llvm_execution_engine()->addModule(module);
	
	Function *func = codegen_node_function(name, graph, module);
	assert(func != NULL && "codegen_node_function returned NULL!");
	
	FunctionPassManager fpm(module);
	PassManagerBuilder builder;
	builder.OptLevel = 0;
	builder.populateFunctionPassManager(fpm);
//	builder.populateModulePassManager(MPM);
	
	fpm.run(*func);
	
	llvm_execution_engine()->finalizeObject();
	
	/* XXX according to docs, getFunctionAddress should be used
	 * for MCJIT, but always returns 0
	 */
//	uint64_t address = llvm_execution_engine()->getFunctionAddress(name);
//	uint64_t address = (uint64_t)llvm_execution_engine()->getPointerToFunction(func);
	void (*fp)(void) = (void (*)(void))llvm_execution_engine()->getPointerToFunction(func);
	printf("GOT FUNCTION %s: %p\n", name.c_str(), fp);
	BLI_assert(fp != 0);
	uint64_t address = (uint64_t)fp;
	
	llvm_execution_engine()->removeModule(module);
	delete module;
	
	FunctionLLVM *fn = new FunctionLLVM(address);
	return fn;
}

} /* namespace blenvm */

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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenjit/intern/codegen.cpp
 *  \ingroup bjit
 */

#include <map>
#include <set>
#include <string>
#include <iostream>

#include "bjit_intern.h"
#include "bjit_llvm.h"
#include "bjit_nodegraph.h"
#include "BJIT_forcefield.h"

namespace bjit {

using namespace llvm;

static Value *codegen_array_to_pointer(IRBuilder<> &builder, Value *array)
{
	Constant *index = ConstantInt::get(builder.getContext(), APInt(32, 0));
	Value *indices[2] = { index, index };
	return builder.CreateInBoundsGEP(array, ArrayRef<Value*>(indices));
}

static Value *codegen_struct_to_pointer(IRBuilder<> &builder, Value *s)
{
	return builder.CreateStructGEP(s, 0);
}

static Value *codegen_const_to_value(IRBuilder<> &builder, Value *valconst)
{
//	if (dyn_cast<ConstantDataArray>(valconst)) {
		AllocaInst *alloc = builder.CreateAlloca(valconst->getType());
//		return builder.CreateStore(valconst, alloc);
		return alloc;
//	}
//	else
//		return valconst;
}

static Value *codegen_get_node_input_value(IRBuilder<> &builder, NodeInstance *node, int index)
{
	Value *value = NULL;
	
	NodeInstance *link_node = node->find_input_link_node(index);
	const NodeSocket *link_socket = node->find_input_link_socket(index);
	if (link_node && link_socket) {
		/* use linked input value */
		value = link_node->find_output_value(link_socket->name);
		BLI_assert(value);
	}
	
	/* use input constant */
	if (!value) {
		Value *valconst = node->find_input_value(index);
		if (valconst) {
			value = codegen_const_to_value(builder, valconst);
		}
	}
	
	/* last resort: socket default */
	if (!value) {
		Value *valconst = node->type->find_input(index)->default_value;
		if (valconst) {
			value = codegen_const_to_value(builder, valconst);
		}
	}
	
	BLI_assert(value);
	
	// XXX how to test this? put explicitly into type template?
//	if (dyn_cast<ArrayType>(value))
	{
		value = codegen_array_to_pointer(builder, value);
	}
	
	return value;
}

static CallInst *codegen_node_function_call(IRBuilder<> &builder, Module *module, NodeInstance *node)
{
//	LLVMContext &context = getGlobalContext();
	
	/* get evaluation function */
	const std::string &evalname = node->type->name;
	Function *evalfunc = bjit_find_function(module, evalname);
	if (!evalfunc) {
		printf("Could not find node function '%s'\n", evalname.c_str());
		return NULL;
	}
	
	/* function call arguments (including possible return struct if MRV is used) */
	std::vector<Value *> args;
	
	Value *retval = NULL;
	if (evalfunc->hasStructRetAttr()) {
		Argument *retarg = &(*evalfunc->getArgumentList().begin());
		AllocaInst *alloc = builder.CreateAlloca(retarg->getType()->getPointerElementType());
		retval = alloc;
		args.push_back(retval);
	}
	
	/* set input arguments */
	int num_inputs = node->type->inputs.size();
	for (int i = 0; i < num_inputs; ++i) {
		Value *value = codegen_get_node_input_value(builder, node, i);
		if (!value) {
			const NodeSocket *socket = node->type->find_input(i);
			printf("Error: no input value defined for '%s':'%s'\n", node->name.c_str(), socket->name.c_str());
		}
		
		args.push_back(value);
	}
	
	CallInst *call = builder.CreateCall(evalfunc, args);
	if (!retval)
		retval = call;
	
	int num_outputs = node->type->outputs.size();
	for (int i = 0; i < num_outputs; ++i) {
		const NodeSocket *socket = node->type->find_output(i);
		Value *value = builder.CreateStructGEP(retval, i);
		if (!value) {
			printf("Error: no output value defined for '%s':'%s'\n", node->name.c_str(), socket->name.c_str());
		}
		
		/* use as node output values */
		node->set_output_value(socket->name, value);
	}
	
	return call;
}

typedef std::vector<NodeInstance *> NodeRefList;
typedef std::set<NodeInstance *> NodeRefSet;
static void toposort_nodes_insert(NodeRefList &result, NodeRefSet &visited, NodeInstance *node)
{
	if (visited.find(node) != visited.end())
		return;
	visited.insert(node);
	
	for (int i = 0; i < node->inputs.size(); ++i) {
		NodeInstance *link = node->find_input_link_node(i);
		if (link)
			toposort_nodes_insert(result, visited, link);
	}
	
	result.push_back(node);
}
static NodeRefList toposort_nodes(NodeGraph &graph)
{
	NodeGraph::NodeInstanceMap::iterator it;
	
	NodeRefList list;
	NodeRefSet visited;
	for (it = graph.nodes.begin(); it != graph.nodes.end(); ++it) {
		toposort_nodes_insert(list, visited, &it->second);
	}
	
	return list;
}

static void codegen_nodegraph(NodeGraph &graph, Module *module, Function *func)
{
	LLVMContext &context = getGlobalContext();
	IRBuilder<> builder(context);
	
	BasicBlock *entry = BasicBlock::Create(context, "entry", func);
	builder.SetInsertPoint(entry);
	
	NodeRefList sorted_nodes = toposort_nodes(graph);
	for (NodeRefList::iterator it = sorted_nodes.begin(); it != sorted_nodes.end(); ++it) {
		NodeInstance *node = *it;
		
		CallInst *call = codegen_node_function_call(builder, module, node);
		if (!call)
			continue;
	}
	
	int num_outputs = graph.outputs.size();
	
	Argument *retarg = func->getArgumentList().begin();
	for (int i = 0; i < num_outputs; ++i) {
		Value *retptr = builder.CreateStructGEP(retarg, i);
		
		const NodeGraph::Output *output = graph.get_output(i);
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
	}
	
#if 0
	/* DEBUG */
	{
		Function *f = bjit_find_function(module, "print_result");
		std::vector<Value *> args;
		args.push_back(retarg);
		builder.CreateCall(f, args);
	}
#endif
	
	builder.CreateRetVoid();
}

Function *codegen(NodeGraph &graph, Module *module)
{
	LLVMContext &context = getGlobalContext();
	
	std::vector<Type *> input_types, output_types;
	
	int num_inputs = graph.inputs.size();
	int num_outputs = graph.outputs.size();
	output_types.reserve(num_outputs);
	for (int i = 0; i < num_outputs; ++i) {
		const NodeGraph::Output &output = graph.outputs[i];
		output_types.push_back(bjit_get_socket_llvm_type(output.type, context));
	}
	StructType *return_type = StructType::get(context, output_types);
	
	input_types.reserve(num_inputs + 1);
	input_types.push_back(PointerType::get(return_type, 0));
	for (int i = 0; i < num_inputs; ++i) {
		const NodeGraph::Input &input = graph.inputs[i];
		input_types.push_back(bjit_get_socket_llvm_type(input.type, context));
	}
	
	FunctionType *functype = FunctionType::get(TypeBuilder<void, true>::get(context), input_types, false);
	
	Function *func = Function::Create(functype, Function::ExternalLinkage, "effector", module);
	Argument *retarg = func->getArgumentList().begin();
	retarg->addAttr(AttributeSet::get(context, AttributeSet::ReturnIndex, Attribute::StructRet));
	
	codegen_nodegraph(graph, module, func);
	
	return func;
}

#if 0
static void bjit_gen_nodetree_function(NodeInstanceMap &nodes)
{
	LLVMContext &context = getGlobalContext();
	IRBuilder<> builder(context);
	
	FunctionType *functype = TypeBuilder<void(const EffectorEvalInput*, EffectorEvalResult*), true>::get(context);
	
	Function *func = Function::Create(functype, Function::ExternalLinkage, "effector", theModule);
	Value *arg_input, *arg_result;
	{
		Function::ArgumentListType::iterator it = func->arg_begin();
		arg_input = it++;
		arg_result = it++;
	}
	
	BasicBlock *entry = BasicBlock::Create(context, "entry", func);
	builder.SetInsertPoint(entry);
	
	/* initialize result */
	Value *res_force = builder.CreateStructGEP(arg_result, 0);
	Value *res_impulse = builder.CreateStructGEP(arg_result, 1);
	Value *vec3_zero = ConstantDataArray::get(context, std::vector<float>({0.0f, 0.0f, 0.0f}));
	builder.CreateStore(vec3_zero, res_force);
	builder.CreateStore(vec3_zero, res_impulse);
	
	for (EffectorCache *eff = (EffectorCache *)effctx->effectors.first; eff; eff = eff->next) {
		if (!eff->ob || !eff->pd)
			continue;
		
		std::string prefix = get_effector_prefix(eff->pd->forcefield);
		if (prefix.empty()) {
			/* undefined force type */
			continue;
		}
		
		std::string funcname = prefix + "_eval";
		
		Value *arg_settings = make_effector_settings(builder, eff);
		Value *arg_eff_result = make_effector_result(builder);
		
		{
			SocketValueMap inputs, outputs;
			inputs["input"] = arg_input;
			inputs["settings"] = arg_settings;
			outputs["result"] = arg_eff_result;
			gen_node_function_call(builder, funcname, inputs, outputs);
		}
		
		/* call function to combine results */
		{
			SocketValueMap inputs, outputs;
			inputs["R"] = arg_result;
			inputs["a"] = arg_result;
			outputs["b"] = arg_eff_result;
			gen_node_function_call(builder, "effector_result_combine", inputs, outputs);
		}
	}
	
	builder.CreateRetVoid();
	
	verifyFunction(*func, &outs());
	bjit_finalize_function(theModule, func, 2);
	
//	printf("=== The Module ===\n");
//	theModule->dump();
//	printf("==================\n");
	
	effctx->eval = (EffectorEvalFp)bjit_compile_function(func);
	effctx->eval_data = func;
}
#endif

} /* namespace bjit */

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

#include "bjit_intern.h"
#include "bjit_llvm.h"
#include "bjit_nodegraph.h"
#include "BJIT_forcefield.h"

namespace bjit {

using namespace llvm;

static CallInst *codegen_node_function_call(IRBuilder<> &builder, Module *module, NodeInstance *node)
{
	/* get evaluation function */
	const std::string &evalname = node->type->name;
	Function *evalfunc = bjit_find_function(module, evalname);
	if (!evalfunc) {
		printf("Could not find node function '%s'\n", evalname.c_str());
		return NULL;
	}
	
	/* set input arguments */
	int num_inputs = node->type->inputs.size();
	std::vector<Value *> args;
	args.reserve(num_inputs);
	for (int i = 0; i < num_inputs; ++i) {
		Value *value = NULL;
		
		NodeInstance *link_node = node->find_input_link_node(i);
		const NodeSocket *link_socket = node->find_input_link_socket(i);
		if (link_node && link_socket) {
			/* use linked input value */
			value = node->find_output_value(link_socket->name);
		}
		else if ((value = node->find_input_value(i))) {
			/* use input constant */
		}
		else {
			/* last resort: socket default */
			value = node->type->find_input(i)->default_value;
		}
		if (!value) {
			const NodeSocket *socket = node->type->find_input(i);
			printf("Error: no input value defined for '%s':'%s'\n", node->name.c_str(), socket->name.c_str());
		}
		
		args.push_back(value);
	}
#if 0
	std::vector<Value *> args;
	Function::ArgumentListType::iterator it = evalfunc->arg_begin();
	for (int i = 0; it != evalfunc->arg_end(); ++it, ++i) {
		Argument *arg = it;
		printf("FUN %s(%s)\n", evalfunc->getName().str().c_str(), arg->getName().str().c_str());
	}
#endif
	
	CallInst *call = builder.CreateCall(evalfunc, args);
	
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
	
//	Value *arg_input, *arg_result;
//	{
//		Function::ArgumentListType::iterator it = func->arg_begin();
//		arg_input = it++;
//		arg_result = it++;
//	}
	
	NodeRefList sorted_nodes = toposort_nodes(graph);
	for (NodeRefList::iterator it = sorted_nodes.begin(); it != sorted_nodes.end(); ++it) {
		NodeInstance *node = *it;
		
		CallInst *call = codegen_node_function_call(builder, module, node);
		if (!call)
			continue;
	}
}

Function *codegen(NodeGraph &graph, Module *module)
{
	LLVMContext &context = getGlobalContext();
	
//	Type *return_type = TypeBuilder<void, true>::get(context);
	std::vector<Type *> input_types, output_types;
	
	int num_inputs = graph.inputs.size();
	int num_outputs = graph.outputs.size();
	input_types.reserve(num_inputs);
	output_types.reserve(num_outputs);
	for (int i = 0; i < num_inputs; ++i) {
		const NodeGraph::Input &input = graph.inputs[i];
		input_types.push_back(bjit_get_socket_llvm_type(input.type, context));
	}
	for (int i = 0; i < num_outputs; ++i) {
		const NodeGraph::Output &output = graph.outputs[i];
		output_types.push_back(bjit_get_socket_llvm_type(output.type, context));
	}
	
	StructType *return_type = StructType::get(context, output_types);
	FunctionType *functype = FunctionType::get(return_type, input_types, false);
	
	Function *func = Function::Create(functype, Function::ExternalLinkage, "effector", module);
	
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

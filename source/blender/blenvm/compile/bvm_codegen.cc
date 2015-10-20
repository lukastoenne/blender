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

/** \file bvm_codegen.cc
 *  \ingroup bvm
 */

#include <cstdio>
#include <set>

#include "bvm_codegen.h"
#include "bvm_eval.h"
#include "bvm_nodegraph.h"

namespace bvm {

BVMCompiler::BVMCompiler() :
    expr(NULL)
{
	stack_users.resize(BVM_STACK_SIZE, 0);
}

BVMCompiler::~BVMCompiler()
{
}

StackIndex BVMCompiler::find_stack_index(int size) const
{
	int unused = 0;
	
	for (int i = 0; i < BVM_STACK_SIZE; ++i) {
		if (stack_users[i] == 0) {
			++unused;
			if (unused == size)
				return i + 1 - size;
		}
		else
			unused = 0;
	}
	
	// TODO better reporting ...
	printf("ERROR: out of stack space");
	
	return BVM_STACK_INVALID;
}

StackIndex BVMCompiler::assign_stack_index(const TypeDesc &typedesc)
{
	int size = typedesc.stack_size();
	
	StackIndex stack_offset = find_stack_index(size);
	for (int i = 0; i < size; ++i) {
		// TODO keep track of value users
		stack_users[stack_offset + i] += 1;
	}
	
	return stack_offset;
}

void BVMCompiler::push_opcode(OpCode op)
{
	expr->add_instruction(op);
}

void BVMCompiler::push_stack_index(StackIndex arg)
{
	if (arg != BVM_STACK_INVALID)
		expr->add_instruction(arg);
}

void BVMCompiler::push_float(float f)
{
	expr->add_instruction(float_to_instruction(f));
}

void BVMCompiler::push_float3(float3 f)
{
	expr->add_instruction(float_to_instruction(f.x));
	expr->add_instruction(float_to_instruction(f.y));
	expr->add_instruction(float_to_instruction(f.z));
}

StackIndex BVMCompiler::codegen_value(const Value *value)
{
	StackIndex offset = assign_stack_index(value->typedesc());
	
	switch (value->typedesc().base_type) {
		case BVM_FLOAT: {
			float f = 0.0f;
			value->get(&f);
			
			push_opcode(OP_VALUE_FLOAT);
			push_float(f);
			push_stack_index(offset);
			break;
		}
		case BVM_FLOAT3: {
			float3 f = float3(0.0f, 0.0f, 0.0f);
			value->get(&f);
			
			push_opcode(OP_VALUE_FLOAT3);
			push_float3(f);
			push_stack_index(offset);
			break;
		}
	}
	
	return offset;
}

#if 0
StackIndex BVMCompiler::codegen_link(const TypeDesc &from, StackIndex stack_from,
                                     const TypeDesc &to, StackIndex stack_to)
{
	if (to.assignable(from)) {
		switch (to.base_type) {
			case BVM_FLOAT:
				push_opcode(OP_PASS_FLOAT);
				break;
			case BVM_FLOAT3:
				push_opcode(OP_PASS_FLOAT3);
				break;
		}
		push_stack_index(stack_from);
		push_stack_index(stack_to);
	}
}
#endif

typedef std::vector<const NodeInstance *> NodeList;
typedef std::set<const NodeInstance *> NodeSet;

static void sort_nodes_append(const NodeInstance *node, NodeList &result, NodeSet &visited)
{
	if (visited.find(node) != visited.end())
		return;
	visited.insert(node);
	
	for (size_t i = 0; i < node->inputs.size(); ++i) {
		const NodeInstance *link_node = node->find_input_link_node(i);
		if (link_node) {
			sort_nodes_append(link_node, result, visited);
		}
	}
	
	result.push_back(node);
}

static void sort_nodes(const NodeGraph &graph, NodeList &result)
{
	NodeSet visited;
	
	for (NodeGraph::NodeInstanceMap::const_iterator it = graph.nodes.begin(); it != graph.nodes.end(); ++it) {
		sort_nodes_append(&it->second, result, visited);
	}
}

Expression *BVMCompiler::codegen_expression(const NodeGraph &graph)
{
	typedef std::pair<const NodeInstance *, const NodeSocket *> SocketPair;
	typedef std::map<SocketPair, StackIndex> SocketIndexMap;
	
	expr = new Expression();
	
	NodeList sorted_nodes;
	sort_nodes(graph, sorted_nodes);
	
	SocketIndexMap socket_index;
	
	for (NodeList::const_iterator it = sorted_nodes.begin(); it != sorted_nodes.end(); ++it) {
		const NodeInstance &node = **it;
		
		for (int i = 0; i < node.type->inputs.size(); ++i) {
			const NodeSocket &input = node.type->inputs[i];
			SocketPair key(&node, &input);
			
			if (node.has_input_link(i)) {
				const NodeInstance *link_node = node.find_input_link_node(i);
				const NodeSocket *link_socket = node.find_input_link_socket(i);
				SocketPair link_key(link_node, link_socket);
				socket_index[key] = socket_index[link_key];
			}
			else if (node.has_input_value(i)) {
				Value *value = node.find_input_value(i);
				socket_index[key] = codegen_value(value);
			}
			else {
				socket_index[key] = codegen_value(input.default_value);
			}
		}
		
		OpCode op = get_opcode_from_node_type(node.type->name);
		push_opcode(op);
		
		for (int i = 0; i < node.type->inputs.size(); ++i) {
			const NodeSocket &input = node.type->inputs[i];
			SocketPair key(&node, &input);
			
			push_stack_index(socket_index[key]);
		}
		for (int i = 0; i < node.type->outputs.size(); ++i) {
			const NodeSocket &output = node.type->outputs[i];
			SocketPair key(&node, &output);
			
			socket_index[key] = assign_stack_index(TypeDesc(output.type));
			
			push_stack_index(socket_index[key]);
		}
	}
	
	for (NodeGraph::OutputList::const_iterator it = graph.outputs.begin();
	     it != graph.outputs.end();
	     ++it) {
		const NodeGraphOutput &output = *it;
		
		ReturnValue &rval = expr->add_return_value(TypeDesc(output.type), output.name);
		
		if (output.link_node && output.link_socket) {
			SocketPair link_key(output.link_node, output.link_socket);
			
			rval.stack_offset = socket_index[link_key];
		}
		else {
			rval.stack_offset = assign_stack_index(rval.typedesc);
		}
	}
	
#if 0
	// XXX TESTING
	{
		push_opcode(OP_VALUE_FLOAT3);
		push_float3(float3(0.3, -0.6, 0.0));
		push_stack_index(expr->return_value(0).stack_offset);
	}
#endif
	
	push_opcode(OP_END);
	
	Expression *result = expr;
	expr = NULL;
	return result;
}

} /* namespace bvm */

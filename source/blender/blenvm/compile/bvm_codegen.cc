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
#include "bvm_function.h"
#include "bvm_nodegraph.h"

namespace bvm {

BVMCompiler::BVMCompiler() :
    fn(NULL)
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
	fn->add_instruction(op);
}

void BVMCompiler::push_stack_index(StackIndex arg)
{
	if (arg != BVM_STACK_INVALID)
		fn->add_instruction(arg);
}

void BVMCompiler::push_float(float f)
{
	fn->add_instruction(float_to_instruction(f));
}

void BVMCompiler::push_float3(float3 f)
{
	fn->add_instruction(float_to_instruction(f.x));
	fn->add_instruction(float_to_instruction(f.y));
	fn->add_instruction(float_to_instruction(f.z));
}

void BVMCompiler::push_float4(float4 f)
{
	fn->add_instruction(float_to_instruction(f.x));
	fn->add_instruction(float_to_instruction(f.y));
	fn->add_instruction(float_to_instruction(f.z));
	fn->add_instruction(float_to_instruction(f.w));
}

void BVMCompiler::push_int(int i)
{
	fn->add_instruction(int_to_instruction(i));
}

void BVMCompiler::push_matrix44(matrix44 m)
{
	fn->add_instruction(float_to_instruction(m.data[0][0]));
	fn->add_instruction(float_to_instruction(m.data[0][1]));
	fn->add_instruction(float_to_instruction(m.data[0][2]));
	fn->add_instruction(float_to_instruction(m.data[0][3]));
	fn->add_instruction(float_to_instruction(m.data[1][0]));
	fn->add_instruction(float_to_instruction(m.data[1][1]));
	fn->add_instruction(float_to_instruction(m.data[1][2]));
	fn->add_instruction(float_to_instruction(m.data[1][3]));
	fn->add_instruction(float_to_instruction(m.data[2][0]));
	fn->add_instruction(float_to_instruction(m.data[2][1]));
	fn->add_instruction(float_to_instruction(m.data[2][2]));
	fn->add_instruction(float_to_instruction(m.data[2][3]));
	fn->add_instruction(float_to_instruction(m.data[3][0]));
	fn->add_instruction(float_to_instruction(m.data[3][1]));
	fn->add_instruction(float_to_instruction(m.data[3][2]));
	fn->add_instruction(float_to_instruction(m.data[3][3]));
}

void BVMCompiler::push_pointer(PointerRNA p)
{
	fn->add_instruction(pointer_to_instruction_hi(p.id.data));
	fn->add_instruction(pointer_to_instruction_lo(p.id.data));
	fn->add_instruction(pointer_to_instruction_hi(p.type));
	fn->add_instruction(pointer_to_instruction_lo(p.type));
	fn->add_instruction(pointer_to_instruction_hi(p.data));
	fn->add_instruction(pointer_to_instruction_lo(p.data));
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
		case BVM_FLOAT4: {
			float4 f = float4(0.0f, 0.0f, 0.0f, 0.0f);
			value->get(&f);
			
			push_opcode(OP_VALUE_FLOAT4);
			push_float4(f);
			push_stack_index(offset);
			break;
		}
		case BVM_INT: {
			int i = 0;
			value->get(&i);
			
			push_opcode(OP_VALUE_INT);
			push_int(i);
			push_stack_index(offset);
			break;
		}
		case BVM_MATRIX44: {
			matrix44 m = matrix44::identity();
			value->get(&m);
			
			push_opcode(OP_VALUE_MATRIX44);
			push_matrix44(m);
			push_stack_index(offset);
			break;
		}
		case BVM_POINTER: {
			PointerRNA p = PointerRNA_NULL;
			value->get(&p);
			
			push_opcode(OP_VALUE_POINTER);
			push_pointer(p);
			push_stack_index(offset);
			break;
		}
		
		case BVM_MESH:
			push_opcode(OP_VALUE_MESH);
			push_stack_index(offset);
			break;
	}
	
	return offset;
}

void BVMCompiler::codegen_constant(const Value *value)
{
	switch (value->typedesc().base_type) {
		case BVM_FLOAT: {
			float f = 0.0f;
			value->get(&f);
			
			push_float(f);
			break;
		}
		case BVM_FLOAT3: {
			float3 f = float3(0.0f, 0.0f, 0.0f);
			value->get(&f);
			
			push_float3(f);
			break;
		}
		case BVM_FLOAT4: {
			float4 f = float4(0.0f, 0.0f, 0.0f, 0.0f);
			value->get(&f);
			
			push_float4(f);
			break;
		}
		case BVM_INT: {
			int i = 0;
			value->get(&i);
			
			push_int(i);
			break;
		}
		case BVM_MATRIX44: {
			matrix44 m = matrix44::identity();
			value->get(&m);
			
			push_matrix44(m);
			break;
		}
		case BVM_POINTER: {
			PointerRNA p = PointerRNA_NULL;
			value->get(&p);
			
			push_pointer(p);
			break;
		}
		
		case BVM_MESH:
			break;
	}
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

Function *BVMCompiler::codegen_function(const NodeGraph &graph)
{
	typedef std::pair<const NodeInstance *, const NodeSocket *> SocketPair;
	typedef std::map<SocketPair, StackIndex> SocketIndexMap;
	
	fn = new Function();
	
	NodeList sorted_nodes;
	sort_nodes(graph, sorted_nodes);
	
	SocketIndexMap socket_index;
	
	for (NodeList::const_iterator it = sorted_nodes.begin(); it != sorted_nodes.end(); ++it) {
		const NodeInstance &node = **it;
		
		for (int i = 0; i < node.type->inputs.size(); ++i) {
			const NodeSocket &input = node.type->inputs[i];
			SocketPair key(&node, &input);
			
			if (node.is_input_constant(i)) {
				/* value is stored directly in the instructions list,
				 * after the opcode
				 */
			}
			else if (node.has_input_link(i)) {
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
			
			if (node.is_input_constant(i)) {
				Value *value = node.find_input_value(i);
				codegen_constant(value);
			}
			else {
				push_stack_index(socket_index[key]);
			}
		}
		for (int i = 0; i < node.type->outputs.size(); ++i) {
			const NodeSocket &output = node.type->outputs[i];
			SocketPair key(&node, &output);
			
			socket_index[key] = assign_stack_index(output.typedesc);
			
			push_stack_index(socket_index[key]);
		}
	}
	
	for (NodeGraph::OutputList::const_iterator it = graph.outputs.begin();
	     it != graph.outputs.end();
	     ++it) {
		const NodeGraphOutput &output = *it;
		
		ReturnValue &rval = fn->add_return_value(TypeDesc(output.type), output.name);
		
		if (output.link_node && output.link_socket) {
			SocketPair link_key(output.link_node, output.link_socket);
			
			rval.stack_offset = socket_index[link_key];
		}
		else {
			rval.stack_offset = codegen_value(output.default_value);
		}
	}
	
	push_opcode(OP_END);
	
	Function *result = fn;
	fn = NULL;
	return result;
}

} /* namespace bvm */

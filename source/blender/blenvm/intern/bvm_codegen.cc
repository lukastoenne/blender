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

void BVMCompiler::assign_stack_index(ReturnValue &rval)
{
	int size = rval.typedesc.stack_size();
	
	rval.stack_offset = find_stack_index(size);
	for (int i = 0; i < size; ++i) {
		// TODO keep track of value users
		stack_users[rval.stack_offset + i] += 1;
	}
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

Expression *BVMCompiler::codegen_expression(const NodeGraph &graph)
{
	expr = new Expression();
	
	for (NodeGraph::OutputList::const_iterator it = graph.outputs.begin();
	     it != graph.outputs.end();
	     ++it) {
		const NodeGraphOutput &output = *it;
		
		ReturnValue &rval = expr->add_return_value(TypeDesc(output.type), output.name);
		
		assign_stack_index(rval);
	}
	
	for (NodeGraph::NodeInstanceMap::const_iterator it = graph.nodes.begin();
	     it != graph.nodes.end();
	     ++it) {
		const NodeInstance &node = it->second;
		
	}
	
	// XXX TESTING
	{
		push_opcode(OP_VALUE_FLOAT3);
		push_float3(float3(0.3, -0.6, 0.0));
		push_stack_index(expr->return_value(0).stack_offset);
//		push_stack_index(0x0F);
//		push_opcode(OP_ASSIGN_FLOAT3);
		push_opcode(OP_END);
	}
	
	Expression *result = expr;
	expr = NULL;
	return result;
}

} /* namespace bvm */

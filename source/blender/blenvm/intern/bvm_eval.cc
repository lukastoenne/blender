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

/** \file bvm_eval.cc
 *  \ingroup bvm
 */

#include <cassert>

#include "bvm_eval.h"
#include "bvm_expression.h"

namespace bvm {

EvalContext::EvalContext()
{
}

EvalContext::~EvalContext()
{
}

inline static float stack_load_float(float *stack, StackIndex offset)
{
	return *(float *)(&stack[offset]);
}

inline static float3 stack_load_float3(float *stack, StackIndex offset)
{
	return *(float3 *)(&stack[offset]);
}

inline static void stack_store_float(float *stack, StackIndex offset, float f)
{
	*(float *)(&stack[offset]) = f;
}

inline static void stack_store_float3(float *stack, StackIndex offset, float3 f)
{
	*(float3 *)(&stack[offset]) = f;
}

static void eval_op_value_float(float *stack, float value, StackIndex offset)
{
	stack_store_float(stack, offset, value);
}

static void eval_op_value_float3(float *stack, float3 value, StackIndex offset)
{
	stack_store_float3(stack, offset, value);
}

static void eval_op_pass_float(float *stack, StackIndex offset_from, StackIndex offset_to)
{
	float f = stack_load_float(stack, offset_from);
	stack_store_float(stack, offset_to, f);
}

static void eval_op_pass_float3(float *stack, StackIndex offset_from, StackIndex offset_to)
{
	float3 f = stack_load_float3(stack, offset_from);
	stack_store_float3(stack, offset_to, f);
}

void EvalContext::eval_instructions(const Expression &expr, float *stack) const
{
	int instr = 0;
	
	while (true) {
		OpCode op = expr.read_opcode(&instr);
		
		switch(op) {
			case OP_NOOP:
				break;
			case OP_VALUE_FLOAT: {
				float value = expr.read_float(&instr);
				StackIndex offset = expr.read_stack_index(&instr);
				eval_op_value_float(stack, value, offset);
				break;
			}
			case OP_VALUE_FLOAT3: {
				float3 value = expr.read_float3(&instr);
				StackIndex offset = expr.read_stack_index(&instr);
				eval_op_value_float3(stack, value, offset);
				break;
			}
			case OP_PASS_FLOAT: {
				StackIndex offset_from = expr.read_stack_index(&instr);
				StackIndex offset_to = expr.read_stack_index(&instr);
				eval_op_pass_float(stack, offset_from, offset_to);
				break;
			}
			case OP_PASS_FLOAT3: {
				StackIndex offset_from = expr.read_stack_index(&instr);
				StackIndex offset_to = expr.read_stack_index(&instr);
				eval_op_pass_float3(stack, offset_from, offset_to);
				break;
			}
			case OP_END:
				return;
			default:
				assert(!"Unknown opcode");
				return;
		}
	}
}

void EvalContext::eval_expression(const Expression &expr, void **results) const
{
	float stack[BVM_STACK_SIZE];
	
	eval_instructions(expr, stack);
	
	for (int i = 0; i < expr.return_values_size(); ++i) {
		const ReturnValue &rval = expr.return_value(i);
		float *value = &stack[rval.stack_offset];
		
		rval.typedesc.copy_value(results[i], (void *)value);
	}
}

} /* namespace bvm */

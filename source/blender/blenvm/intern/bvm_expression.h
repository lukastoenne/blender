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

#ifndef __BVM_EXPRESSION_H__
#define __BVM_EXPRESSION_H__

/** \file bvm_expression.h
 *  \ingroup bvm
 */

#include <vector>
#include <stdint.h>

#include "bvm_opcode.h"
#include "bvm_type_desc.h"
#include "bvm_util_string.h"

namespace bvm {

typedef uint32_t Instruction;
typedef Instruction StackIndex;
#define BVM_STACK_INVALID 0xFFFFFFFF

static inline Instruction float_to_instruction(float f)
{
	union { uint32_t i; float f; } u;
	u.f = f;
	return u.i;
}

static inline float instruction_to_float(Instruction i)
{
	union { uint32_t i; float f; } u;
	u.i = i;
	return u.f;
}

struct ReturnValue {
	ReturnValue(const TypeDesc &typedesc, const string &name) :
	    typedesc(typedesc),
	    name(name),
	    stack_offset(BVM_STACK_INVALID)
	{}
	
	TypeDesc typedesc;
	string name;
	StackIndex stack_offset;
};

struct Expression {
	typedef std::vector<ReturnValue> ReturnValueList;
	typedef std::vector<Instruction> InstructionList;
	
	Expression();
	~Expression();
	
	OpCode read_opcode(int *instr) const
	{
		OpCode op = (OpCode)instructions[*instr];
		++(*instr);
		return op;
	}
	StackIndex read_stack_index(int *instr) const
	{
		StackIndex index = instructions[*instr];
		++(*instr);
		return index;
	}
	float read_float(int *instr) const
	{
		float f = instruction_to_float(instructions[*instr]);
		++(*instr);
		return f;
	}
	float3 read_float3(int *instr) const
	{
		float3 f;
		f.x = instruction_to_float(instructions[*instr + 0]);
		f.y = instruction_to_float(instructions[*instr + 1]);
		f.z = instruction_to_float(instructions[*instr + 2]);
		(*instr) += 3;
		return f;
	}
	
	void add_instruction(Instruction v);
	
	ReturnValue &add_return_value(const TypeDesc &typedesc, const string &name = "");
	size_t return_values_size() const;
	const ReturnValue &return_value(size_t index) const;
	const ReturnValue &return_value(const string &name) const;
	
private:
	ReturnValueList return_values;
	InstructionList instructions;
};

} /* namespace bvm */

#endif /* __BVM_EXPRESSION_H__ */

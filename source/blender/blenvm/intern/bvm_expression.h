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

#include "BVM_types.h"
#include "bvm_util_string.h"

namespace bvm {

typedef int32_t Instruction;
typedef std::vector<Instruction> InstructionList;

enum OpCode {
	OP_NOOP = 0,
	OP_END,
	OP_JUMP,
	OP_JUMP_IF_ZERO,
	OP_JUMP_IF_ONE,
};

struct Expression {
	struct ReturnValue {
		BVMType type;
		string name;
	};
	typedef std::vector<ReturnValue> ReturnValueList;
	
	Expression();
	~Expression();
	
	void add_return_value(BVMType type, const string &name = "");
	size_t return_values_size() const;
	const ReturnValue &return_value(size_t index) const;
	const ReturnValue &return_value(const string &name) const;
	
private:
	ReturnValueList return_values;
	InstructionList instructions;
};

} /* namespace bvm */

#endif /* __BVM_EXPRESSION_H__ */

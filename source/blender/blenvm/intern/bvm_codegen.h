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

#ifndef __BVM_CODEGEN_H__
#define __BVM_CODEGEN_H__

/** \file bvm_codegen.h
 *  \ingroup bvm
 */

#include <vector>

#include "bvm_expression.h"
#include "bvm_opcode.h"
#include "bvm_util_string.h"

namespace bvm {

struct Expression;
struct NodeGraph;
struct NodeInstance;
struct TypeDesc;
struct ReturnValue;

struct BVMCompiler {
	typedef std::vector<int> StackUsers;
	
	BVMCompiler();
	~BVMCompiler();
	
	StackIndex find_stack_index(int size) const;
	StackIndex assign_stack_index(const TypeDesc &typedesc);
	
	void push_opcode(OpCode op);
	void push_stack_index(StackIndex arg);
	void push_float(float f);
	void push_float3(float3 f);
	
	StackIndex codegen_value(const Value *value);
	Expression *codegen_expression(const NodeGraph &graph);
	
private:
	StackUsers stack_users;
	Expression *expr;
};

} /* namespace bvm */

#endif /* __BVM_CODEGEN_H__ */

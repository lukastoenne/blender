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

#ifndef __BVM_FUNCTION_H__
#define __BVM_FUNCTION_H__

/** \file bvm_function.h
 *  \ingroup bvm
 */

#include <vector>
#include <stdint.h>

#include "MEM_guardedalloc.h"

#include "typedesc.h"

#include "bvm_instruction_list.h"

#include "util_opcode.h"
#include "util_string.h"
#include "util_thread.h"

namespace blenvm {

struct EvalContext;
struct EvalGlobals;

struct Argument {
	Argument(const TypeDesc &typedesc, const string &name, StackIndex stack_offset) :
	    typedesc(typedesc),
	    name(name),
	    stack_offset(stack_offset)
	{}
	
	TypeDesc typedesc;
	string name;
	StackIndex stack_offset;

	MEM_CXX_CLASS_ALLOC_FUNCS("BVM:ReturnValue")
};

struct Function : public InstructionList {
	typedef std::vector<Argument> ArgumentList;
	
	Function();
	~Function();
	
	static void retain(Function *fn);
	static void release(Function **fn);
	
	size_t num_return_values() const;
	const Argument &return_value(size_t index) const;
	const Argument &return_value(const string &name) const;
	size_t num_arguments() const;
	const Argument &argument(size_t index) const;
	const Argument &argument(const string &name) const;
	
	void add_argument(const TypeDesc &typedesc, const string &name, StackIndex stack_offset);
	void add_return_value(const TypeDesc &typedesc, const string &name, StackIndex stack_offset);
	
	void eval(EvalContext *context, const EvalGlobals *globals, const void *arguments[], void *results[]) const;
	
private:
	ArgumentList m_arguments;
	ArgumentList m_return_values;
	
	int m_users;
	
	static mutex users_mutex;
	static spin_lock users_lock;
	
	MEM_CXX_CLASS_ALLOC_FUNCS("BVM:Function")
};

} /* namespace bvm */

#endif /* __BVM_FUNCTION_H__ */

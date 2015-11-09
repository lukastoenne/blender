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

/** \file bvm_function.cc
 *  \ingroup bvm
 */

#include "MEM_guardedalloc.h"

#include "bvm_function.h"

namespace bvm {

Function::Function()
{
}

Function::~Function()
{
}

void Function::add_instruction(Instruction v)
{
	instructions.push_back(v);
}

ReturnValue &Function::add_return_value(const TypeDesc &typedesc, const string &name)
{
	return_values.push_back(ReturnValue(typedesc, name));
	return return_values.back();
}

size_t Function::return_values_size() const
{
	return return_values.size();
}

const ReturnValue &Function::return_value(size_t index) const
{
	return return_values[index];
}

const ReturnValue &Function::return_value(const string &name) const
{
	for (ReturnValueList::const_iterator it = return_values.begin(); it != return_values.end(); ++it)
		if ((*it).name == name)
			return *it;
	return *(return_values.end());
}

} /* namespace bvm */

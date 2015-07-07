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

/** \file blender/blenjit/intern/types.cpp
 *  \ingroup bjit
 */

#include "bjit_llvm.h"
#include "bjit_types.h"

using namespace llvm;

namespace bjit {

template <SocketType type>
struct SocketTypeGetter {
	static Type *get(SocketType t, LLVMContext &context)
	{
		if (t == type)
			return TypeBuilder<typename SocketTypeImpl<type>::type, true>::get(context);
		return SocketTypeGetter<(SocketType)((int)type + 1)>::get(t, context);
	}
};

template <>
struct SocketTypeGetter<BJIT_NUMTYPES> {
	static Type *get(SocketType /*t*/, LLVMContext &/*context*/)
	{
		return NULL;
	}
};


Type *bjit_get_socket_llvm_type(SocketType type, LLVMContext &context)
{
	return SocketTypeGetter<(SocketType)0>::get(type, context);
//	return TypeBuilder<typename SocketTypeImpl<type>::type, true>::get(context);
}

} /* namespace bjit */

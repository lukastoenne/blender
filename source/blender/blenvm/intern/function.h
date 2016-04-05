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

#ifndef __FUNCTION_H__
#define __FUNCTION_H__

/** \file blender/blenvm/intern/function.h
 *  \ingroup blenvm
 */

#include "MEM_guardedalloc.h"

namespace blenvm {

struct FunctionBase {
	FunctionBase();
	~FunctionBase();
	
	/* Increment user count */
	static void retain(FunctionBase *fn);
	/* Decrement user count.
	 * Caller should delete the function if result is True!
	 */
	static bool release(FunctionBase *fn);
	
private:
	int m_users;
	
	MEM_CXX_CLASS_ALLOC_FUNCS("BVM:Function")
};

} /* namespace blenvm */

#endif /* __FUNCTION_H__ */

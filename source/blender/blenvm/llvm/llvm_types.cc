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

/** \file blender/blenvm/llvm/llvm_types.cc
 *  \ingroup llvm
 */

#include "llvm_headers.h"
#include "llvm_types.h"


namespace blenvm {

bool llvm_use_argument_pointer(const TypeSpec *typespec)
{
	using namespace llvm;
	
	if (typespec->is_structure()) {
		/* pass by reference */
		return true;
	}
	else {
		switch (typespec->base_type()) {
			case BVM_FLOAT:
			case BVM_INT:
				/* pass by value */
				return false;
			case BVM_FLOAT3:
			case BVM_FLOAT4:
			case BVM_MATRIX44:
				/* pass by reference */
				return true;
				
			case BVM_STRING:
			case BVM_RNAPOINTER:
			case BVM_MESH:
			case BVM_DUPLIS:
				/* TODO */
				break;
		}
	}
	
	return false;
}

} /* namespace blenvm */

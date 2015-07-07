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

/** \file blender/blenjit/BJIT_types.h
 *  \ingroup bjit
 */

#ifndef __BJIT_TYPES_H__
#define __BJIT_TYPES_H__

#include "bjit_llvm.h"

namespace bjit {

using namespace llvm;

typedef enum {
	BJIT_TYPE_FLOAT,
	BJIT_TYPE_INT,
	BJIT_TYPE_VEC3,
	
	BJIT_NUMTYPES
} SocketType;

Type *bjit_get_socket_llvm_type(SocketType type, LLVMContext &context);

typedef types::ieee_float vec2_t[2];
typedef types::ieee_float vec3_t[3];
typedef types::ieee_float vec4_t[4];
typedef types::ieee_float mat3_t[3][3];
typedef types::ieee_float mat4_t[4][4];

/* ------------------------------------------------------------------------- */

template <SocketType T>
struct SocketTypeImpl;

template <> struct SocketTypeImpl<BJIT_TYPE_FLOAT> {
	typedef typename types::ieee_float type;
};

template <> struct SocketTypeImpl<BJIT_TYPE_INT> {
	typedef typename types::i<32> type;
};

template <> struct SocketTypeImpl<BJIT_TYPE_VEC3> {
	typedef vec3_t type;
};

} /* namespace bjit */

#endif

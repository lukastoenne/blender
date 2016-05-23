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

#ifndef __MOD_VALUE_H__
#define __MOD_VALUE_H__

#include "mod_defines.h"

#include "util_data_ptr.h"
#include "util_math.h"
#include "util_string.h"

extern "C" {
#include "RNA_access.h"
}

BVM_MOD_NAMESPACE_BEGIN

bvm_extern void NOOP(void)
{
}
BVM_DECL_FUNCTION_VALUE(NOOP)

bvm_extern void VALUE_FLOAT(float &result, float value)
{
	result = value;
}
BVM_DECL_FUNCTION_VALUE(VALUE_FLOAT)

bvm_extern void VALUE_FLOAT3(float3 &result, const float3 &value)
{
	result = value;
}
BVM_DECL_FUNCTION_VALUE(VALUE_FLOAT3)

bvm_extern void VALUE_FLOAT4(float4 &result, const float4 &value)
{
	result = value;
}
BVM_DECL_FUNCTION_VALUE(VALUE_FLOAT4)

bvm_extern void VALUE_INT(int &result, int value)
{
	result = value;
}
BVM_DECL_FUNCTION_VALUE(VALUE_INT)

bvm_extern void VALUE_MATRIX44(matrix44 &result, const matrix44 &value)
{
	result = value;
}
BVM_DECL_FUNCTION_VALUE(VALUE_MATRIX44)

bvm_extern void VALUE_STRING(const char *&result, const char *value)
{
	result = value;
}
BVM_DECL_FUNCTION_VALUE(VALUE_STRING)

bvm_extern void VALUE_RNAPOINTER(PointerRNA &result, const PointerRNA &value)
{
	result = value;
}
BVM_DECL_FUNCTION_VALUE(VALUE_RNAPOINTER)

bvm_extern void VALUE_MESH(mesh_ptr &result, const mesh_ptr &value)
{
	result = value;
}
BVM_DECL_FUNCTION_VALUE(VALUE_MESH)

bvm_extern void VALUE_DUPLIS(duplis_ptr &result, const duplis_ptr &value)
{
	result = value;
}
BVM_DECL_FUNCTION_VALUE(VALUE_DUPLIS)

bvm_extern void FLOAT_TO_INT(int &result, float value)
{
	result = (int)value;
}
BVM_DECL_FUNCTION_VALUE(FLOAT_TO_INT)

bvm_extern void INT_TO_FLOAT(float &result, int value)
{
	result = (float)value;
}
BVM_DECL_FUNCTION_VALUE(INT_TO_FLOAT)

bvm_extern void SET_FLOAT3(float3 &result, float x, float y, float z)
{
	result = float3(x, y, z);
}
BVM_DECL_FUNCTION_VALUE(SET_FLOAT3)

bvm_extern void GET_ELEM_FLOAT3(float &result, int index, const float3 &f)
{
	BLI_assert(index >= 0 && index < 3);
	result = f[index];
}
BVM_DECL_FUNCTION_VALUE(GET_ELEM_FLOAT3)

bvm_extern void SET_FLOAT4(float4 &result, float x, float y, float z, float w)
{
	result = float4(x, y, z, w);
}
BVM_DECL_FUNCTION_VALUE(SET_FLOAT4)

bvm_extern void GET_ELEM_FLOAT4(float &result, int index, const float4 &f)
{
	BLI_assert(index >= 0 && index < 4);
	result = f[index];
}
BVM_DECL_FUNCTION_VALUE(GET_ELEM_FLOAT4)

BVM_MOD_NAMESPACE_END

#endif /* __MOD_VALUE_H__ */

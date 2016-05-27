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

bvm_extern void V__NOOP(void)
{
}
BVM_DECL_FUNCTION_VALUE(NOOP)

bvm_extern void V__VALUE_FLOAT(float &result, float value)
{
	result = value;
}
bvm_extern void D__VALUE_FLOAT(float &dr, float /*f*/)
{
	dr = 0.0f;
}
BVM_DECL_FUNCTION_DUAL(VALUE_FLOAT)

bvm_extern void V__VALUE_FLOAT3(float3 &result, const float3 &value)
{
	result = value;
}
bvm_extern void D__VALUE_FLOAT3(float3 &dr, const float3 &/*f*/)
{
	dr = float3(0.0f, 0.0f, 0.0f);
}
BVM_DECL_FUNCTION_DUAL(VALUE_FLOAT3)

bvm_extern void V__VALUE_FLOAT4(float4 &result, const float4 &value)
{
	result = value;
}
bvm_extern void D__VALUE_FLOAT4(float4 &dr, const float4 &/*f*/)
{
	dr = float4(0.0f, 0.0f, 0.0f, 0.0f);
}
BVM_DECL_FUNCTION_DUAL(VALUE_FLOAT4)

bvm_extern void V__VALUE_INT(int &result, int value)
{
	result = value;
}
BVM_DECL_FUNCTION_VALUE(VALUE_INT)

bvm_extern void V__VALUE_MATRIX44(matrix44 &result, const matrix44 &value)
{
	result = value;
}
BVM_DECL_FUNCTION_VALUE(VALUE_MATRIX44)

bvm_extern void V__VALUE_STRING(const char *&result, const char *value)
{
	result = value;
}
BVM_DECL_FUNCTION_VALUE(VALUE_STRING)

bvm_extern void V__VALUE_RNAPOINTER(PointerRNA &result, const PointerRNA &value)
{
	result = value;
}
BVM_DECL_FUNCTION_VALUE(VALUE_RNAPOINTER)

bvm_extern void V__VALUE_MESH(mesh_ptr &result, const mesh_ptr &value)
{
	result = value;
}
BVM_DECL_FUNCTION_VALUE(VALUE_MESH)

bvm_extern void V__VALUE_DUPLIS(duplis_ptr &result, const duplis_ptr &value)
{
	result = value;
}
BVM_DECL_FUNCTION_VALUE(VALUE_DUPLIS)

bvm_extern void V__FLOAT_TO_INT(int &result, float value)
{
	result = (int)value;
}
BVM_DECL_FUNCTION_VALUE(FLOAT_TO_INT)

bvm_extern void V__INT_TO_FLOAT(float &result, int value)
{
	result = (float)value;
}
BVM_DECL_FUNCTION_VALUE(INT_TO_FLOAT)

bvm_extern void V__SET_FLOAT3(float3 &result, float x, float y, float z)
{
	result = float3(x, y, z);
}
bvm_extern void D__SET_FLOAT3(float3 &dr, float /*x*/, float dx, float /*y*/, float dy, float /*z*/, float dz)
{
	dr = float3(dx, dy, dz);
}
BVM_DECL_FUNCTION_DUAL(SET_FLOAT3)

bvm_extern void V__GET_ELEM_FLOAT3(float &result, int index, const float3 &f)
{
	BLI_assert(index >= 0 && index < 3);
	result = f[index];
}
bvm_extern void D__GET_ELEM_FLOAT3(float &dr, int index, const float3 &/*f*/, const float3 &df)
{
	BLI_assert(index >= 0 && index < 3);
	dr = df[index];
}
BVM_DECL_FUNCTION_DUAL(GET_ELEM_FLOAT3)

bvm_extern void V__SET_FLOAT4(float4 &result, float x, float y, float z, float w)
{
	result = float4(x, y, z, w);
}
bvm_extern void D__SET_FLOAT4(float4 &dr,
                             float /*x*/, float dx, float /*y*/, float dy,
                             float /*z*/, float dz, float /*w*/, float dw)
{
	dr = float4(dx, dy, dz, dw);
}
BVM_DECL_FUNCTION_DUAL(SET_FLOAT4)

bvm_extern void V__GET_ELEM_FLOAT4(float &result, int index, const float4 &f)
{
	BLI_assert(index >= 0 && index < 4);
	result = f[index];
}
bvm_extern void D__GET_ELEM_FLOAT4(float &dr, int index, const float4 &/*f*/, const float4 &df)
{
	BLI_assert(index >= 0 && index < 4);
	dr = df[index];
}
BVM_DECL_FUNCTION_DUAL(GET_ELEM_FLOAT4)

BVM_MOD_NAMESPACE_END

#endif /* __MOD_VALUE_H__ */

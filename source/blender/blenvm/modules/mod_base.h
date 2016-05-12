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

BVM_MOD_FUNCTION("NOOP")
void NOOP(void)
{
}

BVM_MOD_FUNCTION("VALUE_FLOAT")
void VALUE_FLOAT(float &result, float value)
{
	result = value;
}

BVM_MOD_FUNCTION("VALUE_FLOAT3")
void VALUE_FLOAT3(float3 &result, const float3 &value)
{
	result = value;
}

BVM_MOD_FUNCTION("VALUE_FLOAT4")
void VALUE_FLOAT4(float4 &result, const float4 &value)
{
	result = value;
}

BVM_MOD_FUNCTION("VALUE_INT")
void VALUE_INT(int &result, int value)
{
	result = value;
}

BVM_MOD_FUNCTION("VALUE_MATRIX44")
void VALUE_MATRIX44(matrix44 &result, const matrix44 &value)
{
	result = value;
}

BVM_MOD_FUNCTION("VALUE_STRING")
void VALUE_STRING(const char *&result, const char *value)
{
	result = value;
}

BVM_MOD_FUNCTION("VALUE_RNAPOINTER")
void VALUE_RNAPOINTER(PointerRNA &result, const PointerRNA &value)
{
	result = value;
}

BVM_MOD_FUNCTION("VALUE_MESH")
void VALUE_MESH(mesh_ptr &result, const mesh_ptr &value)
{
	result = value;
}

BVM_MOD_FUNCTION("VALUE_DUPLIS")
void VALUE_DUPLIS(duplis_ptr &result, const duplis_ptr &value)
{
	result = value;
}

BVM_MOD_FUNCTION("FLOAT_TO_INT")
void FLOAT_TO_INT(int &result, float value)
{
	result = (int)value;
}

BVM_MOD_FUNCTION("INT_TO_FLOAT")
void INT_TO_FLOAT(float &result, int value)
{
	result = (float)value;
}

BVM_MOD_FUNCTION("SET_FLOAT3")
void SET_FLOAT3(float3 &result, float x, float y, float z)
{
	result = float3(x, y, z);
}

BVM_MOD_FUNCTION("SET_FLOAT4")
void GET_ELEM_FLOAT3(float &result, const float3 &f, int index)
{
	BLI_assert(index >= 0 && index < 3);
	result = f[index];
}

BVM_MOD_FUNCTION("SET_FLOAT4")
void SET_FLOAT4(float4 &result, float x, float y, float z, float w)
{
	result = float4(x, y, z, w);
}

BVM_MOD_FUNCTION("SET_FLOAT4")
void GET_ELEM_FLOAT4(float &result, const float4 &f, int index)
{
	BLI_assert(index >= 0 && index < 4);
	result = f[index];
}

BVM_MOD_NAMESPACE_END

#endif /* __MOD_VALUE_H__ */

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

#ifndef __MOD_MATH_H__
#define __MOD_MATH_H__

#include "mod_defines.h"

#include "util_math.h"

BVM_MOD_NAMESPACE_BEGIN

BVM_MOD_FUNCTION("MATRIX44_TO_LOC")
void MATRIX44_TO_LOC(float3 &loc, const matrix44 &m)
{
	copy_v3_v3(loc.data(), m.data[3]);
}

BVM_MOD_FUNCTION("MATRIX44_TO_EULER")
void MATRIX44_TO_EULER(float3 &euler, int order, const matrix44 &m)
{
	mat4_to_eulO(euler.data(), (short)order, m.c_data());
}

BVM_MOD_FUNCTION("MATRIX44_TO_AXISANGLE")
void MATRIX44_TO_AXISANGLE(float3 &axis, float &angle, const matrix44 &m)
{
	mat4_to_axis_angle(axis.data(), &angle, m.c_data());
}

BVM_MOD_FUNCTION("MATRIX44_TO_SCALE")
void MATRIX44_TO_SCALE(float3 &scale, const matrix44 &m)
{
	mat4_to_size(scale.data(), m.c_data());
}

BVM_MOD_FUNCTION("LOC_TO_MATRIX44")
void LOC_TO_MATRIX44(matrix44 &m, const float3 &loc)
{
	m = matrix44::identity();
	copy_v3_v3(m.data[3], loc.data());
}

BVM_MOD_FUNCTION("EULER_TO_MATRIX44")
void EULER_TO_MATRIX44(matrix44 &m, int order, const float3 &euler)
{
	m = matrix44::identity();
	eulO_to_mat4(m.c_data(), euler.data(), (short)order);
}

BVM_MOD_FUNCTION("AXISANGLE_TO_MATRIX44")
void AXISANGLE_TO_MATRIX44(matrix44 &m, const float3 &axis, float angle)
{
	m = matrix44::identity();
	axis_angle_to_mat4(m.c_data(), axis.data(), angle);
}

BVM_MOD_FUNCTION("SCALE_TO_MATRIX44")
void SCALE_TO_MATRIX44(matrix44 &m, const float3 &scale)
{
	m = matrix44::identity();
	size_to_mat4(m.c_data(), scale.data());
}

BVM_MOD_FUNCTION("ADD_FLOAT")
void ADD_FLOAT(float &r, float a, float b)
{
	r = a + b;
}

BVM_MOD_FUNCTION("SUB_FLOAT")
void SUB_FLOAT(float &r, float a, float b)
{
	r = a - b;
}

BVM_MOD_FUNCTION("MUL_FLOAT")
void MUL_FLOAT(float &r, float a, float b)
{
	r = a * b;
}

BVM_MOD_FUNCTION("DIV_FLOAT")
void DIV_FLOAT(float &r, float a, float b)
{
	r = div_safe(a, b);
}

BVM_MOD_FUNCTION("SINE")
void SINE(float &r, float f)
{
	r = sinf(f);
}

BVM_MOD_FUNCTION("COSINE")
void COSINE(float &r, float f)
{
	r = cosf(f);
}

BVM_MOD_FUNCTION("TANGENT")
void TANGENT(float &r, float f)
{
	r = tanf(f);
}

BVM_MOD_FUNCTION("ARCSINE")
void ARCSINE(float &r, float f)
{
	r = asinf(f);
}

BVM_MOD_FUNCTION("ARCCOSINE")
void ARCCOSINE(float &r, float f)
{
	r = acosf(f);
}

BVM_MOD_FUNCTION("ARCTANGENT")
void ARCTANGENT(float &r, float f)
{
	r = atanf(f);
}

BVM_MOD_FUNCTION("POWER")
void POWER(float &r, float a, float b)
{
	r = pow_safe(a, b);
}

BVM_MOD_FUNCTION("LOGARITHM")
void LOGARITHM(float &r, float a, float b)
{
	r = log_safe(a, b);
}

BVM_MOD_FUNCTION("MINIMUM")
void MINIMUM(float &r, float a, float b)
{
	r = min_ff(a, b);
}

BVM_MOD_FUNCTION("MAXIMUM")
void MAXIMUM(float &r, float a, float b)
{
	r = max_ff(a, b);
}

BVM_MOD_FUNCTION("ROUND")
void ROUND(float &r, float f)
{
	r = floorf(f + 0.5f);
}

BVM_MOD_FUNCTION("LESS_THAN")
void LESS_THAN(float &r, float a, float b)
{
	r = (a < b)? 1.0f : 0.0f;
}

BVM_MOD_FUNCTION("GREATER_THAN")
void GREATER_THAN(float &r, float a, float b)
{
	r = (a > b)? 1.0f : 0.0f;
}

BVM_MOD_FUNCTION("MODULO")
void MODULO(float &r, float a, float b)
{
	r = modulo_safe(a, b);
}

BVM_MOD_FUNCTION("ABSOLUTE")
void ABSOLUTE(float &r, float f)
{
	r = fabs(f);
}

BVM_MOD_FUNCTION("CLAMP_ONE")
void CLAMP_ONE(float &r, float f)
{
	r = CLAMPIS(f, 0.0f, 1.0f);
}

BVM_MOD_FUNCTION("SQRT")
void SQRT(float &r, float f)
{
	r = sqrt_safe(f);
}

BVM_MOD_FUNCTION("ADD_FLOAT3")
void ADD_FLOAT3(float3 &r, const float3 &a, const float3 &b)
{
	add_v3_v3v3(r.data(), a.data(), b.data());
}

BVM_MOD_FUNCTION("SUB_FLOAT3")
void SUB_FLOAT3(float3 &r, const float3 &a, const float3 &b)
{
	sub_v3_v3v3(r.data(), a.data(), b.data());
}

BVM_MOD_FUNCTION("MUL_FLOAT3")
void MUL_FLOAT3(float3 &r, const float3 &a, const float3 &b)
{
	mul_v3_v3v3(r.data(), a.data(), b.data());
}

BVM_MOD_FUNCTION("DIV_FLOAT3")
void DIV_FLOAT3(float3 &r, const float3 &a, const float3 &b)
{
	r = float3(div_safe(a.x, b.x), div_safe(a.y, b.y), div_safe(a.z, b.z));
}

BVM_MOD_FUNCTION("MUL_FLOAT3_FLOAT")
void MUL_FLOAT3_FLOAT(float3 &r, const float3 &a, float b)
{
	mul_v3_v3fl(r.data(), a.data(), b);
}

BVM_MOD_FUNCTION("DIV_FLOAT3_FLOAT")
void DIV_FLOAT3_FLOAT(float3 &r, const float3 &a, float b)
{
	r = float3(div_safe(a.x, b), div_safe(a.y, b), div_safe(a.z, b));
}

BVM_MOD_FUNCTION("AVERAGE_FLOAT3")
void AVERAGE_FLOAT3(float3 &r, const float3 &a, const float3 &b)
{
	r = float3(0.5f*(a.x+b.x), 0.5f*(a.y+b.y), 0.5f*(a.z+b.z));
}

BVM_MOD_FUNCTION("DOT_FLOAT3")
void DOT_FLOAT3(float &r, const float3 &a, const float3 &b)
{
	r = dot_v3v3(a.data(), b.data());
}

BVM_MOD_FUNCTION("CROSS_FLOAT3")
void CROSS_FLOAT3(float3 &r, const float3 &a, const float3 &b)
{
	r = float3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}

BVM_MOD_FUNCTION("NORMALIZE_FLOAT3")
void NORMALIZE_FLOAT3(float3 &r_vec, float &r_len, const float3 &vec)
{
	r_len = normalize_v3_v3(r_vec.data(), vec.data());
}

BVM_MOD_FUNCTION("LENGTH_FLOAT3")
void LENGTH_FLOAT3(float &len, const float3 &vec)
{
	len = len_v3(vec.data());
}

BVM_MOD_FUNCTION("ADD_MATRIX44")
void ADD_MATRIX44(matrix44 &r, const matrix44 &a, const matrix44 &b)
{
	add_m4_m4m4(r.c_data(), a.c_data(), b.c_data());
}

BVM_MOD_FUNCTION("SUB_MATRIX44")
void SUB_MATRIX44(matrix44 &r, const matrix44 &a, const matrix44 &b)
{
	sub_m4_m4m4(r.c_data(), a.c_data(), b.c_data());
}

BVM_MOD_FUNCTION("MUL_MATRIX44")
void MUL_MATRIX44(matrix44 &r, const matrix44 &a, const matrix44 &b)
{
	mul_m4_m4m4(r.c_data(), a.c_data(), b.c_data());
}

BVM_MOD_FUNCTION("MUL_MATRIX44_FLOAT")
void MUL_MATRIX44_FLOAT(matrix44 &r, const matrix44 &a, float b)
{
	copy_m4_m4(r.c_data(), a.c_data());
	mul_m4_fl(r.c_data(), b);
}

BVM_MOD_FUNCTION("DIV_MATRIX44_FLOAT")
void DIV_MATRIX44_FLOAT(matrix44 &r, const matrix44 &a, float b)
{
	copy_m4_m4(r.c_data(), a.c_data());
	mul_m4_fl(r.c_data(), div_safe(1.0f, b));
}

BVM_MOD_FUNCTION("NEGATE_MATRIX44")
void NEGATE_MATRIX44(matrix44 &r, const matrix44 &m)
{
	copy_m4_m4(r.c_data(), m.c_data());
	negate_m4(r.c_data());
}

BVM_MOD_FUNCTION("TRANSPOSE_MATRIX44")
void TRANSPOSE_MATRIX44(matrix44 &r, const matrix44 &m)
{
	transpose_m4_m4(r.c_data(), m.c_data());
}

BVM_MOD_FUNCTION("INVERT_MATRIX44")
void INVERT_MATRIX44(matrix44 &r, const matrix44 &m)
{
	invert_m4_m4_safe(r.c_data(), m.c_data());
}

BVM_MOD_FUNCTION("ADJOINT_MATRIX44")
void ADJOINT_MATRIX44(matrix44 &r, const matrix44 &m)
{
	adjoint_m4_m4(r.c_data(), m.c_data());
}

BVM_MOD_FUNCTION("DETERMINANT_MATRIX44")
void DETERMINANT_MATRIX44(float &r, const matrix44 &m)
{
	r = determinant_m4(m.c_data());
}

BVM_MOD_FUNCTION("MUL_MATRIX44_FLOAT3")
void MUL_MATRIX44_FLOAT3(float3 &r, const matrix44 &a, const float3 &b)
{
	mul_v3_m4v3(r.data(), a.c_data(), b.data());
}

BVM_MOD_FUNCTION("MUL_MATRIX44_FLOAT4")
void MUL_MATRIX44_FLOAT4(float4 &r, const matrix44 &a, const float4 &b)
{
	mul_v4_m4v4(r.data(), a.c_data(), b.data());
}

BVM_MOD_NAMESPACE_END

#endif /* __MOD_MATH_H__ */

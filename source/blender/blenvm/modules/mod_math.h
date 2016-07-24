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

bvm_extern void V__MATRIX44_TO_LOC(float3 &loc, const matrix44 &m)
{
	copy_v3_v3(loc.data(), m.data[3]);
}
BVM_DECL_FUNCTION_VALUE(MATRIX44_TO_LOC)

bvm_extern void V__MATRIX44_TO_ROT(matrix44 &r, const matrix44 &m)
{
	normalize_v3_v3(r.data[0], m.data[0]);
	normalize_v3_v3(r.data[1], m.data[1]);
	normalize_v3_v3(r.data[2], m.data[2]);
	zero_v3(r.data[3]);
	r.data[0][3] = 0.0;
	r.data[1][3] = 0.0;
	r.data[2][3] = 0.0;
	r.data[3][3] = 1.0;
}
BVM_DECL_FUNCTION_VALUE(MATRIX44_TO_ROT)

bvm_extern void V__MATRIX44_TO_EULER(float3 &euler, int order, const matrix44 &m)
{
	mat4_to_eulO(euler.data(), (short)order, m.c_data());
}
BVM_DECL_FUNCTION_VALUE(MATRIX44_TO_EULER)

bvm_extern void V__MATRIX44_TO_AXISANGLE(float3 &axis, float &angle, const matrix44 &m)
{
	mat4_to_axis_angle(axis.data(), &angle, m.c_data());
}
BVM_DECL_FUNCTION_VALUE(MATRIX44_TO_AXISANGLE)

bvm_extern void V__MATRIX44_TO_SCALE(float3 &scale, const matrix44 &m)
{
	mat4_to_size(scale.data(), m.c_data());
}
BVM_DECL_FUNCTION_VALUE(MATRIX44_TO_SCALE)

bvm_extern void V__LOC_TO_MATRIX44(matrix44 &m, const float3 &loc)
{
	m = matrix44::identity();
	copy_v3_v3(m.data[3], loc.data());
}
BVM_DECL_FUNCTION_VALUE(LOC_TO_MATRIX44)

bvm_extern void V__EULER_TO_MATRIX44(matrix44 &m, int order, const float3 &euler)
{
	m = matrix44::identity();
	eulO_to_mat4(m.c_data(), euler.data(), (short)order);
}
BVM_DECL_FUNCTION_VALUE(EULER_TO_MATRIX44)

bvm_extern void V__AXISANGLE_TO_MATRIX44(matrix44 &m, const float3 &axis, float angle)
{
	m = matrix44::identity();
	axis_angle_to_mat4(m.c_data(), axis.data(), angle);
}
BVM_DECL_FUNCTION_VALUE(AXISANGLE_TO_MATRIX44)

bvm_extern void V__SCALE_TO_MATRIX44(matrix44 &m, const float3 &scale)
{
	m = matrix44::identity();
	size_to_mat4(m.c_data(), scale.data());
}
BVM_DECL_FUNCTION_VALUE(SCALE_TO_MATRIX44)

bvm_extern void V__ADD_FLOAT(float &r, float a, float b)
{
	r = a + b;
}
bvm_extern void D__ADD_FLOAT(float &dr, float /*a*/, float da, float /*b*/, float db)
{
	dr = da + db;
}
BVM_DECL_FUNCTION_DUAL(ADD_FLOAT)

bvm_extern void V__SUB_FLOAT(float &r, float a, float b)
{
	r = a - b;
}
bvm_extern void D__SUB_FLOAT(float &dr, float /*a*/, float da, float /*b*/, float db)
{
	dr = da - db;
}
BVM_DECL_FUNCTION_DUAL(SUB_FLOAT)

bvm_extern void V__MUL_FLOAT(float &r, float a, float b)
{
	r = a * b;
}
bvm_extern void D__MUL_FLOAT(float &dr, float a, float da, float b, float db)
{
	dr = da * b + a * db;
}
BVM_DECL_FUNCTION_DUAL(MUL_FLOAT)

bvm_extern void V__DIV_FLOAT(float &r, float a, float b)
{
	r = div_safe(a, b);
}
bvm_extern void D__DIV_FLOAT(float &dr, float a, float da, float b, float db)
{
	if (b != 0.0f)
		dr = (da - db * a / b) / b;
	else
		dr = 0.0f;
}
BVM_DECL_FUNCTION_DUAL(DIV_FLOAT)

bvm_extern void V__SINE(float &r, float f)
{
	r = sinf(f);
}
bvm_extern void D__SINE(float &dr, float f, float df)
{
	dr = df * cosf(f);
}
BVM_DECL_FUNCTION_DUAL(SINE)

bvm_extern void V__COSINE(float &r, float f)
{
	r = cosf(f);
}
bvm_extern void D__COSINE(float &dr, float f, float df)
{
	dr = df * (-sinf(f));
}
BVM_DECL_FUNCTION_DUAL(COSINE)

bvm_extern void V__TANGENT(float &r, float f)
{
	r = tanf(f);
}
bvm_extern void D__TANGENT(float &dr, float f, float df)
{
	float c = cosf(f);
	dr = df * div_safe(1.0f, c*c);
}
BVM_DECL_FUNCTION_DUAL(TANGENT)

bvm_extern void V__ARCSINE(float &r, float f)
{
	if (f >= -1.0f && f <= 1.0f)
		r = asinf(f);
	else
		r = 0.0f;
}
bvm_extern void D__ARCSINE(float &dr, float f, float df)
{
	if (f >= -1.0f && f <= 1.0f)
		dr = div_safe(df, sqrtf(1.0f - f*f));
	else
		dr = 0.0f;
}
BVM_DECL_FUNCTION_DUAL(ARCSINE)

bvm_extern void V__ARCCOSINE(float &r, float f)
{
	if (f >= -1.0f && f <= 1.0f)
		r = acosf(f);
	else
		r = 0.0f;
}
bvm_extern void D__ARCCOSINE(float &dr, float f, float df)
{
	if (f >= -1.0f && f <= 1.0f)
		dr = -div_safe(df, sqrtf(1.0f - f*f));
	else
		dr = 0.0f;
}
BVM_DECL_FUNCTION_DUAL(ARCCOSINE)

bvm_extern void V__ARCTANGENT(float &r, float f)
{
	r = atanf(f);
}
bvm_extern void D__ARCTANGENT(float &dr, float f, float df)
{
	dr = df / (1.0f + f*f);
}
BVM_DECL_FUNCTION_DUAL(ARCTANGENT)

bvm_extern void V__POWER(float &r, float a, float b)
{
	r = pow_safe(a, b);
}
bvm_extern void D__POWER(float &dr, float a, float da, float b, float db)
{
	if (a > 0.0f) {
		float ln_a = logf(a);
		dr = (da * b / a + db * ln_a) * expf(b * ln_a);
	}
	else {
		dr = 0.0f;
	}
}
BVM_DECL_FUNCTION_DUAL(POWER)

bvm_extern void V__LOGARITHM(float &r, float a, float b)
{
	r = log_safe(a, b);
}
bvm_extern void D__LOGARITHM(float &dr, float a, float da, float b, float db)
{
	if (a > 0.0f && b > 0.0f) {
		float ln_a = logf(a);
		float ln_b = logf(b);
		dr = (da/a - db/b * ln_a/ln_b) / ln_b;
	}
	else {
		dr = 0.0f;
	}
}
BVM_DECL_FUNCTION_DUAL(LOGARITHM)

bvm_extern void V__MINIMUM(float &r, float a, float b)
{
	r = min_ff(a, b);
}
bvm_extern void D__MINIMUM(float &dr, float a, float da, float b, float db)
{
	dr = (a < b) ? da : db;
}
BVM_DECL_FUNCTION_DUAL(MINIMUM)

bvm_extern void V__MAXIMUM(float &r, float a, float b)
{
	r = max_ff(a, b);
}
bvm_extern void D__MAXIMUM(float &dr, float a, float da, float b, float db)
{
	dr = (a > b) ? da : db;
}
BVM_DECL_FUNCTION_DUAL(MAXIMUM)

bvm_extern void V__ROUND(float &r, float f)
{
	r = floorf(f + 0.5f);
}
BVM_DECL_FUNCTION_VALUE(ROUND)

bvm_extern void V__LESS_THAN(float &r, float a, float b)
{
	r = (a < b)? 1.0f : 0.0f;
}
BVM_DECL_FUNCTION_VALUE(LESS_THAN)

bvm_extern void V__GREATER_THAN(float &r, float a, float b)
{
	r = (a > b)? 1.0f : 0.0f;
}
BVM_DECL_FUNCTION_VALUE(GREATER_THAN)

bvm_extern void V__MODULO(float &r, float a, float b)
{
	r = modulo_safe(a, b);
}
bvm_extern void D__MODULO(float &dr, float /*a*/, float da, float /*b*/, float /*db*/)
{
	dr = da;
}
BVM_DECL_FUNCTION_DUAL(MODULO)

bvm_extern void V__ABSOLUTE(float &r, float f)
{
	r = fabs(f);
}
bvm_extern void D__ABSOLUTE(float &dr, float f, float df)
{
	dr = (f >= 0.0f) ? df : -df;
}
BVM_DECL_FUNCTION_DUAL(ABSOLUTE)

bvm_extern void V__CLAMP_ONE(float &r, float f)
{
	r = CLAMPIS(f, 0.0f, 1.0f);
}
bvm_extern void D__CLAMP_ONE(float &dr, float f, float df)
{
	dr = (f >= 0.0f || f <= 1.0f) ? df : 0.0f;
}
BVM_DECL_FUNCTION_DUAL(CLAMP_ONE)

bvm_extern void V__SQRT(float &r, float f)
{
	r = sqrt_safe(f);
}
bvm_extern void D__SQRT(float &dr, float f, float df)
{
	if (f > 0.0f)
		dr = df * 0.5f / sqrt_safe(f);
	else
		dr = 0.0f;
}
BVM_DECL_FUNCTION_DUAL(SQRT)

bvm_extern void V__ADD_FLOAT3(float3 &r, const float3 &a, const float3 &b)
{
	add_v3_v3v3(r.data(), a.data(), b.data());
}
bvm_extern void D__ADD_FLOAT3(float3 &dr, const float3 &/*a*/, const float3 &da, const float3 &/*b*/, const float3 &db)
{
	add_v3_v3v3(dr.data(), da.data(), db.data());
}
BVM_DECL_FUNCTION_DUAL(ADD_FLOAT3)

bvm_extern void V__SUB_FLOAT3(float3 &r, const float3 &a, const float3 &b)
{
	sub_v3_v3v3(r.data(), a.data(), b.data());
}
bvm_extern void D__SUB_FLOAT3(float3 &dr, const float3 &/*a*/, const float3 &da, const float3 &/*b*/, const float3 &db)
{
	sub_v3_v3v3(dr.data(), da.data(), db.data());
}
BVM_DECL_FUNCTION_DUAL(SUB_FLOAT3)

bvm_extern void V__MUL_FLOAT3(float3 &r, const float3 &a, const float3 &b)
{
	mul_v3_v3v3(r.data(), a.data(), b.data());
}
bvm_extern void D__MUL_FLOAT3(float3 &dr, const float3 &a, const float3 &da, const float3 &b, const float3 &db)
{
	float3 dra, drb;
	mul_v3_v3v3(dra.data(), da.data(), b.data());
	mul_v3_v3v3(drb.data(), a.data(), db.data());
	add_v3_v3v3(dr.data(), dra.data(), drb.data());
}
BVM_DECL_FUNCTION_DUAL(MUL_FLOAT3)

bvm_extern void V__DIV_FLOAT3(float3 &r, const float3 &a, const float3 &b)
{
	r = float3(div_safe(a.x, b.x), div_safe(a.y, b.y), div_safe(a.z, b.z));
}
bvm_extern void D__DIV_FLOAT3(float3 &dr, const float3 &a, const float3 &da, const float3 &b, const float3 &db)
{
	dr = float3((db.x != 0.0f) ? (da.x - db.x * a.x / b.x) / b.x : 0.0f,
	            (db.y != 0.0f) ? (da.y - db.y * a.y / b.y) / b.y : 0.0f,
	            (db.z != 0.0f) ? (da.z - db.z * a.z / b.z) / b.z : 0.0f);
}
BVM_DECL_FUNCTION_DUAL(DIV_FLOAT3)

bvm_extern void V__MUL_FLOAT3_FLOAT(float3 &r, const float3 &a, float b)
{
	mul_v3_v3fl(r.data(), a.data(), b);
}
bvm_extern void D__MUL_FLOAT3_FLOAT(float3 &dr, const float3 &a, const float3 &da, float b, float db)
{
	mul_v3_v3fl(dr.data(), da.data(), b);
	madd_v3_v3fl(dr.data(), a.data(), db);
}
BVM_DECL_FUNCTION_DUAL(MUL_FLOAT3_FLOAT)

bvm_extern void V__DIV_FLOAT3_FLOAT(float3 &r, const float3 &a, float b)
{
	if (b != 0.0f)
		r = float3(a.x/b, a.y/b, a.z/b);
	else
		r = float3(0.0f, 0.0f, 0.0f);
}
bvm_extern void D__DIV_FLOAT3_FLOAT(float3 &dr, const float3 &a, const float3 &da, float b, float db)
{
	if (b != 0.0f)
		dr = float3((da.x - db * a.x / b) / b,
		           (da.y - db * a.y / b) / b,
		           (da.z - db * a.z / b) / b);
	else
		dr = float3(0.0f, 0.0f, 0.0f);
}
BVM_DECL_FUNCTION_DUAL(DIV_FLOAT3_FLOAT)

bvm_extern void V__AVERAGE_FLOAT3(float3 &r, const float3 &a, const float3 &b)
{
	r = float3(0.5f*(a.x+b.x), 0.5f*(a.y+b.y), 0.5f*(a.z+b.z));
}
bvm_extern void D__AVERAGE_FLOAT3(float3 &dr, const float3 &/*a*/, const float3 &da, const float3 &/*b*/, const float3 &db)
{
	dr = float3(0.5f*(da.x+db.x), 0.5f*(da.y+db.y), 0.5f*(da.z+db.z));
}
BVM_DECL_FUNCTION_DUAL(AVERAGE_FLOAT3)

bvm_extern void V__DOT_FLOAT3(float &r, const float3 &a, const float3 &b)
{
	r = dot_v3v3(a.data(), b.data());
}
bvm_extern void D__DOT_FLOAT3(float &dr, const float3 &a, const float3 &da, const float3 &b, const float3 &db)
{
	dr = dot_v3v3(da.data(), b.data()) + dot_v3v3(a.data(), db.data());
}
BVM_DECL_FUNCTION_DUAL(DOT_FLOAT3)

bvm_extern void V__CROSS_FLOAT3(float3 &r, const float3 &a, const float3 &b)
{
	cross_v3_v3v3(r.data(), a.data(), b.data());
}
bvm_extern void D__CROSS_FLOAT3(float3 &dr, const float3 &a, const float3 &da, const float3 &b, const float3 &db)
{
	float3 c, d;
	cross_v3_v3v3(c.data(), da.data(), b.data());
	cross_v3_v3v3(d.data(), a.data(), db.data());
	add_v3_v3v3(dr.data(), c.data(), d.data());
}
BVM_DECL_FUNCTION_DUAL(CROSS_FLOAT3)

bvm_extern void V__NORMALIZE_FLOAT3(float3 &r_vec, float &r_len, const float3 &vec)
{
	r_len = normalize_v3_v3(r_vec.data(), vec.data());
}
bvm_extern void D__NORMALIZE_FLOAT3(float3 &dr_vec, float &dr_len, const float3 &vec, const float3 &dvec)
{
	float3 n;
	float len = normalize_v3_v3(n.data(), vec.data());
	if (len > 0.0f) {
		dr_len = dot_v3v3(n.data(), dvec.data());
		madd_v3_v3v3fl(dr_vec.data(), dvec.data(), n.data(), -dr_len);
		mul_v3_fl(dr_vec.data(), 1.0f / len);
	}
	else {
		dr_len = 0.0f;
		zero_v3(dr_vec.data());
	}
}
BVM_DECL_FUNCTION_DUAL(NORMALIZE_FLOAT3)

bvm_extern void V__LENGTH_FLOAT3(float &len, const float3 &vec)
{
	len = len_v3(vec.data());
}
bvm_extern void D__LENGTH_FLOAT3(float &dlen, const float3 &vec, const float3 &dvec)
{
	float len = len_v3(vec.data());
	if (len > 0.0f)
		dlen = dot_v3v3(vec.data(), dvec.data()) / len;
	else
		dlen = 0.0f;
}
BVM_DECL_FUNCTION_DUAL(LENGTH_FLOAT3)

bvm_extern void V__ADD_MATRIX44(matrix44 &r, const matrix44 &a, const matrix44 &b)
{
	add_m4_m4m4(r.c_data(), a.c_data(), b.c_data());
}
BVM_DECL_FUNCTION_VALUE(ADD_MATRIX44)

bvm_extern void V__SUB_MATRIX44(matrix44 &r, const matrix44 &a, const matrix44 &b)
{
	sub_m4_m4m4(r.c_data(), a.c_data(), b.c_data());
}
BVM_DECL_FUNCTION_VALUE(SUB_MATRIX44)

bvm_extern void V__MUL_MATRIX44(matrix44 &r, const matrix44 &a, const matrix44 &b)
{
	mul_m4_m4m4(r.c_data(), a.c_data(), b.c_data());
}
BVM_DECL_FUNCTION_VALUE(MUL_MATRIX44)

bvm_extern void V__MUL_MATRIX44_FLOAT(matrix44 &r, const matrix44 &a, float b)
{
	copy_m4_m4(r.c_data(), a.c_data());
	mul_m4_fl(r.c_data(), b);
}
BVM_DECL_FUNCTION_VALUE(MUL_MATRIX44_FLOAT)

bvm_extern void V__DIV_MATRIX44_FLOAT(matrix44 &r, const matrix44 &a, float b)
{
	copy_m4_m4(r.c_data(), a.c_data());
	mul_m4_fl(r.c_data(), div_safe(1.0f, b));
}
BVM_DECL_FUNCTION_VALUE(DIV_MATRIX44_FLOAT)

bvm_extern void V__NEGATE_MATRIX44(matrix44 &r, const matrix44 &m)
{
	copy_m4_m4(r.c_data(), m.c_data());
	negate_m4(r.c_data());
}
BVM_DECL_FUNCTION_VALUE(NEGATE_MATRIX44)

bvm_extern void V__TRANSPOSE_MATRIX44(matrix44 &r, const matrix44 &m)
{
	transpose_m4_m4(r.c_data(), m.c_data());
}
BVM_DECL_FUNCTION_VALUE(TRANSPOSE_MATRIX44)

bvm_extern void V__INVERT_MATRIX44(matrix44 &r, const matrix44 &m)
{
	invert_m4_m4_safe(r.c_data(), m.c_data());
}
BVM_DECL_FUNCTION_VALUE(INVERT_MATRIX44)

bvm_extern void V__ADJOINT_MATRIX44(matrix44 &r, const matrix44 &m)
{
	adjoint_m4_m4(r.c_data(), m.c_data());
}
BVM_DECL_FUNCTION_VALUE(ADJOINT_MATRIX44)

bvm_extern void V__DETERMINANT_MATRIX44(float &r, const matrix44 &m)
{
	r = determinant_m4(m.c_data());
}
BVM_DECL_FUNCTION_VALUE(DETERMINANT_MATRIX44)

bvm_extern void V__MUL_MATRIX44_FLOAT3(float3 &r, const matrix44 &a, const float3 &b)
{
	mul_v3_m4v3(r.data(), a.c_data(), b.data());
}
bvm_extern void D__MUL_MATRIX44_FLOAT3(float3 &dr, const matrix44 &a, const float3 &/*b*/, const float3 &db)
{
	mul_v3_mat3_m4v3(dr.data(), a.c_data(), db.data());
}
BVM_DECL_FUNCTION_DUAL(MUL_MATRIX44_FLOAT3)

bvm_extern void V__MUL_MATRIX44_FLOAT4(float4 &r, const matrix44 &a, const float4 &b)
{
	mul_v4_m4v4(r.data(), a.c_data(), b.data());
}
bvm_extern void D__MUL_MATRIX44_FLOAT4(float4 &dr, const matrix44 &a, const float4 &/*b*/, const float4 &db)
{
	mul_v4_m4v4(dr.data(), a.c_data(), db.data());
}
BVM_DECL_FUNCTION_DUAL(MUL_MATRIX44_FLOAT4)

BVM_MOD_NAMESPACE_END

#endif /* __MOD_MATH_H__ */

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

#ifndef __BVM_EVAL_MATH_H__
#define __BVM_EVAL_MATH_H__

/** \file bvm_eval_math.h
 *  \ingroup bvm
 */

extern "C" {
#include "BLI_math.h"
}

#include "bvm_eval_common.h"

#include "bvm_util_math.h"

namespace bvm {

static void eval_op_matrix44_to_locrotscale(float *stack, StackIndex offset_from, StackIndex offset_loc,
                                            StackIndex offset_rot, StackIndex offset_scale)
{
	matrix44 m = stack_load_matrix44(stack, offset_from);
	float loc[3], rot[4], scale[3];
	mat4_decompose(loc, rot, scale, m.data);
	stack_store_float3(stack, offset_loc, float3::from_data(loc));
	stack_store_float4(stack, offset_rot, float4::from_data(rot));
	stack_store_float3(stack, offset_scale, float3::from_data(scale));
}

static void eval_op_locrotscale_to_matrix44(float *stack, StackIndex offset_loc, StackIndex offset_rot,
                                              StackIndex offset_scale, StackIndex offset_to)
{
	float3 loc = stack_load_float3(stack, offset_loc);
	float4 rot = stack_load_float4(stack, offset_rot);
	float3 scale = stack_load_float3(stack, offset_scale);
	float mat[4][4];
	loc_quat_size_to_mat4(mat, loc.data(), rot.data(), scale.data());
	stack_store_matrix44(stack, offset_to, matrix44::from_data(&mat[0][0]));
}

static void eval_op_add_float(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	float a = stack_load_float(stack, offset_a);
	float b = stack_load_float(stack, offset_b);
	stack_store_float(stack, offset_r, a + b);
}

static void eval_op_sub_float(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	float a = stack_load_float(stack, offset_a);
	float b = stack_load_float(stack, offset_b);
	stack_store_float(stack, offset_r, a - b);
}

static void eval_op_mul_float(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	float a = stack_load_float(stack, offset_a);
	float b = stack_load_float(stack, offset_b);
	stack_store_float(stack, offset_r, a * b);
}

static void eval_op_div_float(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	float a = stack_load_float(stack, offset_a);
	float b = stack_load_float(stack, offset_b);
	stack_store_float(stack, offset_r, div_safe(a, b));
}

static void eval_op_sine(float *stack, StackIndex offset, StackIndex offset_r)
{
	float f = stack_load_float(stack, offset);
	stack_store_float(stack, offset_r, sinf(f));
}

static void eval_op_cosine(float *stack, StackIndex offset, StackIndex offset_r)
{
	float f = stack_load_float(stack, offset);
	stack_store_float(stack, offset_r, cosf(f));
}

static void eval_op_tangent(float *stack, StackIndex offset, StackIndex offset_r)
{
	float f = stack_load_float(stack, offset);
	stack_store_float(stack, offset_r, tanf(f));
}

static void eval_op_arcsine(float *stack, StackIndex offset, StackIndex offset_r)
{
	float f = stack_load_float(stack, offset);
	stack_store_float(stack, offset_r, asinf(f));
}

static void eval_op_arccosine(float *stack, StackIndex offset, StackIndex offset_r)
{
	float f = stack_load_float(stack, offset);
	stack_store_float(stack, offset_r, acosf(f));
}

static void eval_op_arctangent(float *stack, StackIndex offset, StackIndex offset_r)
{
	float f = stack_load_float(stack, offset);
	stack_store_float(stack, offset_r, atanf(f));
}

static void eval_op_power(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	float a = stack_load_float(stack, offset_a);
	float b = stack_load_float(stack, offset_b);
	stack_store_float(stack, offset_r, (a >= 0.0f)? powf(a, b): 0.0f);
}

static void eval_op_logarithm(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	float a = stack_load_float(stack, offset_a);
	float b = stack_load_float(stack, offset_b);
	stack_store_float(stack, offset_r, (a >= 0.0f && b >= 0.0f)? logf(a) / logf(b): 0.0f);
}

static void eval_op_minimum(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	float a = stack_load_float(stack, offset_a);
	float b = stack_load_float(stack, offset_b);
	stack_store_float(stack, offset_r, min_ff(a, b));
}

static void eval_op_maximum(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	float a = stack_load_float(stack, offset_a);
	float b = stack_load_float(stack, offset_b);
	stack_store_float(stack, offset_r, max_ff(a, b));
}

static void eval_op_round(float *stack, StackIndex offset, StackIndex offset_r)
{
	float f = stack_load_float(stack, offset);
	stack_store_float(stack, offset_r, floorf(f + 0.5f));
}

static void eval_op_less_than(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	float a = stack_load_float(stack, offset_a);
	float b = stack_load_float(stack, offset_b);
	stack_store_float(stack, offset_r, (a < b) ? 1.0f : 0.0f);
}

static void eval_op_greater_than(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	float a = stack_load_float(stack, offset_a);
	float b = stack_load_float(stack, offset_b);
	stack_store_float(stack, offset_r, (a > b) ? 1.0f : 0.0f);
}

static void eval_op_modulo(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	float a = stack_load_float(stack, offset_a);
	float b = stack_load_float(stack, offset_b);
	stack_store_float(stack, offset_r, (b != 0.0f) ? fmodf(a, b) : 0.0f);
}

static void eval_op_absolute(float *stack, StackIndex offset, StackIndex offset_r)
{
	float f = stack_load_float(stack, offset);
	stack_store_float(stack, offset_r, fabsf(f));
}

static void eval_op_clamp(float *stack, StackIndex offset, StackIndex offset_r)
{
	float f = stack_load_float(stack, offset);
	stack_store_float(stack, offset_r, CLAMPIS(f, 0.0f, 1.0f));
}

static void eval_op_add_float3(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	float3 a = stack_load_float3(stack, offset_a);
	float3 b = stack_load_float3(stack, offset_b);
	stack_store_float3(stack, offset_r, float3(a.x + b.x, a.y + b.y, a.z + b.z));
}

static void eval_op_sub_float3(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	float3 a = stack_load_float3(stack, offset_a);
	float3 b = stack_load_float3(stack, offset_b);
	stack_store_float3(stack, offset_r, float3(a.x - b.x, a.y - b.y, a.z - b.z));
}

static void eval_op_mul_float3(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	float3 a = stack_load_float3(stack, offset_a);
	float3 b = stack_load_float3(stack, offset_b);
	stack_store_float3(stack, offset_r, float3(a.x * b.x, a.y * b.y, a.z * b.z));
}

static void eval_op_div_float3(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	float3 a = stack_load_float3(stack, offset_a);
	float3 b = stack_load_float3(stack, offset_b);
	stack_store_float3(stack, offset_r, float3(div_safe(a.x, b.x), div_safe(a.y, b.y), div_safe(a.z, b.z)));
}

static void eval_op_mul_float3_float(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	float3 a = stack_load_float3(stack, offset_a);
	float b = stack_load_float(stack, offset_b);
	stack_store_float3(stack, offset_r, float3(a.x * b, a.y * b, a.z * b));
}

static void eval_op_div_float3_float(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	float3 a = stack_load_float3(stack, offset_a);
	float b = stack_load_float(stack, offset_b);
	stack_store_float3(stack, offset_r, float3(div_safe(a.x, b), div_safe(a.y, b), div_safe(a.z, b)));
}

static void eval_op_average_float3(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	float3 a = stack_load_float3(stack, offset_a);
	float3 b = stack_load_float3(stack, offset_b);
	stack_store_float3(stack, offset_r, float3(0.5f*(a.x+b.x), 0.5f*(a.y+b.y), 0.5f*(a.z+b.z)));
}

static void eval_op_dot_float3(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	float3 a = stack_load_float3(stack, offset_a);
	float3 b = stack_load_float3(stack, offset_b);
	stack_store_float(stack, offset_r, a.x * b.x + a.y * b.y + a.z * b.z);
}

static void eval_op_cross_float3(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	float3 a = stack_load_float3(stack, offset_a);
	float3 b = stack_load_float3(stack, offset_b);
	stack_store_float3(stack, offset_r, float3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x));
}

static void eval_op_normalize_float3(float *stack, StackIndex offset, StackIndex offset_vec, StackIndex offset_val)
{
	float3 v = stack_load_float3(stack, offset);
	float l = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
	float f = l > 0.0f ? 1.0f/l : 0.0f;
	float3 vec(v.x * f, v.y * f, v.z * f);
	stack_store_float3(stack, offset_vec, vec);
	stack_store_float(stack, offset_val, l);
}

static void eval_op_add_matrix44(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	matrix44 a = stack_load_matrix44(stack, offset_a);
	matrix44 b = stack_load_matrix44(stack, offset_b);
	matrix44 r;
	add_m4_m4m4(r.data, a.data, b.data);
	stack_store_matrix44(stack, offset_r, r);
}

static void eval_op_sub_matrix44(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	matrix44 a = stack_load_matrix44(stack, offset_a);
	matrix44 b = stack_load_matrix44(stack, offset_b);
	matrix44 r;
	sub_m4_m4m4(r.data, a.data, b.data);
	stack_store_matrix44(stack, offset_r, r);
}

static void eval_op_mul_matrix44(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	matrix44 a = stack_load_matrix44(stack, offset_a);
	matrix44 b = stack_load_matrix44(stack, offset_b);
	matrix44 r;
	mul_m4_m4m4(r.data, a.data, b.data);
	stack_store_matrix44(stack, offset_r, r);
}

static void eval_op_mul_matrix44_float(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	matrix44 a = stack_load_matrix44(stack, offset_a);
	float b = stack_load_float(stack, offset_b);
	matrix44 r;
	copy_m4_m4(r.data, a.data);
	mul_m4_fl(r.data, b);
	stack_store_matrix44(stack, offset_r, r);
}

static void eval_op_div_matrix44_float(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	matrix44 a = stack_load_matrix44(stack, offset_a);
	float b = stack_load_float(stack, offset_b);
	matrix44 r;
	copy_m4_m4(r.data, a.data);
	mul_m4_fl(r.data, div_safe(1.0, b));
	stack_store_matrix44(stack, offset_r, r);
}

static void eval_op_negate_matrix44(float *stack, StackIndex offset, StackIndex offset_r)
{
	matrix44 m = stack_load_matrix44(stack, offset);
	matrix44 r;
	copy_m4_m4(r.data, m.data);
	negate_m4(r.data);
	stack_store_matrix44(stack, offset_r, r);
}

static void eval_op_transpose_matrix44(float *stack, StackIndex offset, StackIndex offset_r)
{
	matrix44 m = stack_load_matrix44(stack, offset);
	matrix44 r;
	transpose_m4_m4(r.data, m.data);
	stack_store_matrix44(stack, offset_r, r);
}

static void eval_op_invert_matrix44(float *stack, StackIndex offset, StackIndex offset_r)
{
	matrix44 m = stack_load_matrix44(stack, offset);
	matrix44 r;
	invert_m4_m4_safe(r.data, m.data);
	stack_store_matrix44(stack, offset_r, r);
}

static void eval_op_adjoint_matrix44(float *stack, StackIndex offset, StackIndex offset_r)
{
	matrix44 m = stack_load_matrix44(stack, offset);
	matrix44 r;
	adjoint_m4_m4(r.data, m.data);
	stack_store_matrix44(stack, offset_r, r);
}

static void eval_op_determinant_matrix44(float *stack, StackIndex offset, StackIndex offset_r)
{
	matrix44 m = stack_load_matrix44(stack, offset);
	float d = determinant_m4(m.data);
	stack_store_float(stack, offset_r, d);
}

static void eval_op_mul_matrix44_float3(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	matrix44 a = stack_load_matrix44(stack, offset_a);
	float3 b = stack_load_float3(stack, offset_b);
	float3 r;
	mul_v3_m4v3(r.data(), a.data, b.data());
	stack_store_float3(stack, offset_r, r);
}

static void eval_op_mul_matrix44_float4(float *stack, StackIndex offset_a, StackIndex offset_b, StackIndex offset_r)
{
	matrix44 a = stack_load_matrix44(stack, offset_a);
	float4 b = stack_load_float4(stack, offset_b);
	float4 r;
	mul_v4_m4v4(r.data(), a.data, b.data());
	stack_store_float4(stack, offset_r, r);
}

} /* namespace bvm */

#endif /* __BVM_EVAL_MATH_H__ */

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

/** \file bvm_eval.cc
 *  \ingroup bvm
 */

#include <cassert>

extern "C" {
#include "BLI_math.h"

#include "DNA_object_types.h"

#include "BKE_bvhutils.h"
#include "BKE_DerivedMesh.h"
}

#include "bvm_eval.h"
#include "bvm_eval_common.h"
#include "bvm_eval_texture.h"

namespace bvm {

EvalContext::EvalContext()
{
}

EvalContext::~EvalContext()
{
}

/* ------------------------------------------------------------------------- */

static void eval_op_value_float(float *stack, float value, StackIndex offset)
{
	stack_store_float(stack, offset, value);
}

static void eval_op_value_float3(float *stack, float3 value, StackIndex offset)
{
	stack_store_float3(stack, offset, value);
}

static void eval_op_value_float4(float *stack, float4 value, StackIndex offset)
{
	stack_store_float4(stack, offset, value);
}

static void eval_op_value_int(float *stack, int value, StackIndex offset)
{
	stack_store_int(stack, offset, value);
}

static void eval_op_value_matrix44(float *stack, matrix44 value, StackIndex offset)
{
	stack_store_matrix44(stack, offset, value);
}

static void eval_op_pass_float(float *stack, StackIndex offset_from, StackIndex offset_to)
{
	float f = stack_load_float(stack, offset_from);
	stack_store_float(stack, offset_to, f);
}

static void eval_op_pass_float3(float *stack, StackIndex offset_from, StackIndex offset_to)
{
	float3 f = stack_load_float3(stack, offset_from);
	stack_store_float3(stack, offset_to, f);
}

static void eval_op_pass_float4(float *stack, StackIndex offset_from, StackIndex offset_to)
{
	float4 f = stack_load_float4(stack, offset_from);
	stack_store_float4(stack, offset_to, f);
}

static void eval_op_set_float3(float *stack, StackIndex offset_x, StackIndex offset_y, StackIndex offset_z, StackIndex offset_to)
{
	float x = stack_load_float(stack, offset_x);
	float y = stack_load_float(stack, offset_y);
	float z = stack_load_float(stack, offset_z);
	stack_store_float3(stack, offset_to, float3(x, y, z));
}

static void eval_op_set_float4(float *stack, StackIndex offset_x, StackIndex offset_y,
                               StackIndex offset_z, StackIndex offset_w, StackIndex offset_to)
{
	float x = stack_load_float(stack, offset_x);
	float y = stack_load_float(stack, offset_y);
	float z = stack_load_float(stack, offset_z);
	float w = stack_load_float(stack, offset_w);
	stack_store_float4(stack, offset_to, float4(x, y, z, w));
}

static void eval_op_get_elem_float3(float *stack, int index, StackIndex offset_from, StackIndex offset_to)
{
	assert(index >= 0 && index < 3);
	float3 f = stack_load_float3(stack, offset_from);
	stack_store_float(stack, offset_to, f[index]);
}

static void eval_op_get_elem_float4(float *stack, int index, StackIndex offset_from, StackIndex offset_to)
{
	assert(index >= 0 && index < 4);
	float4 f = stack_load_float4(stack, offset_from);
	stack_store_float(stack, offset_to, f[index]);
}

static void eval_op_point_position(const EvalData *data, float *stack, StackIndex offset)
{
	stack_store_float3(stack, offset, data->effector.position);
}

static void eval_op_point_velocity(const EvalData *data, float *stack, StackIndex offset)
{
	stack_store_float3(stack, offset, data->effector.velocity);
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
	stack_store_float(stack, offset_r, b != 0.0f ? a / b : 0.0f);
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

static void eval_op_tex_coord(const EvalData *data, float *stack, StackIndex offset)
{
	stack_store_float3(stack, offset, data->texture.co);
}

static void eval_op_effector_transform(const EvalGlobals *globals, float *stack, int object_index, StackIndex offset_tfm)
{
	Object *ob = globals->objects[object_index];
	matrix44 m = matrix44::from_data(&ob->obmat[0][0], matrix44::COL_MAJOR);
	stack_store_matrix44(stack, offset_tfm, m);
}

static void eval_op_effector_closest_point(const EvalGlobals *globals, float *stack, int object_index, StackIndex offset_vector,
                                           StackIndex offset_position, StackIndex offset_normal, StackIndex offset_tangent)
{
	Object *ob = globals->objects[object_index];
	DerivedMesh *dm = object_get_derived_final(ob, false);
	
	float world[4][4];
	SpaceTransform transform;
	unit_m4(world);
	BLI_space_transform_from_matrices(&transform, world, ob->obmat);
	
	float3 vec;
	vec = stack_load_float3(stack, offset_vector);
	BLI_space_transform_apply(&transform, &vec.x);
	
	BVHTreeFromMesh treeData = {NULL};
	bvhtree_from_mesh_looptri(&treeData, dm, 0.0, 2, 6);
	
	BVHTreeNearest nearest;
	nearest.index = -1;
	nearest.dist_sq = FLT_MAX;
	BLI_bvhtree_find_nearest(treeData.tree, &vec.x, &nearest, treeData.nearest_callback, &treeData);
	
	if (nearest.index != -1) {
		float3 pos, nor;
		copy_v3_v3(&pos.x, nearest.co);
		copy_v3_v3(&nor.x, nearest.no);
		BLI_space_transform_invert(&transform, &pos.x);
		BLI_space_transform_invert_normal(&transform, &nor.x);
		
		stack_store_float3(stack, offset_position, pos);
		stack_store_float3(stack, offset_normal, nor);
		// TODO
		stack_store_float3(stack, offset_tangent, float3(0.0f, 0.0f, 0.0f));
	}
}

void EvalContext::eval_instructions(const EvalGlobals *globals, const EvalData *data, const Expression *expr, float *stack) const
{
	int instr = 0;
	
	while (true) {
		OpCode op = expr->read_opcode(&instr);
		
		switch(op) {
			case OP_NOOP:
				break;
			case OP_VALUE_FLOAT: {
				float value = expr->read_float(&instr);
				StackIndex offset = expr->read_stack_index(&instr);
				eval_op_value_float(stack, value, offset);
				break;
			}
			case OP_VALUE_FLOAT3: {
				float3 value = expr->read_float3(&instr);
				StackIndex offset = expr->read_stack_index(&instr);
				eval_op_value_float3(stack, value, offset);
				break;
			}
			case OP_VALUE_FLOAT4: {
				float4 value = expr->read_float4(&instr);
				StackIndex offset = expr->read_stack_index(&instr);
				eval_op_value_float4(stack, value, offset);
				break;
			}
			case OP_VALUE_INT: {
				int value = expr->read_int(&instr);
				StackIndex offset = expr->read_stack_index(&instr);
				eval_op_value_int(stack, value, offset);
				break;
			}
			case OP_VALUE_MATRIX44: {
				matrix44 value = expr->read_matrix44(&instr);
				StackIndex offset = expr->read_stack_index(&instr);
				eval_op_value_matrix44(stack, value, offset);
				break;
			}
			case OP_PASS_FLOAT: {
				StackIndex offset_from = expr->read_stack_index(&instr);
				StackIndex offset_to = expr->read_stack_index(&instr);
				eval_op_pass_float(stack, offset_from, offset_to);
				break;
			}
			case OP_PASS_FLOAT3: {
				StackIndex offset_from = expr->read_stack_index(&instr);
				StackIndex offset_to = expr->read_stack_index(&instr);
				eval_op_pass_float3(stack, offset_from, offset_to);
				break;
			}
			case OP_PASS_FLOAT4: {
				StackIndex offset_from = expr->read_stack_index(&instr);
				StackIndex offset_to = expr->read_stack_index(&instr);
				eval_op_pass_float4(stack, offset_from, offset_to);
				break;
			}
			case OP_SET_FLOAT3: {
				StackIndex offset_x = expr->read_stack_index(&instr);
				StackIndex offset_y = expr->read_stack_index(&instr);
				StackIndex offset_z = expr->read_stack_index(&instr);
				StackIndex offset_to = expr->read_stack_index(&instr);
				eval_op_set_float3(stack, offset_x, offset_y, offset_z, offset_to);
				break;
			}
			case OP_GET_ELEM_FLOAT3: {
				int index = expr->read_int(&instr);
				StackIndex offset_from = expr->read_stack_index(&instr);
				StackIndex offset_to = expr->read_stack_index(&instr);
				eval_op_get_elem_float3(stack, index, offset_from, offset_to);
				break;
			}
			case OP_SET_FLOAT4: {
				StackIndex offset_x = expr->read_stack_index(&instr);
				StackIndex offset_y = expr->read_stack_index(&instr);
				StackIndex offset_z = expr->read_stack_index(&instr);
				StackIndex offset_w = expr->read_stack_index(&instr);
				StackIndex offset_to = expr->read_stack_index(&instr);
				eval_op_set_float4(stack, offset_x, offset_y, offset_z, offset_w, offset_to);
				break;
			}
			case OP_GET_ELEM_FLOAT4: {
				int index = expr->read_int(&instr);
				StackIndex offset_from = expr->read_stack_index(&instr);
				StackIndex offset_to = expr->read_stack_index(&instr);
				eval_op_get_elem_float4(stack, index, offset_from, offset_to);
				break;
			}
			case OP_POINT_POSITION: {
				StackIndex offset = expr->read_stack_index(&instr);
				eval_op_point_position(data, stack, offset);
				break;
			}
			case OP_POINT_VELOCITY: {
				StackIndex offset = expr->read_stack_index(&instr);
				eval_op_point_velocity(data, stack, offset);
				break;
			}
			case OP_ADD_FLOAT: {
				StackIndex offset_a = expr->read_stack_index(&instr);
				StackIndex offset_b = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_add_float(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_SUB_FLOAT: {
				StackIndex offset_a = expr->read_stack_index(&instr);
				StackIndex offset_b = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_sub_float(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_MUL_FLOAT: {
				StackIndex offset_a = expr->read_stack_index(&instr);
				StackIndex offset_b = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_mul_float(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_DIV_FLOAT: {
				StackIndex offset_a = expr->read_stack_index(&instr);
				StackIndex offset_b = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_div_float(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_SINE: {
				StackIndex offset = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_sine(stack, offset, offset_r);
				break;
			}
			case OP_COSINE: {
				StackIndex offset = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_cosine(stack, offset, offset_r);
				break;
			}
			case OP_TANGENT: {
				StackIndex offset = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_tangent(stack, offset, offset_r);
				break;
			}
			case OP_ARCSINE: {
				StackIndex offset = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_arcsine(stack, offset, offset_r);
				break;
			}
			case OP_ARCCOSINE: {
				StackIndex offset = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_arccosine(stack, offset, offset_r);
				break;
			}
			case OP_ARCTANGENT: {
				StackIndex offset = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_arctangent(stack, offset, offset_r);
				break;
			}
			case OP_POWER: {
				StackIndex offset_a = expr->read_stack_index(&instr);
				StackIndex offset_b = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_power(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_LOGARITHM: {
				StackIndex offset_a = expr->read_stack_index(&instr);
				StackIndex offset_b = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_logarithm(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_MINIMUM: {
				StackIndex offset_a = expr->read_stack_index(&instr);
				StackIndex offset_b = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_minimum(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_MAXIMUM: {
				StackIndex offset_a = expr->read_stack_index(&instr);
				StackIndex offset_b = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_maximum(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_ROUND: {
				StackIndex offset = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_round(stack, offset, offset_r);
				break;
			}
			case OP_LESS_THAN: {
				StackIndex offset_a = expr->read_stack_index(&instr);
				StackIndex offset_b = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_less_than(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_GREATER_THAN: {
				StackIndex offset_a = expr->read_stack_index(&instr);
				StackIndex offset_b = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_greater_than(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_MODULO: {
				StackIndex offset_a = expr->read_stack_index(&instr);
				StackIndex offset_b = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_modulo(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_ABSOLUTE: {
				StackIndex offset = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_absolute(stack, offset, offset_r);
				break;
			}
			case OP_CLAMP: {
				StackIndex offset = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_clamp(stack, offset, offset_r);
				break;
			}
			case OP_ADD_FLOAT3: {
				StackIndex offset_a = expr->read_stack_index(&instr);
				StackIndex offset_b = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_add_float3(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_SUB_FLOAT3: {
				StackIndex offset_a = expr->read_stack_index(&instr);
				StackIndex offset_b = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_sub_float3(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_AVERAGE_FLOAT3: {
				StackIndex offset_a = expr->read_stack_index(&instr);
				StackIndex offset_b = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_average_float3(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_DOT_FLOAT3: {
				StackIndex offset_a = expr->read_stack_index(&instr);
				StackIndex offset_b = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_dot_float3(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_CROSS_FLOAT3: {
				StackIndex offset_a = expr->read_stack_index(&instr);
				StackIndex offset_b = expr->read_stack_index(&instr);
				StackIndex offset_r = expr->read_stack_index(&instr);
				eval_op_cross_float3(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_NORMALIZE_FLOAT3: {
				StackIndex offset = expr->read_stack_index(&instr);
				StackIndex offset_vec = expr->read_stack_index(&instr);
				StackIndex offset_val = expr->read_stack_index(&instr);
				eval_op_normalize_float3(stack, offset, offset_vec, offset_val);
				break;
			}
			
			case OP_TEX_COORD: {
				StackIndex offset = expr->read_stack_index(&instr);
				eval_op_tex_coord(data, stack, offset);
				break;
			}
			case OP_TEX_PROC_VORONOI: {
				int distance_metric = expr->read_int(&instr);
				int color_type = expr->read_int(&instr);
				StackIndex iMinkowskiExponent = expr->read_stack_index(&instr);
				StackIndex iScale = expr->read_stack_index(&instr);
				StackIndex iNoiseSize = expr->read_stack_index(&instr);
				StackIndex iNabla = expr->read_stack_index(&instr);
				StackIndex iW1 = expr->read_stack_index(&instr);
				StackIndex iW2 = expr->read_stack_index(&instr);
				StackIndex iW3 = expr->read_stack_index(&instr);
				StackIndex iW4 = expr->read_stack_index(&instr);
				StackIndex iPos = expr->read_stack_index(&instr);
				StackIndex oIntensity = expr->read_stack_index(&instr);
				StackIndex oColor = expr->read_stack_index(&instr);
				StackIndex oNormal = expr->read_stack_index(&instr);
				eval_op_tex_proc_voronoi(stack, distance_metric, color_type,
				                         iMinkowskiExponent, iScale, iNoiseSize, iNabla,
				                         iW1, iW2, iW3, iW4, iPos,
				                         oIntensity, oColor, oNormal);
				break;
			}
			
			case OP_EFFECTOR_TRANSFORM: {
				int object_index = expr->read_int(&instr);
				StackIndex offset_tfm = expr->read_stack_index(&instr);
				eval_op_effector_transform(globals, stack, object_index, offset_tfm);
				break;
			}
			case OP_EFFECTOR_CLOSEST_POINT: {
				int object_index = expr->read_int(&instr);
				StackIndex offset_vector = expr->read_stack_index(&instr);
				StackIndex offset_position = expr->read_stack_index(&instr);
				StackIndex offset_normal = expr->read_stack_index(&instr);
				StackIndex offset_tangent = expr->read_stack_index(&instr);
				eval_op_effector_closest_point(globals, stack, object_index, offset_vector,
				                               offset_position, offset_normal, offset_tangent);
				break;
			}
			case OP_END:
				return;
			default:
				assert(!"Unknown opcode");
				return;
		}
	}
}

void EvalContext::eval_expression(const EvalGlobals *globals, const EvalData *data, const Expression *expr, void **results) const
{
	float stack[BVM_STACK_SIZE];
	
	eval_instructions(globals, data, expr, stack);
	
	for (int i = 0; i < expr->return_values_size(); ++i) {
		const ReturnValue &rval = expr->return_value(i);
		float *value = &stack[rval.stack_offset];
		
		rval.typedesc.copy_value(results[i], (void *)value);
	}
}

} /* namespace bvm */

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
#include "BLI_ghash.h"

#include "DNA_ID.h"
#include "DNA_object_types.h"

#include "BKE_bvhutils.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_DerivedMesh.h"
#include "BKE_image.h"
#include "BKE_material.h"

#include "IMB_imbuf_types.h"
}

#include "bvm_eval.h"
#include "bvm_eval_common.h"
#include "bvm_eval_curve.h"
#include "bvm_eval_image.h"
#include "bvm_eval_math.h"
#include "bvm_eval_mesh.h"
#include "bvm_eval_texture.h"

#include "util_hash.h"

namespace blenvm {

EvalGlobals::EvalGlobals()
{
	m_image_pool = BKE_image_pool_new();
}

EvalGlobals::~EvalGlobals()
{
	BKE_image_pool_free(m_image_pool);
}

int EvalGlobals::get_id_key(ID *id)
{
	int hash = BLI_ghashutil_strhash(id->name);
	if (id->lib) {
		hash = hash_combine(hash, BLI_ghashutil_strhash(id->lib->name));
	}
	return hash;
}

void EvalGlobals::add_object(int key, Object *ob)
{
	m_objects[key] = ob;
}

PointerRNA EvalGlobals::lookup_object(int key) const
{
	ObjectMap::const_iterator it = m_objects.find(key);
	if (it != m_objects.end()) {
		PointerRNA ptr;
		RNA_id_pointer_create((ID *)it->second, &ptr);
		return ptr;
	}
	else {
		return PointerRNA_NULL;
	}
}

void EvalGlobals::add_image(int key, Image *ima)
{
	m_images[key] = ima;
}

ImBuf *EvalGlobals::lookup_imbuf(int key, ImageUser *iuser) const
{
	ImageMap::const_iterator ima_it = m_images.find(key);
	Image *ima = (ima_it != m_images.end()) ? ima_it->second : NULL;
	if (!ima)
		return NULL;
	
	/* local changes to the original ImageUser */
//	if (!BKE_image_is_multilayer(ima))
//		iuser->multi_index = BKE_scene_multiview_view_id_get(this->m_rd, this->m_viewName);
	
	ImBuf *ibuf = BKE_image_pool_acquire_ibuf(ima, iuser, m_image_pool);
	if (!ibuf || (!ibuf->rect && !ibuf->rect_float)) {
		BKE_image_pool_release_ibuf(ima, ibuf, m_image_pool);
		return NULL;
	}
	
	return ibuf;
}

/* ------------------------------------------------------------------------- */

int EvalStack::stack_size(size_t datasize)
{
	return int_div_ceil(datasize, sizeof(EvalStack));
}

/* ------------------------------------------------------------------------- */

EvalContext::EvalContext()
{
}

EvalContext::~EvalContext()
{
}

/* ------------------------------------------------------------------------- */

static void eval_op_value_float(EvalStack *stack, float value, StackIndex offset)
{
	stack_store_float(stack, offset, value);
}

static void eval_op_value_float3(EvalStack *stack, float3 value, StackIndex offset)
{
	stack_store_float3(stack, offset, value);
}

static void eval_op_value_float4(EvalStack *stack, float4 value, StackIndex offset)
{
	stack_store_float4(stack, offset, value);
}

static void eval_op_value_int(EvalStack *stack, int value, StackIndex offset)
{
	stack_store_int(stack, offset, value);
}

static void eval_op_value_matrix44(EvalStack *stack, matrix44 value, StackIndex offset)
{
	stack_store_matrix44(stack, offset, value);
}

static void eval_op_value_string(EvalStack *stack, const char *value, StackIndex offset)
{
	stack_store_string(stack, offset, value);
}

/* Note: pointer data is not explicitly stored on the stack,
 * this function always creates simply a NULL pointer.
 */
static void eval_op_value_rnapointer(EvalStack *stack, StackIndex offset)
{
	stack_store_rnapointer(stack, offset, PointerRNA_NULL);
}

/* Note: mesh data is not explicitly stored on the stack,
 * this function always creates simply an empty mesh.
 */
static void eval_op_value_mesh(EvalStack *stack, StackIndex offset)
{
	stack_store_mesh(stack, offset, CDDM_new(0, 0, 0, 0, 0));
}

/* Note: dupli data is not explicitly stored on the stack,
 * this function always creates simply an empty dupli list.
 */
static void eval_op_value_duplis(EvalStack *stack, StackIndex offset)
{
	stack_store_duplis(stack, offset, new DupliList());
}

static void eval_op_range_int(EvalStack *stack,
                              int start, int /*end*/, int step,
                              StackIndex offset_index,
                              StackIndex offset_value)
{
	int index = stack_load_int(stack, offset_index);
	stack_store_int(stack, offset_value, start + index * step);
}

static void eval_op_float_to_int(EvalStack *stack, StackIndex offset_from, StackIndex offset_to)
{
	float f = stack_load_float(stack, offset_from);
	stack_store_int(stack, offset_to, (int)f);
}

static void eval_op_int_to_float(EvalStack *stack, StackIndex offset_from, StackIndex offset_to)
{
	int i = stack_load_int(stack, offset_from);
	stack_store_float(stack, offset_to, (float)i);
}

static void eval_op_set_float3(EvalStack *stack, StackIndex offset_x, StackIndex offset_y, StackIndex offset_z, StackIndex offset_to)
{
	float x = stack_load_float(stack, offset_x);
	float y = stack_load_float(stack, offset_y);
	float z = stack_load_float(stack, offset_z);
	stack_store_float3(stack, offset_to, float3(x, y, z));
}

static void eval_op_set_float4(EvalStack *stack, StackIndex offset_x, StackIndex offset_y,
                               StackIndex offset_z, StackIndex offset_w, StackIndex offset_to)
{
	float x = stack_load_float(stack, offset_x);
	float y = stack_load_float(stack, offset_y);
	float z = stack_load_float(stack, offset_z);
	float w = stack_load_float(stack, offset_w);
	stack_store_float4(stack, offset_to, float4(x, y, z, w));
}

static void eval_op_get_elem_float3(EvalStack *stack, int index, StackIndex offset_from, StackIndex offset_to)
{
	assert(index >= 0 && index < 3);
	float3 f = stack_load_float3(stack, offset_from);
	stack_store_float(stack, offset_to, f[index]);
}

static void eval_op_get_elem_float4(EvalStack *stack, int index, StackIndex offset_from, StackIndex offset_to)
{
	assert(index >= 0 && index < 4);
	float4 f = stack_load_float4(stack, offset_from);
	stack_store_float(stack, offset_to, f[index]);
}

static void eval_op_init_mesh_ptr(EvalStack *stack, StackIndex offset, int use_count)
{
	mesh_ptr p(NULL);
	p.set_use_count(use_count);
	stack_store_mesh_ptr(stack, offset, p);
}

static void eval_op_release_mesh_ptr(EvalStack *stack, StackIndex offset)
{
	mesh_ptr p = stack_load_mesh_ptr(stack, offset);
	p.decrement_use_count();
	stack_store_mesh_ptr(stack, offset, p);
}

static void eval_op_init_duplis_ptr(EvalStack *stack, StackIndex offset, int use_count)
{
	static DupliList __empty_duplilist__ = DupliList();
	duplis_ptr p(&__empty_duplilist__);
	p.set_use_count(use_count);
	stack_store_duplis_ptr(stack, offset, p);
}

static void eval_op_release_duplis_ptr(EvalStack *stack, StackIndex offset)
{
	duplis_ptr p = stack_load_duplis_ptr(stack, offset);
	p.decrement_use_count();
	stack_store_duplis_ptr(stack, offset, p);
}

static void eval_op_mix_rgb(EvalStack *stack, int mode, StackIndex offset_col_a, StackIndex offset_col_b, StackIndex offset_fac, StackIndex offset_r)
{
	float4 a = stack_load_float4(stack, offset_col_a);
	float4 b = stack_load_float4(stack, offset_col_b);
	float f = stack_load_float(stack, offset_fac);
	
	ramp_blend(mode, a.data(), f, b.data());
	
	stack_store_float4(stack, offset_r, a);
}

static void eval_op_object_lookup(const EvalGlobals *globals, EvalStack *stack, int key, StackIndex offset_object)
{
	PointerRNA ptr = globals->lookup_object(key);
	stack_store_rnapointer(stack, offset_object, ptr);
}

static void eval_op_object_transform(EvalStack *stack, StackIndex offset_object, StackIndex offset_transform)
{
	PointerRNA ptr = stack_load_rnapointer(stack, offset_object);
	matrix44 obmat;
	if (ptr.data && RNA_struct_is_a(&RNA_Object, ptr.type)) {
		Object *ob = (Object *)ptr.data;
		copy_m4_m4(obmat.data, ob->obmat);
	}
	else
		obmat = matrix44::identity();
	
	stack_store_matrix44(stack, offset_transform, obmat);
}

static void eval_op_effector_transform(const EvalGlobals *globals, EvalStack *stack, int object_index, StackIndex offset_tfm)
{
	// TODO the way objects are stored in globals has changed a lot, this needs updating
	(void)globals;
	(void)stack;
	(void)object_index;
	(void)offset_tfm;
//	Object *ob = globals->objects[object_index];
//	matrix44 m = matrix44::from_data(&ob->obmat[0][0], matrix44::COL_MAJOR);
//	stack_store_matrix44(stack, offset_tfm, m);
}

static void eval_op_effector_closest_point(EvalStack *stack, StackIndex offset_object, StackIndex offset_vector,
                                           StackIndex offset_position, StackIndex offset_normal, StackIndex offset_tangent)
{
	PointerRNA ptr = stack_load_rnapointer(stack, offset_object);
	if (!ptr.data)
		return;
	Object *ob = (Object *)ptr.data;
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

static void eval_op_make_dupli(EvalStack *stack, StackIndex offset_object, StackIndex offset_transform, StackIndex offset_index,
                               StackIndex offset_hide, StackIndex offset_recursive, StackIndex offset_dupli)
{
	PointerRNA object = stack_load_rnapointer(stack, offset_object);
	if (!object.data || !RNA_struct_is_a(&RNA_Object, object.type))
		return;
	
	DupliList *list = new DupliList(1);
	Dupli &dupli = list->back();
	dupli.object = (Object *)object.data;
	dupli.transform = stack_load_matrix44(stack, offset_transform);
	dupli.index = stack_load_int(stack, offset_index);
	dupli.hide = stack_load_int(stack, offset_hide);
	dupli.recursive = stack_load_int(stack, offset_recursive);
	
	stack_store_duplis(stack, offset_dupli, list);
}

static void eval_op_duplis_combine(EvalStack *stack, StackIndex offset_duplis_a, StackIndex offset_duplis_b,
                                   StackIndex offset_duplis)
{
	const DupliList *a = stack_load_duplis(stack, offset_duplis_a);
	const DupliList *b = stack_load_duplis(stack, offset_duplis_b);
	
	DupliList *result = new DupliList();
	result->reserve(a->size() + b->size());
	result->insert(result->end(), a->begin(), a->end());
	result->insert(result->end(), b->begin(), b->end());
	
	stack_store_duplis(stack, offset_duplis, result);
}

void EvalContext::eval_instructions(const EvalGlobals *globals, const InstructionList *fn, int entry_point, EvalStack *stack) const
{
	EvalKernelData kd;
	kd.context = this;
	kd.function = fn;
	int instr = entry_point;
	
	while (true) {
		OpCode op = fn->read_opcode(&instr);
		
		switch(op) {
			case OP_NOOP:
				break;
			case OP_VALUE_FLOAT: {
				float value = fn->read_float(&instr);
				StackIndex offset = fn->read_stack_index(&instr);
				eval_op_value_float(stack, value, offset);
				break;
			}
			case OP_VALUE_FLOAT3: {
				float3 value = fn->read_float3(&instr);
				StackIndex offset = fn->read_stack_index(&instr);
				eval_op_value_float3(stack, value, offset);
				break;
			}
			case OP_VALUE_FLOAT4: {
				float4 value = fn->read_float4(&instr);
				StackIndex offset = fn->read_stack_index(&instr);
				eval_op_value_float4(stack, value, offset);
				break;
			}
			case OP_VALUE_INT: {
				int value = fn->read_int(&instr);
				StackIndex offset = fn->read_stack_index(&instr);
				eval_op_value_int(stack, value, offset);
				break;
			}
			case OP_VALUE_MATRIX44: {
				matrix44 value = fn->read_matrix44(&instr);
				StackIndex offset = fn->read_stack_index(&instr);
				eval_op_value_matrix44(stack, value, offset);
				break;
			}
			case OP_VALUE_STRING: {
				const char *value = fn->read_string(&instr);
				StackIndex offset = fn->read_stack_index(&instr);
				eval_op_value_string(stack, value, offset);
				break;
			}
			case OP_VALUE_RNAPOINTER: {
				StackIndex offset = fn->read_stack_index(&instr);
				eval_op_value_rnapointer(stack, offset);
				break;
			}
			case OP_VALUE_MESH: {
				StackIndex offset = fn->read_stack_index(&instr);
				eval_op_value_mesh(stack, offset);
				break;
			}
			case OP_VALUE_DUPLIS: {
				StackIndex offset = fn->read_stack_index(&instr);
				eval_op_value_duplis(stack, offset);
				break;
			}
			case OP_RANGE_INT: {
				StackIndex offset_index = fn->read_stack_index(&instr);
				int start = fn->read_int(&instr);
				int end = fn->read_int(&instr);
				int step = fn->read_int(&instr);
				StackIndex offset_value = fn->read_stack_index(&instr);
				eval_op_range_int(stack, start, end, step, offset_index, offset_value);
				break;
			}
			case OP_FLOAT_TO_INT: {
				StackIndex offset_from = fn->read_stack_index(&instr);
				StackIndex offset_to = fn->read_stack_index(&instr);
				eval_op_float_to_int(stack, offset_from, offset_to);
				break;
			}
			case OP_INT_TO_FLOAT: {
				StackIndex offset_from = fn->read_stack_index(&instr);
				StackIndex offset_to = fn->read_stack_index(&instr);
				eval_op_int_to_float(stack, offset_from, offset_to);
				break;
			}
			case OP_SET_FLOAT3: {
				StackIndex offset_x = fn->read_stack_index(&instr);
				StackIndex offset_y = fn->read_stack_index(&instr);
				StackIndex offset_z = fn->read_stack_index(&instr);
				StackIndex offset_to = fn->read_stack_index(&instr);
				eval_op_set_float3(stack, offset_x, offset_y, offset_z, offset_to);
				break;
			}
			case OP_GET_ELEM_FLOAT3: {
				int index = fn->read_int(&instr);
				StackIndex offset_from = fn->read_stack_index(&instr);
				StackIndex offset_to = fn->read_stack_index(&instr);
				eval_op_get_elem_float3(stack, index, offset_from, offset_to);
				break;
			}
			case OP_SET_FLOAT4: {
				StackIndex offset_x = fn->read_stack_index(&instr);
				StackIndex offset_y = fn->read_stack_index(&instr);
				StackIndex offset_z = fn->read_stack_index(&instr);
				StackIndex offset_w = fn->read_stack_index(&instr);
				StackIndex offset_to = fn->read_stack_index(&instr);
				eval_op_set_float4(stack, offset_x, offset_y, offset_z, offset_w, offset_to);
				break;
			}
			case OP_GET_ELEM_FLOAT4: {
				int index = fn->read_int(&instr);
				StackIndex offset_from = fn->read_stack_index(&instr);
				StackIndex offset_to = fn->read_stack_index(&instr);
				eval_op_get_elem_float4(stack, index, offset_from, offset_to);
				break;
			}
			case OP_INIT_MESH_PTR: {
				StackIndex offset = fn->read_stack_index(&instr);
				int use_count = fn->read_int(&instr);
				eval_op_init_mesh_ptr(stack, offset, use_count);
				break;
			}
			case OP_RELEASE_MESH_PTR: {
				StackIndex offset = fn->read_stack_index(&instr);
				eval_op_release_mesh_ptr(stack, offset);
				break;
			}
			case OP_INIT_DUPLIS_PTR: {
				StackIndex offset = fn->read_stack_index(&instr);
				int use_count = fn->read_int(&instr);
				eval_op_init_duplis_ptr(stack, offset, use_count);
				break;
			}
			case OP_RELEASE_DUPLIS_PTR: {
				StackIndex offset = fn->read_stack_index(&instr);
				eval_op_release_duplis_ptr(stack, offset);
				break;
			}
			case OP_ADD_FLOAT: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_add_float(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_SUB_FLOAT: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_sub_float(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_MUL_FLOAT: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_mul_float(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_DIV_FLOAT: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_div_float(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_SINE: {
				StackIndex offset = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_sine(stack, offset, offset_r);
				break;
			}
			case OP_COSINE: {
				StackIndex offset = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_cosine(stack, offset, offset_r);
				break;
			}
			case OP_TANGENT: {
				StackIndex offset = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_tangent(stack, offset, offset_r);
				break;
			}
			case OP_ARCSINE: {
				StackIndex offset = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_arcsine(stack, offset, offset_r);
				break;
			}
			case OP_ARCCOSINE: {
				StackIndex offset = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_arccosine(stack, offset, offset_r);
				break;
			}
			case OP_ARCTANGENT: {
				StackIndex offset = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_arctangent(stack, offset, offset_r);
				break;
			}
			case OP_POWER: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_power(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_LOGARITHM: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_logarithm(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_MINIMUM: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_minimum(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_MAXIMUM: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_maximum(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_ROUND: {
				StackIndex offset = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_round(stack, offset, offset_r);
				break;
			}
			case OP_LESS_THAN: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_less_than(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_GREATER_THAN: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_greater_than(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_MODULO: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_modulo(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_ABSOLUTE: {
				StackIndex offset = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_absolute(stack, offset, offset_r);
				break;
			}
			case OP_CLAMP_ONE: {
				StackIndex offset = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_clamp(stack, offset, offset_r);
				break;
			}
			case OP_SQRT: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_sqrt_float(stack, offset_a, offset_r);
				break;
			}
			case OP_ADD_FLOAT3: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_add_float3(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_SUB_FLOAT3: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_sub_float3(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_MUL_FLOAT3: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_mul_float3(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_DIV_FLOAT3: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_div_float3(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_MUL_FLOAT3_FLOAT: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_mul_float3_float(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_DIV_FLOAT3_FLOAT: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_div_float3_float(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_AVERAGE_FLOAT3: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_average_float3(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_DOT_FLOAT3: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_dot_float3(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_CROSS_FLOAT3: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_cross_float3(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_NORMALIZE_FLOAT3: {
				StackIndex offset = fn->read_stack_index(&instr);
				StackIndex offset_vec = fn->read_stack_index(&instr);
				StackIndex offset_val = fn->read_stack_index(&instr);
				eval_op_normalize_float3(stack, offset, offset_vec, offset_val);
				break;
			}
			case OP_LENGTH_FLOAT3: {
				StackIndex offset = fn->read_stack_index(&instr);
				StackIndex offset_len = fn->read_stack_index(&instr);
				eval_op_length_float3(stack, offset, offset_len);
				break;
			}
			case OP_ADD_MATRIX44: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_add_matrix44(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_SUB_MATRIX44: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_sub_matrix44(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_MUL_MATRIX44: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_mul_matrix44(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_MUL_MATRIX44_FLOAT: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_mul_matrix44_float(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_DIV_MATRIX44_FLOAT: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_div_matrix44_float(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_NEGATE_MATRIX44: {
				StackIndex offset = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_negate_matrix44(stack, offset, offset_r);
				break;
			}
			case OP_TRANSPOSE_MATRIX44: {
				StackIndex offset = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_transpose_matrix44(stack, offset, offset_r);
				break;
			}
			case OP_INVERT_MATRIX44: {
				StackIndex offset = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_invert_matrix44(stack, offset, offset_r);
				break;
			}
			case OP_ADJOINT_MATRIX44: {
				StackIndex offset = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_adjoint_matrix44(stack, offset, offset_r);
				break;
			}
			case OP_DETERMINANT_MATRIX44: {
				StackIndex offset = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_determinant_matrix44(stack, offset, offset_r);
				break;
			}
			case OP_MUL_MATRIX44_FLOAT3: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_mul_matrix44_float3(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_MUL_MATRIX44_FLOAT4: {
				StackIndex offset_a = fn->read_stack_index(&instr);
				StackIndex offset_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_mul_matrix44_float4(stack, offset_a, offset_b, offset_r);
				break;
			}
			case OP_MATRIX44_TO_LOC: {
				StackIndex offset_mat = fn->read_stack_index(&instr);
				StackIndex offset_loc = fn->read_stack_index(&instr);
				eval_op_matrix44_to_loc(stack, offset_mat, offset_loc);
				break;
			}
			case OP_MATRIX44_TO_EULER: {
				int order = fn->read_int(&instr);
				StackIndex offset_mat = fn->read_stack_index(&instr);
				StackIndex offset_euler = fn->read_stack_index(&instr);
				eval_op_matrix44_to_euler(stack, order, offset_mat, offset_euler);
				break;
			}
			case OP_MATRIX44_TO_AXISANGLE: {
				StackIndex offset_mat = fn->read_stack_index(&instr);
				StackIndex offset_axis = fn->read_stack_index(&instr);
				StackIndex offset_angle = fn->read_stack_index(&instr);
				eval_op_matrix44_to_axisangle(stack, offset_mat, offset_axis, offset_angle);
				break;
			}
			case OP_MATRIX44_TO_SCALE: {
				StackIndex offset_mat = fn->read_stack_index(&instr);
				StackIndex offset_scale = fn->read_stack_index(&instr);
				eval_op_matrix44_to_scale(stack, offset_mat, offset_scale);
				break;
			}
			case OP_LOC_TO_MATRIX44: {
				StackIndex offset_loc = fn->read_stack_index(&instr);
				StackIndex offset_mat = fn->read_stack_index(&instr);
				eval_op_loc_to_matrix44(stack, offset_loc, offset_mat);
				break;
			}
			case OP_EULER_TO_MATRIX44: {
				int order = fn->read_int(&instr);
				StackIndex offset_euler = fn->read_stack_index(&instr);
				StackIndex offset_mat = fn->read_stack_index(&instr);
				eval_op_euler_to_matrix44(stack, order, offset_euler, offset_mat);
				break;
			}
			case OP_AXISANGLE_TO_MATRIX44: {
				StackIndex offset_axis = fn->read_stack_index(&instr);
				StackIndex offset_angle = fn->read_stack_index(&instr);
				StackIndex offset_mat = fn->read_stack_index(&instr);
				eval_op_axisangle_to_matrix44(stack, offset_axis, offset_angle, offset_mat);
				break;
			}
			case OP_SCALE_TO_MATRIX44: {
				StackIndex offset_scale = fn->read_stack_index(&instr);
				StackIndex offset_mat = fn->read_stack_index(&instr);
				eval_op_scale_to_matrix44(stack, offset_scale, offset_mat);
				break;
			}
			
			case OP_MIX_RGB: {
				int mode = fn->read_int(&instr);
				StackIndex offset_fac = fn->read_stack_index(&instr);
				StackIndex offset_col_a = fn->read_stack_index(&instr);
				StackIndex offset_col_b = fn->read_stack_index(&instr);
				StackIndex offset_r = fn->read_stack_index(&instr);
				eval_op_mix_rgb(stack, mode, offset_col_a, offset_col_b, offset_fac, offset_r);
				break;
			}
			
			case OP_INT_TO_RANDOM: {
				int seed = fn->read_int(&instr);
				StackIndex offset = fn->read_stack_index(&instr);
				StackIndex offset_irandom = fn->read_stack_index(&instr);
				StackIndex offset_frandom = fn->read_stack_index(&instr);
				eval_op_int_to_random(stack, (uint64_t)seed, offset, offset_irandom, offset_frandom);
				break;
			}
			case OP_FLOAT_TO_RANDOM: {
				int seed = fn->read_int(&instr);
				StackIndex offset = fn->read_stack_index(&instr);
				StackIndex offset_irandom = fn->read_stack_index(&instr);
				StackIndex offset_frandom = fn->read_stack_index(&instr);
				eval_op_float_to_random(stack, (uint64_t)seed, offset, offset_irandom, offset_frandom);
				break;
			}
			
			case OP_TEX_PROC_VORONOI: {
				int distance_metric = fn->read_int(&instr);
				int color_type = fn->read_int(&instr);
				StackIndex iMinkowskiExponent = fn->read_stack_index(&instr);
				StackIndex iScale = fn->read_stack_index(&instr);
				StackIndex iNoiseSize = fn->read_stack_index(&instr);
				StackIndex iNabla = fn->read_stack_index(&instr);
				StackIndex iW1 = fn->read_stack_index(&instr);
				StackIndex iW2 = fn->read_stack_index(&instr);
				StackIndex iW3 = fn->read_stack_index(&instr);
				StackIndex iW4 = fn->read_stack_index(&instr);
				StackIndex iPos = fn->read_stack_index(&instr);
				StackIndex oIntensity = fn->read_stack_index(&instr);
				StackIndex oColor = fn->read_stack_index(&instr);
				StackIndex oNormal = fn->read_stack_index(&instr);
				eval_op_tex_proc_voronoi(stack, distance_metric, color_type,
				                         iMinkowskiExponent, iScale, iNoiseSize, iNabla,
				                         iW1, iW2, iW3, iW4, iPos,
				                         oIntensity, oColor, oNormal);
				break;
			}
			case OP_TEX_PROC_CLOUDS: {
				StackIndex iPos = fn->read_stack_index(&instr);
				StackIndex iNabla = fn->read_stack_index(&instr);
				StackIndex iSize = fn->read_stack_index(&instr);
				int iDepth = fn->read_int(&instr);
				int iNoiseBasis = fn->read_int(&instr);
				int iNoiseHard = fn->read_int(&instr);
				StackIndex oIntensity = fn->read_stack_index(&instr);
				StackIndex oColor = fn->read_stack_index(&instr);
				StackIndex oNormal = fn->read_stack_index(&instr);
				eval_op_tex_proc_clouds(stack, iPos, iNabla, iSize,
				                        iDepth, iNoiseBasis, iNoiseHard,
				                        oIntensity, oColor, oNormal);
				break;
			}
			case OP_TEX_PROC_WOOD: {
				StackIndex iPos = fn->read_stack_index(&instr);
				StackIndex iNabla = fn->read_stack_index(&instr);
				StackIndex iSize = fn->read_stack_index(&instr);
				StackIndex iTurb = fn->read_stack_index(&instr);
				int iNoiseBasis = fn->read_int(&instr);
				int iNoiseBasis2 = fn->read_int(&instr);
				int iNoiseHard = fn->read_int(&instr);
				int iWoodType = fn->read_int(&instr);
				StackIndex oIntensity = fn->read_stack_index(&instr);
				StackIndex oNormal = fn->read_stack_index(&instr);
				eval_op_tex_proc_wood(stack, iPos, iNabla, iSize, iTurb,
				                      iNoiseBasis, iNoiseBasis2, iNoiseHard, iWoodType,
				                      oIntensity, oNormal);
				break;
			}
			case OP_TEX_PROC_MUSGRAVE: {
				StackIndex iPos = fn->read_stack_index(&instr);
				StackIndex iNabla = fn->read_stack_index(&instr);
				StackIndex iSize = fn->read_stack_index(&instr);
				StackIndex iDim = fn->read_stack_index(&instr);
				StackIndex iLac = fn->read_stack_index(&instr);
				StackIndex iOct = fn->read_stack_index(&instr);
				StackIndex iInt = fn->read_stack_index(&instr);
				StackIndex iOff = fn->read_stack_index(&instr);
				StackIndex iGain = fn->read_stack_index(&instr);
				int iNoiseBasis = fn->read_int(&instr);
				int iNoiseType = fn->read_int(&instr);
				StackIndex oIntensity = fn->read_stack_index(&instr);
				StackIndex oNormal = fn->read_stack_index(&instr);
				eval_op_tex_proc_musgrave(stack, iPos, iNabla, iSize, iDim,
				                          iLac, iOct, iInt, iOff, iGain,
				                          iNoiseBasis, iNoiseType,
				                          oIntensity, oNormal);
				break;
			}
			case OP_TEX_PROC_MAGIC: {
				StackIndex iPos = fn->read_stack_index(&instr);
				StackIndex iTurb = fn->read_stack_index(&instr);
				int iDepth = fn->read_int(&instr);
				StackIndex oIntensity = fn->read_stack_index(&instr);
				StackIndex oColor = fn->read_stack_index(&instr);
				StackIndex oNormal = fn->read_stack_index(&instr);
				eval_op_tex_proc_magic(stack, iPos, iTurb, iDepth, oIntensity,
				                       oColor, oNormal);
				break;
			}
			case OP_TEX_PROC_STUCCI: {
				StackIndex iPos = fn->read_stack_index(&instr);
				StackIndex iSize = fn->read_stack_index(&instr);
				StackIndex iTurb = fn->read_stack_index(&instr);
				int iBasis = fn->read_int(&instr);
				int iHard = fn->read_int(&instr);
				int iType = fn->read_int(&instr);
				StackIndex oIntensity = fn->read_stack_index(&instr);
				StackIndex oNormal = fn->read_stack_index(&instr);
				eval_op_tex_proc_stucci(stack, iPos, iSize, iTurb, iBasis, iHard,
				                        iType, oIntensity, oNormal);
				break;
			}
			case OP_TEX_PROC_MARBLE: {
				StackIndex iPos = fn->read_stack_index(&instr);
				StackIndex iSize = fn->read_stack_index(&instr);
				StackIndex iNabla = fn->read_stack_index(&instr);
				StackIndex iTurb = fn->read_stack_index(&instr);
				int iDepth = fn->read_int(&instr);
				int iNoiseBasis = fn->read_int(&instr);
				int iNoiseBasis2 = fn->read_int(&instr);
				int iNoiseHard = fn->read_int(&instr);
				int iMarbleType = fn->read_int(&instr);
				StackIndex oIntensity = fn->read_stack_index(&instr);
				StackIndex oNormal = fn->read_stack_index(&instr);
				eval_op_tex_proc_marble(stack, iPos, iNabla, iSize, iTurb,
				                        iDepth, iNoiseBasis, iNoiseBasis2,
				                        iNoiseHard, iMarbleType,
				                        oIntensity, oNormal);
				break;
			}
			case OP_TEX_PROC_DISTNOISE: {
				StackIndex iPos = fn->read_stack_index(&instr);
				StackIndex iSize = fn->read_stack_index(&instr);
				StackIndex iNabla = fn->read_stack_index(&instr);
				StackIndex iDist = fn->read_stack_index(&instr);
				int iNoiseBasis = fn->read_int(&instr);
				int iNoiseBasis2 = fn->read_int(&instr);
				StackIndex oIntensity = fn->read_stack_index(&instr);
				StackIndex oNormal = fn->read_stack_index(&instr);
				eval_op_tex_proc_distnoise(stack, iPos, iNabla, iSize, iDist,
				                           iNoiseBasis, iNoiseBasis2,
				                           oIntensity, oNormal);
				break;
			}
			
			case OP_OBJECT_LOOKUP: {
				int key = fn->read_int(&instr);
				StackIndex offset_object = fn->read_stack_index(&instr);
				eval_op_object_lookup(globals, stack, key, offset_object);
				break;
			}
			case OP_OBJECT_TRANSFORM: {
				StackIndex offset_object = fn->read_stack_index(&instr);
				StackIndex offset_transform = fn->read_stack_index(&instr);
				eval_op_object_transform(stack, offset_object, offset_transform);
				break;
			}
			case OP_OBJECT_FINAL_MESH: {
				StackIndex offset_object = fn->read_stack_index(&instr);
				StackIndex offset_mesh = fn->read_stack_index(&instr);
				eval_op_object_final_mesh(stack, offset_object, offset_mesh);
				break;
			}
			
			case OP_EFFECTOR_TRANSFORM: {
				int object_index = fn->read_int(&instr);
				StackIndex offset_tfm = fn->read_stack_index(&instr);
				eval_op_effector_transform(globals, stack, object_index, offset_tfm);
				break;
			}
			case OP_EFFECTOR_CLOSEST_POINT: {
				StackIndex offset_object = fn->read_stack_index(&instr);
				StackIndex offset_vector = fn->read_stack_index(&instr);
				StackIndex offset_position = fn->read_stack_index(&instr);
				StackIndex offset_normal = fn->read_stack_index(&instr);
				StackIndex offset_tangent = fn->read_stack_index(&instr);
				eval_op_effector_closest_point(stack, offset_object, offset_vector,
				                               offset_position, offset_normal, offset_tangent);
				break;
			}
			case OP_MESH_LOAD: {
				StackIndex offset_base_mesh = fn->read_stack_index(&instr);
				StackIndex offset_mesh = fn->read_stack_index(&instr);
				eval_op_mesh_load(stack, offset_base_mesh, offset_mesh);
				break;
			}
			case OP_MESH_COMBINE: {
				StackIndex offset_mesh_a = fn->read_stack_index(&instr);
				StackIndex offset_mesh_b = fn->read_stack_index(&instr);
				StackIndex offset_mesh_out = fn->read_stack_index(&instr);
				eval_op_mesh_combine(&kd, stack, offset_mesh_a, offset_mesh_b, offset_mesh_out);
				break;
			}
			case OP_MESH_ARRAY: {
				fn->read_jump_address(&instr);
				StackIndex offset_mesh_in = fn->read_stack_index(&instr);
				fn->read_jump_address(&instr);
				StackIndex offset_count = fn->read_stack_index(&instr);
				int adr_transform = fn->read_jump_address(&instr);
				StackIndex offset_transform = fn->read_stack_index(&instr);
				StackIndex offset_mesh_out = fn->read_stack_index(&instr);
				StackIndex offset_index = fn->read_stack_index(&instr);
				eval_op_mesh_array(globals, &kd, stack,
				                   offset_mesh_in, offset_mesh_out, offset_count,
				                   adr_transform, offset_transform, offset_index);
				break;
			}
			case OP_MESH_DISPLACE: {
				fn->read_jump_address(&instr);
				StackIndex offset_mesh_in = fn->read_stack_index(&instr);
				int fn_vector = fn->read_jump_address(&instr);
				StackIndex offset_vector = fn->read_stack_index(&instr);
				StackIndex offset_mesh_out = fn->read_stack_index(&instr);
				StackIndex offset_index = fn->read_stack_index(&instr);
				eval_op_mesh_displace(globals, &kd, stack,
				                      offset_mesh_in, offset_mesh_out, fn_vector, offset_vector,
				                      offset_index);
				break;
			}
			case OP_MESH_BOOLEAN: {
				StackIndex offset_mesh_in = fn->read_stack_index(&instr);
				StackIndex offset_object = fn->read_stack_index(&instr);
				StackIndex offset_transform = fn->read_stack_index(&instr);
				StackIndex offset_invtransform = fn->read_stack_index(&instr);
				StackIndex offset_operation = fn->read_stack_index(&instr);
				StackIndex offset_separate = fn->read_stack_index(&instr);
				StackIndex offset_dissolve = fn->read_stack_index(&instr);
				StackIndex offset_connect_regions = fn->read_stack_index(&instr);
				StackIndex offset_threshold = fn->read_stack_index(&instr);
				StackIndex offset_mesh_out = fn->read_stack_index(&instr);
				eval_op_mesh_boolean(globals, &kd, stack, offset_mesh_in,
				                     offset_object, offset_transform, offset_invtransform, offset_operation,
				                     offset_separate, offset_dissolve, offset_connect_regions,
				                     offset_threshold, offset_mesh_out);
				break;
			}
			case OP_MESH_CLOSEST_POINT: {
				StackIndex offset_mesh = fn->read_stack_index(&instr);
				StackIndex offset_transform = fn->read_stack_index(&instr);
				StackIndex offset_invtransform = fn->read_stack_index(&instr);
				StackIndex offset_vector = fn->read_stack_index(&instr);
				StackIndex offset_position = fn->read_stack_index(&instr);
				StackIndex offset_normal = fn->read_stack_index(&instr);
				StackIndex offset_tangent = fn->read_stack_index(&instr);
				eval_op_mesh_closest_point(stack, offset_mesh, offset_transform, offset_invtransform, offset_vector,
				                           offset_position, offset_normal, offset_tangent);
				break;
			}
			
			case OP_CURVE_PATH: {
				StackIndex offset_object = fn->read_stack_index(&instr);
				StackIndex offset_transform = fn->read_stack_index(&instr);
				StackIndex offset_invtransform = fn->read_stack_index(&instr);
				StackIndex offset_param = fn->read_stack_index(&instr);
				StackIndex offset_loc = fn->read_stack_index(&instr);
				StackIndex offset_dir = fn->read_stack_index(&instr);
				StackIndex offset_nor = fn->read_stack_index(&instr);
				StackIndex offset_rot = fn->read_stack_index(&instr);
				StackIndex offset_radius = fn->read_stack_index(&instr);
				StackIndex offset_weight = fn->read_stack_index(&instr);
				StackIndex offset_tilt = fn->read_stack_index(&instr);
				eval_op_curve_path(stack, offset_object, offset_transform, offset_invtransform,
				                   offset_param, offset_loc, offset_dir, offset_nor,
				                   offset_rot, offset_radius, offset_weight,
				                   offset_tilt);
				break;
			}
			
			case OP_IMAGE_SAMPLE: {
				StackIndex offset_image = fn->read_stack_index(&instr);
				StackIndex offset_uv = fn->read_stack_index(&instr);
				StackIndex offset_color = fn->read_stack_index(&instr);
				eval_op_image_sample(globals, stack, offset_image, offset_uv, offset_color);
				break;
			}
			
			case OP_MAKE_DUPLI: {
				StackIndex offset_object = fn->read_stack_index(&instr);
				StackIndex offset_transform = fn->read_stack_index(&instr);
				StackIndex offset_index = fn->read_stack_index(&instr);
				StackIndex offset_hide = fn->read_stack_index(&instr);
				StackIndex offset_recursive = fn->read_stack_index(&instr);
				StackIndex offset_dupli = fn->read_stack_index(&instr);
				eval_op_make_dupli(stack, offset_object, offset_transform, offset_index,
				                   offset_hide, offset_recursive, offset_dupli);
				break;
			}
			
			case OP_DUPLIS_COMBINE: {
				StackIndex offset_duplis_a = fn->read_stack_index(&instr);
				StackIndex offset_duplis_b = fn->read_stack_index(&instr);
				StackIndex offset_result = fn->read_stack_index(&instr);
				eval_op_duplis_combine(stack, offset_duplis_a, offset_duplis_b, offset_result);
				break;
			}
			
			case OP_END:
				return;
		}
	}
}

void EvalContext::eval_expression(const EvalGlobals *globals, const InstructionList *fn, int entry_point, EvalStack *stack) const
{
	if (entry_point != BVM_JMP_INVALID)
		eval_instructions(globals, fn, entry_point, stack);
}

} /* namespace blenvm */

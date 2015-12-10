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

#ifndef __BVM_EVAL_CURVE_H__
#define __BVM_EVAL_CURVE_H__

/** \file bvm_eval_curve.h
 *  \ingroup bvm
 */

extern "C" {
#include "BLI_math.h"

#include "BKE_anim.h"
}

#include "bvm_eval_common.h"

#include "bvm_util_math.h"

namespace bvm {

static void eval_op_curve_path(float *stack, StackIndex offset_object, StackIndex offset_param,
                               StackIndex offset_loc, StackIndex offset_dir, StackIndex offset_nor,
                               StackIndex offset_rot, StackIndex offset_radius, StackIndex offset_weight)
{
	PointerRNA ptr = stack_load_pointer(stack, offset_object);
	float t = stack_load_float(stack, offset_param);
	
	float3 loc(0, 0, 0), dir(0, 0, 0), nor(0, 0, 0);
	matrix44 rot = matrix44::identity();
	float radius=0.0f, weight=0.0f;
	
	if (ptr.data && RNA_struct_is_a(&RNA_Object, ptr.type)) {
		Object *ob = (Object *)ptr.data;
		float qt[4];
		
		/* XXX normal (curvature) is not yet defined! */
		where_on_path(ob, t, loc.data(), dir.data(), qt, &radius, &weight);
		quat_to_mat4(rot.data, qt);
	}
	
	stack_store_float3(stack, offset_loc, loc);
	stack_store_float3(stack, offset_dir, dir);
	stack_store_float3(stack, offset_nor, nor);
	stack_store_matrix44(stack, offset_rot, rot);
	stack_store_float(stack, offset_radius, radius);
	stack_store_float(stack, offset_weight, weight);
}

} /* namespace bvm */

#endif /* __BVM_EVAL_CURVE_H__ */

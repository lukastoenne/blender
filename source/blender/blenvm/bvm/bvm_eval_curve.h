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

#include "util_math.h"

namespace blenvm {

static void eval_op_curve_path(EvalStack *stack,
                               StackIndex offset_object,
                               StackIndex offset_transform,
                               StackIndex offset_invtransform,
                               StackIndex offset_param,
                               StackIndex offset_loc,
                               StackIndex offset_dir,
                               StackIndex offset_nor,
                               StackIndex offset_rot,
                               StackIndex offset_radius,
                               StackIndex offset_weight,
                               StackIndex offset_tilt)
{
	PointerRNA ptr = stack_load_rnapointer(stack, offset_object);
	float t = stack_load_float(stack, offset_param);
	
	/* where_on_path is touchy about 0 > t > 1 */
	CLAMP(t, 0.0f, 1.0f);
	
	float3 loc(0, 0, 0), dir(0, 0, 0), nor(0, 0, 0);
	matrix44 rot = matrix44::identity();
	float radius=0.0f, weight=0.0f, tilt=0.0f;
	
	if (ptr.data && RNA_struct_is_a(&RNA_Object, ptr.type)) {
		Object *ob = (Object *)ptr.data;
		matrix44 omat = stack_load_matrix44(stack, offset_transform);
		matrix44 imat = stack_load_matrix44(stack, offset_invtransform);
		
		/* XXX normal (curvature) is not yet defined! */
		/* XXX where_on_path expects a vec[4], and uses the last
		 * element for storing tilt ...
		 */
		float vec[4], qt[4], qtm[3][3];
		where_on_path(ob, t, vec, dir.data(), qt, &radius, &weight);
		
		mul_v3_m4v3(loc.data(), omat.data, vec);
		mul_mat3_m4_v3(omat.data, dir.data());
		mul_mat3_m4_v3(omat.data, nor.data());
		quat_to_mat3(qtm, qt);
		mul_m4_m3m4(rot.data, qtm, imat.data);
		mul_m4_m4m4(rot.data, omat.data, rot.data);
		tilt = vec[3];
	}
	
	stack_store_float3(stack, offset_loc, loc);
	stack_store_float3(stack, offset_dir, dir);
	stack_store_float3(stack, offset_nor, nor);
	stack_store_matrix44(stack, offset_rot, rot);
	stack_store_float(stack, offset_radius, radius);
	stack_store_float(stack, offset_weight, weight);
	stack_store_float(stack, offset_tilt, tilt);
}

} /* namespace blenvm */

#endif /* __BVM_EVAL_CURVE_H__ */

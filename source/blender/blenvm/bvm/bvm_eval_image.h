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

#ifndef __BVM_EVAL_IMAGE_H__
#define __BVM_EVAL_IMAGE_H__

/** \file bvm_eval_image.h
 *  \ingroup bvm
 */

extern "C" {
#include "BLI_math_color.h"

#include "DNA_image_types.h"

#include "BKE_image.h"
#include "BKE_scene.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_colormanagement.h"
}

#include "bvm_eval_common.h"

#include "util_math.h"

namespace blenvm {

/* nearest sampling mode */
inline static void imbuf_sample_nearest(struct ImBuf *ibuf, const float3 &uv, float4 &color)
{
	/* ImBuf in must have a valid rect or rect_float, assume this is already checked */
	
	int x1 = (int)uv.x;
	int y1 = (int)uv.y;
	
	/* sample area entirely outside image? */
	if (x1 < 0 || x1 > ibuf->x - 1 || y1 < 0 || y1 > ibuf->y - 1) {
		color.x = color.y = color.z = color.w = 0.0f;
		return;
	}
	
	if (ibuf->rect_float) {
		const float *data = ibuf->rect_float + ((size_t)ibuf->x * y1 + x1) * 4;
		copy_v4_v4(color.data(), data);
	}
	else {
		const unsigned char *data = (unsigned char *)ibuf->rect + ((size_t)ibuf->x * y1 + x1) * 4;
		rgba_uchar_to_float(color.data(), data);
	}
}

static void eval_op_image_sample(const EvalGlobals *globals,
                                 EvalStack *stack,
                                 StackIndex offset_image,
                                 StackIndex offset_uv,
                                 StackIndex offset_color)
{
	float4 color(0.0f, 0.0f, 0.0f, 0.0f);
	
	int ima_key = stack_load_int(stack, offset_image);
	
	/* TODO just a dummy ImageUser for now */
	ImageUser iuser = {0};
	iuser.ok = true;
	
	ImBuf *ibuf = globals->lookup_imbuf(ima_key, &iuser);
	if (ibuf) {
		float3 uv = stack_load_float3(stack, offset_uv);
		
		imbuf_sample_nearest(ibuf, uv, color);
	}
	
	stack_store_float4(stack, offset_color, color);
}

} /* namespace blenvm */

#endif /* __BVM_EVAL_IMAGE_H__ */

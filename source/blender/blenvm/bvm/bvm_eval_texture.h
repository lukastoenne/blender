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

#ifndef __BVM_EVAL_TEXTURE_H__
#define __BVM_EVAL_TEXTURE_H__

/** \file bvm_eval_texture.h
 *  \ingroup bvm
 */

extern "C" {
#include "BLI_noise.h"
}

#include "bvm_eval_common.h"

namespace bvm {

inline void eval_op_tex_proc_voronoi(float *stack, int distance_metric, int color_type,
                                     StackIndex iMinkowskiExponent, StackIndex iScale,
                                     StackIndex iNoiseSize, StackIndex iNabla,
                                     StackIndex iW1, StackIndex iW2, StackIndex iW3, StackIndex iW4,
                                     StackIndex iPos,
                                     StackIndex oIntensity, StackIndex oColor, StackIndex oNormal)
{
	float3 texvec = stack_load_float3(stack, iPos);
	float mexp = stack_load_float(stack, iMinkowskiExponent);
	float noisesize = stack_load_float(stack, iNoiseSize);
	float nabla = stack_load_float(stack, iNabla);
	
	float w1 = stack_load_float(stack, iW1);
	float w2 = stack_load_float(stack, iW2);
	float w3 = stack_load_float(stack, iW3);
	float w4 = stack_load_float(stack, iW4);
	float aw1 = fabsf(w1);
	float aw2 = fabsf(w2);
	float aw3 = fabsf(w3);
	float aw4 = fabsf(w4);
	float sc = (aw1 + aw2 + aw3 + aw4);
	if (sc != 0.0f)
		sc = stack_load_float(stack, iScale) / sc;
	
	float da[4], pa[12];	/* distance and point coordinate arrays of 4 nearest neighbors */

	voronoi(texvec.x, texvec.y, texvec.z, da, pa, mexp, distance_metric);
	
	float intensity = sc * fabsf(w1*da[0] + w2*da[1] + w3*da[2] + w4*da[3]);

	float4 color;
	if (color_type == 0) {
		color = float4(intensity, intensity, intensity, 1.0f);
	}
	else {
		float ca[3];	/* cell color */
		float r, g, b;
		cellNoiseV(pa[0], pa[1], pa[2], ca);
		r = aw1*ca[0];
		g = aw1*ca[1];
		b = aw1*ca[2];
		cellNoiseV(pa[3], pa[4], pa[5], ca);
		r += aw2*ca[0];
		g += aw2*ca[1];
		b += aw2*ca[2];
		cellNoiseV(pa[6], pa[7], pa[8], ca);
		r += aw3*ca[0];
		g += aw3*ca[1];
		b += aw3*ca[2];
		cellNoiseV(pa[9], pa[10], pa[11], ca);
		r += aw4*ca[0];
		g += aw4*ca[1];
		b += aw4*ca[2];
		
		if (color_type > 1) {
			float t1 = (da[1] - da[0]) * 10;
			if (t1 > 1.0f)
				t1 = 1.0f;
			if (color_type > 2)
				t1 *= intensity;
			else
				t1 *= sc;
			
			r *= t1;
			g *= t1;
			b *= t1;
		}
		else {
			r *= sc;
			g *= sc;
			b *= sc;
		}
		
		color = float4(r, g, b, 1.0f);
	}

	float offs = nabla / noisesize;	/* also scaling of texvec */
	/* calculate bumpnormal */
	float3 normal;
	voronoi(texvec.x + offs, texvec.y, texvec.z, da, pa, mexp,  distance_metric);
	normal.x = sc * fabsf(w1*da[0] + w2*da[1] + w3*da[2] + w4*da[3]);
	voronoi(texvec.x, texvec.y + offs, texvec.z, da, pa, mexp,  distance_metric);
	normal.y = sc * fabsf(w1*da[0] + w2*da[1] + w3*da[2] + w4*da[3]);
	voronoi(texvec.x, texvec.y, texvec.z + offs, da, pa, mexp,  distance_metric);
	normal.z = sc * fabsf(w1*da[0] + w2*da[1] + w3*da[2] + w4*da[3]);
	
	stack_store_float(stack, oIntensity, intensity);
	stack_store_float4(stack, oColor, color);
	stack_store_float3(stack, oNormal, normal);
}

} /* namespace bvm */

#endif /* __BVM_EVAL_TEXTURE_H__ */

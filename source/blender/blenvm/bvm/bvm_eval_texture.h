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

namespace blenvm {

inline void eval_op_tex_proc_voronoi(EvalStack *stack, int distance_metric, int color_type,
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
	float3 normal;
	
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

	/* calculate bumpnormal */
	{
		float offs = nabla / noisesize;	/* also scaling of texvec */
		voronoi(texvec.x + offs, texvec.y, texvec.z, da, pa, mexp,  distance_metric);
		normal.x = sc * fabsf(w1*da[0] + w2*da[1] + w3*da[2] + w4*da[3]);
		voronoi(texvec.x, texvec.y + offs, texvec.z, da, pa, mexp,  distance_metric);
		normal.y = sc * fabsf(w1*da[0] + w2*da[1] + w3*da[2] + w4*da[3]);
		voronoi(texvec.x, texvec.y, texvec.z + offs, da, pa, mexp,  distance_metric);
		normal.z = sc * fabsf(w1*da[0] + w2*da[1] + w3*da[2] + w4*da[3]);
	}
	
	stack_store_float(stack, oIntensity, intensity);
	stack_store_float4(stack, oColor, color);
	stack_store_float3(stack, oNormal, normal);
}

inline void eval_op_tex_proc_clouds(EvalStack *stack,
                                    StackIndex iPos, StackIndex iNabla, StackIndex iSize,
                                    int depth, int noise_basis, int noise_hard,
                                    StackIndex oIntensity, StackIndex oColor, StackIndex oNormal)
{
	float3 texvec = stack_load_float3(stack, iPos);
	float size = stack_load_float(stack, iSize);
	float nabla = stack_load_float(stack, iNabla);
	
	float intensity = BLI_gTurbulence(size, texvec.x, texvec.y, texvec.z, depth, noise_hard, noise_basis);
	float4 color;
	float3 normal;

	/* calculate bumpnormal */
	{
		float x = BLI_gTurbulence(size, texvec.x + nabla, texvec.y, texvec.z, depth, noise_hard, noise_basis);
		float y = BLI_gTurbulence(size, texvec.x, texvec.y + nabla, texvec.z, depth, noise_hard, noise_basis);
		float z = BLI_gTurbulence(size, texvec.x, texvec.y, texvec.z + nabla, depth, noise_hard, noise_basis);
		normal = float3(x, y, z);
	}

	{
		/* in this case, int. value should really be computed from color,
		 * and bumpnormal from that, would be too slow, looks ok as is */
		float r = intensity;
		float g = BLI_gTurbulence(size, texvec.y, texvec.x, texvec.z, depth, noise_hard, noise_basis);
		float b = BLI_gTurbulence(size, texvec.y, texvec.z, texvec.x, depth, noise_hard, noise_basis);
		color = float4(r, g, b, 1.0f);
	}
	
	stack_store_float(stack, oIntensity, intensity);
	stack_store_float4(stack, oColor, color);
	stack_store_float3(stack, oNormal, normal);
}

/* creates a sine wave */
static float tex_sin(float a)
{
	a = 0.5f + 0.5f * sinf(a);

	return a;
}

/* creates a saw wave */
static float tex_saw(float a)
{
	const float b = 2*M_PI;

	int n = (int)(a / b);
	a -= n*b;
	if (a < 0) a += b;
	return a / b;
}

/* creates a triangle wave */
static float tex_tri(float a)
{
	const float b = 2*M_PI;
	const float rmax = 1.0;

	a = rmax - 2.0f*fabsf(floorf((a*(1.0f/b))+0.5f) - (a*(1.0f/b)));

	return a;
}

/* computes basic wood intensity value at x,y,z */
static float wood_int(float size, float x, float y, float z, float turb,
                      int noise_basis, int noise_basis_2, int noise_hard,
                      int wood_type)
{
	float wi = 0;

	float (*waveform[3])(float);	/* create array of pointers to waveform functions */
	waveform[0] = tex_sin;			/* assign address of tex_sin() function to pointer array */
	waveform[1] = tex_saw;
	waveform[2] = tex_tri;

	 /* check to be sure noise_basis_2 is initialized ahead of time */
	if ((noise_basis_2 > 2) || (noise_basis_2 < 0)) noise_basis_2 = 0;

	switch (wood_type) {
		case 0: // TEX_BAND
			wi = waveform[noise_basis_2]((x + y + z)*10.0f);
			break;
		case 1: // TEX_RING
			wi = waveform[noise_basis_2](sqrtf(x*x + y*y + z*z)*20.0f);
			break;
		case 2: // TEX_BANDNOISE
			wi = turb * BLI_gNoise(size, x, y, z, noise_hard, noise_basis);
			wi = waveform[noise_basis_2]((x + y + z)*10.0f + wi);
			break;
		case 3: // TEX_RINGNOISE
			wi = turb * BLI_gNoise(size, x, y, z, noise_hard, noise_basis);
			wi = waveform[noise_basis_2](sqrtf(x*x + y*y + z*z)*20.0f + wi);
			break;
	}

	return wi;
}

inline void eval_op_tex_proc_wood(EvalStack *stack,
                                  StackIndex iPos, StackIndex iNabla, StackIndex iSize, StackIndex iTurb,
                                  int noise_basis, int noise_basis_2, int noise_hard, int wood_type,
                                  StackIndex oIntensity, StackIndex oNormal)
{
	float3 texvec = stack_load_float3(stack, iPos);
	float size = stack_load_float(stack, iSize);
	float nabla = stack_load_float(stack, iNabla);
	float turb = stack_load_float(stack, iTurb);

	float intensity = wood_int(size, texvec.x, texvec.y, texvec.z, turb, noise_basis, noise_basis_2, noise_hard, wood_type);
	float3 normal;

	/* calculate bumpnormal */
	{
		float x = wood_int(size, texvec.x + nabla, texvec.y, texvec.z, turb, noise_basis, noise_basis_2, noise_hard, wood_type);
		float y = wood_int(size, texvec.x, texvec.y + nabla, texvec.z, turb, noise_basis, noise_basis_2, noise_hard, wood_type);
		float z = wood_int(size, texvec.x, texvec.y, texvec.z + nabla, turb, noise_basis, noise_basis_2, noise_hard, wood_type);
		normal = float3(x, y, z);
	}

	stack_store_float(stack, oIntensity, intensity);
	stack_store_float3(stack, oNormal, normal);
}

/* computes basic marble intensity at x,y,z */
static float marble_int(float size, float x, float y, float z, float turb,
                        int depth, int noise_basis, int noise_basis_2,
                        int noise_hard, int marble_type)
{
	float (*waveform[3])(float);	/* create array of pointers to waveform functions */
	waveform[0] = tex_sin;			/* assign address of tex_sin() function to pointer array */
	waveform[1] = tex_saw;
	waveform[2] = tex_tri;

	/* check to be sure noise_basis_2 isn't initialized ahead of time */
	if ((noise_basis_2 > 2) || (noise_basis_2 < 0))
		noise_basis_2 = 0;

	float n = 5.0f * (x + y + z);
	float intensity = n + turb * BLI_gTurbulence(size, x, y, z, depth, noise_hard,  noise_basis);

	if (marble_type >=0 ) {  /* TEX_SOFT always true */
		intensity = waveform[noise_basis_2](intensity);
		if (marble_type == 1) {
			intensity = sqrtf(intensity);
		}
		else if (marble_type == 2) {
			intensity = sqrtf(sqrtf(intensity));
		}
	}

	return intensity;
}

inline void eval_op_tex_proc_marble(EvalStack *stack,
                                  StackIndex iPos, StackIndex iNabla, StackIndex iSize, StackIndex iTurb,
                                  int depth, int noise_basis, int noise_basis_2,
                                  int noise_hard, int marble_type,
                                  StackIndex oIntensity, StackIndex oNormal)
{
	float3 texvec = stack_load_float3(stack, iPos);
	float size = stack_load_float(stack, iSize);
	float nabla = stack_load_float(stack, iNabla);
	float turb = stack_load_float(stack, iTurb);

	float intensity = marble_int(size, texvec.x, texvec.y, texvec.z, turb, depth, noise_basis, noise_basis_2, noise_hard, marble_type);
	float3 normal;

	/* calculate bumpnormal */
	{
		float x = marble_int(size, texvec.x + nabla, texvec.y, texvec.z, turb, depth, noise_basis, noise_basis_2, noise_hard, marble_type);
		float y = marble_int(size, texvec.x, texvec.y + nabla, texvec.z, turb, depth, noise_basis, noise_basis_2, noise_hard, marble_type);
		float z = marble_int(size, texvec.x, texvec.y, texvec.z + nabla, turb, depth, noise_basis, noise_basis_2, noise_hard, marble_type);
		normal = float3(x, y, z);
	}

	stack_store_float(stack, oIntensity, intensity);
	stack_store_float3(stack, oNormal, normal);
}

inline void eval_op_tex_proc_musgrave(EvalStack *stack,
                                      StackIndex iPos, StackIndex iNabla, StackIndex iSize,
                                      StackIndex iDim, StackIndex iLac, StackIndex iOct,
                                      StackIndex iInt, StackIndex iOffset, StackIndex iGain,
                                      int noise_basis, int noise_type,
			                          StackIndex oIntensity, StackIndex oNormal)
{
	float3 texvec = stack_load_float3(stack, iPos);
	float size = stack_load_float(stack, iSize);
	float nabla = stack_load_float(stack, iNabla);
	float dimension = stack_load_float(stack, iDim);
	float lacunarity = stack_load_float(stack, iLac);
	float octaves = stack_load_float(stack, iOct);
	float nintensity = stack_load_float(stack, iInt);
	float offset = stack_load_float(stack, iOffset);
	float gain = stack_load_float(stack, iGain);

	float intensity, x, y, z;

	float offs = nabla / size;	/* also scaling of texvec */

	switch (noise_type) {
		case 0: /* TEX_MFRACTAL */
		case 3: /* TEX_FBM */ {
			float (*mgravefunc)(float, float, float, float, float, float, int);

			if (noise_type == 0)
				mgravefunc = mg_MultiFractal;
			else
				mgravefunc = mg_fBm;

			intensity = nintensity*mgravefunc(texvec.x, texvec.y, texvec.z, dimension, lacunarity, octaves, noise_basis);

			/* calculate bumpnormal */
			x = nintensity*mgravefunc(texvec.x + offs, texvec.y, texvec.z, dimension, lacunarity, octaves, noise_basis);
			y = nintensity*mgravefunc(texvec.x, texvec.y + offs, texvec.z, dimension, lacunarity, octaves, noise_basis);
			z = nintensity*mgravefunc(texvec.x, texvec.y, texvec.z + offs, dimension, lacunarity, octaves, noise_basis);

			break;
		}
		case 1: /* TEX_RIDGEDMF */
		case 2: /* TEX_HYBRIDMF */ {
			float (*mgravefunc)(float, float, float, float, float, float, float, float, int);

			if (noise_type == 2)
				mgravefunc = mg_RidgedMultiFractal;
			else
				mgravefunc = mg_HybridMultiFractal;

			intensity = nintensity*mgravefunc(texvec.x, texvec.y, texvec.z, dimension, lacunarity, octaves, offset, gain, noise_basis);

			/* calculate bumpnormal */
			x = nintensity*mgravefunc(texvec.x + offs, texvec.y, texvec.z, dimension, lacunarity, octaves, offset, gain, noise_basis);
			y = nintensity*mgravefunc(texvec.x, texvec.y + offs, texvec.z, dimension, lacunarity, octaves, offset, gain, noise_basis);
			z = nintensity*mgravefunc(texvec.x, texvec.y, texvec.z + offs, dimension, lacunarity, octaves, offset, gain, noise_basis);
			break;
		}
		case 4: /* TEX_HTERRAIN */ {
			intensity = nintensity*mg_HeteroTerrain(texvec.x, texvec.y, texvec.z, dimension, lacunarity, octaves, offset, noise_basis);

			/* calculate bumpnormal */
			x = nintensity*mg_HeteroTerrain(texvec.x + offs, texvec.y, texvec.z, dimension, lacunarity, octaves, offset, noise_basis);
			y = nintensity*mg_HeteroTerrain(texvec.x, texvec.y + offs, texvec.z, dimension, lacunarity, octaves, offset, noise_basis);
			z = nintensity*mg_HeteroTerrain(texvec.x, texvec.y, texvec.z + offs, dimension, lacunarity, octaves, offset, noise_basis);

			break;
		}
	}

	float3 normal = float3(x, y, z);

	stack_store_float(stack, oIntensity, intensity);
	stack_store_float3(stack, oNormal, normal);
}

inline void eval_op_tex_proc_magic(EvalStack *stack,
                                   StackIndex iPos, StackIndex iTurb,
                                   int depth,
                                   StackIndex oIntensity, StackIndex oColor, StackIndex oNormal)
{
	float3 texvec = stack_load_float3(stack, iPos);
	float turbulence = stack_load_float(stack, iTurb);

	float turb = turbulence / 5.0f;

	float x =  sinf(( texvec[0] + texvec[1] + texvec[2]) * 5.0f);
	float y =  cosf((-texvec[0] + texvec[1] - texvec[2]) * 5.0f);
	float z = -cosf((-texvec[0] - texvec[1] + texvec[2]) * 5.0f);
	if (depth>0) {
		x *= turb;
		y *= turb;
		z *= turb;
		y = -cosf(x-y+z);
		y *= turb;

		if (depth>1) {
			x= cosf(x-y-z);
			x*= turb;
			if (depth>2) {
				z= sinf(-x-y-z);
				z*= turb;
				if (depth>3) {
					x= -cosf(-x+y-z);
					x*= turb;
					if (depth>4) {
						y= -sinf(-x+y+z);
						y*= turb;
						if (depth>5) {
							y= -cosf(-x+y+z);
							y*= turb;
							if (depth>6) {
								x= cosf(x+y+z);
								x*= turb;
								if (depth>7) {
									z= sinf(x+y-z);
									z*= turb;
									if (depth>8) {
										x= -cosf(-x-y+z);
										x*= turb;
										if (depth>9) {
											y= -sinf(x-y+z);
											y*= turb;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (turb != 0.0f) {
		turb *= 2.0f;
		x /= turb;
		y /= turb;
		z /= turb;
	}

	float3 normal(x, y, z);
	float4 color(0.5f - x, 0.5f - y, 0.5f - z, 1.0f);
	float intensity = (1.0f / 3.0f) * (color.x + color.y + color.z);

	stack_store_float(stack, oIntensity, intensity);
	stack_store_float4(stack, oColor, color);
	stack_store_float3(stack, oNormal, normal);
}

inline void eval_op_tex_proc_stucci(EvalStack *stack,
                                   StackIndex iPos, StackIndex iSize, StackIndex iTurb,
                                   int noise_basis, int noisehard, int noise_type,
                                   StackIndex oIntensity, StackIndex oNormal)
{
	float3 texvec = stack_load_float3(stack, iPos);
	float noisesize = stack_load_float(stack, iSize);
	float turbulence = stack_load_float(stack, iTurb);

	float b2 = BLI_gNoise(noisesize, texvec[0], texvec[1], texvec[2], noisehard, noise_basis);
	float offset = turbulence / 200.0f;

	if (noise_type)
		offset *= (b2 * b2);

	float x = BLI_gNoise(noisesize, texvec[0] + offset, texvec[1], texvec[2], noisehard, noise_basis);
	float y = BLI_gNoise(noisesize, texvec[0], texvec[1] + offset, texvec[2], noisehard, noise_basis);
	float z = BLI_gNoise(noisesize, texvec[0], texvec[1], texvec[2] + offset, noisehard, noise_basis);

	float intensity = z;

	if (noise_type == 2) { /* TEX_WALLOUT */
		x = -x;
		y = -y;
		z = -z;
		intensity = 1.0f - intensity;
	}

	if (intensity < 0.0f)
		intensity = 0.0f;

	float3 normal(x, y, z);

	stack_store_float(stack, oIntensity, intensity);
	stack_store_float3(stack, oNormal, normal);
}

inline void eval_op_tex_proc_distnoise(EvalStack *stack,
                                       StackIndex iPos, StackIndex iSize,
                                       StackIndex iNabla, StackIndex iDist,
                                       int noise_basis, int noise_basis_2,
                                       StackIndex oIntensity, StackIndex oNormal)
{
	float3 texvec = stack_load_float3(stack, iPos);
	float noisesize = stack_load_float(stack, iSize);
	float nabla = stack_load_float(stack, iNabla);
	float dist_amount = stack_load_float(stack, iDist);

	float intensity = mg_VLNoise(texvec[0], texvec[1], texvec[2], dist_amount, noise_basis, noise_basis_2);

	/* calculate bumpnormal */
	float offs = nabla / noisesize;	/* also scaling of texvec */

	float x = mg_VLNoise(texvec[0] + offs, texvec[1], texvec[2], dist_amount, noise_basis, noise_basis_2);
	float y = mg_VLNoise(texvec[0], texvec[1] + offs, texvec[2], dist_amount, noise_basis, noise_basis_2);
	float z = mg_VLNoise(texvec[0], texvec[1], texvec[2] + offs, dist_amount, noise_basis, noise_basis_2);

	float3 normal(x, y, z);

	stack_store_float(stack, oIntensity, intensity);
	stack_store_float3(stack, oNormal, normal);
}

} /* namespace bvm */

#endif /* __BVM_EVAL_TEXTURE_H__ */

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

/*
Copyright (c) 2012 Sony Pictures Imageworks Inc., et al.
All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
* Neither the name of Sony Pictures Imageworks nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/** \file blender/blenlib/intern/noise_gabor.c
 *  \ingroup bli
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_sys_types.h"

#include "BLI_noise.h"

/* ------------------------------------------------------------------------- */
/* Local RNG definition, to avoid alloc from BLI_rand */

typedef struct RNG {
	uint64_t X;
} RNG;

#define MULTIPLIER  0x5DEECE66Dll
#define MASK        0x0000FFFFFFFFFFFFll

#define ADDEND      0xB
#define LOWSEED     0x330E

BLI_INLINE void rng_init(RNG *rng, unsigned int seed)
{
	rng->X = (((uint64_t) seed) << 16) | LOWSEED;
}

BLI_INLINE void rng_step(RNG *rng)
{
	rng->X = (MULTIPLIER * rng->X + ADDEND) & MASK;
}

BLI_INLINE int rng_get_int(RNG *rng)
{
	rng_step(rng);
	return (int) (rng->X >> 17);
}

BLI_INLINE unsigned int rng_get_uint(RNG *rng)
{
	rng_step(rng);
	return (unsigned int) (rng->X >> 17);
}

BLI_INLINE float rng_get_float(RNG *rng)
{
	return (float) rng_get_int(rng) / 0x80000000;
}

/* ------------------------------------------------------------------------- */

BLI_INLINE unsigned int cell_hash(const int index[3])
{
	unsigned int n = index[0] + index[1] * 1301 + index[2] * 314159;
	n ^= (n << 13);
	return (n * (n * n * 15731 + 789221) + 1376312589);
}

/* Poisson distribution generator according to Knuth */
BLI_INLINE unsigned int poisson_rng(RNG *rng, float lambda)
{
	const int maxiter = 100000;
	const float L = expf(-lambda);
	float p = 1.0f;
	for (unsigned int k = 0; k < maxiter; ++k) {
		p *= rng_get_float(rng);
		if (p <= L)
			return k;
	}
	return 0;
}

BLI_INLINE float gabor_kernel(
        const float v[3],
        float K, float a,
        const float omega0[3], float phi0)
{
	float envelope = K * expf(-M_PI * a*a * dot_v3v3(v, v));
	float harmonic = cos(2.0f*M_PI * dot_v3v3(v, omega0) + phi0);
	return envelope * harmonic;
}

static float accum_cell(const int index[3], const float offset[3],
                        float density, float width,
                        GaborNoiseSampler *sampler)
{
	unsigned int hash = cell_hash(index);
	/* XXX Need to use multiple RNG states to generate consistent samples with changing density.
	 * Could also integrate poisson rng into the sample loop to avoid using multiple rngs.
	 */
	RNG rng1, rng2;
	int num_pulses;
	
	rng_init(&rng1, hash);
	num_pulses = poisson_rng(&rng1, density);
	
	rng_init(&rng2, hash ^ 0xdeadbeef);
	float sum = 0.0f;
	for (int i = 0; i < num_pulses; ++i) {
		float co[3];
		co[0] = rng_get_float(&rng2);
		co[1] = rng_get_float(&rng2);
		co[2] = rng_get_float(&rng2);
		sub_v3_v3v3(co, offset, co);
		
		float weight = 2.0f * rng_get_float(&rng2) - 1.0f;
		
		float omega[3], phi;
		sampler->sample(sampler, rng2.X, omega, &phi);
		
		sum += gabor_kernel(co, weight, width, omega, phi);
	}
	
	return sum;
}

static const float gabor_frequency = 2.0f;
static const float gabor_cutoff = 0.05f;

static void gabor_params(float impulses, float bandwidth, float *r_density, float *r_width)
{
	float exp2_bandwidth = powf(2.0f, bandwidth);
	float a = gabor_frequency * ((exp2_bandwidth - 1.0) / (exp2_bandwidth + 1.0)) * sqrtf(M_PI / M_LN2);
	
	float r = sqrtf(-logf(gabor_cutoff) / M_PI) / a;
	float r2 = r * r;
	float r3 = r2 * r;
	float density = impulses / (4.0f/3.0f * M_PI * r3);
	
	*r_density = density;
	*r_width = a;
}

/**
 * @brief BLI_gabor_noise calculates noise using a sparse Gabor convolution.
 * @param size Scaling factor for the input coordinates
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @param impulses Number of expected impulses per convolution kernel area
 * @return Noise value in [0,1] interval
 */
float BLI_gabor_noise(float size, float x, float y, float z,
                      float impulses, float bandwidth,
                      GaborNoiseSampler *sampler)
{
	/* impulses (N) is expected number per kernel area, density (=N/pi) is number per cell */
	float density, width;
	gabor_params(impulses, bandwidth, &density, &width);
	
	float isize = (size != 0.0f ? 1.0f/size : 0.0f);
	
	float u[3];
	u[0] = x * isize;
	u[1] = y * isize;
	u[2] = z * isize;
	
	int idx_center[3];
	float ofs_center[3];
	idx_center[0] = (int)(floor(u[0]));
	idx_center[1] = (int)(floor(u[1]));
	idx_center[2] = (int)(floor(u[2]));
	ofs_center[0] = u[0] - floorf(u[0]) - 0.5f;
	ofs_center[1] = u[1] - floorf(u[1]) - 0.5f;
	ofs_center[2] = u[2] - floorf(u[2]) - 0.5f;
	
	float sum = 0.0f;
	for (int iz = -1; iz < +1; ++iz) {
		for (int iy = -1; iy < +1; ++iy) {
			for (int ix = -1; ix < +1; ++ix) {
				int idx[3];
				float ofs[3];
				idx[0] = idx_center[0] + ix;
				idx[1] = idx_center[1] + iy;
				idx[2] = idx_center[2] + iz;
				ofs[0] = ofs_center[0] - (float)ix;
				ofs[1] = ofs_center[1] - (float)iy;
				ofs[2] = ofs_center[2] - (float)iz;
				
				sum += accum_cell(idx, ofs, density, width, sampler);
			}
		}
	}
	
	return (sum + 1.0f) * 0.5f;
}

typedef struct IsotropicSampler {
	GaborNoiseSampler base;
	float frequency;
} IsotropicSampler;

static void isotropic_sample(GaborNoiseSampler *_sampler, uint64_t _rng,
                             float *r_omega, float *r_phi)
{
	IsotropicSampler *sampler = (IsotropicSampler *)_sampler;
	
	RNG rng = { _rng };
	
	float cos_p = 2.0f * rng_get_float(&rng) - 1.0f;
	float sin_p = sqrtf(1.0f - cos_p * cos_p);
	
	float t = 2.0f*M_PI * rng_get_float(&rng);
	float sin_t = sinf(t);
	float cos_t = cosf(t);
	
	r_omega[0] = cos_t * sin_p;
	r_omega[1] = sin_t * sin_p;
	r_omega[2] = cos_p;
	mul_v3_fl(r_omega, sampler->frequency);
	
	*r_phi = 2.0f*M_PI * rng_get_float(&rng);
}

void BLI_gabor_noise_sampler_free(GaborNoiseSampler *sampler)
{
	if (sampler->free)
		sampler->free(sampler);
	else
		MEM_freeN(sampler);
}

GaborNoiseSampler *BLI_gabor_noise_sampler_isotropic(float frequency)
{
	IsotropicSampler *sampler = MEM_mallocN(sizeof(IsotropicSampler), "isotropic gabor noise sampler");
	sampler->base.sample = isotropic_sample;
	sampler->base.free = NULL;
	sampler->frequency = frequency;
	return &sampler->base;
}

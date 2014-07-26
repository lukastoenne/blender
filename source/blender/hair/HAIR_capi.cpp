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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

extern "C" {
#include "DNA_hair_types.h"
}

#include "HAIR_capi.h"

#include "HAIR_smoothing.h"
#include "HAIR_types.h"

using namespace HAIR_NAMESPACE;

struct SmoothingIteratorFloat3 *HAIR_smoothing_iter_new(struct HairCurve *curve, float rest_length, float amount)
{
	SmoothingIterator<float3> *iter = new SmoothingIterator<float3>(rest_length, amount);
	
	if (curve->totpoints >= 2) {
		float *co0 = curve->points[0].co;
		float *co1 = curve->points[1].co;
		iter->begin(float3(co0[0], co0[1], co0[2]), float3(co1[0], co1[1], co1[2]));
	}
	else {
		iter->num = 1; /* XXX not nice, find a better way to indicate invalid iterator */
	}
	
	return (struct SmoothingIteratorFloat3 *)iter;
}

void HAIR_smoothing_iter_free(struct SmoothingIteratorFloat3 *citer)
{
	SmoothingIterator<float3> *iter = (SmoothingIterator<float3> *)citer;
	
	delete iter;
}

bool HAIR_smoothing_iter_valid(struct HairCurve *curve, struct SmoothingIteratorFloat3 *citer)
{
	SmoothingIterator<float3> *iter = (SmoothingIterator<float3> *)citer;
	
	return iter->num < curve->totpoints - 1;
}

void HAIR_smoothing_iter_next(struct HairCurve *curve, struct SmoothingIteratorFloat3 *citer, float cval[3])
{
	SmoothingIterator<float3> *iter = (SmoothingIterator<float3> *)citer;
	
	float *co = curve->points[iter->num + 1].co;
	float3 val = iter->next(float3(co[0], co[1], co[2]));
	cval[0] = val.x;
	cval[1] = val.y;
	cval[2] = val.z;
}

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

#ifndef __HAIR_MATH_H__
#define __HAIR_MATH_H__

#ifndef FREE_WINDOWS64
#define __forceinline inline __attribute__((always_inline))
#endif

#include <math.h>

#include "HAIR_types.h"

HAIR_NAMESPACE_BEGIN

/* utility functions */

__forceinline float min_ff(float a, float b)
{
	return a < b ? a : b;
}

__forceinline float max_ff(float a, float b)
{
	return a > b ? a : b;
}

/* standard arithmetic */

__forceinline float3 operator + (const float3 &a, const float3 &b)
{
	return float3(a.x + b.x, a.y + b.y, a.z + b.z);
}

__forceinline float3 operator - (const float3 &a, const float3 &b)
{
	return float3(a.x - b.x, a.y - b.y, a.z - b.z);
}

__forceinline float3 operator * (float fac, const float3 &a)
{
	return float3(fac * a.x, fac * a.y, fac * a.z);
}

__forceinline float3 operator * (const float3 &a, float fac)
{
	return float3(a.x * fac, a.y * fac, a.z * fac);
}

__forceinline float3 operator / (const float3 &a, float d)
{
	return float3(a.x / d, a.y / d, a.z / d);
}

/* vector functions */

__forceinline float dot_v3_v3(const float3 &a, const float3 &b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

__forceinline float len_v3(const float3 &v)
{
	return sqrtf(dot_v3_v3(v, v));
}

__forceinline float normalize_v3_v3(float3 &r, const float3 &v)
{
	float len = len_v3(v);
	r = len > 0.0f ? v / len : float3(0.0f, 0.0f, 0.0f);
	return len;
}

HAIR_NAMESPACE_END

#endif

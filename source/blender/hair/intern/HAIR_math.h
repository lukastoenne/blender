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

/* vector arithmetic */

__forceinline float2 operator + (const float2 &a, const float2 &b)
{
	return float2(a.x + b.x, a.y + b.y);
}

__forceinline float2 operator - (const float2 &a, const float2 &b)
{
	return float2(a.x - b.x, a.y - b.y);
}

__forceinline float2 operator - (const float2 &a)
{
	return float2(-a.x, -a.y);
}

__forceinline float2 operator * (float fac, const float2 &a)
{
	return float2(fac * a.x, fac * a.y);
}

__forceinline float2 operator * (const float2 &a, float fac)
{
	return float2(a.x * fac, a.y * fac);
}

__forceinline float2 operator / (const float2 &a, float d)
{
	return float2(a.x / d, a.y / d);
}


__forceinline float3 operator + (const float3 &a, const float3 &b)
{
	return float3(a.x + b.x, a.y + b.y, a.z + b.z);
}

__forceinline float3 operator - (const float3 &a, const float3 &b)
{
	return float3(a.x - b.x, a.y - b.y, a.z - b.z);
}

__forceinline float3 operator - (const float3 &a)
{
	return float3(-a.x, -a.y, -a.z);
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


__forceinline float4 operator + (const float4 &a, const float4 &b)
{
	return float4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

__forceinline float4 operator - (const float4 &a, const float4 &b)
{
	return float4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

__forceinline float4 operator * (float fac, const float4 &a)
{
	return float4(fac * a.x, fac * a.y, fac * a.z, fac * a.w);
}

__forceinline float4 operator * (const float4 &a, float fac)
{
	return float4(a.x * fac, a.y * fac, a.z * fac, a.w * fac);
}

__forceinline float4 operator / (const float4 &a, float d)
{
	return float4(a.x / d, a.y / d, a.z / d, a.w / d);
}

/* vector functions */

__forceinline float dot_v3v3(const float3 &a, const float3 &b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

__forceinline float dot_v4_v4(const float4 &a, const float4 &b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

__forceinline float3 cross_v3_v3(const float3 &a, const float3 &b)
{
	return float3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}

__forceinline float len_v3(const float3 &v)
{
	return sqrtf(dot_v3v3(v, v));
}

__forceinline float normalize_v3_v3(float3 &r, const float3 &v)
{
	float len = len_v3(v);
	r = len > 0.0f ? v / len : float3(0.0f, 0.0f, 0.0f);
	return len;
}

/**
 * slerp, treat vectors as spherical coordinates
 */
bool interp_v3v3_slerp(float3 &r, const float3 &a, const float3 &b, float t);

/* quaternion functions */

__forceinline float3 mul_qt_v3(const float4 &q, const float3 &v)
{
	float t0, t1, t2;
	float3 r;
	
	t0  = -q.x * v.x - q.y * v.y - q.z * v.z;
	t1  =  q.w * v.x + q.y * v.z - q.z * v.y;
	t2  =  q.w * v.y + q.z * v.x - q.x * v.z;
	r.z =  q.w * v.z + q.x * v.y - q.y * v.x;
	r.x =  t1;
	r.y =  t2;
	
	t1  = t0 * -q.x + r.x * q.w - r.y * q.z + r.z * q.y;
	t2  = t0 * -q.y + r.y * q.w - r.z * q.x + r.x * q.z;
	r.z = t0 * -q.z + r.z * q.w - r.x * q.y + r.y * q.x;
	r.x = t1;
	r.y = t2;
	
	return r;
}

__forceinline float4 mul_qt_v4(const float4 &q, const float4 &v)
{
	float t0, t1, t2;
	float4 r;
	
	t0  = -q.x * v.x - q.y * v.y - q.z * v.z;
	t1  =  q.w * v.x + q.y * v.z - q.z * v.y;
	t2  =  q.w * v.y + q.z * v.x - q.x * v.z;
	r.z =  q.w * v.z + q.x * v.y - q.y * v.x;
	r.x =  t1;
	r.y =  t2;
	
	t1  = t0 * -q.x + r.x * q.w - r.y * q.z + r.z * q.y;
	t2  = t0 * -q.y + r.y * q.w - r.z * q.x + r.x * q.z;
	r.z = t0 * -q.z + r.z * q.w - r.x * q.y + r.y * q.x;
	r.x = t1;
	r.y = t2;
	r.w = 1.0f;
	
	return r;
}

/* matrix arithmetic */

__forceinline Transform operator + (const Transform &a, const Transform &b)
{
	return Transform(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

__forceinline Transform operator - (const Transform &a, const Transform &b)
{
	return Transform(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

Transform transform_transpose(const Transform &tfm); /* forward declaration */

__forceinline Transform operator * (const Transform &a, const Transform &b)
{
	Transform c = transform_transpose(b);
	Transform t;

	t.x = float4(dot_v4_v4(a.x, c.x), dot_v4_v4(a.x, c.y), dot_v4_v4(a.x, c.z), dot_v4_v4(a.x, c.w));
	t.y = float4(dot_v4_v4(a.y, c.x), dot_v4_v4(a.y, c.y), dot_v4_v4(a.y, c.z), dot_v4_v4(a.y, c.w));
	t.z = float4(dot_v4_v4(a.z, c.x), dot_v4_v4(a.z, c.y), dot_v4_v4(a.z, c.z), dot_v4_v4(a.z, c.w));
	t.w = float4(dot_v4_v4(a.w, c.x), dot_v4_v4(a.w, c.y), dot_v4_v4(a.w, c.z), dot_v4_v4(a.w, c.w));

	return t;
}

__forceinline Transform operator * (const Transform &a, float fac)
{
	return Transform(a.x * fac, a.y * fac, a.z * fac, a.w * fac);
}

__forceinline Transform operator * (float fac, const Transform &a)
{
	return Transform(fac * a.x, fac * a.y, fac * a.z, fac * a.w);
}

__forceinline Transform operator / (const Transform &a, float d)
{
	return Transform(a.x / d, a.y / d, a.z / d, a.w / d);
}

/* matrix functions */

__forceinline float3 transform_perspective(const Transform &t, const float3 &a)
{
	float4 b = float4(a.x, a.y, a.z, 1.0f);
	float3 c = float3(dot_v4_v4(t.x, b), dot_v4_v4(t.y, b), dot_v4_v4(t.z, b));
	float w = dot_v4_v4(t.w, b);

	return (w != 0.0f)? c/w: float3(0.0f, 0.0f, 0.0f);
}

__forceinline float3 transform_point(const Transform &t, const float3 &a)
{
	float3 c = float3(
		a.x*t.x.x + a.y*t.x.y + a.z*t.x.z + t.x.w,
		a.x*t.y.x + a.y*t.y.y + a.z*t.y.z + t.y.w,
		a.x*t.z.x + a.y*t.z.y + a.z*t.z.z + t.z.w);

	return c;
}

__forceinline float3 transform_direction(const Transform &t, const float3 &a)
{
	float3 c = float3(
		a.x*t.x.x + a.y*t.x.y + a.z*t.x.z,
		a.x*t.y.x + a.y*t.y.y + a.z*t.y.z,
		a.x*t.z.x + a.y*t.z.y + a.z*t.z.z);

	return c;
}

__forceinline float3 transform_direction_transposed(const Transform &t, const float3 &a)
{
	float3 x = float3(t.x.x, t.y.x, t.z.x);
	float3 y = float3(t.x.y, t.y.y, t.z.y);
	float3 z = float3(t.x.z, t.y.z, t.z.z);

	return float3(dot_v3v3(x, a), dot_v3v3(y, a), dot_v3v3(z, a));
}

__forceinline Transform transform_transpose(const Transform &tfm)
{
	Transform t;

	t.x.x = tfm.x.x; t.x.y = tfm.y.x; t.x.z = tfm.z.x; t.x.w = tfm.w.x;
	t.y.x = tfm.x.y; t.y.y = tfm.y.y; t.y.z = tfm.z.y; t.y.w = tfm.w.y;
	t.z.x = tfm.x.z; t.z.y = tfm.y.z; t.z.z = tfm.z.z; t.z.w = tfm.w.z;
	t.w.x = tfm.x.w; t.w.y = tfm.y.w; t.w.z = tfm.z.w; t.w.w = tfm.w.w;

	return t;
}

Transform transform_inverse(const Transform& tfm);

HAIR_NAMESPACE_END

#endif

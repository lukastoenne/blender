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

#ifndef __HAIR_TYPES_H__
#define __HAIR_TYPES_H__

#ifndef FREE_WINDOWS64
#define __forceinline inline __attribute__((always_inline))
#endif

HAIR_NAMESPACE_BEGIN

struct float2 {
	float x, y;

	__forceinline float2(float x, float y) : x(x), y(y) {}

	__forceinline float operator[](int i) const { return *(&x + i); }
	__forceinline float& operator[](int i) { return *(&x + i); }
};

struct float3 {
#ifdef __HAIR_SSE__
	union {
		__m128 m128;
		struct { float x, y, z, w; };
	};

	__forceinline float3() {}
	__forceinline float3(const __m128 a) : m128(a) {}
	__forceinline operator const __m128&(void) const { return m128; }
	__forceinline operator __m128&(void) { return m128; }
#else
	float x, y, z, w;

	__forceinline float3() {}
	__forceinline float3(float x, float y, float z) : x(x), y(y), z(z) {}
#endif

	__forceinline float operator[](int i) const { return *(&x + i); }
	__forceinline float& operator[](int i) { return *(&x + i); }
};

struct float4 {
#ifdef __HAIR_SSE__
	union {
		__m128 m128;
		struct { float x, y, z, w; };
	};

	__forceinline float4() {}
	__forceinline float4(const __m128 a) : m128(a) {}
	__forceinline operator const __m128&(void) const { return m128; }
	__forceinline operator __m128&(void) { return m128; }
#else
	float x, y, z, w;
#endif

	__forceinline float4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

	__forceinline float operator[](int i) const { return *(&x + i); }
	__forceinline float& operator[](int i) { return *(&x + i); }
};

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
	return float3(fac * a.x, fac * a.y, fac * a.z);
}

HAIR_NAMESPACE_END

#endif

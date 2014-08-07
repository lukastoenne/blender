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

struct float2;
struct float3;
struct float4;

struct float2 {
	float x, y;

	
	__forceinline float2() {}
	__forceinline float2(float x, float y) : x(x), y(y) {}
	__forceinline float2(float *data) : x(data[0]), y(data[1]) {}

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
	__forceinline float3(float *data) : x(data[0]), y(data[1]), z(data[2]) {}
#endif

	__forceinline float operator[](int i) const { return *(&x + i); }
	__forceinline float& operator[](int i) { return *(&x + i); }

	__forceinline float *data() { return &x; }
	__forceinline const float *data() const { return &x; }

	__forceinline float4 to_point() const;
	__forceinline float4 to_direction() const;
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
	
	__forceinline float4() {}
	__forceinline float4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
	__forceinline float4(float *data) : x(data[0]), y(data[1]), z(data[2]), w(data[3]) {}
#endif

	__forceinline float operator[](int i) const { return *(&x + i); }
	__forceinline float& operator[](int i) { return *(&x + i); }

	__forceinline float *data() { return &x; }
	__forceinline const float *data() const { return &x; }
};


struct Transform {
	float4 x, y, z, w; /* rows */
	
	static const Transform Identity;
	
	struct float4_col {
		__forceinline operator float4 () const { return float4(*f, *(f+4), *(f+8), *(f+12)); }
		__forceinline float operator[](int i) const { return *(f + (i<<2)); }
		__forceinline float& operator[](int i) { return *(f + (i<<2)); }
		
	protected:
		float4_col(Transform &tfm, int i) : f(&tfm.x.x + i) {}
		
		float *f;
		
		friend class Transform;
	};
	
	struct float4_col_const {
		__forceinline operator float4 () const { return float4(*f, *(f+4), *(f+8), *(f+12)); }
		__forceinline float operator[](int i) const { return *(f + (i<<2)); }
		
	protected:
		float4_col_const(const Transform &tfm, int i) : f(&tfm.x.x + i) {}
		
		const float *f;
		
		friend class Transform;
	};
	
	__forceinline Transform() {}
	__forceinline Transform(const float4 &x, const float4 &y, const float4 &z, const float4 &w) : x(x), y(y), z(z), w(w) {}
	__forceinline Transform(float data[4][4]) :
	    x(data[0][0], data[1][0], data[2][0], data[3][0]),
	    y(data[0][1], data[1][1], data[2][1], data[3][1]),
	    z(data[0][2], data[1][2], data[2][2], data[3][2]),
	    w(data[0][3], data[1][3], data[2][3], data[3][3])
	{}
	
	__forceinline float4 operator[](int i) const { return *(&x + i); }
	__forceinline float4& operator[](int i) { return *(&x + i); }
	
	float4 row(int i) const { return *(&x + i); }
	float4 &row(int i) { return *(&x + i); }

	float4_col col(int i) { return float4_col(*this, i); }
	float4_col_const col(int i) const { return float4_col_const(*this, i); }
};

struct Transform3 {
	float3 x, y, z; /* rows */
	
	static const Transform3 Identity;
	
	__forceinline Transform3() {}
	__forceinline Transform3(const float3 &x, const float3 &y, const float3 &z) : x(x), y(y), z(z) {}
	__forceinline Transform3(float data[3][3]) :
	    x(data[0][0], data[1][0], data[2][0]),
	    y(data[0][1], data[1][1], data[2][1]),
	    z(data[0][2], data[1][2], data[2][2])
	{}
	
	__forceinline float3 operator[](int i) const { return *(&x + i); }
	__forceinline float3& operator[](int i) { return *(&x + i); }
	
	float3 row(int i) const { return *(&x + i); }
	float3 &row(int i) { return *(&x + i); }
};

/* -------------------------------------------------- */

__forceinline float4 float3::to_point() const
{
	return float4(x, y, z, 1.0f);
}

__forceinline float4 float3::to_direction() const
{
	return float4(x, y, z, 0.0f);
}

const float4 unit_qt(0.0f, 0.0f, 0.0f, 1.0f);

HAIR_NAMESPACE_END

#endif

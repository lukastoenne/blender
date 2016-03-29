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

#ifndef __BVM_UTIL_MATH_H__
#define __BVM_UTIL_MATH_H__

/** \file bvm_util_math.h
 *  \ingroup bvm
 */

#include <assert.h>

extern "C" {
#include "BLI_math.h"
}

namespace blenvm {

static inline int int_div_floor(int a, int b)
{
	assert(a > 0);
	assert(b > 0);
	return a / b;
}

static inline int int_div_ceil(int a, int b)
{
	assert(a > 0);
	assert(b > 0);
	return 1 + ((a - 1) / b);
}

/* types */

struct float3 {
	float3()
	{}
	
	float3(float x, float y, float z) :
	    x(x), y(y), z(z)
	{}
	
	float* data() { return &x; }
	const float* data() const { return &x; }
	
	inline static float3 from_data(const float *values)
	{
		float3 f;
		f.x = values[0];
		f.y = values[1];
		f.z = values[2];
		return f;
	}
	
	float& operator[] (int index)
	{
		return ((float*)(&x))[index];
	}
	float operator[] (int index) const
	{
		return ((float*)(&x))[index];
	}
	
	float x;
	float y;
	float z;
};

struct float4 {
	float4()
	{}
	
	float4(float x, float y, float z, float w) :
	    x(x), y(y), z(z), w(w)
	{}
	
	float* data() { return &x; }
	const float* data() const { return &x; }
	
	inline static float4 from_data(const float *values)
	{
		float4 f;
		f.x = values[0];
		f.y = values[1];
		f.z = values[2];
		f.w = values[3];
		return f;
	}
	
	float& operator[] (int index)
	{
		return ((float*)(&x))[index];
	}
	float operator[] (int index) const
	{
		return ((float*)(&x))[index];
	}
	
	float x;
	float y;
	float z;
	float w;
};

struct matrix44 {
	enum Layout {
		COL_MAJOR,
		ROW_MAJOR,
	};
	
	matrix44()
	{}
	
	matrix44(const float3 &x, const float3 &y, const float3 &z, const float3 &t)
	{
		data[0][0] = x.x;	data[1][0] = y.x;	data[2][0] = z.x;	data[3][0] = t.x;
		data[0][1] = x.y;	data[1][1] = y.y;	data[2][1] = z.y;	data[3][1] = t.y;
		data[0][2] = x.z;	data[1][2] = y.z;	data[2][2] = z.z;	data[3][2] = t.z;
		data[0][3] = 0.0f;	data[1][3] = 0.0f;	data[2][3] = 0.0f;	data[3][3] = 1.0f;
	}
	
	matrix44(const float3 &t)
	{
		data[0][0] = 1.0f;	data[1][0] = 0.0f;	data[2][0] = 0.0f;	data[3][0] = t.x;
		data[0][1] = 0.0f;	data[1][1] = 1.0f;	data[2][1] = 0.0f;	data[3][1] = t.y;
		data[0][2] = 0.0f;	data[1][2] = 0.0f;	data[2][2] = 1.0f;	data[3][2] = t.z;
		data[0][3] = 0.0f;	data[1][3] = 0.0f;	data[2][3] = 0.0f;	data[3][3] = 1.0f;
	}
	
	inline static matrix44 from_data(const float *values, Layout layout = COL_MAJOR) {
		matrix44 m;
		if (layout == COL_MAJOR) {
			m.data[0][0] = values[0]; m.data[1][0] = values[1]; m.data[2][0] = values[2]; m.data[3][0] = values[3];
			m.data[0][1] = values[4]; m.data[1][1] = values[5]; m.data[2][1] = values[6]; m.data[3][1] = values[7];
			m.data[0][2] = values[8]; m.data[1][2] = values[9]; m.data[2][2] = values[10]; m.data[3][2] = values[11];
			m.data[0][3] = values[12]; m.data[1][3] = values[13]; m.data[2][3] = values[14]; m.data[3][3] = values[15];
		}
		else {
			m.data[0][0] = values[0]; m.data[1][0] = values[4]; m.data[2][0] = values[8]; m.data[3][0] = values[12];
			m.data[0][1] = values[1]; m.data[1][1] = values[5]; m.data[2][1] = values[9]; m.data[3][1] = values[13];
			m.data[0][2] = values[2]; m.data[1][2] = values[6]; m.data[2][2] = values[10]; m.data[3][2] = values[14];
			m.data[0][3] = values[3]; m.data[1][3] = values[7]; m.data[2][3] = values[11]; m.data[3][3] = values[15];
		}
		return m;
	}
	
	inline static matrix44 identity()
	{
		return matrix44(float3(1.0f, 0.0f, 0.0f),
		                float3(0.0f, 1.0f, 0.0f),
		                float3(0.0f, 0.0f, 1.0f),
		                float3(0.0f, 0.0f, 0.0f));
	}
	
	float data[4][4];
};

/* utilities */

typedef enum eEulerRotationOrders {
	EULER_ORDER_DEFAULT = ::EULER_ORDER_DEFAULT, /* blender classic = XYZ */
	EULER_ORDER_XYZ = ::EULER_ORDER_XYZ,
	EULER_ORDER_XZY = ::EULER_ORDER_XZY,
	EULER_ORDER_YXZ = ::EULER_ORDER_YXZ,
	EULER_ORDER_YZX = ::EULER_ORDER_YZX,
	EULER_ORDER_ZXY = ::EULER_ORDER_ZXY,
	EULER_ORDER_ZYX = ::EULER_ORDER_ZYX,
	/* there are 6 more entries with dulpicate entries included */
} eEulerRotationOrders;

inline static float div_safe(float a, float b)
{
	if (b != 0.0f)
		return a / b;
	else
		return 0.0f;
}

inline static float sqrt_safe(float a)
{
	if (a > 0.0f)
		return sqrtf(a);
	else
		return 0.0f;
}

} /* namespace bvm */

#endif /* __BVM_UTIL_MATH_H__ */

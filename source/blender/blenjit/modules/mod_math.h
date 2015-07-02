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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __MOD_MATH_H__
#define __MOD_MATH_H__

#include "math.h"

static inline void copy_v3_v3(float r[3], const float a[3])
{
	r[0] = a[0];
	r[1] = a[1];
	r[2] = a[2];
}

static inline void add_v3_v3v3(float r[3], const float a[3], const float b[3])
{
	r[0] = a[0] + b[0];
	r[1] = a[1] + b[1];
	r[2] = a[2] + b[2];
}

static inline void sub_v3_v3v3(float r[3], const float a[3], const float b[3])
{
	r[0] = a[0] - b[0];
	r[1] = a[1] - b[1];
	r[2] = a[2] - b[2];
}

static inline void mul_v3_m4v3(float r[3], float mat[4][4], const float vec[3])
{
	const float x = vec[0];
	const float y = vec[1];

	r[0] = x * mat[0][0] + y * mat[1][0] + mat[2][0] * vec[2] + mat[3][0];
	r[1] = x * mat[0][1] + y * mat[1][1] + mat[2][1] * vec[2] + mat[3][1];
	r[2] = x * mat[0][2] + y * mat[1][2] + mat[2][2] * vec[2] + mat[3][2];
}

static inline void mul_v3_v3fl(float r[3], const float v[3], float f)
{
	r[0] = v[0] * f;
	r[1] = v[1] * f;
	r[2] = v[2] * f;
}

static inline float dot_v3v3(const float a[3], const float b[3])
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static inline void zero_v3(float v[3])
{
	v[0] = 0.0f;
	v[1] = 0.0f;
	v[2] = 0.0f;
}

static inline float len_v3(const float v[3])
{
	float d = dot_v3v3(v, v);
	return sqrtf(d);
}

static inline float normalize_v3_v3(float r[3], const float a[3])
{
	float d = dot_v3v3(a, a);

	/* a larger value causes normalize errors in a
	 * scaled down models with camera extreme close */
	if (d > 1.0e-35f) {
		d = sqrtf(d);
		mul_v3_v3fl(r, a, 1.0f / d);
	}
	else {
		zero_v3(r);
		d = 0.0f;
	}

	return d;
}

static inline float normalize_v3(float n[3])
{
	return normalize_v3_v3(n, n);
}

/* Project v1 on v2 */
static inline void project_v3_v3v3(float c[3], const float v1[3], const float v2[3])
{
	const float mul = dot_v3v3(v1, v2) / dot_v3v3(v2, v2);

	c[0] = mul * v2[0];
	c[1] = mul * v2[1];
	c[2] = mul * v2[2];
}

/**
 * In this case plane is a 3D vector only (no 4th component).
 *
 * Projecting will make \a c a copy of \a v orthogonal to \a v_plane.
 *
 * \note If \a v is exactly perpendicular to \a v_plane, \a c will just be a copy of \a v.
 */
static inline void project_plane_v3_v3v3(float c[3], const float v[3], const float v_plane[3])
{
	float delta[3];
	project_v3_v3v3(delta, v, v_plane);
	sub_v3_v3v3(c, v, delta);
}

#endif

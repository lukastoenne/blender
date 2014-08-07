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

#ifndef __HAIR_CURVE_H__
#define __HAIR_CURVE_H__

extern "C" {
#include "RBI_api.h"
}

#include <BulletCollision/CollisionShapes/btSphereShape.h>

#include "rb_internal_types.h"

#include "HAIR_memalloc.h"

#include "HAIR_types.h"

HAIR_NAMESPACE_BEGIN

struct Point {
	struct State {
		float3 co;
		float3 vel;
	};
	
	Point();
	Point(const float3 &rest_co);
	
	State cur;
	State next;
	
	float3 rest_co;
	float3 rest_bend;
	
	float radius;
	
	rbGhostObject rb_ghost;
	btSphereShape bt_shape;
	
	HAIR_CXX_CLASS_ALLOC(Point)
};

struct CurveRoot {
	float3 co;
	float3 nor;
};

struct Curve {
	Curve();
	Curve(int totpoints, Point *points);
	
	Point *points;
	int totpoints;
	float avg_rest_length;
	
	CurveRoot root0, root1;
	
	HAIR_CXX_CLASS_ALLOC(Curve)
};

struct Frame {
	Frame() :
	    normal(float3(1.0f, 0.0f, 0.0f)),
	    tangent(float3(0.0f, 1.0f, 0.0f)),
	    cotangent(float3(0.0f, 0.0f, 1.0f))
	{}
	Frame(const float3 &normal, const float3 &tangent, const float3 &cotangent) :
	    normal(normal),
	    tangent(tangent),
	    cotangent(cotangent)
	{}
	Frame(const Transform &t) :
	    normal(float3(t.x.x, t.y.x, t.z.x)),
	    tangent(float3(t.x.y, t.y.y, t.z.y)),
	    cotangent(float3(t.x.z, t.y.z, t.z.z))
	{}
	
	float3 normal;
	float3 tangent;
	float3 cotangent;
	
	__forceinline Transform to_transform() const
	{
		return Transform(normal.to_direction(), tangent.to_direction(), cotangent.to_direction(), float4(0.0f, 0.0f, 0.0f, 1.0f));
	}
};

HAIR_NAMESPACE_END

#endif

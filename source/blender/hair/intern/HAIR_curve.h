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

#include "HAIR_memalloc.h"

#include "HAIR_types.h"

HAIR_NAMESPACE_BEGIN

struct Point {
	struct State {
		float3 co;
		float3 vel;
	};
	
	Point();
	Point(const float3 &co, const float3 &vel);
	
	State cur;
	State next;
	
	HAIR_CXX_CLASS_ALLOC(Point)
};

struct Curve {
	Curve();
	Curve(int totpoints, Point *points);
	
	Point *points;
	int totpoints;

	HAIR_CXX_CLASS_ALLOC(Curve)
};

HAIR_NAMESPACE_END

#endif

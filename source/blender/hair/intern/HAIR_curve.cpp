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

#include "rb_internal_types.h"

#include "HAIR_curve.h"

HAIR_NAMESPACE_BEGIN

Point::Point() :
    rb_ghost(),
    bt_shape(1.0f)
{
	rb_ghost.ghost.setCollisionShape(&bt_shape);
}

Point::Point(const float3 &rest_co) :
    rest_co(rest_co),
    radius(0.0f),
    rb_ghost(),
    bt_shape(1.0f)
{
	cur.co = rest_co;
	cur.vel = float3(0.0f, 0.0f, 0.0f);
	
	rb_ghost.ghost.setCollisionShape(&bt_shape);
}

Curve::Curve(int totpoints, Point *points) :
    points(points),
    totpoints(totpoints)
{
}

Curve::Curve() :
    points(NULL),
    totpoints(0)
{
}

HAIR_NAMESPACE_END

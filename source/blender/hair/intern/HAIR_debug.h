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

#ifndef __HAIR_DEBUG_H__
#define __HAIR_DEBUG_H__

#include <vector>

extern "C" {
#include "BLI_math.h"
}

#include "HAIR_debug_types.h"
#include "HAIR_smoothing.h"
#include "HAIR_types.h"

HAIR_NAMESPACE_BEGIN

#ifndef NDEBUG
#define HAIR_DEBUG
#endif

struct SolverData;

struct DebugThreadData
{
	typedef std::vector<HAIR_SolverDebugPoint> Points;
	typedef std::vector<HAIR_SolverDebugContact> CollisionContacts;
	
	CollisionContacts contacts;
	Points points;
};

struct Debug {
	
	static void point(DebugThreadData *data, int index, const float3 &co, const float3 &bend, const Frame &frame)
	{
#ifdef HAIR_DEBUG
		if (data) {
			HAIR_SolverDebugPoint p;
			p.index = index;
			copy_v3_v3(p.co, co.data());
			copy_v3_v3(p.bend, bend.data());
			copy_v3_v3(p.frame[0], frame.normal.data());
			copy_v3_v3(p.frame[1], frame.tangent.data());
			copy_v3_v3(p.frame[2], frame.cotangent.data());
			data->points.push_back(p);
		}
#else
		(void)data;
		(void)index;
		(void)co;
		(void)bend;
		(void)frame;
#endif
	}
	
	static void collision_contact(DebugThreadData *data, const float3 &coA, const float3 &coB)
	{
#ifdef HAIR_DEBUG
		if (data) {
			HAIR_SolverDebugContact c;
			copy_v3_v3(c.coA, coA.data());
			copy_v3_v3(c.coB, coB.data());
			data->contacts.push_back(c);
		}
#else
		(void)data;
		(void)coA;
		(void)coB;
#endif
	}
};

HAIR_NAMESPACE_END

#endif

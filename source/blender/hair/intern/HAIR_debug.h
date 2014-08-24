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
#include "BLI_threads.h"
#include "BLI_math.h"
}

#include "HAIR_debug_types.h"
#include "HAIR_smoothing.h"
#include "HAIR_types.h"

struct VoxelData;

HAIR_NAMESPACE_BEGIN

#ifndef NDEBUG
//#define HAIR_DEBUG
#endif

struct SolverData;
struct Volume;

struct Debug {
	typedef std::vector<HAIR_SolverDebugElement> Elements;
	
	static void init()
	{
#ifdef HAIR_DEBUG
		BLI_mutex_init(&mutex);
		active = true;
#endif
	}
	
	static void end()
	{
#ifdef HAIR_DEBUG
		elements.clear();
		
		BLI_mutex_end(&mutex);
		active = false;
#endif
	}
	
	static void dot(const float3 &p, float r, float g, float b, int hash)
	{
#ifdef HAIR_DEBUG
		HAIR_SolverDebugElement elem;
		elem.type = HAIR_DEBUG_ELEM_DOT;
		elem.hash = hash;
		
		copy_v3_v3(elem.a, p.data());
		
		elem.color[0] = r;
		elem.color[1] = g;
		elem.color[2] = b;
		
		BLI_mutex_lock(&mutex);
		elements.push_back(elem);
		BLI_mutex_unlock(&mutex);
#else
		(void)p;
		(void)r;
		(void)g;
		(void)b;
		(void)hash;
#endif
	}
	
	static void line(const float3 &v1, const float3 &v2, float r, float g, float b, int hash)
	{
#ifdef HAIR_DEBUG
		HAIR_SolverDebugElement elem;
		elem.type = HAIR_DEBUG_ELEM_LINE;
		elem.hash = hash;
		
		copy_v3_v3(elem.a, v1.data());
		copy_v3_v3(elem.b, v2.data());
		
		elem.color[0] = r;
		elem.color[1] = g;
		elem.color[2] = b;
		
		BLI_mutex_lock(&mutex);
		elements.push_back(elem);
		BLI_mutex_unlock(&mutex);
#else
		(void)v1;
		(void)v2;
		(void)r;
		(void)g;
		(void)b;
		(void)hash;
#endif
	}
	
	static void vector(const float3 &p, const float3 &d, float r, float g, float b, int hash)
	{
#ifdef HAIR_DEBUG
		HAIR_SolverDebugElement elem;
		elem.type = HAIR_DEBUG_ELEM_VECTOR;
		elem.hash = hash;
		
		copy_v3_v3(elem.a, p.data());
		copy_v3_v3(elem.b, d.data());
		
		elem.color[0] = r;
		elem.color[1] = g;
		elem.color[2] = b;
		
		BLI_mutex_lock(&mutex);
		elements.push_back(elem);
		BLI_mutex_unlock(&mutex);
#else
		(void)p;
		(void)d;
		(void)r;
		(void)g;
		(void)b;
		(void)hash;
#endif
	}
	
	static bool texture_volume(Volume *vol, VoxelData *vd);
	
	static bool active;
#ifdef HAIR_DEBUG
	static ThreadMutex mutex;
	static Elements elements;
#endif
};

HAIR_NAMESPACE_END

#endif

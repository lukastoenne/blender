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

#ifndef __HAIR_SOLVER_H__
#define __HAIR_SOLVER_H__

#include <vector>

#include <BulletCollision/CollisionShapes/btBoxShape.h>

extern "C" {
#include "DNA_hair_types.h"
}

#include "HAIR_curve.h"
#include "HAIR_memalloc.h"

struct rbDynamicsWorld;
struct rbGhostObject;

HAIR_NAMESPACE_BEGIN

struct DebugThreadData;
struct SolverTaskData;

struct SolverData {
	SolverData();
	SolverData(int totcurves, int totpoints);
	SolverData(const SolverData &other);
	~SolverData();
	
	Curve *curves;
	Point *points;
	int totcurves;
	int totpoints;
	
	float t0, t1;
	
	rbGhostObject rb_ghost;
	btBoxShape bt_shape;
	
	void precompute_rest_bend(const HairParams &params);
	
	void add_to_world(rbDynamicsWorld *world, int col_groups);
	void remove_from_world(rbDynamicsWorld *world);
	
	HAIR_CXX_CLASS_ALLOC(SolverData)
};

/* Utility classes for smoothing algorithms */

struct SolverDataLocWalker {
	typedef float3 data_t;
	
	SolverDataLocWalker(Curve *curve) :
	    curve(curve),
	    i(0)
	{}
	
	float3 read()
	{
		float3 result = curve->points[i].cur.co;
		if (i < curve->totpoints-1)
			++i;
		return result;
	}
	
	int size() const { return curve->totpoints; }
	
	Curve *curve;
	int i;
	
	int dbg_kstart;
};

struct SolverDataRestLocWalker {
	typedef float3 data_t;
	
	SolverDataRestLocWalker(Curve *curve) :
	    curve(curve),
	    i(0)
	{}
	
	float3 read()
	{
		float3 result = curve->points[i].rest_co;
		if (i < curve->totpoints-1)
			++i;
		return result;
	}
	
	int size() const { return curve->totpoints; }
	
	Curve *curve;
	int i;
};

struct SolverForces {
	SolverForces();
	
	struct rbDynamicsWorld *dynamics_world;
	
	float3 gravity;
};

/* Point contact info */

struct PointContactInfo {
	PointContactInfo() {}
	PointContactInfo(const btManifoldPoint &bt_point, const btCollisionObject *ob0, const btCollisionObject *ob1, int point_index);
	
	int point_index;
	
	float3 local_point_body;
	float3 local_point_hair;
	float3 world_point_body;
	float3 world_point_hair;
	float3 world_normal_body;
	
	float distance;
	float friction;
	float restitution;
	
	float3 world_vel_body;
};

typedef std::vector<PointContactInfo> PointContactCache;
typedef std::vector<DebugThreadData> DebugThreadDataVector;

class Solver
{
public:
	Solver();
	~Solver();
	
	void params(const HairParams &params) { m_params = params; }
	const HairParams &params() const { return m_params; }
	
	SolverForces &forces() { return m_forces; }
	
	void set_data(SolverData *data);
	void free_data();
	SolverData *data() const { return m_data; }
	
	void cache_point_contacts(PointContactCache &cache) const;
	
	void do_integration(float time, float timestep, const SolverTaskData &data) const;
	
	void step_threaded(float time, float timestep, DebugThreadDataVector *debug_thread_data = NULL);
	
private:
	HairParams m_params;
	SolverForces m_forces;
	SolverData *m_data;

	HAIR_CXX_CLASS_ALLOC(Solver)
};

HAIR_NAMESPACE_END

#endif

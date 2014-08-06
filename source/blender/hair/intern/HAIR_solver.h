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

#include <BulletCollision/CollisionShapes/btBoxShape.h>

extern "C" {
#include "DNA_hair_types.h"
}

#include "HAIR_curve.h"
#include "HAIR_memalloc.h"

struct rbDynamicsWorld;
struct rbGhostObject;

HAIR_NAMESPACE_BEGIN

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
	
	void precompute_rest_bend();
	
	void add_to_world(rbDynamicsWorld *world, int col_groups);
	void remove_from_world(rbDynamicsWorld *world);
	
	void debug_contacts(rbDynamicsWorld *world);
	
	HAIR_CXX_CLASS_ALLOC(SolverData)
};

/* Utility class for smoothing algorithms */
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
	
	void step_threaded(float time, float timestep);
	
protected:
	void calc_root_animation(float t0, float t1, float t, Curve *curve, float3 &co, float3 &vel) const;
	float3 calc_velocity(Curve *curve, Point *point, float time, Point::State &state) const;
	float3 calc_acceleration(Curve *curve, Point *point, float time, Point::State &state) const;
	float3 calc_stretch_force(Curve *curve, Point *point0, Point *point1, float time) const;
	float3 calc_bend_force(Curve *curve, Point *point0, Point *point1, float time) const;
	
private:
	HairParams m_params;
	SolverForces m_forces;
	SolverData *m_data;

	HAIR_CXX_CLASS_ALLOC(Solver)
};

HAIR_NAMESPACE_END

#endif

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

#include <vector>

extern "C" {
#include "BLI_task.h"
#include "BLI_threads.h"

#include "RBI_api.h"
}

#include "HAIR_debug.h"
#include "HAIR_math.h"
#include "HAIR_smoothing.h"
#include "HAIR_solver.h"

HAIR_NAMESPACE_BEGIN

SolverData::SolverData() :
    curves(NULL),
    points(NULL),
    totcurves(0),
    totpoints(0),
    rb_ghost(),
    bt_shape(btVector3(1.0f, 1.0f, 1.0f))
{
	rb_ghost.ghost.setCollisionShape(&bt_shape);
}

SolverData::SolverData(int totcurves, int totpoints) :
    totcurves(totcurves),
    totpoints(totpoints),
    rb_ghost(),
    bt_shape(btVector3(1.0f, 1.0f, 1.0f))
{
	curves = new Curve[totcurves];
	points = new Point[totpoints];

	rb_ghost.ghost.setCollisionShape(&bt_shape);
}

SolverData::SolverData(const SolverData &other) :
    curves(other.curves),
    points(other.points),
    totcurves(other.totcurves),
    totpoints(other.totpoints),
    rb_ghost(),
    bt_shape(btVector3(1.0f, 1.0f, 1.0f))
{
	rb_ghost.ghost.setCollisionShape(&bt_shape);
}

SolverData::~SolverData()
{
	if (curves)
		delete[] curves;
	if (points)
		delete[] points;
}

void SolverData::add_to_world(rbDynamicsWorld *world, int col_groups)
{
	if (!world)
		return;
	
	RB_dworld_add_ghost(world, &rb_ghost, col_groups);
}

void SolverData::remove_from_world(rbDynamicsWorld *world)
{
	if (!world)
		return;
	
	RB_dworld_remove_ghost(world, &rb_ghost);
}

static float3 calc_bend(const Frame &frame, const float3 &co0, const float3 &co1)
{
	float3 edge = co1 - co0;
	return float3(dot_v3v3(edge, frame.normal), dot_v3v3(edge, frame.tangent), dot_v3v3(edge, frame.cotangent));
}

void SolverData::precompute_rest_bend(const HairParams &params)
{
	Curve *curve;
	int i;
	
	for (curve = curves, i = 0; i < totcurves; ++curve, ++i) {
		Point *pt = curve->points;
		Point *next_pt = pt + 1;
		
		/* calculate average rest length */
		curve->avg_rest_length = 0.0f;
		if (curve->totpoints > 1) {
			for (int k = 0; k < curve->totpoints - 1; ++k)
				curve->avg_rest_length += len_v3(curve->points[k+1].rest_co - curve->points[k].rest_co);
			curve->avg_rest_length /= curve->totpoints;
		}
		
		if (curve->totpoints == 0)
			continue;
		else if (curve->totpoints == 1)
			pt->rest_bend = float3(0.0f, 0.0f, 0.0f);
		else {
			
			float3 normal = curve->rest_root_normal, tangent = curve->rest_root_tangent;
			Frame rest_frame(normal, tangent, cross_v3_v3(normal, tangent));
			
			FrameIterator<SolverDataRestLocWalker> iter(SolverDataRestLocWalker(curve), curve->avg_rest_length, params.bend_smoothing, rest_frame);
			while (iter.index() < curve->totpoints - 1) {
				pt->rest_bend = calc_bend(iter.frame(), pt->rest_co, next_pt->rest_co);
				
				iter.next();
				++pt;
				++next_pt;
			}
			
			/* last point has no defined rest bending vector */
			pt->rest_bend = float3(0.0f, 0.0f, 0.0f);
		}
	}
}

struct HairContactResultCallback : btCollisionWorld::ContactResultCallback {
	btScalar addSingleResult(btManifoldPoint &cp,
	                         const btCollisionObjectWrapper *colObj0Wrap, int partId0, int index0,
	                         const btCollisionObjectWrapper *colObj1Wrap, int partId1, int index1)
	{
		if (cp.getDistance() < 0.f) {
			const btVector3 &ptA = cp.getPositionWorldOnA();
			const btVector3 &ptB = cp.getPositionWorldOnB();
//			const btVector3 &normalOnB = cp.m_normalWorldOnB;
			
			Debug::collision_contact(float3(ptA.x(), ptA.y(), ptA.z()), float3(ptB.x(), ptB.y(), ptB.z()));
		}
		
		/* note: return value is unused
		 * http://bulletphysics.org/Bullet/phpBB3/viewtopic.php?p=20990#p20990
		 */
		return 0.0f;
	}
};

static void debug_ghost_contacts(SolverData *data, btDynamicsWorld *dworld, rbGhostObject *object)
{
	btPairCachingGhostObject *ghost = &object->ghost;
	
	btManifoldArray manifold_array;
	const btBroadphasePairArray& pairs = ghost->getOverlappingPairCache()->getOverlappingPairArray();
	int num_pairs = pairs.size();
	
	for (int i = 0; i < num_pairs; i++) {
		manifold_array.clear();
		
		const btBroadphasePair& pair = pairs[i];
		
		/* unless we manually perform collision detection on this pair, the contacts are in the dynamics world paircache */
		btBroadphasePair* collision_pair = dworld->getPairCache()->findPair(pair.m_pProxy0,pair.m_pProxy1);
		if (!collision_pair)
			continue;
		
		btCollisionObject *ob0 = (btCollisionObject *)pair.m_pProxy0->m_clientObject;
		btCollisionObject *ob1 = (btCollisionObject *)pair.m_pProxy1->m_clientObject;
		btCollisionObject *other = ob0 == ghost ? ob1 : ob0;
		
		HairContactResultCallback cb;
		
		Curve *curve = data->curves;
		for (int i = 0; i < data->totcurves; ++i, ++curve) {
			Point *pt = curve->points;
			for (int k = 0; k < curve->totpoints; ++k, ++pt) {
				dworld->contactPairTest(&pt->rb_ghost.ghost, other, cb);
			}
		}
		
#if 0
		if (collision_pair->m_algorithm)
			collision_pair->m_algorithm->getAllContactManifolds(manifold_array);
		
		for (int j = 0; j < manifold_array.size(); j++) {
			btPersistentManifold* manifold = manifold_array[j];
			btScalar direction_sign = manifold->getBody0() == ghost ? btScalar(-1.0) : btScalar(1.0);
			for (int p = 0; p < manifold->getNumContacts(); p++) {
				const btManifoldPoint &pt = manifold->getContactPoint(p);
				if (pt.getDistance() < 0.f) {
					const btVector3 &ptA = pt.getPositionWorldOnA();
					const btVector3 &ptB = pt.getPositionWorldOnB();
					const btVector3 &normalOnB = pt.m_normalWorldOnB;
					
					Debug::collision_contact(float3(ptA.x(), ptA.y(), ptA.z()), float3(ptB.x(), ptB.y(), ptB.z()));
				}
			}
		}
#endif
	}
}

void SolverData::debug_contacts(rbDynamicsWorld *world)
{
	btDynamicsWorld *dworld = world->dynamicsWorld;
	
	debug_ghost_contacts(this, dworld, &rb_ghost);

	if (rb_ghost.ghost.getBroadphaseHandle()) {
		float3 c[8];
		c[0] = float3((float *)rb_ghost.ghost.getBroadphaseHandle()->m_aabbMin.m_floats);
		c[7] = float3((float *)rb_ghost.ghost.getBroadphaseHandle()->m_aabbMax.m_floats);
		c[1] = float3(c[7].x, c[0].y, c[0].z);
		c[2] = float3(c[0].x, c[7].y, c[0].z);
		c[3] = float3(c[7].x, c[7].y, c[0].z);
		c[4] = float3(c[0].x, c[0].y, c[7].z);
		c[5] = float3(c[7].x, c[0].y, c[7].z);
		c[6] = float3(c[0].x, c[7].y, c[7].z);
		Debug::collision_contact(c[0], c[1]);
		Debug::collision_contact(c[1], c[3]);
		Debug::collision_contact(c[3], c[2]);
		Debug::collision_contact(c[2], c[0]);
		
		Debug::collision_contact(c[0], c[4]);
		Debug::collision_contact(c[1], c[5]);
		Debug::collision_contact(c[2], c[6]);
		Debug::collision_contact(c[3], c[7]);
		
		Debug::collision_contact(c[4], c[5]);
		Debug::collision_contact(c[5], c[7]);
		Debug::collision_contact(c[7], c[6]);
		Debug::collision_contact(c[6], c[4]);
	}
}


SolverForces::SolverForces()
{
	gravity = float3(0.0f, 0.0f, 0.0f);
}


Solver::Solver() :
    m_data(NULL)
{
}

Solver::~Solver()
{
	if (m_data)
		delete m_data;
}

void Solver::set_data(SolverData *data)
{
	if (m_data)
		delete m_data;
	m_data = data;
}

void Solver::free_data()
{
	if (m_data) {
		delete m_data;
		m_data = NULL;
	}
}

static void calc_root_animation(float t0, float t1, float t, Curve *curve, float3 &co, float3 &vel, float3 &normal, float3 &tangent)
{
	const CurveRoot &root0 = curve->root0;
	const CurveRoot &root1 = curve->root1;
	
	if (t1 > t0) {
		float x = (t - t0) / (t1 - t0);
		float mx = 1.0f - x;
		
		co = root0.co * mx + root1.co * x;
		vel = (root1.co - root0.co) / (t1 - t0);
		interp_v3v3_slerp(normal, root0.nor, root1.nor, x);
		interp_v3v3_slerp(tangent, root0.tan, root1.tan, x);
	}
	else {
		co = root0.co;
		vel = float3(0.0f, 0.0f, 0.0f);
		normal = root0.nor;
		tangent = root0.tan;
	}
}

static float3 calc_velocity(Curve *curve, Point *point, float time, Point::State &state)
{
	return state.vel;
}

static float3 calc_stretch_force(const HairParams &params, Curve *curve, Point *point0, Point *point1, float time)
{
	/* XXX this could be cached in SolverData */
	float3 dir;
	float rest_length = len_v3(point1->rest_co - point0->rest_co);
	float length = normalize_v3_v3(dir, point1->cur.co - point0->cur.co);
	float3 dvel = point1->cur.vel - point0->cur.vel;
	
	float3 stretch_force = params.stretch_stiffness * (length - rest_length) * dir;
	float3 stretch_damp = params.stretch_damping * dot_v3v3(dvel, dir) * dir;
	
	return stretch_force + stretch_damp;
}

static float3 bend_target(const Frame &frame, Point *pt)
{
	float3 rest_bend = pt->rest_bend;
	float3 target(frame.normal.x * rest_bend.x + frame.tangent.x * rest_bend.y + frame.cotangent.x * rest_bend.z,
	              frame.normal.y * rest_bend.x + frame.tangent.y * rest_bend.y + frame.cotangent.y * rest_bend.z,
	              frame.normal.z * rest_bend.x + frame.tangent.z * rest_bend.y + frame.cotangent.z * rest_bend.z);
	return target;
}

static float3 calc_bend_force(const HairParams &params, Curve *curve, Point *point0, Point *point1, const Frame &frame, float time)
{
	float3 target = bend_target(frame, point0);
	
	float3 dir;
	float3 edge = point1->cur.co - point0->cur.co;
	normalize_v3_v3(dir, edge);
	float3 dvel = point1->cur.vel - point0->cur.vel;
	
	float3 bend_force = params.bend_stiffness * (edge - target);
	float3 bend_damp = params.bend_damping * (dvel - dot_v3v3(dvel, dir) * dir);
	
	return bend_force + bend_damp;
}

static float3 calc_acceleration(const SolverForces &forces, Curve *curve, Point *point, float time, Point::State &state)
{
	float3 acc = float3(0.0f, 0.0f, 0.0f);
	
	acc = acc + forces.gravity;
	
	return acc;
}

struct SolverTaskData {
	Curve *curves;
	Point *points;
	int totcurves;
	int totpoints;
};

static void step(const HairParams &params, const SolverForces &forces, float time, float timestep, float t0, float t1, const SolverTaskData &data)
{
	Curve *curve;
	Point *point;
	int totcurve = data.totcurves;
	/*int totpoint = data.totpoints;*/
	int i, k, ktot = 0;
	
	for (i = 0, curve = data.curves; i < totcurve; ++i, ++curve) {
		int numpoints = curve->totpoints;
		float3 stretch, prev_stretch, bend, prev_bend;
		
		/* Root point animation */
		k = 0;
		point = curve->points;
		
		/* note: roots are evaluated at the end of the timestep: time + timestep
		 * so the hair points align perfectly with them
		 */
		float3 normal, tangent;
		calc_root_animation(t0, t1, time + timestep, curve, point->next.co, point->next.vel, normal, tangent);
		
		Frame rest_frame(normal, tangent, cross_v3_v3(normal, tangent));
		FrameIterator<SolverDataLocWalker> frame_iter(SolverDataLocWalker(curve), curve->avg_rest_length, params.bend_smoothing, rest_frame);
		
		if (k < numpoints-1) {
			stretch = calc_stretch_force(params, curve, point, point+1, time);
			bend = calc_bend_force(params, curve, point, point+1, frame_iter.frame(), time);
		}
		else {
			stretch = float3(0.0f, 0.0f, 0.0f);
			bend = float3(0.0f, 0.0f, 0.0f);
		}
		
		Debug::point(ktot, bend_target(frame_iter.frame(), point), frame_iter.frame());
		
		frame_iter.next();
		prev_stretch = stretch;
		prev_bend = bend;
		
		/* Integrate the remaining free points */
		for (++k, ++ktot, ++point; k < numpoints; ++k, ++ktot, ++point) {
			if (k < numpoints-1) {
				stretch = calc_stretch_force(params, curve, point, point+1, time);
				bend = calc_bend_force(params, curve, point, point+1, frame_iter.frame(), time);
			}
			else {
				stretch = float3(0.0f, 0.0f, 0.0f);
				bend = float3(0.0f, 0.0f, 0.0f);
			}
			
			float3 acc = calc_acceleration(forces, curve, point, time, point->cur);
			acc = acc - prev_stretch + stretch - prev_bend + bend;
			point->next.vel = point->cur.vel + acc * timestep;
			
			float3 vel = calc_velocity(curve, point, time, point->next);
			point->next.co = point->cur.co + vel * timestep;
			
			Debug::point(ktot, bend_target(frame_iter.frame(), point), frame_iter.frame());
			
			frame_iter.next();
			prev_stretch = stretch;
			prev_bend = bend;
		}
	}
	
	/* advance state */
	for (i = 0, curve = data.curves; i < totcurve; ++i, ++curve) {
		int numpoints = curve->totpoints;
		for (k = 0, point = curve->points; k < numpoints; ++k, ++point) {
			point->cur = point->next;
		}
	}
}

struct SolverPoolData {
	const HairParams *params;
	const SolverForces *forces;
	float time, timestep, t0, t1;
};

static void step_threaded_func(TaskPool *pool, void *vtaskdata, int UNUSED(threadid))
{
	SolverPoolData *pooldata = (SolverPoolData *)BLI_task_pool_userdata(pool);
	SolverTaskData *taskdata = (SolverTaskData *)vtaskdata;
	step(*pooldata->params, *pooldata->forces, pooldata->time, pooldata->timestep, pooldata->t0, pooldata->t1, *taskdata);
}

void Solver::step_threaded(float time, float timestep)
{
	if (m_forces.dynamics_world)
		m_data->debug_contacts(m_forces.dynamics_world);
	
	typedef std::vector<SolverTaskData> SolverTaskVector;
	
	const int max_points_per_task = 1024;
	const int max_hairs_per_task = 256;
	
	SolverPoolData pooldata;
	pooldata.params = &m_params;
	pooldata.forces = &m_forces;
	pooldata.time = time;
	pooldata.timestep = timestep;
	pooldata.t0 = m_data->t0;
	pooldata.t1 = m_data->t1;
	
	TaskScheduler *task_scheduler = BLI_task_scheduler_get();
	TaskPool *task_pool = BLI_task_pool_create(task_scheduler, (void*)(&pooldata));
	
	SolverTaskVector taskdata;
	
	int point_start = 0;
	int num_points = 0;
	int hair_start = 0;
	int num_hairs = 0;
	Curve *curve = m_data->curves;
	
	/* Generate tasks
	 * The approach for now is to distribute whole hairs among tasks,
	 * such that each task has roughly the same amount of points in total
	 */
	while (hair_start < m_data->totcurves) {
		++num_hairs;
		num_points += curve->totpoints;
		
		if (num_points > max_points_per_task || num_hairs > max_hairs_per_task ||
		    hair_start + num_hairs >= m_data->totcurves) {
			
			SolverTaskData solver_task;
			solver_task.curves = m_data->curves + hair_start;
			solver_task.points = m_data->points + point_start;
			solver_task.totcurves = num_hairs;
			solver_task.totpoints = num_points;
			taskdata.push_back(solver_task);
			
			hair_start += num_hairs;
			point_start += num_points;
			num_hairs = 0;
			num_points = 0;
		}
		
		++curve;
	}
	
	/* Note: can't create tasks right away in the loop above,
	 * because they reference data in a vector which can be altered afterward.
	 */
	for (SolverTaskVector::const_iterator it = taskdata.begin(); it != taskdata.end(); ++it) {
		BLI_task_pool_push(task_pool, step_threaded_func, (void*)(&(*it)), false, TASK_PRIORITY_LOW);
	}
	
	/* work and wait until tasks are done */
	BLI_task_pool_work_and_wait(task_pool);
	
	BLI_task_pool_free(task_pool);
}

HAIR_NAMESPACE_END

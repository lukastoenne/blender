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

/* XXX could cache rest_length in SolverData */

static float3 calc_stretch_force(const HairParams &params, const Point *point0, const Point *point1, float time)
{
	float rest_length = len_v3(point1->rest_co - point0->rest_co);
	float3 dir;
	float length = normalize_v3_v3(dir, point1->cur.co - point0->cur.co);
	
	return params.stretch_stiffness * (length - rest_length) * dir;
}

static float3 calc_stretch_damping(const HairParams &params, const Point *point0, const Point *point1, float time)
{
	float3 dir;
	float3 edge = point1->cur.co - point0->cur.co;
	normalize_v3_v3(dir, edge);
	float3 dvel = point1->cur.vel - point0->cur.vel;
	
	return params.stretch_damping * dot_v3v3(dvel, dir) * dir;
}

static float3 bend_target(const Frame &frame, const Point *pt)
{
	float3 rest_bend = pt->rest_bend;
	float3 target(frame.normal.x * rest_bend.x + frame.tangent.x * rest_bend.y + frame.cotangent.x * rest_bend.z,
	              frame.normal.y * rest_bend.x + frame.tangent.y * rest_bend.y + frame.cotangent.y * rest_bend.z,
	              frame.normal.z * rest_bend.x + frame.tangent.z * rest_bend.y + frame.cotangent.z * rest_bend.z);
	return target;
}

static float3 calc_bend_force(const HairParams &params, const Point *point0, const Point *point1, const Frame &frame, float time)
{
	float3 target = bend_target(frame, point0);
	
	float3 dir;
	float3 edge = point1->cur.co - point0->cur.co;
	normalize_v3_v3(dir, edge);
	
	return params.bend_stiffness * (edge - target);
}

static float3 calc_bend_damping(const HairParams &params, const Point *point0, const Point *point1, const Frame &frame, float time)
{
	float3 dir;
	float3 edge = point1->cur.co - point0->cur.co;
	normalize_v3_v3(dir, edge);
	float3 dvel = point1->cur.vel - point0->cur.vel;
	
	return params.bend_damping * (dvel - dot_v3v3(dvel, dir) * dir);
}

struct SolverTaskData {
	Curve *curves;
	Point *points;
	int startcurve, totcurves;
	int startpoint, totpoints;
};

static void do_internal_forces(const HairParams &params, const SolverForces &forces, float time, float timestep, const Point *point0, const Point *point1, const Frame &frame,
                               float3 &force0, float3 &force1)
{
	float3 stretch, bend;
	
	if (point1) {
		stretch = calc_stretch_force(params, point0, point1, time);
		bend = calc_bend_force(params, point0, point1, frame, time);
	}
	else {
		stretch = float3(0.0f, 0.0f, 0.0f);
		bend = float3(0.0f, 0.0f, 0.0f);
	}
	
	force0 = stretch + bend;
	force1 = - stretch - bend;
}

static void do_external_forces(const HairParams &params, const SolverForces &forces, float time, float timestep, const Point *point0, const Point *point1, const Frame &frame,
                               float3 &force)
{
	float3 acc = float3(0.0f, 0.0f, 0.0f);
	
	acc = acc + forces.gravity;
	
	force = acc;
}

static void do_damping(const HairParams &params, const SolverForces &forces, float time, float timestep, const Point *point0, const Point *point1, const Frame &frame,
                       float3 &force0, float3 &force1)
{
	const int totsteps = 1; /* XXX TODO can have multiple damping steps per integration step for accuracy */
	float dt = timestep / (float)totsteps;
	float3 stretch, bend;
	
	force0 = float3(0.0f, 0.0f, 0.0f);
	force1 = float3(0.0f, 0.0f, 0.0f);
	
	if (point1) {
		for (int step = 0; step < totsteps; ++step) {
			stretch = calc_stretch_damping(params, point0, point1, time);
			bend = calc_bend_damping(params, point0, point1, frame, time);
			force0 = force0 + stretch + bend;
			force1 = force1 - stretch - bend;
			
			time += dt;
		}
	}
}

static void do_collision(const HairParams &params, const SolverForces &forces, float time, float timestep, const SolverTaskData &data, const PointContactCache &contacts)
{
	int start = data.startpoint;
	int end = start + data.totpoints;
	
	/* XXX there could be a bit of overhead here since we skip points that are not in the task point range.
	 * contacts could be sorted by point index to avoid this, but the sorting might actually be more costly than it's worth ...
	 */
	for (int i = 0; i < contacts.size(); ++i) {
		const PointContactInfo &info = contacts[i];
		
		if (info.point_index < start || info.point_index >= end)
			continue;
		
		Point *point = data.points + (info.point_index - start);
		
		/* Collision response based on
		 * "Simulating Complex Hair with Robust Collision Handling"
		 * Choe, Choi, Ko 2005
		 * http://graphics.snu.ac.kr/publications/2005-choe-HairSim/Choe_2005_SCA.pdf
		 */
		
		/* XXX we don't have a nice way of handling deformation velocity yet,
		 * assume constant linear/rotational velocity for now
		 */
		float3 obj_v0 = info.world_vel_body;
		float3 obj_v1 = obj_v0;
		float3 v0 = point->cur.vel;
		if (dot_v3v3(v0, info.world_normal_body) < 0.0f) {
			/* estimate for velocity change to prevent collision (3.2, (8)) */
			float3 dv_a = dot_v3v3(info.restitution * (obj_v0 - v0) + (obj_v1 - v0), info.world_normal_body) * info.world_normal_body;
			
			point->force_accum = point->force_accum + dv_a / timestep;
//			Debug::collision_contact(point->cur.co, point->cur.co + point->force_accum);
		}
	}
}

void Solver::do_integration(float time, float timestep, const SolverTaskData &data) const
{
	const int totsteps = m_params.substeps_forces; /* can have multiple integration steps per tick for accuracy */
	float dt = timestep / (float)totsteps;
	
	int totcurve = data.totcurves;
	/*int totpoint = data.totpoints;*/
	
	PointContactCache contacts;
	cache_point_contacts(contacts);
	
	for (int step = 0; step < totsteps; ++step) {
		
		/* clear forces */
		for (int k = 0; k < data.totpoints; ++k) {
			data.points[k].force_accum = float3(0.0f, 0.0f, 0.0f);
		}
		
		Curve *curve = data.curves;
		for (int i = 0; i < totcurve; ++i, ++curve) {
			int numpoints = curve->totpoints;
			
			/* note: roots are evaluated at the end of the timestep: time + timestep
			 * so the hair points align perfectly with them
			 */
			float3 root_co, root_vel, normal, tangent;
			calc_root_animation(m_data->t0, m_data->t1, time + timestep, curve, root_co, root_vel, normal, tangent);
			
			Frame rest_frame(normal, tangent, cross_v3_v3(normal, tangent));
			FrameIterator<SolverDataLocWalker> frame_iter(SolverDataLocWalker(curve), curve->avg_rest_length, m_params.bend_smoothing, rest_frame);
			
			float3 intern_force, intern_force_next, extern_force, damping, damping_next;
			float3 acc_prev; /* reactio from previous point */
			
			/* Root point animation */
			Point *point = curve->points;
			int k = 0;
			if (numpoints > 0) {
				Point *point_next = k < numpoints-1 ? point+1 : NULL;
				do_internal_forces(m_params, m_forces, time, dt, point, point_next, frame_iter.frame(), intern_force, intern_force_next);
				do_external_forces(m_params, m_forces, time, dt, point, point_next, frame_iter.frame(), extern_force);
				do_damping(m_params, m_forces, time, dt, point, point_next, frame_iter.frame(), damping, damping_next);
				
				frame_iter.next();
				acc_prev = intern_force_next + damping_next;
				++k;
				++point;
			}
			
			/* Integrate free points */
			for (; k < numpoints; ++k, ++point) {
				Point *point_next = k < numpoints-1 ? point+1 : NULL;
				do_internal_forces(m_params, m_forces, time, dt, point, point_next, frame_iter.frame(), intern_force, intern_force_next);
				do_external_forces(m_params, m_forces, time, dt, point, point_next, frame_iter.frame(), extern_force);
				do_damping(m_params, m_forces, time, dt, point, point_next, frame_iter.frame(), damping, damping_next);
				
				point->force_accum = point->force_accum + intern_force + extern_force + damping + acc_prev;
				
				frame_iter.next();
				acc_prev = intern_force_next + damping_next;
			}
			
		}
		
		do_collision(m_params, m_forces, time, dt, data, contacts);
		
		/* integrate */
		{
			Curve *curve = data.curves;
			for (int i = 0; i < totcurve; ++i, ++curve) {
				int numpoints = curve->totpoints;
				
				/* note: roots are evaluated at the end of the timestep: time + timestep
				 * so the hair points align perfectly with them
				 */
				float3 root_co, root_vel, normal, tangent;
				calc_root_animation(m_data->t0, m_data->t1, time + timestep, curve, root_co, root_vel, normal, tangent);
				
				/* Root point animation */
				Point *point = curve->points;
				int k = 0;
				if (numpoints > 0) {
					point->next.co = root_co;
					point->next.vel = root_vel;
					
					++k;
					++point;
				}
				
				/* Integrate free points */
				for (; k < numpoints; ++k, ++point) {
					/* semi-implicit Euler step */
					point->next.vel = point->cur.vel + point->force_accum * timestep;
					point->next.co = point->cur.co + point->next.vel * timestep;
				}
			}
			
			time += dt;
		}
	}
}

struct SolverPoolData {
	const Solver *solver;
	float time, timestep;
};

static void step_threaded_func(TaskPool *pool, void *vtaskdata, int UNUSED(threadid))
{
	const SolverPoolData *pooldata = (const SolverPoolData *)BLI_task_pool_userdata(pool);
	SolverTaskData *taskdata = (SolverTaskData *)vtaskdata;
	pooldata->solver->do_integration(pooldata->time, pooldata->timestep, *taskdata);
}

static void advance_state(SolverData *data)
{
//	Curve *curve;
	Point *point;
//	int i, totcurves = data->totcurves;
	int k, totpoints = data->totpoints;
	
//	for (int i = 0, curve = data->curves; i < totcurves; ++i, ++curve) {
//	}
	for (k = 0, point = data->points; k < totpoints; ++k, ++point) {
		point->cur = point->next;
	}
}

void Solver::step_threaded(float time, float timestep, DebugThreadDataVector *debug_thread_data)
{
	typedef std::vector<SolverTaskData> SolverTaskVector;
	
	const int max_points_per_task = 1024;
	const int max_hairs_per_task = 256;
	
	SolverPoolData pooldata;
	pooldata.solver = this;
	pooldata.time = time;
	pooldata.timestep = timestep;
	
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
			solver_task.startcurve = hair_start;
			solver_task.startpoint = point_start;
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
	
	advance_state(m_data);
}

HAIR_NAMESPACE_END

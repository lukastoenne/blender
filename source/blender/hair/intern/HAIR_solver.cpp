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

static float3 get_root_location(float t0, float t1, float t, Curve *curve)
{
	const CurveRoot &root0 = curve->root0;
	const CurveRoot &root1 = curve->root1;
	
	if (t1 > t0) {
		float x = (t - t0) / (t1 - t0);
		float mx = 1.0f - x;
		
		return root0.co * mx + root1.co * x;
	}
	else {
		return root0.co;
	}
}

static float3 get_root_velocity(float t0, float t1, float t, Curve *curve)
{
	const CurveRoot &root0 = curve->root0;
	const CurveRoot &root1 = curve->root1;
	
	if (t1 > t0) {
		return (root1.co - root0.co) / (t1 - t0);
	}
	else {
		return float3(0.0f, 0.0f, 0.0f);
	}
}

static void get_root_frame(float t0, float t1, float t, Curve *curve, float3 &normal, float3 &tangent)
{
	const CurveRoot &root0 = curve->root0;
	const CurveRoot &root1 = curve->root1;
	
	if (t1 > t0) {
		float x = (t - t0) / (t1 - t0);
		
		interp_v3v3_slerp(normal, root0.nor, root1.nor, x);
		interp_v3v3_slerp(tangent, root0.tan, root1.tan, x);
	}
	else {
		normal = root0.nor;
		tangent = root0.tan;
	}
}

/* XXX could cache rest_length in SolverData */

static float3 calc_stretch_force(const HairParams &params, const Point *point0, const Point *point1, float time)
{
	float rest_length = len_v3(point1->rest_co - point0->rest_co);
	float3 dir;
	float length = normalize_v3_v3(dir, point1->cur.co - point0->cur.co);
	
	return params.stretch_stiffness * (length - rest_length) * dir;
}

static float3 calc_stretch_damping(const HairParams &params, const Point *point0, const Point *point1)
{
	float3 dir;
	float3 edge = point1->cur.co - point0->cur.co;
	normalize_v3_v3(dir, edge);
	/* NB: for damping, use the updated next.vel velocities! */
	float3 dvel = point1->next.vel - point0->next.vel;
	
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

static float3 calc_bend_damping(const HairParams &params, const Point *point0, const Point *point1)
{
	float3 dir;
	float3 edge = point1->cur.co - point0->cur.co;
	normalize_v3_v3(dir, edge);
	/* NB: for damping, use the updated next.vel velocities! */
	float3 dvel = point1->next.vel - point0->next.vel;
	
	return params.bend_damping * (dvel - dot_v3v3(dvel, dir) * dir);
}

struct SolverTaskData {
	Curve *curves;
	Point *points;
	int startcurve, totcurves;
	int startpoint, totpoints;
	
	DebugThreadData *debug_data;
};

static void accum_internal_forces(const HairParams &params, const SolverForces &forces, float time, float timestep, const Point *point0, const Point *point1, const Frame &frame,
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

static void accum_external_forces(const HairParams &params, const SolverForces &forces, float time, float timestep, const Point *point0, const Point *point1, const Frame &frame,
                               float3 &force)
{
	float3 acc = float3(0.0f, 0.0f, 0.0f);
	
	float3 drag = - params.drag * point0->cur.vel;
	
	acc = acc + forces.gravity + drag;
	
	force = acc;
}

static void do_collision(const HairParams &params, const SolverForces &forces, float time, float timestep, float restitution_scale, const SolverTaskData &data, const PointContactCache &contacts)
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
			
			float3 dv_b = (params.margin - info.distance) * restitution_scale * info.world_normal_body - v0;
			
			float3 dv(max_ff(dv_a[0], dv_b[0]), max_ff(dv_a[1], dv_b[1]), max_ff(dv_a[2], dv_b[2]));
			point->force_accum = point->force_accum + dv * restitution_scale;
//			Debug::collision_contact(point->cur.co, point->cur.co + point->force_accum);
		}
	}
}

static void calc_forces(const HairParams &params, const SolverForces &forces, float time, float timestep, float restitution_scale, float t0, float t1,
                        const SolverTaskData &data, const PointContactCache &contacts)
{
	/* clear forces */
	for (int k = 0; k < data.totpoints; ++k) {
		data.points[k].force_accum = float3(0.0f, 0.0f, 0.0f);
	}
	
	Curve *curve = data.curves;
	for (int i = 0; i < data.totcurves; ++i, ++curve) {
		int numpoints = curve->totpoints;
		
		/* note: roots are evaluated at the end of the timestep: time + timestep
		 * so the hair points align perfectly with them
		 */
		float3 root_nor, root_tan;
		get_root_frame(t0, t1, time + timestep, curve, root_nor, root_tan);
		
		Frame rest_frame(root_nor, root_tan, cross_v3_v3(root_nor, root_tan));
		FrameIterator<SolverDataLocWalker> frame_iter(SolverDataLocWalker(curve), curve->avg_rest_length, params.bend_smoothing, rest_frame);
		
		float3 intern_force, intern_force_next, extern_force;
		float3 acc_prev; /* reactio from previous point */
		
		/* Root point animation */
		Point *point = curve->points;
		int k = 0;
		if (numpoints > 0) {
			Point *point_next = k < numpoints-1 ? point+1 : NULL;
			accum_internal_forces(params, forces, time, timestep, point, point_next, frame_iter.frame(), intern_force, intern_force_next);
			accum_external_forces(params, forces, time, timestep, point, point_next, frame_iter.frame(), extern_force);
			
			frame_iter.next();
			acc_prev = intern_force_next;
			++k;
			++point;
		}
		
		/* Integrate free points */
		for (; k < numpoints; ++k, ++point) {
			Point *point_next = k < numpoints-1 ? point+1 : NULL;
			accum_internal_forces(params, forces, time, timestep, point, point_next, frame_iter.frame(), intern_force, intern_force_next);
			accum_external_forces(params, forces, time, timestep, point, point_next, frame_iter.frame(), extern_force);
			
			point->force_accum = point->force_accum + intern_force + extern_force + acc_prev;
			
			frame_iter.next();
			acc_prev = intern_force_next;
		}
		
	}
	
	do_collision(params, forces, time, timestep, restitution_scale, data, contacts);
}

static void damping_forces(const HairParams &params, const Point *point0, const Point *point1,
                           float3 &force0, float3 &force1)
{
	float3 stretch, bend;
	
	stretch = calc_stretch_damping(params, point0, point1);
	bend = calc_bend_damping(params, point0, point1);
	
	force0 = stretch + bend;
	force1 = -(stretch + bend);
}

static void apply_acceleration(const SolverTaskData &data, float time, float timestep, float t0, float t1)
{
	Curve *curve = data.curves;
	for (int i = 0; i < data.totcurves; ++i, ++curve) {
		int totpoints = curve->totpoints;
		
		/* note: roots are evaluated at the end of the timestep: time + timestep
			 * so the hair points align perfectly with them
			 */
		float3 root_vel = get_root_velocity(t0, t1, time + timestep, curve);
		
		/* Root point animation */
		Point *point = curve->points;
		int k = 0;
		if (totpoints > 0) {
			curve->points->next.vel = root_vel;
			
			++k;
			++point;
		}
		
		/* Integrate free points */
		for (; k < totpoints; ++k, ++point) {
			/* semi-implicit Euler step */
			point->next.vel = point->cur.vel + point->force_accum * timestep;
		}
	}
}

static void apply_velocity(const SolverTaskData &data, float time, float timestep, float t0, float t1)
{
	Curve *curve = data.curves;
	for (int i = 0; i < data.totcurves; ++i, ++curve) {
		int totpoints = curve->totpoints;
		
		/* note: roots are evaluated at the end of the timestep: time + timestep
		 * so the hair points align perfectly with them
		 */
		float3 root_co = get_root_location(t0, t1, time + timestep, curve);
		
		/* Root point animation */
		Point *point = curve->points;
		int k = 0;
		if (totpoints > 0) {
			point->next.co = root_co;
			
			++k;
			++point;
		}
		
		/* Integrate free points */
		for (; k < totpoints; ++k, ++point) {
			/* semi-implicit Euler step */
			point->next.co = point->cur.co + point->next.vel * timestep;
		}
	}
}

static void do_damping(const HairParams &params, float time, float timestep, const SolverTaskData &data)
{
	const int totsteps = params.substeps_damping;
	float dt = timestep / (float)totsteps;
	
	for (int step = 0; step < totsteps; ++step) {
		Curve *curve = data.curves;
		for (int i = 0; i < data.totcurves; ++i, ++curve) {
			int totpoints = curve->totpoints;
			
			int k;
			Point *point;
			
			/* initialize force */
			for (k = 0, point = curve->points; k < totpoints; ++k, ++point) {
				point->force_accum = float3(0.0f, 0.0f, 0.0f);
			}
			
			/* accumulate forces */
			for (k = 0, point = curve->points; k < totpoints - 1; ++k, ++point) {
				float3 force, force_next;
				damping_forces(params, point, point + 1, force, force_next);
				
				point->force_accum = point->force_accum + force;
				(point+1)->force_accum = (point+1)->force_accum + force_next;
			}
			
			/* update velocities of free points */
			for (k = 1, point = curve->points + 1; k < totpoints - 1; ++k, ++point) {
				point->next.vel = point->next.vel + point->force_accum * timestep;
			}
		}
	}
	
	time += dt;
}

void Solver::do_integration(float time, float timestep, const SolverTaskData &data, const PointContactCache &contacts) const
{
	const int totsteps = m_params.substeps_forces; /* can have multiple integration steps per tick for accuracy */
	float dt = timestep / (float)totsteps;
	
	/* Note: the restitution scale is determined by the overall timestep,
	 * since collisions are only generated once during this interval.
	 * Otherwise restitution forces would be num_force_steps times too large
	 */
	float restitution_scale = 1.0f / timestep;
	
	for (int step = 0; step < totsteps; ++step) {
		
		/* calculate Point.force_accum vectors */
		calc_forces(m_params, m_forces, time, dt, restitution_scale, m_data->t0, m_data->t1, data, contacts);
		
		/* apply velocity change from accumulated forces */
		apply_acceleration(data, time, timestep, m_data->t0, m_data->t1);
		
		/* apply velocity change from viscous damping */
		do_damping(m_params, time, timestep, data);
		
		/* apply positional changes */
		apply_velocity(data, time, timestep, m_data->t0, m_data->t1);
		
		time += dt;
	}
}

struct SolverPoolData {
	const Solver *solver;
	float time, timestep;
	const PointContactCache *contacts;
};

static void step_threaded_func(TaskPool *pool, void *vtaskdata, int UNUSED(threadid))
{
	const SolverPoolData *pooldata = (const SolverPoolData *)BLI_task_pool_userdata(pool);
	SolverTaskData *taskdata = (SolverTaskData *)vtaskdata;
	pooldata->solver->do_integration(pooldata->time, pooldata->timestep, *taskdata, *pooldata->contacts);
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
	/* global debug data for the host thread */
	DebugThreadData *debug_data = NULL;
	if (debug_thread_data) {
		debug_thread_data->push_back(DebugThreadData());
		debug_data = &debug_thread_data->back();
	}
	
	/* filter and cache Bullet contact information */
	PointContactCache contacts;
	cache_point_contacts(contacts);
	
	if (debug_data) {
		for (int i = 0; i < contacts.size(); ++i) {
			const PointContactInfo &info = contacts[i];
			Debug::collision_contact(debug_data, info.world_point_hair, info.world_point_body);
		}
	}
	
	typedef std::vector<SolverTaskData> SolverTaskVector;
	
	const int max_points_per_task = 1024;
	const int max_hairs_per_task = 256;
	
	SolverPoolData pooldata;
	pooldata.solver = this;
	pooldata.time = time;
	pooldata.timestep = timestep;
	pooldata.contacts = &contacts;
	
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
			solver_task.debug_data = NULL; /* defined below if needed */
			taskdata.push_back(solver_task);
			
			hair_start += num_hairs;
			point_start += num_points;
			num_hairs = 0;
			num_points = 0;
		}
		
		++curve;
	}
	
	/* Note: have to create the debug data stack afterward to safely point to the std::vector */
	if (debug_thread_data) {
		/* reserve ensures no re-alloc happens and we can safely push and reference */
		debug_thread_data->reserve(debug_thread_data->size() + taskdata.size());
		for (SolverTaskVector::iterator it = taskdata.begin(); it != taskdata.end(); ++it) {
			SolverTaskData &solver_task = *it;
			debug_thread_data->push_back(DebugThreadData());
			solver_task.debug_data = &debug_thread_data->back();
		}
	}
	
	/* Note: can't create tasks right away in the loop above,
	 * because they reference data in a vector which can be altered afterward.
	 */
	for (SolverTaskVector::const_iterator it = taskdata.begin(); it != taskdata.end(); ++it) {
		const SolverTaskData &solver_task = *it;
		BLI_task_pool_push(task_pool, step_threaded_func, (void*)(&solver_task), false, TASK_PRIORITY_LOW);
	}
	
	/* work and wait until tasks are done */
	BLI_task_pool_work_and_wait(task_pool);
	
	BLI_task_pool_free(task_pool);
	
	advance_state(m_data);
}

HAIR_NAMESPACE_END

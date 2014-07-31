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
}

#include "HAIR_math.h"
#include "HAIR_smoothing.h"
#include "HAIR_solver.h"

HAIR_NAMESPACE_BEGIN

SolverData::SolverData() :
    curves(NULL),
    points(NULL),
    totcurves(0),
    totpoints(0)
{
}

SolverData::SolverData(int totcurves, int totpoints) :
    totcurves(totcurves),
    totpoints(totpoints)
{
	curves = new Curve[totcurves];
	points = new Point[totpoints];
}

SolverData::SolverData(const SolverData &other) :
    curves(other.curves),
    points(other.points),
    totcurves(other.totcurves),
    totpoints(other.totpoints)
{
}

SolverData::~SolverData()
{
	if (curves)
		delete[] curves;
	if (points)
		delete[] points;
}

static float3 calc_bend(const Frame &frame, const float3 &co0, const float3 &co1)
{
	float3 edge = co1 - co0;
	return float3(dot_v3_v3(edge, frame.normal), dot_v3_v3(edge, frame.tangent), dot_v3_v3(edge, frame.cotangent));
}

void SolverData::precompute_rest_bend()
{
	Curve *curve;
	int i;
	
	for (curve = curves, i = 0; i < totcurves; ++curve, ++i) {
		Point *pt0, *pt1;
		
		if (curve->totpoints >= 2) {
			pt0 = &curve->points[0];
			pt1 = &curve->points[1];
		}
		else if (curve->totpoints >= 1) {
			pt0 = pt1 = &curve->points[0];
		}
		else
			continue;
		
		FrameIterator iter(1.0f / curve->totpoints, 0.1f, curve->totpoints, pt0->rest_co, pt1->rest_co);
		
		pt0->rest_bend = calc_bend(iter.frame(), pt0->rest_co, pt1->rest_co);
		
		Point *pt = pt1;
		while (iter.valid()) {
			Point *next_pt = &curve->points[iter.cur()];
			
			pt->rest_bend = calc_bend(iter.frame(), pt->rest_co, next_pt->rest_co);
			
			iter.next(next_pt->rest_co);
			pt = next_pt;
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

static void calc_root_animation(float t0, float t1, float t, Curve *curve, float3 &co, float3 &vel)
{
	const CurveRoot &root0 = curve->root0;
	const CurveRoot &root1 = curve->root1;
	
	if (t1 > t0) {
		float x = (t - t0) / (t1 - t0);
		float mx = 1.0f - x;
		
		co = root0.co * mx + root1.co * x;
		vel = (root1.co - root0.co) / (t1 - t0);
	}
	else {
		co = root0.co;
		vel = float3(0.0f, 0.0f, 0.0f);
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
	float3 stretch_damp = params.stretch_damping * dot_v3_v3(dvel, dir) * dir;
	
	return stretch_force + stretch_damp;
}

static float3 calc_bend_force(const HairParams &params, Curve *curve, Point *point0, Point *point1, float time)
{
	float3 dir;
	float3 edge = point1->cur.co - point0->cur.co;
	normalize_v3_v3(dir, edge);
	float3 dvel = point1->cur.vel - point0->cur.vel;
	
	float3 bend_force = params.bend_stiffness * (edge - point0->rest_bend);
	float3 bend_damp = params.bend_damping * (dvel - dot_v3_v3(dvel, dir) * dir);
	
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
	int i, k;
	
	for (i = 0, curve = data.curves; i < totcurve; ++i, ++curve) {
		int numpoints = curve->totpoints;
		float3 stretch, prev_stretch, bend, prev_bend;
		
		/* Root point animation */
		k = 0;
		point = curve->points;
		
		/* note: roots are evaluated at the end of the timestep: time + timestep
		 * so the hair points align perfectly with them
		 */
		calc_root_animation(t0, t1, time + timestep, curve, point->next.co, point->next.vel);
		
		if (k < numpoints-1) {
			stretch = calc_stretch_force(params, curve, point, point+1, time);
			bend = calc_bend_force(params, curve, point, point+1, time);
		}
		else {
			stretch = float3(0.0f, 0.0f, 0.0f);
			bend = float3(0.0f, 0.0f, 0.0f);
		}
		prev_stretch = stretch;
		prev_bend = bend;
		
		/* Integrate the remaining free points */
		for (++k, ++point; k < numpoints; ++k, ++point) {
			if (k < numpoints-1) {
				stretch = calc_stretch_force(params, curve, point, point+1, time);
				bend = calc_bend_force(params, curve, point, point+1, time);
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

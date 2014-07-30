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


Solver::Solver(const HairParams &params) :
    m_params(params),
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

void Solver::calc_root_animation(float t0, float t1, float t, Curve *curve, float3 &co, float3 &vel) const
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

float3 Solver::calc_velocity(Curve *curve, Point *point, float time, Point::State &state) const
{
	return state.vel;
}

float3 Solver::calc_stretch_force(Curve *curve, Point *point0, Point *point1, float time) const
{
	/* XXX this could be cached in SolverData */
	float3 dir;
	float rest_length = len_v3(point1->rest_co - point0->rest_co);
	float length = normalize_v3_v3(dir, point1->cur.co - point0->cur.co);
	float3 dvel = point1->cur.vel - point0->cur.vel;
	
	float3 stretch_force = m_params.stretch_stiffness * (length - rest_length) * dir;
	float3 stretch_damp = m_params.stretch_damping * dot_v3_v3(dvel, dir) * dir;
	
	return stretch_force + stretch_damp;
}

#if 0
float3 Solver::calc_bend_force(Curve *curve, const Frame &frame, Point *point0, Point *point1, float time) const
{
	float3 bend = calc_bend(frame, point0->cur.co, point1->cur.co);
	
	
}
#endif

float3 Solver::calc_bend_force(Curve *curve, Point *point0, Point *point1, float time) const
{
	float3 dir;
	float3 edge = point1->cur.co - point0->cur.co;
	normalize_v3_v3(dir, edge);
	float3 dvel = point1->cur.vel - point0->cur.vel;
	
	float3 bend_force = m_params.bend_stiffness * (edge - point0->rest_bend);
	float3 bend_damp = m_params.bend_damping * (dvel - dot_v3_v3(dvel, dir) * dir);
	
	return bend_force + bend_damp;
}

float3 Solver::calc_acceleration(Curve *curve, Point *point, float time, Point::State &state) const
{
	float3 acc = float3(0.0f, 0.0f, 0.0f);
	
	acc = acc + m_forces.gravity;
	
	return acc;
}

void Solver::step(float time, float timestep)
{
	Curve *curve;
	Point *point;
	int totcurve = m_data->totcurves;
	/*int totpoint = m_data->totpoints;*/
	int i, k;
	
	for (i = 0, curve = m_data->curves; i < totcurve; ++i, ++curve) {
		int numpoints = curve->totpoints;
		float3 stretch, prev_stretch, bend, prev_bend;
		
		/* Root point animation */
		k = 0;
		point = curve->points;
		
		calc_root_animation(m_data->t0, m_data->t1, time, curve, point->next.co, point->next.vel);
		
		if (k < numpoints-1) {
			stretch = calc_stretch_force(curve, point, point+1, time);
			bend = calc_bend_force(curve, point, point+1, time);
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
				stretch = calc_stretch_force(curve, point, point+1, time);
				bend = calc_bend_force(curve, point, point+1, time);
			}
			else {
				stretch = float3(0.0f, 0.0f, 0.0f);
				bend = float3(0.0f, 0.0f, 0.0f);
			}
			
			float3 acc = calc_acceleration(curve, point, time, point->cur);
			acc = acc - prev_stretch + stretch - prev_bend + bend;
			point->next.vel = point->cur.vel + acc * timestep;
			
			float3 vel = calc_velocity(curve, point, time, point->next);
			point->next.co = point->cur.co + vel * timestep;
			
			prev_stretch = stretch;
			prev_bend = bend;
		}
	}
	
	/* advance state */
	for (i = 0, curve = m_data->curves; i < totcurve; ++i, ++curve) {
		int numpoints = curve->totpoints;
		for (k = 0, point = curve->points; k < numpoints; ++k, ++point) {
			point->cur = point->next;
		}
	}
}

HAIR_NAMESPACE_END

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
	float3 dir;
	normalize_v3_v3(dir, co1 - co0);
	return float3(dot_v3_v3(dir, frame.normal), dot_v3_v3(dir, frame.tangent), dot_v3_v3(dir, frame.cotangent));
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

void Solver::init_data(int totcurves, int totpoints)
{
	if (!m_data) {
		m_data = new SolverData(totcurves, totpoints);
	}
}

void Solver::prepare_data()
{
	if (!m_data)
		return;
	
	m_data->precompute_rest_bend();
}

void Solver::free_data()
{
	if (m_data) {
		delete m_data;
		m_data = NULL;
	}
}

float3 Solver::calc_velocity(Curve *curve, Point *point, float time, Point::State &state) const
{
	return state.vel;
}

float3 Solver::calc_stretch(Curve *curve, Point *point0, Point *point1, float time) const
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

float3 Solver::calc_acceleration(Curve *curve, Point *point, float time, float3 prev_stretch, float3 stretch, Point::State &state) const
{
	float3 acc = float3(0.0f, 0.0f, 0.0f);
	
	acc = acc + m_forces.gravity;
	
	acc = acc - prev_stretch + stretch;
	
	return acc;
}

void Solver::step(float timestep)
{
	Curve *curve;
	Point *point;
	int totcurve = m_data->totcurves;
	/*int totpoint = m_data->totpoints;*/
	int i, k;
	
	float time = 0.0f;
	
	for (i = 0, curve = m_data->curves; i < totcurve; ++i, ++curve) {
		int numpoints = curve->totpoints;
		float3 prev_stretch;
		
		/* Root point animation */
		k = 0;
		point = curve->points;
		
		// TODO actually evaluate animation here
		point->next.co = point->cur.co;
		point->next.vel = float3(0.0f, 0.0f, 0.0f);
		
		float3 stretch = k < numpoints-1 ? calc_stretch(curve, point, point+1, time) : float3(0.0f, 0.0f, 0.0f);
		prev_stretch = stretch;
		
		/* Integrate the remaining free points */
		for (++k, ++point; k < numpoints; ++k, ++point) {
			float3 stretch = k < numpoints-1 ? calc_stretch(curve, point, point+1, time) : float3(0.0f, 0.0f, 0.0f);
			
			float3 acc = calc_acceleration(curve, point, time, prev_stretch, stretch, point->cur);
			point->next.vel = point->cur.vel + acc * timestep;
			
			float3 vel = calc_velocity(curve, point, time, point->next);
			point->next.co = point->cur.co + vel * timestep;
			
			prev_stretch = stretch;
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

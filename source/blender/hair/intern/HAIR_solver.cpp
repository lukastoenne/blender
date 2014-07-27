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

float3 Solver::calc_acceleration(Curve *curve, Point *point, float time, Point::State &state) const
{
	return float3(0.0f, 0.0f, -0.01f);
}

void Solver::step(float timestep)
{
	Curve *curve;
	Point *point;
	int totcurve = m_data->totcurves;
	/*int totpoint = m_data->totpoints;*/
	int i, k;
	
	for (i = 0, curve = m_data->curves; i < totcurve; ++i, ++curve) {
		int numpoints = curve->totpoints;
		
		/* Root point animation */
		k = 0;
		point = curve->points;
		
		// TODO actually evaluate animation here
		point->next.co = point->cur.co;
		point->next.vel = float3(0.0f, 0.0f, 0.0f);
		
		/* Integrate the remaining free points */
		for (++k, ++point; k < numpoints; ++k, ++point) {
			float3 acc = calc_acceleration(curve, point, 0.0f, point->cur);
			point->next.vel = point->cur.vel + acc * timestep;
			
			float3 vel = calc_velocity(curve, point, 0.0f, point->next);
			point->next.co = point->cur.co + vel * timestep;
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

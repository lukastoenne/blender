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

extern "C" {
#include "BLI_math.h"

#include "DNA_hair_types.h"

#include "BKE_hair.h"
}

#include "HAIR_capi.h"

#include "HAIR_smoothing.h"
#include "HAIR_solver.h"
#include "HAIR_types.h"

using namespace HAIR_NAMESPACE;

struct HAIR_Solver *HAIR_solver_new(const struct HairParams *params)
{
	Solver *solver = new Solver(*params);
	
	return (HAIR_Solver *)solver;
}

void HAIR_solver_free(struct HAIR_Solver *csolver)
{
	Solver *solver = (Solver *)csolver;
	
	delete solver;
}

void HAIR_solver_init(struct HAIR_Solver *csolver, HairSystem *hsys)
{
	Solver *solver = (Solver *)csolver;
	HairCurve *hair;
	int i;
	
	/* count points */
	int totpoints = 0;
	for (hair = hsys->curves, i = 0; i < hsys->totcurves; ++hair, ++i) {
		totpoints += hair->totpoints;
	}
	
	/* allocate data */
	solver->init_data(hsys->totcurves, totpoints);
	Curve *solver_curves = solver->data()->curves;
	Point *solver_points = solver->data()->points;
	
	/* copy data to solver data */
	Point *point = solver_points;
	for (hair = hsys->curves, i = 0; i < hsys->totcurves; ++hair, ++i) {
		solver_curves[i] = Curve(hair->totpoints, point);
		
		for (int k = 0; k < hair->totpoints; ++k, ++point) {
			HairPoint *hair_pt = hair->points + k;
			bool is_root = (k == 0);
			
			*point = Point(hair_pt->rest_co, !is_root);
			point->cur.co = float3(hair_pt->co);
			point->cur.vel = float3(hair_pt->vel);
		}
	}
}

void HAIR_solver_step(struct HAIR_Solver *csolver, float timestep)
{
	Solver *solver = (Solver *)csolver;
	
	solver->step(timestep);
}

void HAIR_solver_apply(struct HAIR_Solver *csolver, HairSystem *hsys)
{
	Solver *solver = (Solver *)csolver;
	int i;
	
	Curve *solver_curves = solver->data()->curves;
	int totcurves = solver->data()->totcurves;
	
	/* copy solver data to DNA */
	for (i = 0; i < totcurves && i < hsys->totcurves; ++i) {
		Curve *curve = solver_curves + i;
		HairCurve *hcurve = hsys->curves + i;
		
		for (int k = 0; k < curve->totpoints && k < hcurve->totpoints; ++k) {
			Point *point = curve->points + k;
			HairPoint *hpoint = hcurve->points + k;
			
			copy_v3_v3(hpoint->co, point->cur.co.data());
			copy_v3_v3(hpoint->vel, point->cur.vel.data());
		}
	}
}


struct HAIR_SmoothingIteratorFloat3 *HAIR_smoothing_iter_new(HairCurve *curve, float rest_length, float amount, float cval[3])
{
	SmoothingIterator<float3> *iter = new SmoothingIterator<float3>(rest_length, amount);
	
	if (curve->totpoints >= 2) {
		float *co0 = curve->points[0].co;
		float *co1 = curve->points[1].co;
		
		float3 val = iter->begin(co0, co1);
		copy_v3_v3(cval, val.data());
	}
	/* XXX setting iter->num is not nice, find a better way to invalidate the iterator */
	else if (curve->totpoints >= 1) {
		copy_v3_v3(cval, curve->points[0].co);
		iter->num = 2; 
	}
	else
		iter->num = 1;
	
	return (struct HAIR_SmoothingIteratorFloat3 *)iter;
}

void HAIR_smoothing_iter_free(struct HAIR_SmoothingIteratorFloat3 *citer)
{
	SmoothingIterator<float3> *iter = (SmoothingIterator<float3> *)citer;
	
	delete iter;
}

bool HAIR_smoothing_iter_valid(HairCurve *curve, struct HAIR_SmoothingIteratorFloat3 *citer)
{
	SmoothingIterator<float3> *iter = (SmoothingIterator<float3> *)citer;
	
	return iter->num < curve->totpoints;
}

void HAIR_smoothing_iter_next(HairCurve *curve, struct HAIR_SmoothingIteratorFloat3 *citer, float cval[3])
{
	SmoothingIterator<float3> *iter = (SmoothingIterator<float3> *)citer;
	
	float *co = curve->points[iter->num].co;
	float3 val = iter->next(co);
	copy_v3_v3(cval, val.data());
}

void HAIR_smoothing_iter_end(HairCurve *curve, struct HAIR_SmoothingIteratorFloat3 *citer, float cval[3])
{
	SmoothingIterator<float3> *iter = (SmoothingIterator<float3> *)citer;
	
	float *co = curve->points[iter->num-1].co;
	float3 val = iter->next(co);
	copy_v3_v3(cval, val.data());
}

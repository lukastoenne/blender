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
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_hair.h"
}

#include "HAIR_capi.h"

#include "HAIR_scene.h"
#include "HAIR_smoothing.h"
#include "HAIR_solver.h"
#include "HAIR_types.h"

using namespace HAIR_NAMESPACE;

struct HAIR_Solver *HAIR_solver_new(void)
{
	Solver *solver = new Solver();
	
	return (HAIR_Solver *)solver;
}

void HAIR_solver_free(struct HAIR_Solver *csolver)
{
	Solver *solver = (Solver *)csolver;
	
	delete solver;
}

void HAIR_solver_set_params(struct HAIR_Solver *csolver, const struct HairParams *params)
{
	Solver *solver = (Solver *)csolver;
	
	solver->params(*params);
}

void HAIR_solver_build_data(struct HAIR_Solver *csolver, Scene *scene, Object *ob, DerivedMesh *dm, HairSystem *hsys, float time)
{
	Solver *solver = (Solver *)csolver;
	
	SolverData *data = SceneConverter::build_solver_data(scene, ob, dm, hsys, time);
	solver->set_data(data);
}

void HAIR_solver_update_externals(struct HAIR_Solver *csolver, Scene *scene, Object *ob, DerivedMesh *dm, HairSystem *hsys, float time)
{
	Solver *solver = (Solver *)csolver;
	
	SceneConverter::update_solver_data_externals(solver->data(), solver->forces(), scene, ob, dm, hsys, time);
}

void HAIR_solver_step(struct HAIR_Solver *csolver, float time, float timestep)
{
	Solver *solver = (Solver *)csolver;
	
	solver->step_threaded(time, timestep);
}

void HAIR_solver_apply(struct HAIR_Solver *csolver, Scene *scene, Object *ob, HairSystem *hsys)
{
	Solver *solver = (Solver *)csolver;
	
	SceneConverter::apply_solver_data(solver->data(), scene, ob, hsys);
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


struct HAIR_FrameIterator *HAIR_frame_iter_new(HairCurve *curve, float rest_length, float amount, float cnor[3], float ctan[3], float ccotan[3])
{
	FrameIterator *iter;
	float3 co0, co1;
	
	if (curve->totpoints >= 2) {
		co0 = curve->points[0].co;
		co1 = curve->points[1].co;
	}
	else if (curve->totpoints >= 1) {
		co0 = co1 = curve->points[0].co;
	}
	else {
		static const float3 v(0.0f, 0.0f, 0.0f);
		co0 = co1 = v;
	}
	
	iter = new FrameIterator(rest_length, amount, curve->totpoints, co0, co1);
	copy_v3_v3(cnor, iter->frame().normal.data());
	copy_v3_v3(ctan, iter->frame().tangent.data());
	copy_v3_v3(ccotan, iter->frame().cotangent.data());
	
	return (struct HAIR_FrameIterator *)iter;
}

void HAIR_frame_iter_free(struct HAIR_FrameIterator *citer)
{
	FrameIterator *iter = (FrameIterator *)citer;
	
	delete iter;
}

bool HAIR_frame_iter_valid(HairCurve *curve, struct HAIR_FrameIterator *citer)
{
	FrameIterator *iter = (FrameIterator *)citer;
	
	return iter->valid();
}

void HAIR_frame_iter_next(HairCurve *curve, struct HAIR_FrameIterator *citer, float cnor[3], float ctan[3], float ccotan[3])
{
	FrameIterator *iter = (FrameIterator *)citer;
	
	float *co = curve->points[iter->cur()].co;
	iter->next(co);
	copy_v3_v3(cnor, iter->frame().normal.data());
	copy_v3_v3(ctan, iter->frame().tangent.data());
	copy_v3_v3(ccotan, iter->frame().cotangent.data());
}

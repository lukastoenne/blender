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
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"

#include "BKE_hair.h"
}

#include "HAIR_capi.h"

#include "HAIR_debug.h"
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
	RigidBodyWorld *rbw = scene->rigidbody_world;
	rbDynamicsWorld *world = rbw ? (rbDynamicsWorld *)rbw->physics_world : NULL;
	
	if (solver->data())
		solver->data()->remove_from_world(world);
	
	SolverData *data = SceneConverter::build_solver_data(scene, ob, dm, hsys, time);
	solver->set_data(data);
	
	// XXX col_groups ?
	data->add_to_world(world, 0xFFFFFFFF);
}

void HAIR_solver_update_externals(struct HAIR_Solver *csolver, Scene *scene, Object *ob, DerivedMesh *dm, HairSystem *hsys, float time)
{
	Solver *solver = (Solver *)csolver;
	
	SceneConverter::update_solver_data_externals(solver->data(), solver->forces(), scene, ob, dm, hsys, time);
	SceneConverter::sync_rigidbody_data(solver->data());
}

void HAIR_solver_rebuild_rigidbodyworld(struct HAIR_Solver *csolver, struct rbDynamicsWorld *world)
{
	Solver *solver = (Solver *)csolver;
	
	if (solver->data())
		solver->data()->add_to_world(world, 0xFFFFFFFF);
}

void HAIR_solver_step(struct HAIR_Solver *csolver, float time, float timestep)
{
	Solver *solver = (Solver *)csolver;
	
	solver->step_threaded(time, timestep);
}

void HAIR_solver_step_debug(struct HAIR_Solver *csolver, float time, float timestep, HAIR_SolverContact **contacts, int *totcontacts)
{
	Solver *solver = (Solver *)csolver;
	
	Debug::CollisionContacts dbg_contacts;
	Debug::set_collision_contacts(&dbg_contacts);
	
	solver->step_threaded(time, timestep);
	
	Debug::set_collision_contacts(NULL);
	*totcontacts = dbg_contacts.size();
	*contacts = (HAIR_SolverContact *)MEM_mallocN(sizeof(HAIR_SolverContact) * dbg_contacts.size(), "hair solver contact debug data");
	for (int i = 0; i < dbg_contacts.size(); ++i) {
		HAIR_SolverContact *c = (*contacts) + i;
		copy_v3_v3(c->coA, dbg_contacts[i].coA.data());
		copy_v3_v3(c->coB, dbg_contacts[i].coB.data());
	}
}

void HAIR_solver_apply(struct HAIR_Solver *csolver, Scene *scene, Object *ob, HairSystem *hsys)
{
	Solver *solver = (Solver *)csolver;
	
	SceneConverter::apply_solver_data(solver->data(), scene, ob, hsys);
}


struct HairCurveWalker {
	typedef float3 data_t;
	
	HairCurveWalker(HairCurve *curve) :
	    curve(curve),
	    i(0)
	{}
	
	float3 read()
	{
		float3 result(curve->points[i].co);
		if (i < curve->totpoints-1)
			++i;
		return result;
	}
	
	int size() const { return curve->totpoints; }
	
	HairCurve *curve;
	int i;
};

typedef SmoothingIterator<HairCurveWalker> HairCurveSmoothingIterator;
typedef FrameIterator<HairCurveWalker> HairCurveFrameIterator;

struct HAIR_SmoothingIteratorFloat3 *HAIR_smoothing_iter_new(HairCurve *curve, float rest_length, float amount)
{
	HairCurveSmoothingIterator *iter = new HairCurveSmoothingIterator(HairCurveWalker(curve), rest_length, amount);
	
	return (struct HAIR_SmoothingIteratorFloat3 *)iter;
}

void HAIR_smoothing_iter_free(struct HAIR_SmoothingIteratorFloat3 *citer)
{
	HairCurveSmoothingIterator *iter = (HairCurveSmoothingIterator *)citer;
	
	delete iter;
}

bool HAIR_smoothing_iter_valid(struct HAIR_SmoothingIteratorFloat3 *citer)
{
	HairCurveSmoothingIterator *iter = (HairCurveSmoothingIterator *)citer;
	
	return iter->valid();
}

void HAIR_smoothing_iter_get(struct HAIR_SmoothingIteratorFloat3 *citer, float cval[3])
{
	HairCurveSmoothingIterator *iter = (HairCurveSmoothingIterator *)citer;
	
	float3 val = iter->get();
	copy_v3_v3(cval, val.data());
}

void HAIR_smoothing_iter_next(struct HAIR_SmoothingIteratorFloat3 *citer)
{
	HairCurveSmoothingIterator *iter = (HairCurveSmoothingIterator *)citer;
	
	iter->next();
}

struct HAIR_FrameIterator *HAIR_frame_iter_new(HairCurve *curve, float rest_length, float amount)
{
	HairCurveFrameIterator *iter = new HairCurveFrameIterator(HairCurveWalker(curve), rest_length, amount);
	
	return (struct HAIR_FrameIterator *)iter;
}

void HAIR_frame_iter_free(struct HAIR_FrameIterator *citer)
{
	HairCurveFrameIterator *iter = (HairCurveFrameIterator *)citer;
	
	delete iter;
}

bool HAIR_frame_iter_valid(struct HAIR_FrameIterator *citer)
{
	HairCurveFrameIterator *iter = (HairCurveFrameIterator *)citer;
	
	return iter->valid();
}

void HAIR_frame_iter_get(struct HAIR_FrameIterator *citer, float cnor[3], float ctan[3], float ccotan[3])
{
	HairCurveFrameIterator *iter = (HairCurveFrameIterator *)citer;
	
	copy_v3_v3(cnor, iter->frame().normal.data());
	copy_v3_v3(ctan, iter->frame().tangent.data());
	copy_v3_v3(ccotan, iter->frame().cotangent.data());
}

void HAIR_frame_iter_next(struct HAIR_FrameIterator *citer)
{
	HairCurveFrameIterator *iter = (HairCurveFrameIterator *)citer;
	
	iter->next();
}

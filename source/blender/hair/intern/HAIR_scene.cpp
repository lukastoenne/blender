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
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_rigidbody_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_mesh_sample.h"

#include "RBI_api.h"
}

#include "HAIR_curve.h"
#include "HAIR_math.h"
#include "HAIR_memalloc.h"
#include "HAIR_scene.h"
#include "HAIR_solver.h"

HAIR_NAMESPACE_BEGIN

static bool mesh_sample_eval(DerivedMesh *dm, const Transform &tfm, MSurfaceSample *sample, float3 &loc, float3 &nor)
{
	float vloc[3], vnor[3];
	
	bool ok = BKE_mesh_sample_eval(dm, sample, vloc, vnor);
	loc = transform_point(tfm, vloc);
	nor = transform_direction(tfm, vnor);
	
	return ok;
}

SolverData *SceneConverter::build_solver_data(Scene *scene, Object *ob, DerivedMesh *dm, HairSystem *hsys, float time)
{
	HairCurve *hair;
	int i;
	
	if (!dm)
		return new SolverData(0, 0);
	
	Transform mat = Transform(ob->obmat);
	
	/* count points */
	int totpoints = 0;
	for (hair = hsys->curves, i = 0; i < hsys->totcurves; ++hair, ++i) {
		totpoints += hair->totpoints;
	}
	
	/* allocate data */
	SolverData *data = new SolverData(hsys->totcurves, totpoints);
	Curve *solver_curves = data->curves;
	Point *solver_points = data->points;
	
	data->t0 = data->t1 = time;
	
	/* copy scene data to solver data */
	Point *point = solver_points;
	for (hair = hsys->curves, i = 0; i < hsys->totcurves; ++hair, ++i) {
		Curve *curve = solver_curves + i;
		*curve = Curve(hair->totpoints, point);
		
		mesh_sample_eval(dm, mat, &hair->root, curve->root1.co, curve->root1.nor);
		curve->root0 = curve->root1;
		
		for (int k = 0; k < hair->totpoints; ++k, ++point) {
			HairPoint *hair_pt = hair->points + k;
			
			point->rest_co = transform_point(mat, hair_pt->rest_co);
			point->cur.co = transform_point(mat, hair_pt->co);
			point->cur.vel = transform_direction(mat, hair_pt->vel);
		}
	}
	
	/* finalize */
	data->precompute_rest_bend();
	
	return data;
}

void SceneConverter::update_solver_data_externals(SolverData *data, SolverForces &forces, Scene *scene, Object *ob, DerivedMesh *dm, HairSystem *hsys, float time)
{
	int i;
	
	Transform mat = Transform(ob->obmat);
	
	Curve *solver_curves = data->curves;
	int totcurves = data->totcurves;
	
	data->t0 = data->t1;
	data->t1 = time;
	
	for (i = 0; i < totcurves; ++i) {
		HairCurve *hcurve = hsys->curves + i;
		Curve *curve = solver_curves + i;
		
		curve->root0 = curve->root1;
		mesh_sample_eval(dm, mat, &hcurve->root, curve->root1.co, curve->root1.nor);
	}
	
	forces.dynamics_world = scene->rigidbody_world ? (rbDynamicsWorld *)scene->rigidbody_world->physics_world : NULL;
	forces.gravity = float3(scene->physics_settings.gravity);
}

void SceneConverter::apply_solver_data(SolverData *data, Scene *scene, Object *ob, HairSystem *hsys)
{
	int i;
	
	Transform imat = transform_inverse(Transform(ob->obmat));
	
	Curve *solver_curves = data->curves;
	int totcurves = data->totcurves;
	
	/* copy solver data to DNA */
	for (i = 0; i < totcurves && i < hsys->totcurves; ++i) {
		Curve *curve = solver_curves + i;
		HairCurve *hcurve = hsys->curves + i;
		
		for (int k = 0; k < curve->totpoints && k < hcurve->totpoints; ++k) {
			Point *point = curve->points + k;
			HairPoint *hpoint = hcurve->points + k;
			
			copy_v3_v3(hpoint->co, transform_point(imat, point->cur.co).data());
			copy_v3_v3(hpoint->vel, transform_direction(imat, point->cur.vel).data());
		}
	}
}

void SceneConverter::sync_rigidbody_data(SolverData *data)
{
	/* sync settings */
	Curve *solver_curves = data->curves;
	int totcurves = data->totcurves;
	
	for (int i = 0; i < totcurves && i < totcurves; ++i) {
		Curve *curve = solver_curves + i;
		
		for (int k = 0; k < curve->totpoints; ++k) {
			Point *point = curve->points + k;
			
			RB_ghost_set_loc_rot(&point->rb_ghost, point->cur.co.data(), unit_qt.data());
		}
	}
}

HAIR_NAMESPACE_END

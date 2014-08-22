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
#include "DNA_particle_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_mesh_sample.h"
#include "BKE_particle.h"

#include "RBI_api.h"
}

#include "HAIR_curve.h"
#include "HAIR_math.h"
#include "HAIR_memalloc.h"
#include "HAIR_scene.h"
#include "HAIR_solver.h"

HAIR_NAMESPACE_BEGIN

static bool mesh_sample_eval_transformed(DerivedMesh *dm, const Transform &tfm, MSurfaceSample *sample, float3 &loc, float3 &nor)
{
	float vloc[3], vnor[3];
	
	bool ok = BKE_mesh_sample_eval(dm, sample, vloc, vnor);
	loc = transform_point(tfm, vloc);
	nor = transform_direction(tfm, vnor);
	
	return ok;
}

SolverData *SceneConverter::build_solver_data(Scene *scene, Object *ob, DerivedMesh *dm, HairSystem *hsys, float time)
{
	HairParams &params = hsys->params;
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
		
		mesh_sample_eval_transformed(dm, mat, &hair->root, curve->root1.co, curve->root1.nor);
		normalize_v3_v3(curve->root1.tan, float3(0,0,1) - dot_v3v3(float3(0,0,1), curve->root1.nor) * curve->root1.nor);
		
		curve->root0 = curve->root1;
		
		curve->avg_rest_length = hair->avg_rest_length;
		curve->rest_root_normal = float3(hair->rest_nor);
		curve->rest_root_tangent = float3(hair->rest_tan);
		curve->rest_root_normal = transform_direction(mat, curve->rest_root_normal);
		curve->rest_root_tangent = transform_direction(mat, curve->rest_root_tangent);
		
		for (int k = 0; k < hair->totpoints; ++k, ++point) {
			HairPoint *hair_pt = hair->points + k;
			
			point->rest_co = transform_point(mat, hair_pt->rest_co);
			point->radius = hair_pt->radius;
			
			point->cur.co = transform_point(mat, hair_pt->co);
			point->cur.vel = transform_direction(mat, hair_pt->vel);
		}
	}
	
	/* finalize */
	data->precompute_rest_bend(params);
	
	return data;
}


static bool solver_evaluate_root_location(ParticleSystem *psys, ParticleData *pa, struct DerivedMesh *dm, float3 &co, float3 &no)
{
	float mapfw[4];
	int mapindex;
	MVert *mverts = dm->getVertArray(dm), *v1, *v2, *v3;
	MFace *mface;
	float *co1 = NULL, *co2 = NULL, *co3 = NULL, *co4 = NULL;
	float vec[3], vnor[3];
	float w[4];
	unsigned int orig_verts[3];
	unsigned int totverts = (unsigned int)dm->getNumVerts(dm);
	
	if (!psys_get_index_on_dm(psys, dm, pa, &mapindex, mapfw))
		return false;
	
	mface = (MFace *)dm->getTessFaceData(dm, mapindex, CD_MFACE);
	mverts = (MVert *)dm->getVertDataArray(dm, CD_MVERT);

	co1 = mverts[mface->v1].co;
	co2 = mverts[mface->v2].co;
	co3 = mverts[mface->v3].co;

	if (mface->v4) {
		co4 = mverts[mface->v4].co;
		
		interp_v3_v3v3v3v3(vec, co1, co2, co3, co4, mapfw);
	}
	else {
		interp_v3_v3v3v3(vec, co1, co2, co3, mapfw);
	}
	
	/* test both triangles of the face */
	interp_weights_face_v3(w, co1, co2, co3, NULL, vec);
	
	if (w[0] <= 1.0f && w[1] <= 1.0f && w[2] <= 1.0f) {
		orig_verts[0] = mface->v1;
		orig_verts[1] = mface->v2;
		orig_verts[2] = mface->v3;	
	}
	else if (mface->v4) {
		interp_weights_face_v3(w, co3, co4, co1, NULL, vec);
		orig_verts[0] = mface->v3;
		orig_verts[1] = mface->v4;
		orig_verts[2] = mface->v1;
	}
	else
		return false;
	
	zero_v3(co.data());
	zero_v3(no.data());
	
	if (orig_verts[0] >= totverts ||
	    orig_verts[1] >= totverts ||
	    orig_verts[2] >= totverts)
		return false;
	
	v1 = &mverts[orig_verts[0]];
	v2 = &mverts[orig_verts[1]];
	v3 = &mverts[orig_verts[2]];
	
	madd_v3_v3fl(co.data(), v1->co, w[0]);
	madd_v3_v3fl(co.data(), v2->co, w[1]);
	madd_v3_v3fl(co.data(), v3->co, w[2]);
	
	normal_short_to_float_v3(vnor, v1->no);
	madd_v3_v3fl(no.data(), vnor, w[0]);
	normal_short_to_float_v3(vnor, v2->no);
	madd_v3_v3fl(no.data(), vnor, w[1]);
	normal_short_to_float_v3(vnor, v3->no);
	madd_v3_v3fl(no.data(), vnor, w[2]);
	
	normalize_v3(no.data());
	
	return true;
}


SolverData *SceneConverter::build_solver_data(Scene *scene, Object *ob, DerivedMesh *dm, ParticleSystem *psys, float time)
{
	HairParams *params = psys->params;
	ParticleData *pa;
	int i, k;
	float hairmat[4][4];
	const float seglen_to_radius = 2.0f / 3.0f;	
	
	if (!dm)
		return new SolverData(0, 0);
	
	Transform mat = Transform(ob->obmat);
	

	/* count points */
	int totpoints = 0;
	for (pa = psys->particles, i = 0; i < psys->totpart; ++pa, ++i) {
		totpoints += pa->totkey;
	}
	
	/* allocate data */
	SolverData *data = new SolverData(psys->totpart, totpoints);
	Curve *solver_curves = data->curves;
	Point *solver_points = data->points;
	
	data->t0 = data->t1 = time;
	
	/* copy scene data to solver data */
	Point *point = solver_points;
	for (pa = psys->particles, i = 0; i < psys->totpart; ++pa, ++i) {
		Transform finalmat;
		float radius = 0.0f;
		float len_accum = 0.0f;
		
		int totkey = pa->totkey;
		Curve *curve = solver_curves + i;
		*curve = Curve(totkey, point);

		psys_mat_hair_to_object(ob, dm, psys->part->from, pa, hairmat);
		
		Transform hair_tr(hairmat);
		
		/* we are transforming hairs to world space for the solver to work 
		 * on a frame of reference without pseudoforces */
		finalmat = mat * hair_tr;
		
		/* evaluate if particle is on surface. If not, discard */
		if (!solver_evaluate_root_location(psys, pa, dm, curve->root1.co, curve->root1.nor)) {
			point += totkey;
			continue;
		}
		
		/* send to world space (normal matrix should be changed to inverse transpose here) */
		transform_point(mat, curve->root1.co);
		transform_direction(mat, curve->root1.nor);
		
		//mesh_sample_eval_transformed(dm, mat, &hair->root, curve->root1.co, curve->root1.nor);
		normalize_v3_v3(curve->root1.tan, float3(0,0,1) - dot_v3v3(float3(0,0,1), curve->root1.nor) * curve->root1.nor);
		
		curve->root0 = curve->root1;

		for (k = 0; k < totkey; ++k, ++point) {
			HairKey *psys_hair_key = pa->hair + k;
			float len = 0.0f;
			float prev_radius = radius;
			
			point->rest_co = transform_point(finalmat, psys_hair_key->co);
			/* Not optimal, but we don't really have a way to store intermediate results yet */
			point->cur.co = point->rest_co;
			point->cur.vel = float3(0.0f, 0.0f, 0.0f);
			
			if (k < totkey-1) {
				len = len_v3v3(psys_hair_key->co, (psys_hair_key+1)->co);
				radius = seglen_to_radius * len;
				
				len_accum += len;
			}
			
			if (k == 0) {
				point->radius = radius;
			}
			else {
				point->radius = 0.5f * (radius + prev_radius);
			}
		}
		
		if (totkey > 1) {
			len_accum /= (totkey - 1);
		}
		
		curve->avg_rest_length = len_accum;
		curve->rest_root_normal = curve->root1.nor;
		curve->rest_root_tangent = curve->root1.tan;
		
		/*
		for (int k = 0; k < hair->totpoints; ++k, ++point) {
			HairPoint *hair_pt = hair->points + k;
			
			point->rest_co = transform_point(mat, hair_pt->rest_co);
			point->radius = hair_pt->radius;
			
			point->cur.co = transform_point(mat, hair_pt->co);
			point->cur.vel = transform_direction(mat, hair_pt->vel);
		}
		*/
	}
	/* finalize */
	data->precompute_rest_bend(*params);
	
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
		HairCurve *hair = hsys->curves + i;
		Curve *curve = solver_curves + i;
		
		curve->root0 = curve->root1;
		
		mesh_sample_eval_transformed(dm, mat, &hair->root, curve->root1.co, curve->root1.nor);
		normalize_v3_v3(curve->root1.tan, float3(0,0,1) - dot_v3v3(float3(0,0,1), curve->root1.nor) * curve->root1.nor);
	}
	
	forces.dynamics_world = scene->rigidbody_world ? (rbDynamicsWorld *)scene->rigidbody_world->physics_world : NULL;
	forces.gravity = float3(scene->physics_settings.gravity);
}

void SceneConverter::update_solver_data_externals(SolverData *data, SolverForces &forces, Scene *scene, Object *ob, DerivedMesh *dm, ParticleSystem *psys, float time)
{
	int i;
	
	Transform mat = Transform(ob->obmat);
	
	Curve *solver_curves = data->curves;
	int totcurves = data->totcurves;
	
	data->t0 = data->t1;
	data->t1 = time;
	
	for (i = 0; i < totcurves; ++i) {
		ParticleData *pa = psys->particles + i;
		Curve *curve = solver_curves + i;
		
		curve->root0 = curve->root1;

		if (!solver_evaluate_root_location(psys, pa, dm, curve->root1.co, curve->root1.nor)) {
			continue;
		}

		/* send to world space (normal matrix should be changed to inverse transpose here) */
		transform_point(mat, curve->root1.co);
		transform_direction(mat, curve->root1.nor);
		
		normalize_v3_v3(curve->root1.tan, float3(0,0,1) - dot_v3v3(float3(0,0,1), curve->root1.nor) * curve->root1.nor);
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

void SceneConverter::apply_solver_data(SolverData *data, ParticleSystem *psys, float (*vertCoords)[3])
{
	int i, k, pc = 0;
	
	for (i = 0; i < data->totcurves; i++) {
		int part_pc = pc;
		ParticleData *pa = psys->particles + i;
		
		for (k = 0; k < pa->totkey; k++) {
			copy_v3_v3(vertCoords[part_pc + i + k + 1], (data->points + pc)->cur.co.data());
			pc++;
		}
		
		copy_v3_v3(vertCoords[part_pc + i], (data->points + part_pc)->cur.co.data());
	}
}


void SceneConverter::sync_rigidbody_data(SolverData *data, const HairParams &params)
{
	/* sync settings */
	Curve *solver_curves = data->curves;
	int totcurves = data->totcurves;
	btTransform trans;
	btVector3 halfsize;
	
	data->rb_ghost.ghost.setRestitution(params.restitution);
	data->rb_ghost.ghost.setFriction(params.friction);
	
	if (data->totpoints == 0) {
		trans.setIdentity();
		halfsize = btVector3(0.5f, 0.5f, 0.5f);
	}
	else {
		float3 co_min(FLT_MAX, FLT_MAX, FLT_MAX);
		float3 co_max(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		
		for (int i = 0; i < totcurves && i < totcurves; ++i) {
			Curve *curve = solver_curves + i;
			
			for (int k = 0; k < curve->totpoints; ++k) {
				Point *point = curve->points + k;
				
				co_min[0] = min_ff(co_min[0], point->cur.co.x - point->radius);
				co_min[1] = min_ff(co_min[1], point->cur.co.y - point->radius);
				co_min[2] = min_ff(co_min[2], point->cur.co.z - point->radius);
				co_max[0] = max_ff(co_max[0], point->cur.co.x + point->radius);
				co_max[1] = max_ff(co_max[1], point->cur.co.y + point->radius);
				co_max[2] = max_ff(co_max[2], point->cur.co.z + point->radius);
				
				RB_ghost_set_loc_rot(&point->rb_ghost, point->cur.co.data(), unit_qt.data());
				
				point->rb_ghost.ghost.setRestitution(params.restitution);
				point->rb_ghost.ghost.setFriction(params.friction);
				
				point->bt_shape.setUnscaledRadius(point->radius);
			}
		}
		
		trans.setIdentity();
		trans.setOrigin(btVector3(0.5f*(co_min[0] + co_max[0]), 0.5f*(co_min[1] + co_max[1]), 0.5f*(co_min[2] + co_max[2])));
		halfsize = btVector3(0.5f*(co_max[0] - co_min[0]), 0.5f*(co_max[1] - co_min[1]), 0.5f*(co_max[2] - co_min[2]));
	}
	
	data->rb_ghost.ghost.setWorldTransform(trans);
	data->bt_shape.setLocalScaling(halfsize);
}

HAIR_NAMESPACE_END

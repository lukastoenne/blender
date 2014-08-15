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
 * The Original Code is: all of this file.
 *
 * Contributor(s): Blender Foundation,
 *                 Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/physics/hair_ops.c
 *  \ingroup edphys
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_hair_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"

#include "BKE_context.h"
#include "BKE_DerivedMesh.h"
#include "BKE_hair.h"
#include "BKE_mesh_sample.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_report.h"

#include "HAIR_capi.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_screen.h"

#include "WM_types.h"
#include "WM_api.h"

#include "physics_intern.h" // own include

static bool ED_hair_get(bContext *C, Object **r_ob, HairSystem **r_hsys, HairModifierData **r_hmd)
{
	Object *ob;
	HairModifierData *hmd;
	
	ob = CTX_data_active_object(C);
	if (!ob)
		return false;
	
	hmd = (HairModifierData *)modifiers_findByType(ob, eModifierType_Hair);
	if (!hmd)
		return false;
	
	if (r_ob) *r_ob = ob;
	if (r_hsys) *r_hsys = hmd->hairsys;
	if (r_hmd) *r_hmd = hmd;
	return true;
}

static int ED_hair_active_poll(bContext *C)
{
	return ED_hair_get(C, NULL, NULL, NULL);
}

/************************ reset hair to rest position *********************/

static int hair_reset_to_rest_location_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = NULL;
	HairSystem *hsys = NULL;
	HairModifierData *hmd = NULL;
	int i, k;
	ED_hair_get(C, &ob, &hsys, &hmd);
	
	for (i = 0; i < hsys->totcurves; ++i) {
		HairCurve *hair = &hsys->curves[i];
		
		for (k = 0; k < hair->totpoints; ++k) {
			HairPoint *point = &hair->points[k];
			
			copy_v3_v3(point->co, point->rest_co);
			zero_v3(point->vel);
		}
	}
	
	hmd->flag &= ~MOD_HAIR_SOLVER_DATA_VALID;
	
	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	return OPERATOR_FINISHED;
}

void HAIR_OT_reset_to_rest_location(wmOperatorType *ot)
{
	/* identifiers */
	ot->idname = "HAIR_OT_reset to rest location";
	ot->name = "Reset to Rest Location";
	ot->description = "Reset hair data to the rest location";

	/* callbacks */
	ot->exec = hair_reset_to_rest_location_exec;
	ot->poll = ED_hair_active_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ copy hair data from old particles *********************/

static bool hair_copy_particle_emitter_location(Object *UNUSED(ob), ParticleSystem *psys, ParticleData *pa, struct DerivedMesh *dm, MSurfaceSample *result)
{
	float mapfw[4];
	int mapindex;
	MVert *mverts = dm->getVertArray(dm);
	MFace *mface;
	float *co1 = NULL, *co2 = NULL, *co3 = NULL, *co4 = NULL;
	float vec[3];
	float w[4];
	
	if (!psys_get_index_on_dm(psys, dm, pa, &mapindex, mapfw))
		return false;
	
	mface = dm->getTessFaceData(dm, mapindex, CD_MFACE);
	mverts = dm->getVertDataArray(dm, CD_MVERT);

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
		result->orig_verts[0] = mface->v1;
		result->orig_verts[1] = mface->v2;
		result->orig_verts[2] = mface->v3;
		
		copy_v3_v3(result->orig_weights, w);
		return true;
	}
	else if (mface->v4) {
		interp_weights_face_v3(w, co3, co4, co1, NULL, vec);
		result->orig_verts[0] = mface->v3;
		result->orig_verts[1] = mface->v4;
		result->orig_verts[2] = mface->v1;
		
		copy_v3_v3(result->orig_weights, w);
		return true;
	}
	else
		return false;
}

static void hair_copy_root(Object *ob, HairSystem *UNUSED(hsys), ParticleSystem *psys, struct DerivedMesh *dm, HairCurve *hair, int index)
{
	ParticleData *root = &psys->particles[index];
	float loc[3], tan[3];
	
	hair_copy_particle_emitter_location(ob, psys, root, dm, &hair->root);
	BKE_mesh_sample_eval(dm, &hair->root, loc, hair->rest_nor);
	tan[0] = 0.0f; tan[1] = 0.0f; tan[2] = 1.0f;
	madd_v3_v3v3fl(hair->rest_tan, tan, hair->rest_nor, -dot_v3v3(tan, hair->rest_nor));
	normalize_v3(hair->rest_tan);
}

#if 0
/* cached particle data */
static void hair_copy_data(Object *ob, HairSystem *hsys, ParticleSystem *psys, struct DerivedMesh *dm, float mat[4][4], HairCurve *hair, int index)
{
	/* scale of segment lengths to get point radius */
	const float seglen_to_radius = 2.0f / 3.0f;
	
	ParticleCacheKey *pa_cache = psys->pathcache[index];
	HairPoint *points;
	int totpoints;
	float radius;
	int k;
	
	if (pa_cache->steps == 0)
		return;
	
	totpoints = pa_cache->steps + 1;
	points = BKE_hair_point_append_multi(hsys, hair, totpoints);
	
	hair_copy_root(ob, hsys, psys, dm, hair, index);
	
	radius = 0.0f;
	for (k = 0; k < totpoints; ++k) {
		ParticleCacheKey *pa_key = pa_cache + k;
		HairPoint *point = points + k;
		
		mul_v3_m4v3(point->rest_co, mat, pa_key->co);
		/* apply rest position */
		copy_v3_v3(point->co, point->rest_co);
		zero_v3(point->vel);
		
		if (k == 0) {
			if (k < totpoints-1)
				radius = seglen_to_radius * len_v3v3(pa_key->co, (pa_key+1)->co);
			point->radius = radius;
		}
		else {
			float prev_radius = radius;
			if (k < totpoints-1)
				radius = seglen_to_radius * len_v3v3(pa_key->co, (pa_key+1)->co);
			point->radius = 0.5f * (radius + prev_radius);
		}
	}
}
#else
/* base particle data */
static void hair_copy_data(Object *ob, HairSystem *hsys, ParticleSystem *psys, struct DerivedMesh *dm, float UNUSED(mat[4][4]), HairCurve *hair, int index)
{
	/* scale of segment lengths to get point radius */
	const float seglen_to_radius = 2.0f / 3.0f;
	
	ParticleData *pa = psys->particles + index;
	HairPoint *points;
	int totpoints;
	float hairmat[4][4];
	float radius;
	int k;
	
	if (pa->totkey <= 1)
		return;
	
	totpoints = pa->totkey;
	points = BKE_hair_point_append_multi(hsys, hair, totpoints);
	
	hair_copy_root(ob, hsys, psys, dm, hair, index);
	
	/* particle hair is defined in a local face/root space, don't want that */
	psys_mat_hair_to_object(ob, dm, psys->part->from, pa, hairmat);
	
	radius = 0.0f;
	for (k = 0; k < totpoints; ++k) {
		HairKey *psys_hair_key = pa->hair + k;
		HairPoint *point = points + k;
		
		mul_v3_m4v3(point->rest_co, hairmat, psys_hair_key->co);
		/* apply rest position */
		copy_v3_v3(point->co, point->rest_co);
		zero_v3(point->vel);
		
		if (k == 0) {
			if (k < totpoints-1)
				radius = seglen_to_radius * len_v3v3(psys_hair_key->co, (psys_hair_key+1)->co);
			point->radius = radius;
		}
		else {
			float prev_radius = radius;
			if (k < totpoints-1)
				radius = seglen_to_radius * len_v3v3(psys_hair_key->co, (psys_hair_key+1)->co);
			point->radius = 0.5f * (radius + prev_radius);
		}
	}
}
#endif

static void hair_copy_from_particles_psys(Object *ob, HairSystem *hsys, ParticleSystem *psys, struct DerivedMesh *dm)
{
	ParticleSettings *part = psys->part;
	HairCurve *hairs;
	PointerRNA part_ptr, cycles_ptr;
	int tothairs;
	float mat[4][4];
	int i;
	
	/* matrix for bringing hairs from pob to ob space */
	invert_m4_m4(mat, ob->obmat);
	
	/* particle emitter mesh data */
	DM_ensure_tessface(dm);
	
	RNA_id_pointer_create((ID *)part, &part_ptr);
	if (RNA_struct_find_property(&part_ptr, "cycles"))
		cycles_ptr = RNA_pointer_get(&part_ptr, "cycles");
	else
		cycles_ptr.data = NULL;
	
	/* copy system parameters */
	hsys->params.render.flag = 0;
	hsys->params.render.material_slot = (int)part->omat;
	hsys->params.render.num_render_hairs = part->ren_child_nbr;
	if (cycles_ptr.data) {
		/* various cycles settings, now defined by the hair system itself */
		hsys->params.render.radius_scale = RNA_float_get(&cycles_ptr, "radius_scale");
		hsys->params.render.root_width = RNA_float_get(&cycles_ptr, "root_width");
		hsys->params.render.tip_width = RNA_float_get(&cycles_ptr, "tip_width");
		hsys->params.render.shape = RNA_float_get(&cycles_ptr, "shape");
		if (RNA_boolean_get(&cycles_ptr, "use_closetip"))
			hsys->params.render.flag |= HAIR_RENDER_CLOSE_TIP;
	}
	
	/* XXX segment counts are incompatible: we copy from displayed segments ...
	 * set interpolation to 1 to avoid exploding point counts
	 */
	hsys->params.render.interpolation_steps = 1;
	
	tothairs = psys->totpart;
	hairs = BKE_hair_curve_add_multi(hsys, tothairs);
	
	for (i = 0; i < tothairs; ++i) {
		hair_copy_data(ob, hsys, psys, dm, mat, hairs + i, i);
	}
}

static int hair_copy_from_particles_exec(bContext *C, wmOperator *op)
{
	Object *ob = NULL;
	ParticleSystem *psys;
	HairSystem *hsys = NULL;
	HairModifierData *hmd = NULL;
	ED_hair_get(C, &ob, &hsys, &hmd);
	
	BKE_hairsys_clear(hsys);
	
	for (psys = ob->particlesystem.first; psys; psys = psys->next) {
		ParticleSystemModifierData *psmd;
		if (psys->part->type != PART_HAIR) {
			BKE_reportf(op->reports, RPT_WARNING, "Skipping particle system %s: Not a hair particle system", psys->name);
			continue;
		}
		if (psys->part->from != PART_FROM_FACE) {
			BKE_reportf(op->reports, RPT_WARNING, "Skipping particle system %s: Must use face emitter mode", psys->name);
			continue;
		}
		
		psmd = psys_get_modifier(ob, psys);
		if (!psmd || !psmd->dm) {
			BKE_reportf(op->reports, RPT_ERROR, "Skipping particle system %s: Invalid data", psys->name);
			continue;
		}
		
		hair_copy_from_particles_psys(ob, hsys, psys, psmd->dm);
	}
	
	hmd->flag &= ~MOD_HAIR_SOLVER_DATA_VALID;
	
	BKE_hair_calculate_rest(hsys);
	
	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	return OPERATOR_FINISHED;
}

void HAIR_OT_copy_from_particles(wmOperatorType *ot)
{
	/* identifiers */
	ot->idname = "HAIR_OT_copy_from_particles";
	ot->name = "Copy from particles";
	ot->description = "Copy hair data from particles to the hair system";

	/* callbacks */
	ot->exec = hair_copy_from_particles_exec;
	ot->poll = ED_hair_active_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

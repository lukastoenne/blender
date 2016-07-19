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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/editstrands.c
 *  \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_mempool.h"

#include "DNA_customdata_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"

#include "BKE_bvhutils.h"
#include "BKE_customdata.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_DerivedMesh.h"
#include "BKE_editstrands.h"
#include "BKE_effect.h"
#include "BKE_mesh_sample.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_strands.h"

#include "BPH_strands.h"

#include "GPU_buffers.h"
#include "GPU_strands.h"

#include "intern/bmesh_mesh_conv.h"
#include "intern/bmesh_strands_conv.h"

BMEditStrands *BKE_editstrands_create(BMesh *bm, DerivedMesh *root_dm, StrandFiber *fibers, int num_fibers)
{
	BMEditStrands *es = MEM_callocN(sizeof(BMEditStrands), __func__);
	
	es->base.bm = bm;
	es->root_dm = CDDM_copy(root_dm);
	if (fibers && num_fibers > 0) {
		es->fibers = MEM_dupallocN(fibers);
		es->totfibers = num_fibers;
	}
	
	return es;
}

BMEditStrands *BKE_editstrands_copy(BMEditStrands *es)
{
	BMEditStrands *es_copy = MEM_callocN(sizeof(BMEditStrands), __func__);
	*es_copy = *es;
	
	es_copy->base.bm = BM_mesh_copy(es->base.bm);
	es_copy->root_dm = CDDM_copy(es->root_dm);
	if (es->fibers) {
		es_copy->fibers = MEM_dupallocN(es->fibers);
		es_copy->totfibers = es->totfibers;
	}
	
	return es_copy;
}

/**
 * \brief Return the BMEditStrands for a given object
 */
BMEditStrands *BKE_editstrands_from_object(Object *ob)
{
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		if (me->edit_strands)
			return me->edit_strands;
	}
	
	{
		ParticleSystem *psys = psys_get_current(ob);
		if (psys && psys->hairedit)
			return psys->hairedit;
	}
	
	{
		ModifierData *md;
		for (md = ob->modifiers.first; md; md = md->next) {
			if (md->type == eModifierType_Strands) {
				StrandsModifierData *smd = (StrandsModifierData *)md;
				if (smd->edit)
					return smd->edit;
			}
		}
	}
	
	return NULL;
}

void BKE_editstrands_update_linked_customdata(BMEditStrands *es)
{
	BMesh *bm = es->base.bm;
	
	/* this is done for BMEditMesh, but should never exist for strands */
	BLI_assert(!CustomData_has_layer(&bm->pdata, CD_MTEXPOLY));
}

/*does not free the BMEditStrands struct itself*/
void BKE_editstrands_free(BMEditStrands *es)
{
	if (es->base.bm)
		BM_mesh_free(es->base.bm);
	if (es->root_dm)
		es->root_dm->release(es->root_dm);
	if (es->fibers)
		MEM_freeN(es->fibers);
}



bool BKE_editstrands_get_location(BMEditStrands *edit, BMVert *curve, float loc[3])
{
	BMesh *bm = edit->base.bm;
	DerivedMesh *root_dm = edit->root_dm;
	MeshSample root_sample;
	
	BM_elem_meshsample_data_named_get(&bm->vdata, curve, CD_MSURFACE_SAMPLE, CD_HAIR_ROOT_LOCATION, &root_sample);
	float nor[3], tang[3];
	if (BKE_mesh_sample_eval(root_dm, &root_sample, loc, nor, tang)) {
		return true;
	}
	else {
		zero_v3(loc);
		return false;
	}
}

bool BKE_editstrands_get_vectors(BMEditStrands *edit, BMVert *curve, float loc[3], float nor[3], float tang[3])
{
	BMesh *bm = edit->base.bm;
	DerivedMesh *root_dm = edit->root_dm;
	MeshSample root_sample;
	
	BM_elem_meshsample_data_named_get(&bm->vdata, curve, CD_MSURFACE_SAMPLE, CD_HAIR_ROOT_LOCATION, &root_sample);
	if (BKE_mesh_sample_eval(root_dm, &root_sample, loc, nor, tang)) {
		return true;
	}
	else {
		zero_v3(loc);
		zero_v3(nor);
		zero_v3(tang);
		return false;
	}
}

bool BKE_editstrands_get_matrix(BMEditStrands *edit, BMVert *curve, float mat[4][4])
{
	BMesh *bm = edit->base.bm;
	DerivedMesh *root_dm = edit->root_dm;
	MeshSample root_sample;
	
	BM_elem_meshsample_data_named_get(&bm->vdata, curve, CD_MSURFACE_SAMPLE, CD_HAIR_ROOT_LOCATION, &root_sample);
	if (BKE_mesh_sample_eval(root_dm, &root_sample, mat[3], mat[2], mat[0])) {
		cross_v3_v3v3(mat[1], mat[2], mat[0]);
		mat[0][3] = 0.0f;
		mat[1][3] = 0.0f;
		mat[2][3] = 0.0f;
		mat[3][3] = 1.0f;
		return true;
	}
	else {
		unit_m4(mat);
		return false;
	}
}

/* === constraints === */

BMEditStrandsLocations BKE_editstrands_get_locations(BMEditStrands *edit)
{
	BMesh *bm = edit->base.bm;
	BMEditStrandsLocations locs = MEM_mallocN(3*sizeof(float) * bm->totvert, "editstrands locations");
	
	BMVert *v;
	BMIter iter;
	int i;
	
	BM_ITER_MESH_INDEX(v, &iter, bm, BM_VERTS_OF_MESH, i) {
		copy_v3_v3(locs[i], v->co);
	}
	
	return locs;
}

void BKE_editstrands_free_locations(BMEditStrandsLocations locs)
{
	MEM_freeN(locs);
}

void BKE_editstrands_solve_constraints(Object *ob, BMEditStrands *es, BMEditStrandsLocations orig)
{
	BKE_editstrands_ensure(es);
	
	BPH_strands_solve_constraints(ob, es, orig);
}

static void editstrands_calc_segment_lengths(BMesh *bm)
{
	BMVert *root, *v, *vprev;
	BMIter iter, iter_strand;
	int k;
	
	BM_ITER_STRANDS(root, &iter, bm, BM_STRANDS_OF_MESH) {
		BM_ITER_STRANDS_ELEM_INDEX(v, &iter_strand, root, BM_VERTS_OF_STRAND, k) {
			if (k > 0) {
				float length = len_v3v3(v->co, vprev->co);
				BM_elem_float_data_named_set(&bm->vdata, v, CD_PROP_FLT, CD_HAIR_SEGMENT_LENGTH, length);
			}
			vprev = v;
		}
	}
}

void BKE_editstrands_ensure(BMEditStrands *es)
{
	BM_strands_cd_flag_ensure(es->base.bm, 0);
	
	if (es->flag & BM_STRANDS_DIRTY_SEGLEN) {
		editstrands_calc_segment_lengths(es->base.bm);
		
		es->flag &= ~BM_STRANDS_DIRTY_SEGLEN;
	}
}


/* === particle conversion === */

BMesh *BKE_editstrands_particles_to_bmesh(Object *ob, ParticleSystem *psys)
{
	ParticleSystemModifierData *psmd = psys_get_modifier(ob, psys);
	
	const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_PSYS(psys);
	BMesh *bm;

	bm = BM_mesh_create(&allocsize,
	                    &((struct BMeshCreateParams){.use_toolflags = false,}));

	if (psmd && psmd->dm_final) {
		DM_ensure_tessface(psmd->dm_final);
		
		BM_strands_bm_from_psys(bm, ob, psys, psmd->dm_final, true, /*psys->shapenr*/ -1);
		
		editstrands_calc_segment_lengths(bm);
	}

	return bm;
}

void BKE_editstrands_particles_from_bmesh(Object *ob, ParticleSystem *psys)
{
	ParticleSystemModifierData *psmd = psys_get_modifier(ob, psys);
	BMesh *bm = psys->hairedit ? psys->hairedit->base.bm : NULL;
	
	if (bm) {
		if (psmd && psmd->dm_final) {
			BVHTreeFromMesh bvhtree = {NULL};
			
			DM_ensure_tessface(psmd->dm_final);
			
			bvhtree_from_mesh_faces(&bvhtree, psmd->dm_final, 0.0, 2, 6);
			
			BM_strands_bm_to_psys(bm, ob, psys, psmd->dm_final, &bvhtree);
			
			free_bvhtree_from_mesh(&bvhtree);
		}
	}
}


/* === mesh conversion === */

BMesh *BKE_editstrands_mesh_to_bmesh(Object *ob, Mesh *me)
{
	const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(me);
	BMesh *bm;
	struct BMeshFromMeshParams params = {0};
	
	bm = BM_mesh_create(&allocsize,
	                    &((struct BMeshCreateParams){.use_toolflags = false,}));
	
	params.use_shapekey = true;
	params.active_shapekey = ob->shapenr;
	BM_mesh_bm_from_me(bm, me, &params);
	BM_strands_cd_flag_ensure(bm, 0);
	
	editstrands_calc_segment_lengths(bm);
	
	return bm;
}

void BKE_editstrands_mesh_from_bmesh(Object *ob)
{
	Mesh *me = ob->data;
	BMesh *bm = me->edit_strands->base.bm;
	struct BMeshToMeshParams params = {0};

	/* Workaround for T42360, 'ob->shapenr' should be 1 in this case.
	 * however this isn't synchronized between objects at the moment. */
	if (UNLIKELY((ob->shapenr == 0) && (me->key && !BLI_listbase_is_empty(&me->key->block)))) {
		bm->shapenr = 1;
	}

	BM_mesh_bm_to_me(bm, me, &params);

#ifdef USE_TESSFACE_DEFAULT
	BKE_mesh_tessface_calc(me);
#endif

	/* free derived mesh. usually this would happen through depsgraph but there
	 * are exceptions like file save that will not cause this, and we want to
	 * avoid ending up with an invalid derived mesh then */
	BKE_object_free_derived_caches(ob);
}

/* === strands conversion === */

BMesh *BKE_editstrands_strands_to_bmesh(Strands *strands, DerivedMesh *root_dm)
{
	const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_STRANDS(strands);
	BMesh *bm;

	bm = BM_mesh_create(&allocsize,
	                    &((struct BMeshCreateParams){.use_toolflags = false,}));

	BM_bm_from_strands(bm, strands, root_dm, true, -1);
	
	editstrands_calc_segment_lengths(bm);

	return bm;
}

void BKE_editstrands_strands_from_bmesh(Strands *strands, BMesh *bm, DerivedMesh *root_dm)
{
	if (bm) {
		BM_bm_to_strands(bm, strands, root_dm);
	}
}


/* === gpu buffer conversion === */

typedef struct BMStrandCurve
{
	BMVert *root;
	int verts_begin;
	int num_verts;
} BMStrandCurve;

static BMStrandCurve *editstrands_build_curves(BMesh *bm, int *r_totcurves)
{
	BMVert *root;
	BMIter iter;
	
	int totstrands = BM_strands_count(bm);
	BMStrandCurve *curves = MEM_mallocN(sizeof(BMStrandCurve) * totstrands, "BMStrandCurve");
	
	BMStrandCurve *curve = curves;
	BM_ITER_STRANDS(root, &iter, bm, BM_STRANDS_OF_MESH) {
		curve->root = root;
		curve->verts_begin = BM_elem_index_get(root);
		curve->num_verts = BM_strand_verts_count(root);
		
		++curve;
	}
	
	if (r_totcurves) *r_totcurves = totstrands;
	return curves;
}

typedef struct BMEditStrandsConverter {
	GPUStrandsConverter base;
	BMEditStrands *edit;
	BMStrandCurve *curves;
	int totcurves;
} BMEditStrandsConverter;

static void BMEditStrandsConverter_free(GPUStrandsConverter *_conv)
{
	BMEditStrandsConverter *conv = (BMEditStrandsConverter *)_conv;
	if (conv->curves)
		MEM_freeN(conv->curves);
	MEM_freeN(conv);
}

static int BMEditStrandsConverter_getNumFibers(GPUStrandsConverter *_conv)
{
	BMEditStrandsConverter *conv = (BMEditStrandsConverter *)_conv;
	return conv->edit->totfibers;
}

static StrandFiber *BMEditStrandsConverter_getFiberArray(GPUStrandsConverter *_conv)
{
	BMEditStrandsConverter *conv = (BMEditStrandsConverter *)_conv;
	return conv->edit->fibers;
}

static int BMEditStrandsConverter_getNumStrandVerts(GPUStrandsConverter *_conv)
{
	BMEditStrandsConverter *conv = (BMEditStrandsConverter *)_conv;
	return conv->edit->base.bm->totvert;
}

static int BMEditStrandsConverter_getNumStrandCurves(GPUStrandsConverter *_conv)
{
	BMEditStrandsConverter *conv = (BMEditStrandsConverter *)_conv;
	return conv->totcurves;
}

static int BMEditStrandsConverter_getNumStrandCurveVerts(GPUStrandsConverter *_conv, int curve_index)
{
	BMEditStrandsConverter *conv = (BMEditStrandsConverter *)_conv;
	BLI_assert(curve_index < conv->totcurves);
	return conv->curves[curve_index].num_verts;
}

static void BMEditStrandsConverter_foreachStrandVertex(GPUStrandsConverter *_conv, GPUStrandsVertexFunc cb, void *userdata)
{
	BMEditStrandsConverter *conv = (BMEditStrandsConverter *)_conv;
	BMesh *bm = conv->edit->base.bm;
	BMIter iter;
	BMVert *vert;
	int i;
	
	BM_ITER_MESH_INDEX(vert, &iter, bm, BM_VERTS_OF_MESH, i) {
		cb(userdata, i, vert->co, NULL);
	}
}

static void BMEditStrandsConverter_foreachStrandEdge(GPUStrandsConverter *_conv, GPUStrandsEdgeFunc cb, void *userdata)
{
	BMEditStrandsConverter *conv = (BMEditStrandsConverter *)_conv;
	BMesh *bm = conv->edit->base.bm;
	BMIter iter;
	BMEdge *edge;
	
	BM_ITER_MESH(edge, &iter, bm, BM_EDGES_OF_MESH) {
		cb(userdata, BM_elem_index_get(edge->v1), BM_elem_index_get(edge->v2));
	}
}

static void BMEditStrandsConverter_foreachCurve(GPUStrandsConverter *_conv, GPUStrandsCurveFunc cb, void *userdata)
{
	BMEditStrandsConverter *conv = (BMEditStrandsConverter *)_conv;
	BMesh *bm = conv->edit->base.bm;
	BMIter iter;
	BMVert *root;
	
	int verts_begin = 0;
	BM_ITER_STRANDS(root, &iter, bm, BM_STRANDS_OF_MESH) {
		int orig_num_verts = BM_strand_verts_count(root);
		int num_verts = BKE_strand_curve_cache_size(orig_num_verts, conv->base.subdiv);
		
		cb(userdata, verts_begin, num_verts);
		
		verts_begin += num_verts;
	}
}

static void BMEditStrandsConverter_foreachCurveCache(GPUStrandsConverter *_conv, GPUStrandsCurveCacheFunc cb, void *userdata)
{
	BMEditStrandsConverter *conv = (BMEditStrandsConverter *)_conv;
	BMesh *bm = conv->edit->base.bm;
	
	StrandCurveCache *cache = BKE_strand_curve_cache_create_bm(bm, conv->base.subdiv);
	
	BMIter iter;
	BMVert *root;
	BM_ITER_STRANDS(root, &iter, bm, BM_STRANDS_OF_MESH) {
		float rootmat[4][4];
		BKE_editstrands_get_matrix(conv->edit, root, rootmat);
		
		int orig_num_verts = BM_strand_verts_count(root);
		int num_verts = BKE_strand_curve_cache_calc_bm(root, orig_num_verts, cache, rootmat, conv->base.subdiv);
		BLI_assert(orig_num_verts >= 2);
		
		cb(userdata, cache, num_verts);
	}
	
	BKE_strand_curve_cache_free(cache);
}


GPUStrandsConverter *BKE_editstrands_get_gpu_converter(BMEditStrands *edit, struct DerivedMesh *root_dm,
                                                       int subdiv, int fiber_primitive, bool use_geomshader)
{
	BMEditStrandsConverter *conv = MEM_callocN(sizeof(BMEditStrandsConverter), "BMEditStrandsConverter");
	conv->base.free = BMEditStrandsConverter_free;
	conv->base.getNumFibers = BMEditStrandsConverter_getNumFibers;
	conv->base.getFiberArray = BMEditStrandsConverter_getFiberArray;
	conv->base.getNumStrandVerts = BMEditStrandsConverter_getNumStrandVerts;
	conv->base.getNumStrandCurves = BMEditStrandsConverter_getNumStrandCurves;
	conv->base.getNumStrandCurveVerts = BMEditStrandsConverter_getNumStrandCurveVerts;
	
	conv->base.foreachStrandVertex = BMEditStrandsConverter_foreachStrandVertex;
	conv->base.foreachStrandEdge = BMEditStrandsConverter_foreachStrandEdge;
	conv->base.foreachCurve = BMEditStrandsConverter_foreachCurve;
	conv->base.foreachCurveCache = BMEditStrandsConverter_foreachCurveCache;
	
	conv->base.root_dm = root_dm;
	conv->base.subdiv = subdiv;
	conv->base.fiber_primitive = fiber_primitive;
	conv->base.use_geomshader = use_geomshader;
	
	conv->edit = edit;
	conv->curves = editstrands_build_curves(edit->base.bm, &conv->totcurves);
	return (GPUStrandsConverter *)conv;
}

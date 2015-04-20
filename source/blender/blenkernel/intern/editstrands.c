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

#include "BLI_math.h"
#include "BLI_mempool.h"

#include "DNA_customdata_types.h"
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
#include "BKE_particle.h"

#include "BPH_strands.h"

#include "intern/bmesh_strands_conv.h"

BMEditStrands *BKE_editstrands_create(BMesh *bm, DerivedMesh *root_dm)
{
	BMEditStrands *es = MEM_callocN(sizeof(BMEditStrands), __func__);
	
	es->bm = bm;
	es->root_dm = CDDM_copy(root_dm);
	
	return es;
}

BMEditStrands *BKE_editstrands_copy(BMEditStrands *es)
{
	BMEditStrands *es_copy = MEM_callocN(sizeof(BMEditStrands), __func__);
	*es_copy = *es;
	
	es_copy->bm = BM_mesh_copy(es->bm);
	es_copy->root_dm = CDDM_copy(es->root_dm);
	
	return es_copy;
}

/**
 * \brief Return the BMEditStrands for a given object
 */
BMEditStrands *BKE_editstrands_from_object(Object *ob)
{
	ParticleSystem *psys = psys_get_current(ob);
	if (psys) {
		return psys->hairedit;
	}
	return NULL;
}

void BKE_editstrands_update_linked_customdata(BMEditStrands *es)
{
	BMesh *bm = es->bm;
	
	/* this is done for BMEditMesh, but should never exist for strands */
	BLI_assert(!CustomData_has_layer(&bm->pdata, CD_MTEXPOLY));
}

/*does not free the BMEditStrands struct itself*/
void BKE_editstrands_free(BMEditStrands *es)
{
	if (es->bm)
		BM_mesh_free(es->bm);
	if (es->root_dm)
		es->root_dm->release(es->root_dm);
}

/* === constraints === */

BMEditStrandsLocations BKE_editstrands_get_locations(BMEditStrands *edit)
{
	BMesh *bm = edit->bm;
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
	BM_strands_cd_flag_ensure(es->bm, 0);
	
	if (es->flag & BM_STRANDS_DIRTY_SEGLEN) {
		editstrands_calc_segment_lengths(es->bm);
		
		es->flag &= ~BM_STRANDS_DIRTY_SEGLEN;
	}
}


/* === particle conversion === */

BMesh *BKE_particles_to_bmesh(Object *ob, ParticleSystem *psys)
{
	ParticleSystemModifierData *psmd = psys_get_modifier(ob, psys);
	
	const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_PSYS(psys);
	BMesh *bm;

	bm = BM_mesh_create(&allocsize);

	if (psmd && psmd->dm) {
		DM_ensure_tessface(psmd->dm);
		
		BM_strands_bm_from_psys(bm, ob, psys, psmd->dm, true, psys->shapenr);
		
		editstrands_calc_segment_lengths(bm);
	}

	return bm;
}

void BKE_particles_from_bmesh(Object *ob, ParticleSystem *psys)
{
	ParticleSystemModifierData *psmd = psys_get_modifier(ob, psys);
	BMesh *bm = psys->hairedit ? psys->hairedit->bm : NULL;
	
	if (bm) {
		if (psmd && psmd->dm) {
			BVHTreeFromMesh bvhtree = {NULL};
			
			DM_ensure_tessface(psmd->dm);
			
			bvhtree_from_mesh_faces(&bvhtree, psmd->dm, 0.0, 2, 6);
			
			BM_strands_bm_to_psys(bm, ob, psys, psmd->dm, &bvhtree);
			
			free_bvhtree_from_mesh(&bvhtree);
		}
	}
}

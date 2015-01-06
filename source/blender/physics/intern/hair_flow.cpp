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

/** \file blender/blenkernel/intern/hair_flow.c
 *  \ingroup bph
 */

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_editstrands.h"
#include "BKE_effect.h"
#include "BKE_mesh_sample.h"

#include "bmesh.h"
}

#include "BPH_strands.h"

#include "implicit.h"
#include "eigen_utils.h"

struct HairFlowData {
	float cellsize;
};

HairFlowData *BPH_strands_solve_hair_flow(Scene *scene, Object *ob)
{
	/* XXX just a dummy for now ... */
	HairFlowData *data = (HairFlowData *)MEM_callocN(sizeof(HairFlowData), "hair flow data");
	
	data->cellsize = 100.0f;
	
	return data;
}

void BPH_strands_free_hair_flow(HairFlowData *data)
{
	MEM_freeN(data);
}

BLI_INLINE void construct_m4_loc_nor_tan(float mat[4][4], const float loc[3], const float nor[3], const float tang[3])
{
	float cotang[3];
	
	cross_v3_v3v3(cotang, nor, tang);
	
	copy_v3_v3(mat[0], tang);
	copy_v3_v3(mat[1], cotang);
	copy_v3_v3(mat[2], nor);
	copy_v3_v3(mat[3], loc);
	mat[0][3] = 0.0f;
	mat[1][3] = 0.0f;
	mat[2][3] = 0.0f;
	mat[3][3] = 1.0f;
}

static void sample_hair_strand(Object *UNUSED(ob), BMEditStrands *edit, HairFlowData *data,
                               MSurfaceSample *sample, float max_length, int segments,
                               float dt)
{
	DerivedMesh *dm = edit->root_dm;
	const float inv_dt = 1.0f / dt;
	
	float co[3], dir[3];
	BMVert *root, *v;
	
	const float seglen = max_length / (float)segments;
	float t, tseg;
	int numseg;
	
	{
		float tang[3];
		BKE_mesh_sample_eval(dm, sample, co, dir, tang);
	}
	
	root = BM_strands_create(edit->bm, segments + 1, true);
	
	BM_elem_meshsample_data_named_set(&edit->bm->vdata, root, CD_MSURFACE_SAMPLE, CD_HAIR_ROOT_LOCATION, sample);
	
	t = 0.0f;
	numseg = 0;
	tseg = 0.0f;
	v = root;
	
	while (v && t < max_length) {
		float nt = t + dt;
		float prev_co[3];
		
		/* TODO evaluate direction */
		//dir = grid_dir(co);
		
		/* forward integrate position */
		copy_v3_v3(prev_co, co);
		madd_v3_v3fl(co, dir, dt);
		
		while (v && tseg <= nt) {
			float vt;
			
			/* interpolate vertex position */
			vt = (tseg - t) * inv_dt;
			interp_v3_v3v3(v->co, prev_co, co, vt);
			
			tseg += seglen;
			++numseg;
			v = BM_strands_vert_next(v);
		}
		
		t = nt;
	}
	/* make sure all potentially remaining verts have a valid location */
	while (v) {
		copy_v3_v3(v->co, co);
		
		v = BM_strands_vert_next(v);
	}
}

void BPH_strands_sample_hair_flow(Object *ob, BMEditStrands *edit, HairFlowData *data,
                                  unsigned int seed, int max_strands, float max_length, int segments)
{
	MSurfaceSampleStorage storage;
	MSurfaceSample *samples;
	int tot, i;
	float dt;
	
	BLI_assert(segments >= 1);
	
	if (!ob->derivedFinal)
		return;
	
	/* integration step size */
	dt = min_ff(0.5f * data->cellsize, max_length / (float)segments);
	
	samples = (MSurfaceSample *)MEM_mallocN(sizeof(MSurfaceSample) * max_strands, "hair flow sampling origins");
	BKE_mesh_sample_storage_array(&storage, samples, max_strands);
	
	tot = BKE_mesh_sample_generate_random(&storage, ob->derivedFinal, seed, max_strands);
	
	for (i = 0; i < tot; ++i) {
		sample_hair_strand(ob, edit, data, &samples[i], max_length, segments, dt);
	}
	
	BKE_mesh_sample_storage_release(&storage);
	MEM_freeN(samples);
	
	BM_mesh_elem_index_ensure(edit->bm, BM_ALL);
}

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
#include "BKE_object.h"

#include "bmesh.h"
}

#include "BPH_strands.h"

#include "eigen_utils.h"
#include "implicit.h"
#include "grid.h"

using namespace BPH;

BLI_INLINE int floor_int(float value)
{
	return value > 0.0f ? (int)value : ((int)value) - 1;
}

BLI_INLINE float floor_mod(float value)
{
	return value - floorf(value);
}

struct HairFlowData {
	HairFlowData()
	{
	}
	
	~HairFlowData()
	{
	}
	
	Grid grid;
	
	MEM_CXX_CLASS_ALLOC_FUNCS("HairFlowData")
};

HairFlowData *BPH_strands_solve_hair_flow(Scene *scene, Object *ob, float max_length, int max_res, SimDebugData *debug_data)
{
	HairFlowData *data = new HairFlowData();
	int i, k;
	
	BoundBox *bb = BKE_object_boundbox_get(ob);
	if (!bb)
		return NULL;
	
	BoundBox bbworld;
	float bbmin[3], bbmax[3], extent[3];
	INIT_MINMAX(bbmin, bbmax);
	for (i = 0; i < 8; ++i) {
		mul_v3_m4v3(bbworld.vec[i], ob->obmat, bb->vec[i]);
		for (k = 0; k < 3; ++k) {
			if (bbmin[k] > bbworld.vec[i][k])
				bbmin[k] = bbworld.vec[i][k];
			if (bbmax[k] < bbworld.vec[i][k])
				bbmax[k] = bbworld.vec[i][k];
		}
	}
	float max_extent = -1.0f;
	int max_extent_index = -1;
	for (k = 0; k < 3; ++k) {
		/* hair can extend at most max_length in either direction from the mesh,
		 * which defines the maximum extent of the bounding volume we need
		 */
		extent[k] = bbmax[k] - bbmin[k] + 2.0f * max_length;
		BLI_assert(extent[k] >= 0.0f);
		if (max_extent < extent[k]) {
			max_extent = extent[k];
			max_extent_index = k;
		}
	}
	
	/* make sure a 1-cell margin is supported */
	CLAMP_MIN(max_res, 3);
	/* 1-cell margin of the grid means the actual extent uses 2 cells less */
	float cellsize = max_extent / (max_res - 2);
	float offset[3] = {(float)(bbmin[0] - max_length - 0.5*cellsize),
	                   (float)(bbmin[1] - max_length - 0.5*cellsize),
	                   (float)(bbmin[2] - max_length - 0.5*cellsize)};
	
	int res[3];
	res[max_extent_index] = max_res;
	k = (max_extent_index + 1) % 3;
	res[k] = floor_int(extent[k] / cellsize) + 2;
	k = (max_extent_index + 2) % 3;
	res[k] = floor_int(extent[k] / cellsize) + 2;
	
	data->grid.resize(cellsize, offset, res);
	
	GridHash<bool> source;
	GridHash<float3> source_normal;
	source.resize(data->grid.res);
	source.clear();
	source_normal.resize(data->grid.res);
	source_normal.clear();
	
	data->grid.set_inner_cells(source, source_normal, ob);
	
	GridHash<float> divergence;
	divergence.resize(data->grid.res);
	data->grid.calc_divergence(divergence, source, source_normal);
	
	GridHash<float> pressure;
	pressure.resize(data->grid.res);
	data->grid.solve_pressure(pressure, divergence);
	
	{
		float col0[3] = {0.0, 0.0, 0.0};
		float colp[3] = {0.0, 1.0, 1.0};
		float coln[3] = {1.0, 0.0, 1.0};
		float colfac = 10.0f;
		
		BKE_sim_debug_data_clear_category(debug_data, "hair flow");
		
		for (int z = 0; z < res[2]; ++z) {
			for (int y = 0; y < res[1]; ++y) {
				for (int x = 0; x < res[0]; ++x) {
					float vec[3] = {(float)x + 0.5f, (float)y + 0.5f, (float)z + 0.5f};
					mul_v3_fl(vec, cellsize);
					add_v3_v3(vec, offset);
					
					bool is_source = *source.get(x, y, z);
					float div = *divergence.get(x, y, z);
					float prs = *pressure.get(x, y, z);
					
//					if (is_source)
//						BKE_sim_debug_data_add_circle(debug_data, vec, 0.02f, 1,0,0, "hair flow", 111, x, y, z);
//					else
//						BKE_sim_debug_data_add_circle(debug_data, vec, 0.02f, 0,1,0, "hair flow", 111, x, y, z);
					
					float input = prs;
					float fac;
					float col[3];
					if (div > 0.0f) {
						fac = CLAMPIS(input * colfac, 0.0, 1.0);
						interp_v3_v3v3(col, col0, colp, fac);
					}
					else {
						fac = CLAMPIS(-input * colfac, 0.0, 1.0);
						interp_v3_v3v3(col, col0, coln, fac);
					}
					if (fac > 0.05f)
						BKE_sim_debug_data_add_circle(debug_data, vec, 0.02f, col[0], col[1], col[2], "hair_flow", 5522, x, y, z);
				}
			}
		}
	}
	
	return data;
}

void BPH_strands_free_hair_flow(HairFlowData *data)
{
	delete data;
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
	dt = min_ff(0.5f * data->grid.cellsize, max_length / (float)segments);
	
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

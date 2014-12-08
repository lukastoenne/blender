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

/** \file blender/editors/hair/hair_edit.c
 *  \ingroup edhair
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "DNA_brush_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BKE_brush.h"
#include "BKE_DerivedMesh.h"
#include "BKE_edithair.h"
#include "BKE_mesh_sample.h"

#include "bmesh.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_view3d.h"

#include "hair_intern.h"

bool hair_test_depth(HairViewData *viewdata, const float co[3], const int screen_co[2])
{
	View3D *v3d = viewdata->vc.v3d;
	ViewDepths *vd = viewdata->vc.rv3d->depths;
	const bool has_zbuf = (v3d->drawtype > OB_WIRE) && (v3d->flag & V3D_ZBUF_SELECT);
	
	double ux, uy, uz;
	float depth;
	
	/* nothing to do */
	if (!has_zbuf)
		return true;
	
	gluProject(co[0], co[1], co[2],
	           viewdata->mats.modelview, viewdata->mats.projection, viewdata->mats.viewport,
	           &ux, &uy, &uz);
	
	/* check if screen_co is within bounds because brush_cut uses out of screen coords */
	if (screen_co[0] >= 0 && screen_co[0] < vd->w && screen_co[1] >= 0 && screen_co[1] < vd->h) {
		BLI_assert(vd && vd->depths);
		/* we know its not clipped */
		depth = vd->depths[screen_co[1] * vd->w + screen_co[0]];
		
		return ((float)uz - 0.00001f <= depth);
	}
	
	return false;
}

bool hair_test_inside_circle(HairViewData *viewdata, const float mval[2], float radsq, BMVert *v, float *r_dist)
{
	float (*obmat)[4] = viewdata->vc.obact->obmat;
	float co_world[3];
	float dx, dy, distsq;
	int screen_co[2];
	
	mul_v3_m4v3(co_world, obmat, v->co);
	
	/* TODO, should this check V3D_PROJ_TEST_CLIP_BB too? */
	if (ED_view3d_project_int_global(viewdata->vc.ar, co_world, screen_co, V3D_PROJ_TEST_CLIP_WIN) != V3D_PROJ_RET_OK)
		return false;
	
	dx = mval[0] - (float)screen_co[0];
	dy = mval[1] - (float)screen_co[1];
	distsq = dx * dx + dy * dy;
	
	if (distsq > radsq)
		return false;
	
	if (hair_test_depth(viewdata, v->co, screen_co)) {
		*r_dist = sqrtf(distsq);
		return true;
	}
	else
		return false;
}

BLI_INLINE float factor_vertex(HairToolData *data, BMVert *v)
{
	Scene *scene = data->scene;
	Brush *brush = data->settings->brush;
	const float rad = BKE_brush_size_get(scene, brush);
	const float radsq = rad*rad;
	
	float dist;
	
	if (!hair_test_inside_circle(&data->viewdata, data->mval, radsq, v, &dist))
		return 0.0f;
	
	return 1.0f - dist / rad;
}

/* ------------------------------------------------------------------------- */

typedef void (*VertexToolCb)(HairToolData *data, void *userdata, BMVert *v, float factor);

static int hair_tool_apply_vertex(HairToolData *data, VertexToolCb cb, void *userdata)
{
	const float threshold = 0.0f; /* XXX could be useful, is it needed? */
	
	BMVert *v;
	BMIter iter;
	int tot = 0;
	
	BM_ITER_MESH(v, &iter, data->edit->bm, BM_VERTS_OF_MESH) {
		float factor = factor_vertex(data, v);
		if (factor > threshold) {
			cb(data, userdata, v, factor);
			++tot;
		}
	}
	
	return tot;
}

/* ------------------------------------------------------------------------- */

typedef struct CombData {
	float power;
} CombData;

static void hair_vertex_comb(HairToolData *data, void *userdata, BMVert *v, float factor)
{
	CombData *combdata = userdata;
	
	float combfactor = powf(factor, combdata->power);
	
	madd_v3_v3fl(v->co, data->delta, combfactor);
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

static void grow_hair(BMEditStrands *edit, MSurfaceSample *sample)
{
	DerivedMesh *dm = edit->root_dm;
	const float len = 0.1f;
	const int num_verts = 2;
	
	float root_mat[4][4];
	BMVert *root, *v;
	BMIter iter;
	int i;
	
	{
		float co[3], nor[3], tang[3];
		BKE_mesh_sample_eval(dm, sample, co, nor, tang);
		construct_m4_loc_nor_tan(root_mat, co, nor, tang);
	}
	
	root = BM_strands_create(edit->bm, 2, true);
	
	BM_elem_meshsample_data_named_set(&edit->bm->vdata, root, CD_MSURFACE_SAMPLE, CD_HAIR_ROOT_LOCATION, sample);
	
	BM_ITER_STRANDS_ELEM_INDEX(v, &iter, root, BM_VERTS_OF_STRAND, i) {
		float co[3];
		
		co[0] = co[1] = 0.0f;
		co[2] = len * (float)i / (float)(num_verts-1);
		
		mul_m4_v3(root_mat, co);
		
		copy_v3_v3(v->co, co);
	}
	
	BM_mesh_elem_index_ensure(edit->bm, BM_ALL);
}

static bool hair_add_ray_cb(void *vdata, float ray_start[3], float ray_end[3])
{
	HairToolData *data = vdata;
	ViewContext *vc = &data->viewdata.vc;
	
	ED_view3d_win_to_segment(vc->ar, vc->v3d, data->mval, ray_start, ray_end, true);
	
	mul_m4_v3(data->imat, ray_start);
	mul_m4_v3(data->imat, ray_end);
	
	return true;
}

static bool hair_get_surface_sample(HairToolData *data, MSurfaceSample *sample)
{
	DerivedMesh *dm = data->edit->root_dm;
	
	MSurfaceSampleStorage dst;
	int tot;
	
	BKE_mesh_sample_storage_single(&dst, sample);
	tot = BKE_mesh_sample_generate_raycast(&dst, dm, hair_add_ray_cb, data, 1);
	BKE_mesh_sample_storage_release(&dst);
	
	return tot > 0;
}

static bool hair_add(HairToolData *data)
{
#if 0
	MSurfaceSample sample;
	
	if (!hair_get_surface_sample(data, &sample))
		return false;
	
	grow_hair(data->edit, &sample);
#else
	DerivedMesh *dm = data->edit->root_dm;
	
	MSurfaceSample samples[100];
	MSurfaceSampleStorage dst;
	int i, tot;
	
	BKE_mesh_sample_storage_array(&dst, samples, 100);
	
	tot = BKE_mesh_sample_generate_darts(&dst, dm, 1234, 100);
	for (i = 0; i < 100; ++i) {
		grow_hair(data->edit, &samples[i]);
	}
	
	BKE_mesh_sample_storage_release(&dst);
#endif
	
	return true;
}


bool hair_brush_step(HairToolData *data)
{
	Brush *brush = data->settings->brush;
	BrushHairTool hair_tool = brush->hair_tool;
	BMEditStrands *edit = data->edit;
	int tot = 0;
	
	switch (hair_tool) {
		case HAIR_TOOL_COMB: {
			CombData combdata;
			combdata.power = (brush->alpha - 0.5f) * 2.0f;
			if (combdata.power < 0.0f)
				combdata.power = 1.0f - 9.0f * combdata.power;
			else
				combdata.power = 1.0f - combdata.power;
			
			tot = hair_tool_apply_vertex(data, hair_vertex_comb, &combdata);
			break;
		}
		case HAIR_TOOL_CUT:
			break;
		case HAIR_TOOL_LENGTH:
			break;
		case HAIR_TOOL_PUFF:
			break;
		case HAIR_TOOL_ADD:
			if (hair_add(data))
				edit->flag |= BM_STRANDS_DIRTY_SEGLEN;
			break;
		case HAIR_TOOL_SMOOTH:
			break;
		case HAIR_TOOL_WEIGHT:
			break;
	}
	
	return tot > 0;
}

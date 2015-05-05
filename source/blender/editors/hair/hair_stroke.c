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
#include "BLI_lasso.h"
#include "BLI_math.h"
#include "BLI_rect.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BKE_brush.h"
#include "BKE_DerivedMesh.h"
#include "BKE_editstrands.h"
#include "BKE_effect.h"
#include "BKE_mesh_sample.h"

#include "bmesh.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_physics.h"
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

bool hair_test_vertex_inside_circle(HairViewData *viewdata, const float mval[2], float radsq, BMVert *v, float *r_dist)
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

bool hair_test_edge_inside_circle(HairViewData *viewdata, const float mval[2], float radsq, BMVert *v1, BMVert *v2, float *r_dist, float *r_lambda)
{
	float (*obmat)[4] = viewdata->vc.obact->obmat;
	float world_co1[3], world_co2[3];
	float dx, dy, distsq;
	int screen_co1[2], screen_co2[2], screen_cp[2];
	float lambda, world_cp[3], screen_cpf[2], screen_co1f[2], screen_co2f[2];
	
	mul_v3_m4v3(world_co1, obmat, v1->co);
	mul_v3_m4v3(world_co2, obmat, v2->co);
	
	/* TODO, should this check V3D_PROJ_TEST_CLIP_BB too? */
	if (ED_view3d_project_int_global(viewdata->vc.ar, world_co1, screen_co1, V3D_PROJ_TEST_CLIP_WIN) != V3D_PROJ_RET_OK)
		return false;
	if (ED_view3d_project_int_global(viewdata->vc.ar, world_co2, screen_co2, V3D_PROJ_TEST_CLIP_WIN) != V3D_PROJ_RET_OK)
		return false;
	
	screen_co1f[0] = screen_co1[0];
	screen_co1f[1] = screen_co1[1];
	screen_co2f[0] = screen_co2[0];
	screen_co2f[1] = screen_co2[1];
	lambda = closest_to_line_v2(screen_cpf, mval, screen_co1f, screen_co2f);
	if (lambda < 0.0f || lambda > 1.0f) {
		CLAMP(lambda, 0.0f, 1.0f);
		interp_v2_v2v2(screen_cpf, screen_co1f, screen_co2f, lambda);
	}
	
	dx = mval[0] - screen_cpf[0];
	dy = mval[1] - screen_cpf[1];
	distsq = dx * dx + dy * dy;
	
	if (distsq > radsq)
		return false;
	
	interp_v3_v3v3(world_cp, world_co1, world_co2, lambda);
	
	screen_cp[0] = screen_cpf[0];
	screen_cp[1] = screen_cpf[1];
	if (hair_test_depth(viewdata, world_cp, screen_cp)) {
		*r_dist = sqrtf(distsq);
		*r_lambda = lambda;
		return true;
	}
	else
		return false;
}

bool hair_test_vertex_inside_rect(HairViewData *viewdata, rcti *rect, BMVert *v)
{
	float (*obmat)[4] = viewdata->vc.obact->obmat;
	float co_world[3];
	int screen_co[2];
	
	mul_v3_m4v3(co_world, obmat, v->co);
	
	/* TODO, should this check V3D_PROJ_TEST_CLIP_BB too? */
	if (ED_view3d_project_int_global(viewdata->vc.ar, co_world, screen_co, V3D_PROJ_TEST_CLIP_WIN) != V3D_PROJ_RET_OK)
		return false;
	
	if (!BLI_rcti_isect_pt_v(rect, screen_co))
		return false;
	
	if (hair_test_depth(viewdata, v->co, screen_co))
		return true;
	else
		return false;
}

bool hair_test_vertex_inside_lasso(HairViewData *viewdata, const int mcoords[][2], short moves, BMVert *v)
{
	float (*obmat)[4] = viewdata->vc.obact->obmat;
	float co_world[3];
	int screen_co[2];
	
	mul_v3_m4v3(co_world, obmat, v->co);
	
	/* TODO, should this check V3D_PROJ_TEST_CLIP_BB too? */
	if (ED_view3d_project_int_global(viewdata->vc.ar, co_world, screen_co, V3D_PROJ_TEST_CLIP_WIN) != V3D_PROJ_RET_OK)
		return false;
	
	if (!BLI_lasso_is_point_inside(mcoords, moves, screen_co[0], screen_co[1], IS_CLIPPED))
		return false;
	
	if (hair_test_depth(viewdata, v->co, screen_co))
		return true;
	else
		return false;
}

/* ------------------------------------------------------------------------- */

typedef void (*VertexToolCb)(HairToolData *data, void *userdata, BMVert *v, float factor);

/* apply tool directly to each vertex inside the filter area */
static int hair_tool_apply_vertex(HairToolData *data, VertexToolCb cb, void *userdata)
{
	Scene *scene = data->scene;
	Brush *brush = data->settings->brush;
	const float rad = BKE_brush_size_get(scene, brush);
	const float radsq = rad*rad;
	const float threshold = 0.0f; /* XXX could be useful, is it needed? */
	const bool use_mirror = hair_use_mirror_x(data->ob);
	
	BMVert *v, *v_mirr;
	BMIter iter;
	int tot = 0;
	float dist, factor;
	
	if (use_mirror)
		ED_strands_mirror_cache_begin(data->edit, 0, false, false, hair_use_mirror_topology(data->ob));
	
	BM_ITER_MESH(v, &iter, data->edit->bm, BM_VERTS_OF_MESH) {
		if (!hair_test_vertex_inside_circle(&data->viewdata, data->mval, radsq, v, &dist))
			continue;
		
		factor = 1.0f - dist / rad;
		if (factor > threshold) {
			cb(data, userdata, v, factor);
			++tot;
			
			if (use_mirror) {
				v_mirr = ED_strands_mirror_get(data->edit, v);
				if (v_mirr)
					cb(data, userdata, v_mirr, factor);
			}
		}
	}
	
	/* apply mirror */
	if (use_mirror)
		ED_strands_mirror_cache_end(data->edit);
	
	return tot;
}

/* ------------------------------------------------------------------------- */

typedef void (*EdgeToolCb)(HairToolData *data, void *userdata, BMVert *v1, BMVert *v2, float factor, float edge_param);

static int hair_tool_apply_strand_edges(HairToolData *data, EdgeToolCb cb, void *userdata, BMVert *root)
{
	Scene *scene = data->scene;
	Brush *brush = data->settings->brush;
	const float rad = BKE_brush_size_get(scene, brush);
	const float radsq = rad*rad;
	const float threshold = 0.0f; /* XXX could be useful, is it needed? */
	
	BMVert *v, *vprev, *v_mirr, *vprev_mirr;
	BMIter iter;
	int k;
	int tot = 0;
	
	BM_ITER_STRANDS_ELEM_INDEX(v, &iter, root, BM_VERTS_OF_STRAND, k) {
		if (k > 0) {
			float dist, lambda;
			
			if (hair_test_edge_inside_circle(&data->viewdata, data->mval, radsq, vprev, v, &dist, &lambda)) {
				float factor = 1.0f - dist / rad;
				if (factor > threshold) {
					cb(data, userdata, vprev, v, factor, lambda);
					++tot;
					
					v_mirr = ED_strands_mirror_get(data->edit, v);
					if (vprev_mirr && v_mirr)
						cb(data, userdata, vprev_mirr, v_mirr, factor, lambda);
				}
			}
		}
		
		vprev = v;
		vprev_mirr = v_mirr;
	}
	
	return tot;
}

/* apply tool to vertices of edges inside the filter area,
 * using the closest edge point for weighting
 */
static int hair_tool_apply_edge(HairToolData *data, EdgeToolCb cb, void *userdata)
{
	BMVert *root;
	BMIter iter;
	int tot = 0;
	
	if (hair_use_mirror_x(data->ob))
		ED_strands_mirror_cache_begin(data->edit, 0, false, false, hair_use_mirror_topology(data->ob));
	
	BM_ITER_STRANDS(root, &iter, data->edit->bm, BM_STRANDS_OF_MESH) {
		tot += hair_tool_apply_strand_edges(data, cb, userdata, root);
	}
	
	/* apply mirror */
	if (hair_use_mirror_x(data->ob))
		ED_strands_mirror_cache_end(data->edit);
	
	return tot;
}

/* ------------------------------------------------------------------------- */

typedef struct CombData {
	float power;
} CombData;

static void UNUSED_FUNCTION(hair_vertex_comb)(HairToolData *data, void *userdata, BMVert *v, float factor)
{
	CombData *combdata = userdata;
	
	float combfactor = powf(factor, combdata->power);
	
	madd_v3_v3fl(v->co, data->delta, combfactor);
}

/* Edge-based combing tool:
 * Unlike the vertex tool (which simply displaces vertices), the edge tool
 * adjusts edge orientations to follow the stroke direction.
 */
static void hair_edge_comb(HairToolData *data, void *userdata, BMVert *v1, BMVert *v2, float factor, float UNUSED(edge_param))
{
	CombData *combdata = userdata;
	float strokedir[3], edge[3], edgedir[3], strokelen, edgelen;
	float edge_proj[3];
	
	float combfactor = powf(factor, combdata->power);
	float effect;
	
	strokelen = normalize_v3_v3(strokedir, data->delta);
	
	sub_v3_v3v3(edge, v2->co, v1->co);
	edgelen = normalize_v3_v3(edgedir, edge);
	if (edgelen == 0.0f)
		return;
	
	/* This factor prevents sudden changes in direction with small stroke lengths.
	 * The arctan maps the 0..inf range of the length ratio to 0..1 smoothly.
	 */
	effect = atan(strokelen / edgelen * 4.0f / (0.5f*M_PI));
	
	mul_v3_v3fl(edge_proj, strokedir, edgelen);
	
	interp_v3_v3v3(edge, edge, edge_proj, combfactor * effect);
	
	add_v3_v3v3(v2->co, v1->co, edge);
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
	const float len = 1.5f;
	
	float root_mat[4][4];
	BMVert *root, *v;
	BMIter iter;
	int i;
	
	{
		float co[3], nor[3], tang[3];
		BKE_mesh_sample_eval(dm, sample, co, nor, tang);
		construct_m4_loc_nor_tan(root_mat, co, nor, tang);
	}
	
	root = BM_strands_create(edit->bm, 5, true);
	
	BM_elem_meshsample_data_named_set(&edit->bm->vdata, root, CD_MSURFACE_SAMPLE, CD_HAIR_ROOT_LOCATION, sample);
	
	BM_ITER_STRANDS_ELEM_INDEX(v, &iter, root, BM_VERTS_OF_STRAND, i) {
		float co[3];
		
		co[0] = co[1] = 0.0f;
		co[2] = len * (float)i / (float)(len - 1);
		
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
	MSurfaceSample sample;
	
	if (!hair_get_surface_sample(data, &sample))
		return false;
	
	grow_hair(data->edit, &sample);
	
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
			
			/*tot = hair_tool_apply_vertex(data, hair_vertex_comb, &combdata);*/ /* UNUSED */
			tot = hair_tool_apply_edge(data, hair_edge_comb, &combdata);
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

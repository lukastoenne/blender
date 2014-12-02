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
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BKE_edithair.h"

#include "bmesh.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_view3d.h"

#include "hair_intern.h"

BLI_INLINE bool test_depth(HairToolData *data, const float co[3], const int screen_co[2])
{
	View3D *v3d = data->vc.v3d;
	ViewDepths *vd = data->vc.rv3d->depths;
	const bool has_zbuf = (v3d->drawtype > OB_WIRE) && (v3d->flag & V3D_ZBUF_SELECT);
	
	double ux, uy, uz;
	float depth;
	
	/* nothing to do */
	if (!has_zbuf)
		return true;
	
	gluProject(co[0], co[1], co[2],
	           data->mats.modelview, data->mats.projection, data->mats.viewport,
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

BLI_INLINE bool test_inside_circle(HairToolData *data, BMVert *v, float radsq, float *r_dist)
{
	float co_world[3];
	float dx, dy, distsq;
	int screen_co[2];
	
	mul_v3_m4v3(co_world, data->ob->obmat, v->co);
	
	/* TODO, should this check V3D_PROJ_TEST_CLIP_BB too? */
	if (ED_view3d_project_int_global(data->vc.ar, co_world, screen_co, V3D_PROJ_TEST_CLIP_WIN) != V3D_PROJ_RET_OK)
		return false;
	
	dx = data->mval[0] - (float)screen_co[0];
	dy = data->mval[1] - (float)screen_co[1];
	distsq = dx * dx + dy * dy;
	
	if (distsq > radsq)
		return false;
	
	if (test_depth(data, v->co, screen_co)) {
		*r_dist = sqrtf(distsq);
		return true;
	}
	else
		return false;
}

BLI_INLINE float factor_vertex(HairToolData *data, BMVert *v)
{
	const float rad = data->settings->brush->size * 0.5f;
	const float radsq = rad*rad;
	
	float dist;
	
	if (!test_inside_circle(data, v, radsq, &dist))
		return 0.0f;
	
	return 1.0f; // TODO
}

/* ------------------------------------------------------------------------- */

typedef void (*VertexToolCb)(HairToolData *data, BMVert *v, float factor);

static int hair_tool_apply_vertex(HairToolData *data, VertexToolCb cb)
{
	const float threshold = 0.0f; /* XXX could be useful, is it needed? */
	
	BMVert *v;
	BMIter iter;
	int tot = 0;
	
	BM_ITER_MESH(v, &iter, data->edit->bm, BM_VERTS_OF_MESH) {
		float factor = factor_vertex(data, v);
		if (factor > threshold) {
			cb(data, v, factor);
			++tot;
		}
	}
	
	return tot;
}

/* ------------------------------------------------------------------------- */

static void hair_vertex_comb(HairToolData *data, BMVert *v, float factor)
{
	madd_v3_v3fl(v->co, data->delta, factor);
}

bool hair_brush_step(HairToolData *data)
{
	Brush *brush = data->settings->brush;
	BrushHairTool hair_tool = brush->hair_tool;
	int tot = 0;
	
	switch (hair_tool) {
		case HAIR_TOOL_COMB:    tot = hair_tool_apply_vertex(data, hair_vertex_comb);       break;
		case HAIR_TOOL_CUT:     break;
		case HAIR_TOOL_LENGTH:  break;
		case HAIR_TOOL_PUFF:    break;
		case HAIR_TOOL_ADD:     break;
		case HAIR_TOOL_SMOOTH:  break;
		case HAIR_TOOL_WEIGHT:  break;
	}
	
	return tot > 0;
}

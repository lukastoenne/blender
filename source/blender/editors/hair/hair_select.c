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

/** \file blender/editors/hair/hair_select.c
 *  \ingroup edhair
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_edithair.h"

#include "bmesh.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_physics.h"
#include "ED_view3d.h"

#include "hair_intern.h"

typedef bool (*TestVertexCb)(void *userdata, struct BMVert *v);

static int hair_select_verts(BMEditStrands *edit, HairEditSelectMode select_mode, bool select, TestVertexCb cb, void *userdata)
{
	BMesh *bm = edit->bm;
	
	BMVert *v;
	BMIter iter;
	int tot = 0;
	
	bm->selectmode = BM_VERT;
	
	switch (select_mode) {
		case HAIR_SELECT_STRAND:
			break;
		case HAIR_SELECT_VERTEX:
			BM_ITER_MESH(v, &iter, edit->bm, BM_VERTS_OF_MESH) {
				if (BM_elem_flag_test_bool(v, BM_ELEM_SELECT) == select)
					continue;
				if (!cb(userdata, v))
					continue;
				
				BM_elem_flag_set(v, BM_ELEM_SELECT, select);
				++tot;
			}
			break;
		case HAIR_SELECT_TIP:
			BM_ITER_MESH(v, &iter, edit->bm, BM_VERTS_OF_MESH) {
				if (BM_elem_flag_test_bool(v, BM_ELEM_SELECT) == select)
					continue;
				if (!BM_strands_vert_is_tip(v))
					continue;
				if (!cb(userdata, v))
					continue;
				
				BM_elem_flag_set(v, BM_ELEM_SELECT, select);
				++tot;
			}
			break;
	}
	
	BM_mesh_select_mode_flush(bm);
	
	return tot;
}

/* ------------------------------------------------------------------------- */

typedef struct TestVertexCirleData {
	HairViewData viewdata;
	float mval[2];
	float radsq;
} TestVertexCirleData;

static bool test_vertex_circle(void *userdata, struct BMVert *v)
{
	TestVertexCirleData *data = userdata;
	float dist;
	
	return hair_test_inside_circle(&data->viewdata, data->mval, data->radsq, v, &dist);
}

int ED_hair_circle_select(bContext *C, bool select, const int mval[2], float radius)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	BMEditStrands *edit = BKE_editstrands_from_object(ob);
	HairEditSettings *settings = &scene->toolsettings->hair_edit;
	
	TestVertexCirleData data;
	int tot;
	
	if (!edit)
		return 0;
	
	hair_init_viewdata(C, &data.viewdata);
	data.mval[0] = mval[0];
	data.mval[1] = mval[1];
	data.radsq = radius * radius;
	
	tot = hair_select_verts(edit, settings->select_mode, select, test_vertex_circle, &data);
	
	return tot;
}

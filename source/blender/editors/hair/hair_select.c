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
#include "BLI_rect.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_editstrands.h"

#include "bmesh.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_physics.h"
#include "ED_view3d.h"

#include "hair_intern.h"

BLI_INLINE bool apply_select_action_flag(BMVert *v, int action)
{
	bool cursel = BM_elem_flag_test_bool(v, BM_ELEM_SELECT);
	bool newsel;
	
	switch (action) {
		case SEL_SELECT:
			newsel = true;
			break;
		case SEL_DESELECT:
			newsel = false;
			break;
		case SEL_INVERT:
			newsel = !cursel;
			break;
		case SEL_TOGGLE:
			/* toggle case should be converted to SELECT or DESELECT based on global state */
			BLI_assert(false);
			break;
	}
	
	if (newsel != cursel) {
		BM_elem_flag_set(v, BM_ELEM_SELECT, newsel);
		return true;
	}
	else
		return false;
}

/* poll function */
typedef bool (*PollVertexCb)(void *userdata, struct BMVert *v);
/* distance metric function */
typedef bool (*DistanceVertexCb)(void *userdata, struct BMVert *v, float *dist);

static int hair_select_verts_filter(BMEditStrands *edit, HairEditSelectMode select_mode, int action, PollVertexCb cb, void *userdata)
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
				if (!cb(userdata, v))
					continue;
				
				if (apply_select_action_flag(v, action))
					++tot;
			}
			break;
		case HAIR_SELECT_TIP:
			BM_ITER_MESH(v, &iter, edit->bm, BM_VERTS_OF_MESH) {
				if (!BM_strands_vert_is_tip(v))
					continue;
				if (!cb(userdata, v))
					continue;
				
				if (apply_select_action_flag(v, action))
					++tot;
			}
			break;
	}
	
	BM_mesh_select_mode_flush(bm);
	
	return tot;
}

static int hair_select_verts_closest(BMEditStrands *edit, HairEditSelectMode select_mode, int action, DistanceVertexCb cb, void *userdata)
{
	BMesh *bm = edit->bm;
	
	BMVert *v;
	BMIter iter;
	int tot = 0;
	
	float dist;
	BMVert *closest_v = NULL;
	float closest_dist = FLT_MAX;
	
	bm->selectmode = BM_VERT;
	
	switch (select_mode) {
		case HAIR_SELECT_STRAND:
			break;
		case HAIR_SELECT_VERTEX:
			BM_ITER_MESH(v, &iter, edit->bm, BM_VERTS_OF_MESH) {
				if (!cb(userdata, v, &dist))
					continue;
				
				if (dist < closest_dist) {
					closest_v = v;
					closest_dist = dist;
				}
			}
			break;
		case HAIR_SELECT_TIP:
			BM_ITER_MESH(v, &iter, edit->bm, BM_VERTS_OF_MESH) {
				if (!BM_strands_vert_is_tip(v))
					continue;
				if (!cb(userdata, v, &dist))
					continue;
				
				if (dist < closest_dist) {
					closest_v = v;
					closest_dist = dist;
				}
			}
			break;
	}
	
	if (closest_v) {
		if (apply_select_action_flag(closest_v, action))
			++tot;
	}
	
	BM_mesh_select_mode_flush(bm);
	
	return tot;
}

static void hair_deselect_all(BMEditStrands *edit)
{
	BMVert *v;
	BMIter iter;
	
	BM_ITER_MESH(v, &iter, edit->bm, BM_VERTS_OF_MESH) {
		BM_elem_flag_set(v, BM_ELEM_SELECT, false);
	}
}

/* ------------------------------------------------------------------------- */

/************************ select/deselect all operator ************************/

static bool poll_vertex_all(void *UNUSED(userdata), struct BMVert *UNUSED(v))
{
	return true;
}

static int select_all_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	BMEditStrands *edit = BKE_editstrands_from_object(ob);
	HairEditSettings *settings = &scene->toolsettings->hair_edit;
	int action = RNA_enum_get(op->ptr, "action");
	
	if (!edit)
		return 0;
	
	/* toggle action depends on current global selection state */
	if (action == SEL_TOGGLE) {
		if (edit->bm->totvertsel == 0)
			action = SEL_SELECT;
		else
			action = SEL_DESELECT;
	}
	
	hair_select_verts_filter(edit, settings->select_mode, action, poll_vertex_all, NULL);
	
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW | NA_SELECTED, ob);
	
	return OPERATOR_FINISHED;
}

void HAIR_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select/Deselect All";
	ot->idname = "HAIR_OT_select_all";
	ot->description = "Select/Deselect all hair vertices";
	
	/* api callbacks */
	ot->exec = select_all_exec;
	ot->poll = hair_edit_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	WM_operator_properties_select_all(ot);
}

/************************ mouse select operator ************************/

typedef struct DistanceVertexCirleData {
	HairViewData viewdata;
	float mval[2];
	float radsq;
} DistanceVertexCirleData;

static bool distance_vertex_circle(void *userdata, struct BMVert *v, float *dist)
{
	DistanceVertexCirleData *data = userdata;
	
	return hair_test_vertex_inside_circle(&data->viewdata, data->mval, data->radsq, v, dist);
}

int ED_hair_mouse_select(bContext *C, const int mval[2], bool extend, bool deselect, bool toggle)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	BMEditStrands *edit = BKE_editstrands_from_object(ob);
	HairEditSettings *settings = &scene->toolsettings->hair_edit;
	float select_radius = ED_view3d_select_dist_px();
	
	DistanceVertexCirleData data;
	int action;
	
	if (!extend && !deselect && !toggle) {
		hair_deselect_all(edit);
	}
	
	hair_init_viewdata(C, &data.viewdata);
	data.mval[0] = mval[0];
	data.mval[1] = mval[1];
	data.radsq = select_radius * select_radius;
	
	if (extend)
		action = SEL_SELECT;
	else if (deselect)
		action = SEL_DESELECT;
	else
		action = SEL_INVERT;
	
	hair_select_verts_closest(edit, settings->select_mode, action, distance_vertex_circle, &data);
	
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW | NA_SELECTED, ob);
	
	return OPERATOR_FINISHED;
}

/************************ border select operator ************************/

typedef struct PollVertexRectData {
	HairViewData viewdata;
	rcti rect;
} PollVertexRectData;

static bool poll_vertex_inside_rect(void *userdata, struct BMVert *v)
{
	PollVertexRectData *data = userdata;
	
	return hair_test_vertex_inside_rect(&data->viewdata, &data->rect, v);
}

int ED_hair_border_select(bContext *C, rcti *rect, bool select, bool extend)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	BMEditStrands *edit = BKE_editstrands_from_object(ob);
	HairEditSettings *settings = &scene->toolsettings->hair_edit;
	
	PollVertexRectData data;
	int action;
	
	if (!extend && select)
		hair_deselect_all(edit);
	
	hair_init_viewdata(C, &data.viewdata);
	data.rect = *rect;
	
	if (extend)
		action = SEL_SELECT;
	else if (select)
		action = SEL_INVERT;
	else
		action = SEL_DESELECT;
	
	hair_select_verts_filter(edit, settings->select_mode, action, poll_vertex_inside_rect, &data);
	
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW | NA_SELECTED, ob);
	
	return OPERATOR_FINISHED;
}

/************************ circle select operator ************************/

typedef struct PollVertexCirleData {
	HairViewData viewdata;
	float mval[2];
	float radsq;
} PollVertexCirleData;

static bool poll_vertex_inside_circle(void *userdata, struct BMVert *v)
{
	PollVertexCirleData *data = userdata;
	float dist;
	
	return hair_test_vertex_inside_circle(&data->viewdata, data->mval, data->radsq, v, &dist);
}

int ED_hair_circle_select(bContext *C, bool select, const int mval[2], float radius)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	BMEditStrands *edit = BKE_editstrands_from_object(ob);
	HairEditSettings *settings = &scene->toolsettings->hair_edit;
	int action = select ? SEL_SELECT : SEL_DESELECT;
	
	PollVertexCirleData data;
	int tot;
	
	if (!edit)
		return 0;
	
	hair_init_viewdata(C, &data.viewdata);
	data.mval[0] = mval[0];
	data.mval[1] = mval[1];
	data.radsq = radius * radius;
	
	tot = hair_select_verts_filter(edit, settings->select_mode, action, poll_vertex_inside_circle, &data);
	
	return tot;
}

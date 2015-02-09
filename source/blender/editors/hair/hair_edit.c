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
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_brush.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_editstrands.h"
#include "BKE_paint.h"
#include "BKE_particle.h"

#include "bmesh.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_physics.h"
#include "ED_view3d.h"

#include "hair_intern.h"
#include "paint_intern.h"

static bool has_hair_data(Object *ob)
{
	ParticleSystem *psys = psys_get_current(ob);
	if (psys && psys->part->type == PART_HAIR)
		return true;
	
	return false;
}

static bool init_hair_edit(Scene *scene, Object *ob)
{
	ParticleSystem *psys = psys_get_current(ob);
	BMesh *bm;
	DerivedMesh *dm;
	
	if (psys && psys->part->type == PART_HAIR) {
		if (!psys->hairedit) {
			bm = BKE_particles_to_bmesh(ob, psys);
			
			if (ob->type == OB_MESH || ob->derivedFinal)
				dm = ob->derivedFinal ? ob->derivedFinal : mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
			else
				dm = NULL;
			
			psys->hairedit = BKE_editstrands_create(bm, dm);
		}
		return true;
	}
	
	return false;
}

static bool apply_hair_edit(Object *ob)
{
	ParticleSystem *psys = psys_get_current(ob);
	if (psys->part->type == PART_HAIR) {
		if (psys->hairedit) {
			BKE_particles_from_bmesh(ob, psys);
			psys->flag |= PSYS_EDITED;
			
			BKE_editstrands_free(psys->hairedit);
			MEM_freeN(psys->hairedit);
			psys->hairedit = NULL;
		}
		
		return true;
	}
	
	return false;
}

int hair_edit_poll(bContext *C)
{
	Object *obact;
	
	obact = CTX_data_active_object(C);
	if ((obact && obact->mode & OB_MODE_HAIR_EDIT) && CTX_wm_region_view3d(C)) {
		return true;
	}
	
	return false;
}

bool hair_use_mirror_x(Object *ob)
{
	if (ob->type == OB_MESH)
		return ((Mesh *)ob->data)->editflag & ME_EDIT_MIRROR_X;
	else
		return false;
}

bool hair_use_mirror_topology(Object *ob)
{
	if (ob->type == OB_MESH)
		return ((Mesh *)ob->data)->editflag & ME_EDIT_MIRROR_TOPO;
	else
		return false;
}


/* ==== BMesh utilities ==== */

void hair_bm_min_max(BMEditStrands *edit, float min[3], float max[3])
{
	BMVert *v;
	BMIter iter;
	
	if (edit->bm->totvert > 0) {
		INIT_MINMAX(min, max);
		
		BM_ITER_MESH(v, &iter, edit->bm, BM_VERTS_OF_MESH) {
			minmax_v3v3_v3(min, max, v->co);
		}
	}
	else {
		zero_v3(min);
		zero_v3(max);
	}
}


/* ==== edit mode toggle ==== */

int hair_edit_toggle_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	if (ob == NULL)
		return false;
	if (!ob->data || ((ID *)ob->data)->lib)
		return false;
	if (CTX_data_edit_object(C))
		return false;

	return has_hair_data(ob);
}

static void toggle_hair_cursor(bContext *C, bool enable)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	Scene *scene = CTX_data_scene(C);
	HairEditSettings *settings = &scene->toolsettings->hair_edit;

	if (enable) {
		hair_edit_cursor_start(C, hair_edit_toggle_poll);
	}
	else {
		if (settings->paint_cursor) {
			WM_paint_cursor_end(wm, settings->paint_cursor);
			settings->paint_cursor = NULL;
		}
	}
}

static int hair_edit_toggle_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	const int mode_flag = OB_MODE_HAIR_EDIT;
	const bool is_mode_set = (ob->mode & mode_flag) != 0;

	if (!is_mode_set) {
		if (!ED_object_mode_compat_set(C, ob, mode_flag, op->reports)) {
			return OPERATOR_CANCELLED;
		}
	}

	if (!is_mode_set) {
		init_hair_edit(scene, ob);
		ob->mode |= mode_flag;
		
		toggle_hair_cursor(C, true);
		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_MODE_HAIR, NULL);
	}
	else {
		apply_hair_edit(ob);
		ob->mode &= ~mode_flag;
		
		toggle_hair_cursor(C, false);
		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_MODE_HAIR, NULL);
	}

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

	return OPERATOR_FINISHED;
}

void HAIR_OT_hair_edit_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Hair Edit Toggle";
	ot->idname = "HAIR_OT_hair_edit_toggle";
	ot->description = "Toggle hair edit mode";
	
	/* api callbacks */
	ot->exec = hair_edit_toggle_exec;
	ot->poll = hair_edit_toggle_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


/* ==== brush stroke ==== */

void hair_init_viewdata(bContext *C, HairViewData *viewdata)
{
	View3D *v3d;
	bool has_zbuf;
	
	view3d_set_viewcontext(C, &viewdata->vc);
	
	v3d = viewdata->vc.v3d;
	has_zbuf = (v3d->drawtype > OB_WIRE) && (v3d->flag & V3D_ZBUF_SELECT);
	
	view3d_get_transformation(viewdata->vc.ar, viewdata->vc.rv3d, NULL, &viewdata->mats);
	
	if (has_zbuf) {
		if (v3d->flag & V3D_INVALID_BACKBUF) {
			/* needed or else the draw matrix can be incorrect */
			view3d_operator_needs_opengl(C);
			
			view3d_validate_backbuf(&viewdata->vc);
			/* we may need to force an update here by setting the rv3d as dirty
			 * for now it seems ok, but take care!:
			 * rv3d->depths->dirty = 1; */
			ED_view3d_depth_update(viewdata->vc.ar);
		}
	}
}

typedef struct HairStroke {
	Scene *scene;
	Object *ob;
	BMEditStrands *edit;
	
	bool first;
	float lastmouse[2];
	float zfac;
	
	float smoothdir[2];
} HairStroke;

static int hair_stroke_init(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	BMEditStrands *edit = BKE_editstrands_from_object(ob);
	ARegion *ar = CTX_wm_region(C);
	
	HairStroke *stroke;
	float min[3], max[3], center[3];
	
	/* set the 'distance factor' for grabbing (used in comb etc) */
	hair_bm_min_max(edit, min, max);
	mid_v3_v3v3(center, min, max);

	stroke = MEM_callocN(sizeof(HairStroke), "HairStroke");
	stroke->first = true;
	op->customdata = stroke;

	stroke->scene = scene;
	stroke->ob = ob;
	stroke->edit = edit;

	stroke->zfac = ED_view3d_calc_zfac(ar->regiondata, center, NULL);

	return 1;
}

static bool hair_stroke_apply(bContext *C, wmOperator *op, PointerRNA *itemptr)
{
	HairStroke *stroke = op->customdata;
	Scene *scene = stroke->scene;
	Object *ob = stroke->ob;
	BMEditStrands *edit = stroke->edit;
	HairEditSettings *settings = &scene->toolsettings->hair_edit;
	ARegion *ar = CTX_wm_region(C);
	const float smoothfac = 0.9f; /* XXX should this be configurable? */
	
	float mouse[2], mdelta[2], zvec[3], delta_max;
	int totsteps, step;
	HairToolData tool_data;
	bool updated = false;
	
	RNA_float_get_array(itemptr, "mouse", mouse);
	
	if (stroke->first) {
		copy_v2_v2(stroke->lastmouse, mouse);
		zero_v2(stroke->smoothdir);
		stroke->first = false;
	}
	
	if (!settings->brush)
		return false;
	
	sub_v2_v2v2(mdelta, mouse, stroke->lastmouse);
	delta_max = max_ff(fabsf(mdelta[0]), fabsf(mdelta[1]));
	
	totsteps = delta_max / (0.2f * BKE_brush_size_get(scene, settings->brush)) + 1;
	mul_v2_fl(mdelta, 1.0f / (float)totsteps);
	
	/* low-pass filter to smooth out jittery pixel increments in the direction */
	interp_v2_v2v2(stroke->smoothdir, mdelta, stroke->smoothdir, smoothfac);
	
	hair_init_viewdata(C, &tool_data.viewdata);
	tool_data.scene = scene;
	tool_data.ob = ob;
	tool_data.edit = edit;
	tool_data.settings = settings;
	
	invert_m4_m4(tool_data.imat, ob->obmat);
	copy_v2_v2(tool_data.mval, mouse);
	tool_data.mdepth = stroke->zfac;
	
	zvec[0] = 0.0f; zvec[1] = 0.0f; zvec[2] = stroke->zfac;
	ED_view3d_win_to_3d(ar, zvec, mouse, tool_data.loc);
	ED_view3d_win_to_delta(ar, stroke->smoothdir, tool_data.delta, stroke->zfac);
	/* tools work in object space */
	mul_m4_v3(tool_data.imat, tool_data.loc);
	mul_mat3_m4_v3(tool_data.imat, tool_data.delta);

	for (step = 0; step < totsteps; ++step) {
		bool step_updated = hair_brush_step(&tool_data);
		
		if (step_updated)
			BKE_editstrands_solve_constraints(ob, edit, NULL);
		
		updated |= step_updated;
	}
	
	copy_v2_v2(stroke->lastmouse, mouse);
	
	return updated;
}

static void hair_stroke_exit(wmOperator *op)
{
	HairStroke *stroke = op->customdata;
	MEM_freeN(stroke);
}

static int hair_stroke_exec(bContext *C, wmOperator *op)
{
	HairStroke *stroke = op->customdata;
	Object *ob = stroke->ob;
	
	bool updated = false;
	
	if (!hair_stroke_init(C, op))
		return OPERATOR_CANCELLED;
	
	RNA_BEGIN (op->ptr, itemptr, "stroke")
	{
		updated |= hair_stroke_apply(C, op, &itemptr);
	}
	RNA_END;
	
	if (updated) {
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	}
	
	hair_stroke_exit(op);
	
	return OPERATOR_FINISHED;
}

static void hair_stroke_apply_event(bContext *C, wmOperator *op, const wmEvent *event)
{
	HairStroke *stroke = op->customdata;
	Object *ob = stroke->ob;
	
	PointerRNA itemptr;
	float mouse[2];
	bool updated = false;

	mouse[0] = event->mval[0];
	mouse[1] = event->mval[1];

	/* fill in stroke */
	RNA_collection_add(op->ptr, "stroke", &itemptr);

	RNA_float_set_array(&itemptr, "mouse", mouse);

	/* apply */
	updated |= hair_stroke_apply(C, op, &itemptr);

	if (updated) {
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	}
	else {
		/* even if nothing was changed, still trigger redraw
		 * for brush drawing during the modal operator
		 */
		WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	}
}

static int hair_stroke_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	if (!hair_stroke_init(C, op))
		return OPERATOR_CANCELLED;
	
	hair_stroke_apply_event(C, op, event);

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int hair_stroke_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	switch (event->type) {
		case LEFTMOUSE:
		case MIDDLEMOUSE:
		case RIGHTMOUSE: // XXX hardcoded
			hair_stroke_exit(op);
			return OPERATOR_FINISHED;
		case MOUSEMOVE:
			hair_stroke_apply_event(C, op, event);
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

static void hair_stroke_cancel(bContext *UNUSED(C), wmOperator *op)
{
	hair_stroke_exit(op);
}

void HAIR_OT_stroke(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Hair Stroke";
	ot->idname = "HAIR_OT_stroke";
	ot->description = "Use a stroke tool on hair strands";

	/* api callbacks */
	ot->exec = hair_stroke_exec;
	ot->invoke = hair_stroke_invoke;
	ot->modal = hair_stroke_modal;
	ot->cancel = hair_stroke_cancel;
	ot->poll = hair_edit_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* properties */
	RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
}

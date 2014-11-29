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
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_edithair.h"
#include "BKE_paint.h"
#include "BKE_particle.h"

#include "RNA_access.h"

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
	if (psys->part->type == PART_HAIR)
		return true;
	
	return false;
}

static bool init_hair_edit(Object *ob)
{
	ParticleSystem *psys = psys_get_current(ob);
	if (psys->part->type == PART_HAIR) {
		if (!psys->hairedit) {
			BMesh *bm = BKE_particles_to_bmesh(ob, psys);
			psys->hairedit = BKE_editstrands_create(bm);
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

static int hair_edit_toggle_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	const int mode_flag = OB_MODE_HAIR_EDIT;
	const bool is_mode_set = (ob->mode & mode_flag) != 0;

	if (!is_mode_set) {
		if (!ED_object_mode_compat_set(C, ob, mode_flag, op->reports)) {
			return OPERATOR_CANCELLED;
		}
	}

	if (!is_mode_set) {
		init_hair_edit(ob);
		ob->mode |= mode_flag;
		
//		toggle_particle_cursor(C, 1);
		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_MODE_HAIR, NULL);
	}
	else {
		apply_hair_edit(ob);
		ob->mode &= ~mode_flag;
		
//		toggle_particle_cursor(C, 0);
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

typedef struct HairStrokeOperation {
	void *custom_paint;

	float prevmouse[2];
	float startmouse[2];
	double starttime;

	void *cursor;
	ViewContext vc;
} HairStrokeOperation;

static HairStrokeOperation *hair_stroke_init(bContext *C, wmOperator *op, const float mouse[2])
{
	Scene *scene = CTX_data_scene(C);
//	ToolSettings *settings = scene->toolsettings;
	HairStrokeOperation *pop = MEM_callocN(sizeof(HairStrokeOperation), "PaintOperation"); /* caller frees */
//	Brush *brush = BKE_paint_brush(&settings->imapaint.paint);
	int mode = RNA_enum_get(op->ptr, "mode");
	view3d_set_viewcontext(C, &pop->vc);

	copy_v2_v2(pop->prevmouse, mouse);
	copy_v2_v2(pop->startmouse, mouse);

	/* initialize from context */
	if (CTX_wm_region_view3d(C)) {
		Object *ob = OBACT;
		
		pop->custom_paint = paint_proj_new_stroke(C, ob, mouse, mode);
	}

	if (!pop->custom_paint) {
		MEM_freeN(pop);
		return NULL;
	}

//	if ((brush->imagepaint_tool == PAINT_TOOL_FILL) && (brush->flag & BRUSH_USE_GRADIENT)) {
//		pop->cursor = WM_paint_cursor_activate(CTX_wm_manager(C), image_paint_poll, gradient_draw_line, pop);
//	}
	
//	ED_undo_paint_push_begin(UNDO_PAINT_IMAGE, op->type->name,
//	                         ED_image_undo_restore, ED_image_undo_free, NULL);

	return pop;
}

/* restore painting image to previous state. Used for anchored and drag-dot style brushes*/
static void hair_stroke_restore(void)
{
	/* XXX TODO */
//	ListBase *lb = undo_paint_push_get_list(UNDO_PAINT_IMAGE);
//	image_undo_restore_runtime(lb);
//	image_undo_invalidate();
}

static void hair_stroke_update_step(bContext *C, struct PaintStroke *stroke, PointerRNA *itemptr)
{
	HairStrokeOperation *pop = paint_stroke_mode_data(stroke);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *toolsettings = CTX_data_tool_settings(C);
	UnifiedPaintSettings *ups = &toolsettings->unified_paint_settings;
	Brush *brush = BKE_paint_brush(&toolsettings->imapaint.paint);

	float alphafac = (brush->flag & BRUSH_ACCUMULATE) ? ups->overlap_factor : 1.0f;

	/* initial brush values. Maybe it should be considered moving these to stroke system */
	float startalpha = BKE_brush_alpha_get(scene, brush);

	float mouse[2];
	float pressure;
	float size;
	float distance = paint_stroke_distance_get(stroke);
	int eraser;

	RNA_float_get_array(itemptr, "mouse", mouse);
	pressure = RNA_float_get(itemptr, "pressure");
	eraser = RNA_boolean_get(itemptr, "pen_flip");
	size = max_ff(1.0f, RNA_float_get(itemptr, "size"));

	/* stroking with fill tool only acts on stroke end */
	if (brush->imagepaint_tool == PAINT_TOOL_FILL) {
		copy_v2_v2(pop->prevmouse, mouse);
		return;
	}

	if (BKE_brush_use_alpha_pressure(scene, brush))
		BKE_brush_alpha_set(scene, brush, max_ff(0.0f, startalpha * pressure * alphafac));
	else
		BKE_brush_alpha_set(scene, brush, max_ff(0.0f, startalpha * alphafac));

	if ((brush->flag & BRUSH_DRAG_DOT) || (brush->flag & BRUSH_ANCHORED)) {
		hair_stroke_restore();
	}

	paint_proj_stroke(C, pop->custom_paint, pop->prevmouse, mouse, eraser, pressure, distance, size);

	copy_v2_v2(pop->prevmouse, mouse);

	/* restore brush values */
	BKE_brush_alpha_set(scene, brush, startalpha);
}

static void hair_stroke_redraw(const bContext *C, struct PaintStroke *stroke, bool final)
{
	HairStrokeOperation *pop = paint_stroke_mode_data(stroke);

	paint_proj_redraw(C, pop->custom_paint, final);
}

static void hair_stroke_done(const bContext *C, struct PaintStroke *stroke)
{
	Scene *scene = CTX_data_scene(C);
	ToolSettings *toolsettings = scene->toolsettings;
	HairStrokeOperation *pop = paint_stroke_mode_data(stroke);
	Brush *brush = BKE_paint_brush(&toolsettings->imapaint.paint);

	toolsettings->imapaint.flag &= ~IMAGEPAINT_DRAWING;

	if (brush->imagepaint_tool == PAINT_TOOL_FILL) {
		if (brush->flag & BRUSH_USE_GRADIENT) {
			paint_proj_stroke(C, pop->custom_paint, pop->startmouse, pop->prevmouse, paint_stroke_flipped(stroke),
			                  1.0, 0.0, BKE_brush_size_get(scene, brush));
			/* two redraws, one for GPU update, one for notification */
			paint_proj_redraw(C, pop->custom_paint, false);
			paint_proj_redraw(C, pop->custom_paint, true);
		}
		else {
			paint_proj_stroke(C, pop->custom_paint, pop->startmouse, pop->prevmouse, paint_stroke_flipped(stroke),
			                  1.0, 0.0, BKE_brush_size_get(scene, brush));
			/* two redraws, one for GPU update, one for notification */
			paint_proj_redraw(C, pop->custom_paint, false);
			paint_proj_redraw(C, pop->custom_paint, true);
		}
	}
	paint_proj_stroke_done(pop->custom_paint);

	if (pop->cursor) {
		WM_paint_cursor_end(CTX_wm_manager(C), pop->cursor);
	}

	// XXX TODO
//	image_undo_end();

	/* duplicate warning, see texpaint_init */
#if 0
	if (pop->s.warnmultifile)
		BKE_reportf(op->reports, RPT_WARNING, "Image requires 4 color channels to paint: %s", pop->s.warnmultifile);
	if (pop->s.warnpackedfile)
		BKE_reportf(op->reports, RPT_WARNING, "Packed MultiLayer files cannot be painted: %s", pop->s.warnpackedfile);
#endif
	MEM_freeN(pop);
}

static bool hair_stroke_test_start(bContext *C, wmOperator *op, const float mouse[2])
{
	HairStrokeOperation *pop;

	/* TODO Should avoid putting this here. Instead, last position should be requested
	 * from stroke system. */

//	if (!(pop = texture_paint_init(C, op, mouse))) {
		return false;
//	}

	paint_stroke_set_mode_data(op->customdata, pop);

	return true;
}

/* ------------------------------------------------------------------------- */

static Brush *hair_stroke_brush(bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	ToolSettings *settings = scene->toolsettings;

	return BKE_paint_brush(&settings->imapaint.paint);
}

static int hair_stroke_poll(bContext *C)
{
	Object *obact;

	if (!hair_stroke_brush(C))
		return false;

	obact = CTX_data_active_object(C);
	if ((obact && obact->mode & OB_MODE_HAIR_EDIT) && CTX_wm_region_view3d(C)) {
		return true;
	}

	return false;
}

static int hair_stroke_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	int retval;

	op->customdata = paint_stroke_new(C, op, NULL, hair_stroke_test_start,
	                                  hair_stroke_update_step,
	                                  hair_stroke_redraw,
	                                  hair_stroke_done, event->type);

	if ((retval = op->type->modal(C, op, event)) == OPERATOR_FINISHED) {
		paint_stroke_data_free(op);
		return OPERATOR_FINISHED;
	}
	/* add modal handler */
	WM_event_add_modal_handler(C, op);

	OPERATOR_RETVAL_CHECK(retval);
	BLI_assert(retval == OPERATOR_RUNNING_MODAL);

	return OPERATOR_RUNNING_MODAL;
}

static int hair_stroke_exec(bContext *C, wmOperator *op)
{
	HairStrokeOperation *pop;
	PropertyRNA *strokeprop;
	PointerRNA firstpoint;
	float mouse[2];

	strokeprop = RNA_struct_find_property(op->ptr, "stroke");

	if (!RNA_property_collection_lookup_int(op->ptr, strokeprop, 0, &firstpoint))
		return OPERATOR_CANCELLED;

	RNA_float_get_array(&firstpoint, "mouse", mouse);

	if (!(pop = hair_stroke_init(C, op, mouse))) {
		return OPERATOR_CANCELLED;
	}

	op->customdata = paint_stroke_new(C, op, NULL, hair_stroke_test_start,
	                                  hair_stroke_update_step,
	                                  hair_stroke_redraw,
	                                  hair_stroke_done, 0);
	/* frees op->customdata */
	paint_stroke_exec(C, op);

	return OPERATOR_FINISHED;
}

void HAIR_OT_stroke(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Brush Stroke";
	ot->idname = "HAIR_OT_stroke";
	ot->description = "Use a brush tool on hair";

	/* api callbacks */
	ot->invoke = hair_stroke_invoke;
	ot->modal = paint_stroke_modal;
	ot->exec = hair_stroke_exec;
	ot->poll = hair_stroke_poll;
	ot->cancel = paint_stroke_cancel;

	/* flags */
	ot->flag = OPTYPE_BLOCKING;

	paint_stroke_operator_properties(ot);
}

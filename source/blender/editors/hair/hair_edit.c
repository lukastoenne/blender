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
#include "DNA_screen_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_edithair.h"
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

static int hair_stroke_poll(bContext *C)
{
	Object *obact;
	
	obact = CTX_data_active_object(C);
	if ((obact && obact->mode & OB_MODE_HAIR_EDIT) && CTX_wm_region_view3d(C)) {
		return true;
	}
	
	return false;
}

typedef struct HairStroke {
	Scene *scene;
	Object *ob;
	BMEditStrands *edit;
	
	bool first;
	int lastmouse[2];
	float zfac;
	
	/* optional cached view settings to avoid setting on every mousemove */
//	PEData data;
} HairStroke;

static int hair_stroke_init(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
//	ParticleEditSettings *pset = PE_settings(scene);
	BMEditStrands *edit = BKE_editstrands_from_object(ob);
	ARegion *ar = CTX_wm_region(C);
	
	HairStroke *stroke;
	float min[3], max[3], center[3];
	
//	if (pset->brushtype < 0)
//		return 0;

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

	/* cache view depths and settings for re-use */
//	PE_set_view3d_data(C, &stroke->data);

	return 1;
}

static void hair_stroke_apply(bContext *C, wmOperator *op, PointerRNA *itemptr)
{
	HairStroke *stroke = op->customdata;
	Scene *scene = stroke->scene;
	Object *ob = stroke->ob;
	BMEditStrands *edit = stroke->edit;
	ARegion *ar = CTX_wm_region(C);
	
	float mouse[2], mdelta[2], zvec[3], delta_max;
	int totsteps, step;
	HairToolData tool_data;
	
	RNA_float_get_array(itemptr, "mouse", mouse);
	
	if (stroke->first) {
		stroke->lastmouse[0] = mouse[0];
		stroke->lastmouse[1] = mouse[1];
	}
	
	mdelta[0] = mouse[0] - stroke->lastmouse[0];
	mdelta[1] = mouse[1] - stroke->lastmouse[1];
	delta_max = max_ff(fabsf(mdelta[0]), fabsf(mdelta[1]));
	
//	totsteps = delta_max / (0.2f * pe_brush_size_get(scene, brush)) + 1;
	totsteps = 1; // XXX TODO determine brush size for the above
	mul_v2_fl(mdelta, 1.0f / (float)totsteps);
	
	tool_data.scene = scene;
	tool_data.ob = ob;
	tool_data.edit = edit;
	
	copy_v2_v2(tool_data.mval, mouse);
	tool_data.mdepth = stroke->zfac;
	
	zvec[0] = 0.0f; zvec[1] = 0.0f; zvec[2] = stroke->zfac;
	ED_view3d_win_to_3d(ar, zvec, mouse, tool_data.loc);
	ED_view3d_win_to_delta(ar, mdelta, tool_data.delta, stroke->zfac);

	for (step = 0; step < totsteps; ++step) {
		hair_brush_step(&tool_data);
	}
	
	stroke->lastmouse[0] = mouse[0];
	stroke->lastmouse[1] = mouse[1];
	stroke->first = false;
	
//	if (edit->psys) {
//		WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE|NA_EDITED, ob);
//	}
//	else {
//		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
//		WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
//	}


#if 0
	if (((pset->brushtype == PE_BRUSH_ADD) ?
	     (sqrtf(dx * dx + dy * dy) > pset->brush[PE_BRUSH_ADD].step) : (dx != 0 || dy != 0)) || stroke->first)
		PEData data= stroke->data;

		view3d_operator_needs_opengl(C);
		selected= (short)count_selected_keys(scene, edit);

		dmax = max_ff(fabsf(dx), fabsf(dy));
		tot_steps = dmax/(0.2f * pe_brush_size_get(scene, brush)) + 1;

		dx /= (float)tot_steps;
		dy /= (float)tot_steps;

		for (step = 1; step<=tot_steps; step++) {
			mval[0] = stroke->lastmouse[0] + step*dx;
			mval[1] = stroke->lastmouse[1] + step*dy;

			switch (pset->brushtype) {
				case PE_BRUSH_COMB:
				{
					const float mval_f[2] = {dx, dy};
					data.mval= mval;
					data.rad= pe_brush_size_get(scene, brush);

					data.combfac= (brush->strength - 0.5f) * 2.0f;
					if (data.combfac < 0.0f)
						data.combfac= 1.0f - 9.0f * data.combfac;
					else
						data.combfac= 1.0f - data.combfac;

					invert_m4_m4(ob->imat, ob->obmat);

					ED_view3d_win_to_delta(ar, mval_f, vec, stroke->zfac);
					data.dvec= vec;

					foreach_mouse_hit_key(&data, brush_comb, selected);
					break;
				}
				case PE_BRUSH_CUT:
				{
					if (edit->psys && edit->pathcache) {
						data.mval= mval;
						data.rad= pe_brush_size_get(scene, brush);
						data.cutfac= brush->strength;

						if (selected)
							foreach_selected_point(&data, brush_cut);
						else
							foreach_point(&data, brush_cut);

						removed= remove_tagged_particles(ob, edit->psys, pe_x_mirror(ob));
						if (pset->flag & PE_KEEP_LENGTHS)
							recalc_lengths(edit);
					}
					else
						removed= 0;

					break;
				}
				case PE_BRUSH_LENGTH:
				{
					data.mval= mval;
				
					data.rad= pe_brush_size_get(scene, brush);
					data.growfac= brush->strength / 50.0f;

					if (brush->invert ^ flip)
						data.growfac= 1.0f - data.growfac;
					else
						data.growfac= 1.0f + data.growfac;

					foreach_mouse_hit_point(&data, brush_length, selected);

					if (pset->flag & PE_KEEP_LENGTHS)
						recalc_lengths(edit);
					break;
				}
				case PE_BRUSH_PUFF:
				{
					if (edit->psys) {
						data.dm= psmd->dm;
						data.mval= mval;
						data.rad= pe_brush_size_get(scene, brush);
						data.select= selected;

						data.pufffac= (brush->strength - 0.5f) * 2.0f;
						if (data.pufffac < 0.0f)
							data.pufffac= 1.0f - 9.0f * data.pufffac;
						else
							data.pufffac= 1.0f - data.pufffac;

						data.invert= (brush->invert ^ flip);
						invert_m4_m4(ob->imat, ob->obmat);

						foreach_mouse_hit_point(&data, brush_puff, selected);
					}
					break;
				}
				case PE_BRUSH_ADD:
				{
					if (edit->psys && edit->psys->part->from==PART_FROM_FACE) {
						data.mval= mval;

						added= brush_add(&data, brush->count);

						if (pset->flag & PE_KEEP_LENGTHS)
							recalc_lengths(edit);
					}
					else
						added= 0;
					break;
				}
				case PE_BRUSH_SMOOTH:
				{
					data.mval= mval;
					data.rad= pe_brush_size_get(scene, brush);

					data.vec[0] = data.vec[1] = data.vec[2] = 0.0f;
					data.tot= 0;

					data.smoothfac= brush->strength;

					invert_m4_m4(ob->imat, ob->obmat);

					foreach_mouse_hit_key(&data, brush_smooth_get, selected);

					if (data.tot) {
						mul_v3_fl(data.vec, 1.0f / (float)data.tot);
						foreach_mouse_hit_key(&data, brush_smooth_do, selected);
					}

					break;
				}
				case PE_BRUSH_WEIGHT:
				{
					if (edit->psys) {
						data.dm= psmd->dm;
						data.mval= mval;
						data.rad= pe_brush_size_get(scene, brush);

						data.weightfac = brush->strength; /* note that this will never be zero */

						foreach_mouse_hit_key(&data, BKE_brush_weight_get, selected);
					}

					break;
				}
			}
			if ((pset->flag & PE_KEEP_LENGTHS)==0)
				recalc_lengths(edit);

			if (ELEM(pset->brushtype, PE_BRUSH_ADD, PE_BRUSH_CUT) && (added || removed)) {
				if (pset->brushtype == PE_BRUSH_ADD && pe_x_mirror(ob))
					PE_mirror_x(scene, ob, 1);

				update_world_cos(ob, edit);
				psys_free_path_cache(NULL, edit);
				DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
			}
			else
				PE_update_object(scene, ob, 1);
		}
	}
#endif
}

static void hair_stroke_exit(wmOperator *op)
{
	HairStroke *stroke = op->customdata;
	MEM_freeN(stroke);
}

static int hair_stroke_exec(bContext *C, wmOperator *op)
{
	if (!hair_stroke_init(C, op))
		return OPERATOR_CANCELLED;

	RNA_BEGIN (op->ptr, itemptr, "stroke")
	{
		hair_stroke_apply(C, op, &itemptr);
	}
	RNA_END;

	hair_stroke_exit(op);

	return OPERATOR_FINISHED;
}

static void hair_stroke_apply_event(bContext *C, wmOperator *op, const wmEvent *event)
{
	PointerRNA itemptr;
	float mouse[2];

	VECCOPY2D(mouse, event->mval);

	/* fill in stroke */
	RNA_collection_add(op->ptr, "stroke", &itemptr);

	RNA_float_set_array(&itemptr, "mouse", mouse);

	/* apply */
	hair_stroke_apply(C, op, &itemptr);
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
	ot->poll = hair_stroke_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* properties */
	RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
}

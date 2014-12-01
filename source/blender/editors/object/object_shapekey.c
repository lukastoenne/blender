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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, shapekey support
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/object/object_shapekey.c
 *  \ingroup edobj
 */


#include <math.h>
#include <string.h>

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif   

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh_sample.h"
#include "BKE_object.h"

#include "BLI_sys_types.h" // for intptr_t support

#include "BIF_gl.h"
#include "BIF_glutil.h" /* for paint cursor */

#include "ED_object.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_view3d.h"

#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "object_intern.h"

/*********************** add shape key ***********************/

static void ED_object_shape_key_add(bContext *C, Scene *scene, Object *ob, const bool from_mix)
{
	KeyBlock *kb;
	if ((kb = BKE_object_insert_shape_key(scene, ob, NULL, from_mix))) {
		Key *key = BKE_key_from_object(ob);
		/* for absolute shape keys, new keys may not be added last */
		ob->shapenr = BLI_findindex(&key->block, kb) + 1;

		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	}
}

/*********************** remove shape key ***********************/

static bool ED_object_shape_key_remove_all(Main *bmain, Object *ob)
{
	Key *key;

	key = BKE_key_from_object(ob);
	if (key == NULL)
		return false;

	switch (GS(key->from->name)) {
		case ID_ME: ((Mesh *)key->from)->key    = NULL; break;
		case ID_CU: ((Curve *)key->from)->key   = NULL; break;
		case ID_LT: ((Lattice *)key->from)->key = NULL; break;
	}

	BKE_libblock_free_us(bmain, key);

	return true;
}

static bool ED_object_shape_key_remove(Main *bmain, Object *ob)
{
	KeyBlock *kb, *rkb;
	Key *key;

	key = BKE_key_from_object(ob);
	if (key == NULL)
		return false;

	kb = BLI_findlink(&key->block, ob->shapenr - 1);

	if (kb) {
		for (rkb = key->block.first; rkb; rkb = rkb->next)
			if (rkb->relative == ob->shapenr - 1)
				rkb->relative = 0;

		BLI_remlink(&key->block, kb);
		key->totkey--;
		if (key->refkey == kb) {
			key->refkey = key->block.first;

			if (key->refkey) {
				/* apply new basis key on original data */
				switch (ob->type) {
					case OB_MESH:
						BKE_keyblock_convert_to_mesh(key->refkey, ob->data);
						break;
					case OB_CURVE:
					case OB_SURF:
						BKE_keyblock_convert_to_curve(key->refkey, ob->data, BKE_curve_nurbs_get(ob->data));
						break;
					case OB_LATTICE:
						BKE_keyblock_convert_to_lattice(key->refkey, ob->data);
						break;
				}
			}
		}
			
		if (kb->data) MEM_freeN(kb->data);
		MEM_freeN(kb);

		if (ob->shapenr > 1) {
			ob->shapenr--;
		}
	}
	
	if (key->totkey == 0) {
		switch (GS(key->from->name)) {
			case ID_ME: ((Mesh *)key->from)->key    = NULL; break;
			case ID_CU: ((Curve *)key->from)->key   = NULL; break;
			case ID_LT: ((Lattice *)key->from)->key = NULL; break;
		}

		BKE_libblock_free_us(bmain, key);
	}

	return true;
}

static bool object_shape_key_mirror(bContext *C, Object *ob,
                                    int *r_totmirr, int *r_totfail, bool use_topology)
{
	KeyBlock *kb;
	Key *key;
	int totmirr = 0, totfail = 0;

	*r_totmirr = *r_totfail = 0;

	key = BKE_key_from_object(ob);
	if (key == NULL)
		return 0;
	
	kb = BLI_findlink(&key->block, ob->shapenr - 1);

	if (kb) {
		char *tag_elem = MEM_callocN(sizeof(char) * kb->totelem, "shape_key_mirror");


		if (ob->type == OB_MESH) {
			Mesh *me = ob->data;
			MVert *mv;
			int i1, i2;
			float *fp1, *fp2;
			float tvec[3];

			ED_mesh_mirror_spatial_table(ob, NULL, NULL, 's');

			for (i1 = 0, mv = me->mvert; i1 < me->totvert; i1++, mv++) {
				i2 = mesh_get_x_mirror_vert(ob, i1, use_topology);
				if (i2 == i1) {
					fp1 = ((float *)kb->data) + i1 * 3;
					fp1[0] = -fp1[0];
					tag_elem[i1] = 1;
					totmirr++;
				}
				else if (i2 != -1) {
					if (tag_elem[i1] == 0 && tag_elem[i2] == 0) {
						fp1 = ((float *)kb->data) + i1 * 3;
						fp2 = ((float *)kb->data) + i2 * 3;

						copy_v3_v3(tvec,    fp1);
						copy_v3_v3(fp1, fp2);
						copy_v3_v3(fp2, tvec);

						/* flip x axis */
						fp1[0] = -fp1[0];
						fp2[0] = -fp2[0];
						totmirr++;
					}
					tag_elem[i1] = tag_elem[i2] = 1;
				}
				else {
					totfail++;
				}
			}

			ED_mesh_mirror_spatial_table(ob, NULL, NULL, 'e');
		}
		else if (ob->type == OB_LATTICE) {
			Lattice *lt = ob->data;
			int i1, i2;
			float *fp1, *fp2;
			int u, v, w;
			/* half but found up odd value */
			const int pntsu_half = (lt->pntsu / 2) + (lt->pntsu % 2);

			/* currently editmode isn't supported by mesh so
			 * ignore here for now too */

			/* if (lt->editlatt) lt = lt->editlatt->latt; */

			for (w = 0; w < lt->pntsw; w++) {
				for (v = 0; v < lt->pntsv; v++) {
					for (u = 0; u < pntsu_half; u++) {
						int u_inv = (lt->pntsu - 1) - u;
						float tvec[3];
						if (u == u_inv) {
							i1 = BKE_lattice_index_from_uvw(lt, u, v, w);
							fp1 = ((float *)kb->data) + i1 * 3;
							fp1[0] = -fp1[0];
							totmirr++;
						}
						else {
							i1 = BKE_lattice_index_from_uvw(lt, u, v, w);
							i2 = BKE_lattice_index_from_uvw(lt, u_inv, v, w);

							fp1 = ((float *)kb->data) + i1 * 3;
							fp2 = ((float *)kb->data) + i2 * 3;

							copy_v3_v3(tvec, fp1);
							copy_v3_v3(fp1, fp2);
							copy_v3_v3(fp2, tvec);
							fp1[0] = -fp1[0];
							fp2[0] = -fp2[0];
							totmirr++;
						}
					}
				}
			}
		}

		MEM_freeN(tag_elem);
	}
	
	*r_totmirr = totmirr;
	*r_totfail = totfail;

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	return 1;
}

/********************** shape key operators *********************/

static int shape_key_mode_poll(bContext *C)
{
	Object *ob = ED_object_context(C);
	ID *data = (ob) ? ob->data : NULL;
	return (ob && !ob->id.lib && data && !data->lib && ob->mode != OB_MODE_EDIT);
}

static int shape_key_mode_exists_poll(bContext *C)
{
	Object *ob = ED_object_context(C);
	ID *data = (ob) ? ob->data : NULL;

	/* same as shape_key_mode_poll */
	return (ob && !ob->id.lib && data && !data->lib && ob->mode != OB_MODE_EDIT) &&
	       /* check a keyblock exists */
	       (BKE_keyblock_from_object(ob) != NULL);
}

static int shape_key_move_poll(bContext *C)
{
	/* Same as shape_key_mode_exists_poll above, but ensure we have at least two shapes! */
	Object *ob = ED_object_context(C);
	ID *data = (ob) ? ob->data : NULL;
	Key *key = BKE_key_from_object(ob);

	return (ob && !ob->id.lib && data && !data->lib && ob->mode != OB_MODE_EDIT && key && key->totkey > 1);
}

static int shape_key_poll(bContext *C)
{
	Object *ob = ED_object_context(C);
	ID *data = (ob) ? ob->data : NULL;
	return (ob && !ob->id.lib && data && !data->lib);
}

static int shape_key_add_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_context(C);
	const bool from_mix = RNA_boolean_get(op->ptr, "from_mix");

	ED_object_shape_key_add(C, scene, ob, from_mix);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Shape Key";
	ot->idname = "OBJECT_OT_shape_key_add";
	ot->description = "Add shape key to the object";
	
	/* api callbacks */
	ot->poll = shape_key_mode_poll;
	ot->exec = shape_key_add_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "from_mix", true, "From Mix", "Create the new shape key from the existing mix of keys");
}

static int shape_key_remove_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Object *ob = ED_object_context(C);
	bool changed = false;

	if (RNA_boolean_get(op->ptr, "all")) {
		changed = ED_object_shape_key_remove_all(bmain, ob);
	}
	else {
		changed = ED_object_shape_key_remove(bmain, ob);
	}

	if (changed) {
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void OBJECT_OT_shape_key_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Shape Key";
	ot->idname = "OBJECT_OT_shape_key_remove";
	ot->description = "Remove shape key from the object";
	
	/* api callbacks */
	ot->poll = shape_key_mode_exists_poll;
	ot->exec = shape_key_remove_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "all", 0, "All", "Remove all shape keys");
}

static int shape_key_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	Key *key = BKE_key_from_object(ob);
	KeyBlock *kb = BKE_keyblock_from_object(ob);

	if (!key || !kb)
		return OPERATOR_CANCELLED;
	
	for (kb = key->block.first; kb; kb = kb->next)
		kb->curval = 0.0f;

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Shape Keys";
	ot->description = "Clear weights for all shape keys";
	ot->idname = "OBJECT_OT_shape_key_clear";
	
	/* api callbacks */
	ot->poll = shape_key_poll;
	ot->exec = shape_key_clear_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* starting point and step size could be optional */
static int shape_key_retime_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	Key *key = BKE_key_from_object(ob);
	KeyBlock *kb = BKE_keyblock_from_object(ob);
	float cfra = 0.0f;

	if (!key || !kb)
		return OPERATOR_CANCELLED;

	for (kb = key->block.first; kb; kb = kb->next)
		kb->pos = (cfra += 0.1f);

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_retime(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Re-Time Shape Keys";
	ot->description = "Resets the timing for absolute shape keys";
	ot->idname = "OBJECT_OT_shape_key_retime";

	/* api callbacks */
	ot->poll = shape_key_poll;
	ot->exec = shape_key_retime_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int shape_key_mirror_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);
	int totmirr = 0, totfail = 0;
	bool use_topology = RNA_boolean_get(op->ptr, "use_topology");

	if (!object_shape_key_mirror(C, ob, &totmirr, &totfail, use_topology))
		return OPERATOR_CANCELLED;

	ED_mesh_report_mirror(op, totmirr, totfail);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_mirror(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Mirror Shape Key";
	ot->idname = "OBJECT_OT_shape_key_mirror";
	ot->description = "Mirror the current shape key along the local X axis";

	/* api callbacks */
	ot->poll = shape_key_mode_poll;
	ot->exec = shape_key_mirror_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "use_topology", 0, "Topology Mirror",
	                "Use topology based mirroring (for when both sides of mesh have matching, unique topology)");
}


enum {
	KB_MOVE_TOP = -2,
	KB_MOVE_UP = -1,
	KB_MOVE_DOWN = 1,
	KB_MOVE_BOTTOM = 2,
};

static int shape_key_move_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);

	Key *key = BKE_key_from_object(ob);
	const int type = RNA_enum_get(op->ptr, "type");
	const int totkey = key->totkey;
	const int act_index = ob->shapenr - 1;
	int new_index;

	switch (type) {
		case KB_MOVE_TOP:
			/* Replace the ref key only if we're at the top already (only for relative keys) */
			new_index = (ELEM(act_index, 0, 1) || key->type == KEY_NORMAL) ? 0 : 1;
			break;
		case KB_MOVE_BOTTOM:
			new_index = totkey - 1;
			break;
		case KB_MOVE_UP:
		case KB_MOVE_DOWN:
		default:
			new_index = (totkey + act_index + type) % totkey;
			break;
	}

	if (!BKE_keyblock_move(ob, act_index, new_index)) {
		return OPERATOR_CANCELLED;
	}

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_move(wmOperatorType *ot)
{
	static EnumPropertyItem slot_move[] = {
		{KB_MOVE_TOP, "TOP", 0, "Top", "Top of the list"},
		{KB_MOVE_UP, "UP", 0, "Up", ""},
		{KB_MOVE_DOWN, "DOWN", 0, "Down", ""},
		{KB_MOVE_BOTTOM, "BOTTOM", 0, "Bottom", "Bottom of the list"},
		{ 0, NULL, 0, NULL, NULL }
	};

	/* identifiers */
	ot->name = "Move Shape Key";
	ot->idname = "OBJECT_OT_shape_key_move";
	ot->description = "Move the active shape key up/down in the list";

	/* api callbacks */
	ot->poll = shape_key_move_poll;
	ot->exec = shape_key_move_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "type", slot_move, 0, "Type", "");
}

/********************** goal weights *********************/

/* struct for properties used while drawing */
typedef struct GoalWeightsData {
	ARegion *ar;        /* region that knifetool was activated in */
	void *draw_handle;  /* for drawing preview loop */
	ViewContext vc;     /* note: _don't_ use 'mval', instead use the one we define below */
	float mval[2];      /* mouse value with snapping applied */
	//bContext *C;

	Scene *scene;
	Object *ob;
	Key *key;
	float imat[4][4];
	
	/* run by the UI or not */
	bool is_interactive;
	
	enum {
		MODE_IDLE = 0,
		MODE_DRAGGING,
	} mode;
	
	MSurfaceSample sample;
} GoalWeightsData;

static void shape_key_goal_weights_update_header(bContext *C, GoalWeightsData *UNUSED(sgw))
{
#define HEADER_LENGTH 256
	char header[HEADER_LENGTH];
	
	// TODO
	header[0] = '\0';
	
	ED_area_headerprint(CTX_wm_area(C), header);
#undef HEADER_LENGTH
}

/* modal loop selection drawing callback */
static void shape_key_goal_weights_draw(const bContext *C, ARegion *UNUSED(ar), void *arg)
{
	View3D *v3d = CTX_wm_view3d(C);
	const GoalWeightsData *sgw = arg;
	Object *ob = sgw->ob;
	
	if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
	
	glPushMatrix();
	
	if (sgw->mode == MODE_DRAGGING && ob->derivedDeform) {
		float start[3], end[3], nor[3];
		
		if (BKE_mesh_sample_eval(ob->derivedDeform, &sgw->sample, start, nor)) {
			
			mul_m4_v3(sgw->ob->obmat, start);
			
			ED_view3d_win_to_3d(sgw->ar, start, sgw->mval, end);
			
			UI_ThemeColor(TH_WIRE);
			
			glLineWidth(2.0);
			
			glBegin(GL_LINES);
			glVertex3fv(start);
			glVertex3fv(end);
			glEnd();
			
			glLineWidth(1.0);
		}
	}
	
#if 0
	if (sgw->curr.vert) {
		glColor3ubv(sgw->colors.point);
		glPointSize(11);
		
		glBegin(GL_POINTS);
		glVertex3fv(sgw->curr.cage);
		glEnd();
	}
#endif

	if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
}

static bool shape_key_goal_weights_mouse_ray(void *userdata, float ray_start[3], float ray_end[3])
{
	GoalWeightsData *sgw = userdata;
	ViewContext *vc = &sgw->vc;
	
	ED_view3d_win_to_segment(vc->ar, vc->v3d, sgw->mval, ray_start, ray_end, true);

	mul_m4_v3(sgw->imat, ray_start);
	mul_m4_v3(sgw->imat, ray_end);
	
	return true;
}

/* called on tool confirmation */
static void shape_key_goal_weights_pick_sample(GoalWeightsData *sgw)
{
	Scene *scene = sgw->scene;
	Object *ob = sgw->ob;
	DerivedMesh *dm;
	MSurfaceSampleStorage sample_storage;
	int tot;
	
	BLI_assert(sgw->mode == MODE_IDLE);
	
	dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
	
	BKE_mesh_sample_storage_single(&sample_storage, &sgw->sample);
	tot = BKE_mesh_sample_generate_raycast(&sample_storage, dm, shape_key_goal_weights_mouse_ray, sgw, 1);
	BKE_mesh_sample_storage_release(&sample_storage);
	
	if (tot > 0)
		sgw->mode = MODE_DRAGGING;
}

static bool shape_key_goal_weights_apply(GoalWeightsData *sgw)
{
	ViewContext *vc = &sgw->vc;
	Scene *scene = sgw->scene;
	Object *ob = sgw->ob;
	
	DerivedMesh *dm;
	float loc[3], goal[3], nor[3];
	float *weights;
	
	dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
	
	/* get sample location for depth reference */
	BKE_mesh_sample_eval(dm, &sgw->sample, loc, nor);
	
	/* determine goal for the shape key sample */
	ED_view3d_win_to_3d(vc->ar, loc, sgw->mval, goal);
	
	weights = shape_key_goal_weights_solve(sgw->key, &sgw->sample, &goal, 1);
	if (weights) {
		KeyBlock *kb;
		float *w;
		for (kb = sgw->key->block.first, w = weights; kb; kb = kb->next, ++w) {
			kb->curval = *w;
		}
		
		MEM_freeN(weights);
		
		return true;
	}
	else
		return false;
}

static void shape_key_goal_weights_finish(wmOperator *op)
{
	GoalWeightsData *sgw = op->customdata;
	Object *ob = sgw->ob;
	
	if (shape_key_goal_weights_apply(sgw)) {
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ob);
	}
}

/* called when modal loop selection is done... */
static void shape_key_goal_weights_exit_ex(bContext *C, GoalWeightsData *sgw)
{
	if (!sgw)
		return;

	if (sgw->is_interactive) {
		WM_cursor_modal_restore(CTX_wm_window(C));

		/* deactivate the extra drawing stuff in 3D-View */
		ED_region_draw_cb_exit(sgw->ar->type, sgw->draw_handle);
	}

	/* free the custom data */

	/* tag for redraw */
	ED_region_tag_redraw(sgw->ar);

	/* destroy kcd itself */
	MEM_freeN(sgw);
}
static void shape_key_goal_weights_exit(bContext *C, wmOperator *op)
{
	GoalWeightsData *sgw = op->customdata;
	shape_key_goal_weights_exit_ex(C, sgw);
	op->customdata = NULL;
}

static void shape_key_goal_weights_update_mval(GoalWeightsData *sgw, const float mval[2])
{
	copy_v2_v2(sgw->mval, mval);
}

static void shape_key_goal_weights_update_mval_i(GoalWeightsData *sgw, const int mval_i[2])
{
	float mval[2] = {UNPACK2(mval_i)};
	shape_key_goal_weights_update_mval(sgw, mval);
	
	ED_region_tag_redraw(sgw->ar);
}

/* called when modal loop selection gets set up... */
static void shape_key_goal_weights_init(bContext *C, GoalWeightsData *sgw, bool is_interactive)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_context(C);
	Key *key = BKE_key_from_object(ob);
	
	/* assign the drawing handle for drawing preview line... */
	sgw->scene = scene;
	sgw->ob = ob;
	sgw->key = key;
	sgw->ar = CTX_wm_region(C);
	invert_m4_m4(sgw->imat, ob->obmat);
	
	em_setup_viewcontext(C, &sgw->vc);
	
	ED_region_tag_redraw(sgw->ar);
	
	sgw->is_interactive = is_interactive;
	
	if (is_interactive) {
		sgw->draw_handle = ED_region_draw_cb_activate(sgw->ar->type, shape_key_goal_weights_draw, sgw, REGION_DRAW_POST_VIEW);
	}
}

static void shape_key_goal_weights_cancel(bContext *C, wmOperator *op)
{
	/* this is just a wrapper around exit() */
	shape_key_goal_weights_exit(C, op);
}

static int shape_key_goal_weights_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	GoalWeightsData *sgw;

	view3d_operator_needs_opengl(C);

	/* alloc new customdata */
	sgw = op->customdata = MEM_callocN(sizeof(GoalWeightsData), __func__);

	shape_key_goal_weights_init(C, sgw, true);

	/* add a modal handler for this operator - handles loop selection */
	WM_cursor_modal_set(CTX_wm_window(C), BC_CROSSCURSOR);
	WM_event_add_modal_handler(C, op);

	shape_key_goal_weights_update_mval_i(sgw, event->mval);

	shape_key_goal_weights_update_header(C, sgw);

	return OPERATOR_RUNNING_MODAL;
}

enum {
	SGW_MODAL_CANCEL = 1,
	SGW_MODAL_CONFIRM,
};

wmKeyMap *ED_keymap_shape_key_goal_weights(wmKeyConfig *keyconf)
{
	static EnumPropertyItem modal_items[] = {
		{SGW_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
		{SGW_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
		{0, NULL, 0, NULL, NULL}
	};

	wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "Shape Key Goal Weights Modal Map");

	/* this function is called for each spacetype, only needs to add map once */
	if (keymap && keymap->modal_items)
		return NULL;

	keymap = WM_modalkeymap_add(keyconf, "Shape Key Goal Weights Modal Map", modal_items);

	/* items for modal map */
	WM_modalkeymap_add_item(keymap, ESCKEY, KM_PRESS, KM_ANY, 0, SGW_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, RIGHTMOUSE, KM_PRESS, KM_ANY, 0, SGW_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_PRESS, KM_ANY, 0, SGW_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, RETKEY, KM_PRESS, KM_ANY, 0, SGW_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, PADENTER, KM_PRESS, KM_ANY, 0, SGW_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, SPACEKEY, KM_PRESS, KM_ANY, 0, SGW_MODAL_CONFIRM);

	WM_modalkeymap_assign(keymap, "OBJECT_OT_shape_key_goal_weights");

	return keymap;
}

static int shape_key_goal_weights_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	GoalWeightsData *sgw = op->customdata;
	Object *ob = ED_object_context(C);
	Key *key = BKE_key_from_object(ob);
	bool do_refresh = false;

	if (!ob || !key) {
		shape_key_goal_weights_exit(C, op);
		ED_area_headerprint(CTX_wm_area(C), NULL);
		return OPERATOR_FINISHED;
	}

	view3d_operator_needs_opengl(C);
	ED_view3d_init_mats_rv3d(ob, sgw->vc.rv3d);  /* needed to initialize clipping */

	/* handle modal keymap */
	if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case SGW_MODAL_CANCEL:
				/* finish */
				ED_region_tag_redraw(sgw->ar);
				
				shape_key_goal_weights_exit(C, op);
				ED_area_headerprint(CTX_wm_area(C), NULL);
				
				return OPERATOR_CANCELLED;
			case SGW_MODAL_CONFIRM:
				/* finish */
				ED_region_tag_redraw(sgw->ar);
				
				switch (sgw->mode) {
					case MODE_IDLE:
						shape_key_goal_weights_pick_sample(sgw);
						break;
					
					case MODE_DRAGGING:
						shape_key_goal_weights_finish(op);
						
						shape_key_goal_weights_exit(C, op);
						ED_area_headerprint(CTX_wm_area(C), NULL);
						
						return OPERATOR_FINISHED;
						break;
					
					default:
						BLI_assert(0); /* should never happen */
						return OPERATOR_CANCELLED;
						break;
				}
				
				return OPERATOR_RUNNING_MODAL;
		}
	}
	else { /* non-modal-mapped events */
		switch (event->type) {
			case MOUSEPAN:
			case MOUSEZOOM:
			case MOUSEROTATE:
			case WHEELUPMOUSE:
			case WHEELDOWNMOUSE:
				return OPERATOR_PASS_THROUGH;
			case MOUSEMOVE: /* move the goal */
				shape_key_goal_weights_update_mval_i(sgw, event->mval);
				
				if (sgw->mode == MODE_DRAGGING) {
					if (shape_key_goal_weights_apply(sgw)) {
						DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
						WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ob);
					}
				}
				break;
		}
	}

	if (do_refresh) {
		/* we don't really need to update mval,
		 * but this happens to be the best way to refresh at the moment */
		shape_key_goal_weights_update_mval_i(sgw, event->mval);
	}

	/* keep going until the user confirms */
	return OPERATOR_RUNNING_MODAL;
}

void OBJECT_OT_shape_key_goal_weights(wmOperatorType *ot)
{
	/* description */
	ot->name = "Shape Key Goal Weights";
	ot->idname = "OBJECT_OT_shape_key_goal_weights";
	ot->description = "Select a surface point goal and adjust shape key weights for best fit";

	/* callbacks */
	ot->invoke = shape_key_goal_weights_invoke;
	ot->modal = shape_key_goal_weights_modal;
	ot->cancel = shape_key_goal_weights_cancel;
	ot->poll = shape_key_mode_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;
}

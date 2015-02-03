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

/** \file blender/editors/physics/particle_modifier.c
 *  \ingroup edphys
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_particle.h"
#include "ED_physics.h"
#include "ED_screen.h"

#include "physics_intern.h"

/******************************** API ****************************/

ParticleModifierData *ED_particle_modifier_add(ReportList *UNUSED(reports), Main *UNUSED(bmain), Scene *UNUSED(scene), Object *ob, ParticleSystem *psys, const char *name, int type)
{
	ParticleModifierData *new_md = NULL;
	
	/* get new modifier data to add */
	new_md = particle_modifier_new(type);
	BLI_addtail(&psys->modifiers, new_md);
	
	if (name) {
		BLI_strncpy_utf8(new_md->name, name, sizeof(new_md->name));
	}
	
	/* make sure modifier data has unique name */
	particle_modifier_unique_name(&psys->modifiers, new_md);
	
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	
	return new_md;
}

static bool particle_modifier_remove(Main *UNUSED(bmain), Object *UNUSED(ob), ParticleSystem *psys, ParticleModifierData *md,
                                   bool *r_sort_depsgraph)
{
	*r_sort_depsgraph = false;
	
	/* It seems on rapid delete it is possible to
	 * get called twice on same modifier, so make
	 * sure it is in list. */
	if (BLI_findindex(&psys->modifiers, md) == -1) {
		return 0;
	}

	BLI_remlink(&psys->modifiers, md);
	particle_modifier_free(md);

	return 1;
}

bool ED_particle_modifier_remove(ReportList *reports, Main *bmain, Object *ob, ParticleSystem *psys, ParticleModifierData *md)
{
	bool sort_depsgraph = false;
	bool ok;

	ok = particle_modifier_remove(bmain, ob, psys, md, &sort_depsgraph);

	if (!ok) {
		BKE_reportf(reports, RPT_ERROR, "Modifier '%s' not in particle system '%s'", md->name, psys->name);
		return 0;
	}

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	DAG_relations_tag_update(bmain);

	return 1;
}

void ED_particle_modifier_clear(Main *bmain, Object *ob, ParticleSystem *psys)
{
	ParticleModifierData *md = ob->modifiers.first;
	bool sort_depsgraph = false;

	if (!md)
		return;

	while (md) {
		ParticleModifierData *next_md;

		next_md = md->next;

		particle_modifier_remove(bmain, ob, psys, md, &sort_depsgraph);

		md = next_md;
	}

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	DAG_relations_tag_update(bmain);
}

int ED_particle_modifier_move_up(ReportList *UNUSED(reports), Object *UNUSED(ob), ParticleSystem *psys, ParticleModifierData *md)
{
	if (md->prev) {
		BLI_remlink(&psys->modifiers, md);
		BLI_insertlinkbefore(&psys->modifiers, md->prev, md);
	}

	return 1;
}

int ED_particle_modifier_move_down(ReportList *UNUSED(reports), Object *UNUSED(ob), ParticleSystem *psys, ParticleModifierData *md)
{
	if (md->next) {
		BLI_remlink(&psys->modifiers, md);
		BLI_insertlinkafter(&psys->modifiers, md->next, md);
	}

	return 1;
}

/************************ add modifier operator *********************/

static int particle_modifier_add_poll(bContext *C)
{
	Object *ob;
	if (!ED_operator_object_active_editable(C))
		return false;
	
	ob = ED_object_active_context(C);
	if (psys_get_current(ob) == NULL)
		return false;
	
	return true;
}

static int particle_modifier_add_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_active_context(C);
	ParticleSystem *psys = psys_get_current(ob);
	int type = RNA_enum_get(op->ptr, "type");

	if (!ED_particle_modifier_add(op->reports, bmain, scene, ob, psys, NULL, type))
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
	
	return OPERATOR_FINISHED;
}

static EnumPropertyItem *particle_modifier_add_itemf(bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), bool *r_free)
{	
	Object *ob = ED_object_active_context(C);
	EnumPropertyItem *item = NULL, *md_item, *group_item = NULL;
	ModifierTypeInfo *mti;
	int totitem = 0, a;
	
	if (!ob)
		return particle_modifier_type_items;

	for (a = 0; modifier_type_items[a].identifier; a++) {
		md_item = &modifier_type_items[a];

		if (md_item->identifier[0]) {
			mti = modifierType_getInfo(md_item->value);

			if (mti->flags & eModifierTypeFlag_NoUserAdd)
				continue;

			if (!BKE_object_support_modifier_type_check(ob, md_item->value))
				continue;
		}
		else {
			group_item = md_item;
			md_item = NULL;

			continue;
		}

		if (group_item) {
			RNA_enum_item_add(&item, &totitem, group_item);
			group_item = NULL;
		}

		RNA_enum_item_add(&item, &totitem, md_item);
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

void PARTICLE_OT_modifier_add(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Add Modifier";
	ot->description = "Add a modifier to the active object";
	ot->idname = "PARTICLE_OT_modifier_add";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = particle_modifier_add_exec;
	ot->poll = particle_modifier_add_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	prop = RNA_def_enum(ot->srna, "type", modifier_type_items, eModifierType_Subsurf, "Type", "");
	RNA_def_enum_funcs(prop, particle_modifier_add_itemf);
	ot->prop = prop;
}

/************************ generic functions for operators using mod names and data context *********************/

static int edit_particle_modifier_poll_generic(bContext *C, StructRNA *rna_type, int obtype_flag)
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_modifier", rna_type);
	Object *ob = (ptr.id.data) ? ptr.id.data : ED_object_active_context(C);
	
	if (!ob || ob->id.lib) return 0;
	if (obtype_flag && ((1 << ob->type) & obtype_flag) == 0) return 0;
	if (ptr.id.data && ((ID *)ptr.id.data)->lib) return 0;
	
	return 1;
}

static int edit_particle_modifier_poll(bContext *C)
{
	return edit_particle_modifier_poll_generic(C, &RNA_ParticleModifier, 0);
}

static void edit_particle_modifier_properties(wmOperatorType *ot)
{
	RNA_def_string(ot->srna, "particle_modifier", NULL, MAX_NAME, "Particle Modifier", "Name of the particle modifier to edit");
}

static int edit_particle_modifier_invoke_properties(bContext *C, wmOperator *op)
{
	ParticleModifierData *md;
	
	if (RNA_struct_property_is_set(op->ptr, "particle_modifier")) {
		return true;
	}
	else {
		PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_modifier", &RNA_ParticleModifier);
		if (ptr.data) {
			md = ptr.data;
			RNA_string_set(op->ptr, "particle_modifier", md->name);
			return true;
		}
	}

	return false;
}

static ParticleModifierData *edit_particle_modifier_property_get(wmOperator *op, Object *ob, ParticleSystem *psys, int type)
{
	char modifier_name[MAX_NAME];
	ParticleModifierData *md;
	RNA_string_get(op->ptr, "particle_modifier", modifier_name);
	
	md = particle_modifier_findByName(ob, psys, modifier_name);
	
	if (md && type != 0 && md->type != type)
		md = NULL;

	return md;
}

/************************ remove modifier operator *********************/

static int particle_modifier_remove_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Object *ob = ED_object_active_context(C);
	ParticleSystem *psys = psys_get_current(ob);
	ParticleModifierData *md = edit_particle_modifier_property_get(op, ob, psys, 0);
	
	if (!md || !ED_particle_modifier_remove(op->reports, bmain, ob, psys, md))
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
	
	return OPERATOR_FINISHED;
}

static int particle_modifier_remove_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_particle_modifier_invoke_properties(C, op))
		return particle_modifier_remove_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

void PARTICLE_OT_modifier_remove(wmOperatorType *ot)
{
	ot->name = "Remove Particle Modifier";
	ot->description = "Remove a particle modifier from the active object";
	ot->idname = "PARTICLE_OT_modifier_remove";

	ot->invoke = particle_modifier_remove_invoke;
	ot->exec = particle_modifier_remove_exec;
	ot->poll = edit_particle_modifier_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_particle_modifier_properties(ot);
}

/************************ move up modifier operator *********************/

static int particle_modifier_move_up_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_active_context(C);
	ParticleSystem *psys = psys_get_current(ob);
	ParticleModifierData *md = edit_particle_modifier_property_get(op, ob, psys, 0);

	if (!md || !ED_particle_modifier_move_up(op->reports, ob, psys, md))
		return OPERATOR_CANCELLED;

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
	
	return OPERATOR_FINISHED;
}

static int particle_modifier_move_up_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_particle_modifier_invoke_properties(C, op))
		return particle_modifier_move_up_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

void PARTICLE_OT_modifier_move_up(wmOperatorType *ot)
{
	ot->name = "Move Up Particle Modifier";
	ot->description = "Move particle modifier up in the stack";
	ot->idname = "PARTICLE_OT_modifier_move_up";

	ot->invoke = particle_modifier_move_up_invoke;
	ot->exec = particle_modifier_move_up_exec;
	ot->poll = edit_particle_modifier_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_particle_modifier_properties(ot);
}

/************************ move down modifier operator *********************/

static int particle_modifier_move_down_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_active_context(C);
	ParticleSystem *psys = psys_get_current(ob);
	ParticleModifierData *md = edit_particle_modifier_property_get(op, ob, psys, 0);

	if (!md || !ED_particle_modifier_move_down(op->reports, ob, psys, md))
		return OPERATOR_CANCELLED;

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
	
	return OPERATOR_FINISHED;
}

static int particle_modifier_move_down_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_particle_modifier_invoke_properties(C, op))
		return particle_modifier_move_down_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

void PARTICLE_OT_modifier_move_down(wmOperatorType *ot)
{
	ot->name = "Move Down Particle Modifier";
	ot->description = "Move particle modifier down in the stack";
	ot->idname = "PARTICLE_OT_modifier_move_down";

	ot->invoke = particle_modifier_move_down_invoke;
	ot->exec = particle_modifier_move_down_exec;
	ot->poll = edit_particle_modifier_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_particle_modifier_properties(ot);
}

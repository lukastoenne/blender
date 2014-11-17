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

/** \file blender/editors/physics/particle_key.c
 *  \ingroup edphys
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_key_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_particle.h"

#include "BLI_sys_types.h" // for intptr_t support

#include "ED_object.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "physics_intern.h"

/*********************** add shape key ***********************/

static void ED_particles_shape_key_add(bContext *C, Scene *scene, Object *ob, ParticleSystem *psys, const bool from_mix)
{
	KeyBlock *kb;
	if ((kb = BKE_psys_insert_shape_key(scene, ob, psys, NULL, from_mix))) {
		Key *key = psys->key;
		/* for absolute shape keys, new keys may not be added last */
		psys->shapenr = BLI_findindex(&key->block, kb) + 1;

		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	}
}

/*********************** remove shape key ***********************/

static bool ED_particles_shape_key_remove_all(Main *bmain, Object *UNUSED(ob), ParticleSystem *psys)
{
	Key *key = psys->key;
	if (key == NULL)
		return false;

	psys->key = NULL;

	BKE_libblock_free_us(bmain, key);

	return true;
}

static bool ED_particles_shape_key_remove(Main *bmain, Object *ob, ParticleSystem *psys)
{
	KeyBlock *kb, *rkb;
	Key *key = psys->key;
	if (key == NULL)
		return false;
	
	kb = BLI_findlink(&key->block, psys->shapenr - 1);
	
	if (kb) {
		for (rkb = key->block.first; rkb; rkb = rkb->next)
			if (rkb->relative == psys->shapenr - 1)
				rkb->relative = 0;
		
		BLI_remlink(&key->block, kb);
		key->totkey--;
		if (key->refkey == kb) {
			key->refkey = key->block.first;
			
			if (key->refkey) {
				/* apply new basis key on original data */
				BKE_keyblock_convert_to_hair_keys(key->refkey, ob, psys);
			}
		}
			
		if (kb->data) MEM_freeN(kb->data);
		MEM_freeN(kb);
		
		if (psys->shapenr > 1) {
			psys->shapenr--;
		}
	}
	
	if (key->totkey == 0) {
		psys->key = NULL;
		
		BKE_libblock_free_us(bmain, key);
	}
	
	return true;
}

/********************** shape key operators *********************/

static int shape_key_mode_poll(bContext *C)
{
	Object *ob = ED_object_context(C);
	ParticleSystem *psys = psys_get_current(ob);
	return (ob && !ob->id.lib && psys && ob->mode != OB_MODE_PARTICLE_EDIT);
}

static int shape_key_mode_exists_poll(bContext *C)
{
	Object *ob = ED_object_context(C);
	ParticleSystem *psys = psys_get_current(ob);

	/* same as shape_key_mode_poll */
	return (ob && !ob->id.lib && psys && ob->mode != OB_MODE_PARTICLE_EDIT) &&
	       /* check a keyblock exists */
	       (BKE_keyblock_from_particles(psys) != NULL);
}

static int shape_key_poll(bContext *C)
{
	Object *ob = ED_object_context(C);
	ParticleSystem *psys = psys_get_current(ob);
	
	return (ob && !ob->id.lib && psys);
}

static int shape_key_add_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_context(C);
	ParticleSystem *psys = psys_get_current(ob);
	const bool from_mix = RNA_boolean_get(op->ptr, "from_mix");

	ED_particles_shape_key_add(C, scene, ob, psys, from_mix);

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_shape_key_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Shape Key";
	ot->idname = "PARTICLE_OT_shape_key_add";
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
	ParticleSystem *psys = psys_get_current(ob);
	bool changed = false;

	if (RNA_boolean_get(op->ptr, "all")) {
		changed = ED_particles_shape_key_remove_all(bmain, ob, psys);
	}
	else {
		changed = ED_particles_shape_key_remove(bmain, ob, psys);
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

void PARTICLE_OT_shape_key_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Shape Key";
	ot->idname = "PARTICLE_OT_shape_key_remove";
	ot->description = "Remove shape key from the object";
	
	/* api callbacks */
	ot->poll = shape_key_mode_poll;
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
	ParticleSystem *psys = psys_get_current(ob);
	Key *key = psys->key;
	KeyBlock *kb = BKE_keyblock_from_particles(psys);

	if (!key || !kb)
		return OPERATOR_CANCELLED;
	
	for (kb = key->block.first; kb; kb = kb->next)
		kb->curval = 0.0f;

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_shape_key_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Shape Keys";
	ot->description = "Clear weights for all shape keys";
	ot->idname = "PARTICLE_OT_shape_key_clear";
	
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
	ParticleSystem *psys = psys_get_current(ob);
	Key *key = psys->key;
	KeyBlock *kb = BKE_keyblock_from_particles(psys);
	float cfra = 0.0f;

	if (!key || !kb)
		return OPERATOR_CANCELLED;

	for (kb = key->block.first; kb; kb = kb->next)
		kb->pos = (cfra += 0.1f);

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_shape_key_retime(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Re-Time Shape Keys";
	ot->description = "Resets the timing for absolute shape keys";
	ot->idname = "PARTICLE_OT_shape_key_retime";

	/* api callbacks */
	ot->poll = shape_key_poll;
	ot->exec = shape_key_retime_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int shape_key_move_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);
	ParticleSystem *psys = psys_get_current(ob);
	Key *key = psys->key;

	if (!key) {
		return OPERATOR_CANCELLED;
	}

	{
		KeyBlock *kb, *kb_other, *kb_iter;
		const int type = RNA_enum_get(op->ptr, "type");
		const int shape_tot = key->totkey;
		const int shapenr_act = psys->shapenr - 1;
		const int shapenr_swap = (shape_tot + shapenr_act + type) % shape_tot;

		kb = BLI_findlink(&key->block, shapenr_act);
		if (!kb || shape_tot == 1) {
			return OPERATOR_CANCELLED;
		}

		if (type == -1) {
			/* move back */
			kb_other = kb->prev;
			BLI_remlink(&key->block, kb);
			BLI_insertlinkbefore(&key->block, kb_other, kb);
		}
		else {
			/* move next */
			kb_other = kb->next;
			BLI_remlink(&key->block, kb);
			BLI_insertlinkafter(&key->block, kb_other, kb);
		}

		psys->shapenr = shapenr_swap + 1;

		/* for relative shape keys */
		if (kb_other) {
			for (kb_iter = key->block.first; kb_iter; kb_iter = kb_iter->next) {
				if (kb_iter->relative == shapenr_act) {
					kb_iter->relative = shapenr_swap;
				}
				else if (kb_iter->relative == shapenr_swap) {
					kb_iter->relative = shapenr_act;
				}
			}
		}
		/* First key became last, or vice-versa, we have to change all keys' relative value. */
		else {
			for (kb_iter = key->block.first; kb_iter; kb_iter = kb_iter->next) {
				if (kb_iter->relative == shapenr_act) {
					kb_iter->relative = shapenr_swap;
				}
				else {
					kb_iter->relative += type;
				}
			}
		}

		/* for absolute shape keys */
		if (kb_other) {
			SWAP(float, kb_other->pos, kb->pos);
		}
		/* First key became last, or vice-versa, we have to change all keys' pos value. */
		else {
			float pos = kb->pos;
			if (type == -1) {
				for (kb_iter = key->block.first; kb_iter; kb_iter = kb_iter->next) {
					SWAP(float, kb_iter->pos, pos);
				}
			}
			else {
				for (kb_iter = key->block.last; kb_iter; kb_iter = kb_iter->prev) {
					SWAP(float, kb_iter->pos, pos);
				}
			}
		}

		/* First key is refkey, matches interface and BKE_key_sort */
		key->refkey = key->block.first;
	}

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_shape_key_move(wmOperatorType *ot)
{
	static EnumPropertyItem slot_move[] = {
		{-1, "UP", 0, "Up", ""},
		{1, "DOWN", 0, "Down", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Move Shape Key";
	ot->idname = "PARTICLE_OT_shape_key_move";
	ot->description = "Move the active shape key up/down in the list";

	/* api callbacks */
	ot->poll = shape_key_mode_poll;
	ot->exec = shape_key_move_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "type", slot_move, 0, "Type", "");
}

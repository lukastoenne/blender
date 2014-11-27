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

#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_edithair.h"
#include "BKE_particle.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_physics.h"

#include "hair_intern.h"

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
			psys->hairedit = NULL;
		}
		
		return true;
	}
	
	return false;
}


/* ==== edit mode toggle ==== */

static int hair_edit_toggle_poll(bContext *C)
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

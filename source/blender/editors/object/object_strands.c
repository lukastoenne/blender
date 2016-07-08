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

/** \file blender/editors/object/object_strands.c
 *  \ingroup edobj
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_strand_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_strands.h"

#include "ED_object.h"

#include "WM_api.h"
#include "WM_types.h"

#include "object_intern.h"

/************************ LaplacianDeform bind operator *********************/

static int strands_test_init_poll(bContext *C)
{
	return edit_modifier_poll_generic(C, &RNA_StrandsModifier, 0);
}

static int strands_test_init_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_active_context(C);
	StrandsModifierData *smd = (StrandsModifierData *)edit_modifier_property_get(op, ob, eModifierType_Strands);
	
	if (!smd)
		return OPERATOR_CANCELLED;
	
	CustomDataMask mask = CD_MASK_BAREMESH;
	DerivedMesh *scalp = mesh_get_derived_final(scene, ob, mask);
	if (!scalp)
		return OPERATOR_CANCELLED;
	
	int totcurves = RNA_int_get(op->ptr, "amount");
	int maxverts = RNA_int_get(op->ptr, "maxverts");
	unsigned int seed = RNA_int_get(op->ptr, "seed");
	
	BKE_strands_test_init(smd->strands, scalp, totcurves, maxverts, seed);
	
	/* invalidate fibers */
	BKE_strands_free_fibers(smd->strands);
	
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
	return OPERATOR_FINISHED;
}

static int strands_test_init_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	if (edit_modifier_invoke_properties(C, op))
		return WM_operator_props_popup_confirm(C, op, event);
	else 
		return OPERATOR_CANCELLED;
}

void OBJECT_OT_strands_test_init(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Strands Test Init";
	ot->description = "Testing strand with randomized initialization of control curves";
	ot->idname = "OBJECT_OT_strands_test_init";
	
	/* api callbacks */
	ot->poll = strands_test_init_poll;
	ot->invoke = strands_test_init_invoke;
	ot->exec = strands_test_init_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_modifier_properties(ot);
	
	RNA_def_int(ot->srna, "amount", 100, 0, INT_MAX, "Amount", "Number of control curves to generate", 1, 1000000);
	RNA_def_int(ot->srna, "maxverts", 5, 0, INT_MAX, "Vertices", "Maximum number of vertices per strand", 1, 20);
	RNA_def_int(ot->srna, "seed", 0, 0, INT_MAX, "Seed", "Seed value for randomization", 0, INT_MAX);
}

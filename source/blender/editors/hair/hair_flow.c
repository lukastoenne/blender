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

/** \file blender/editors/hair/hair_flow.c
 *  \ingroup edhair
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_editstrands.h"
#include "BKE_particle.h"

#include "BPH_strands.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_physics.h"

#include "hair_intern.h"

static int hair_solve_flow_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	ParticleSystem *psys = psys_get_current(ob);
	struct HairFlowData *data;
	
	unsigned int seed = 111;
	int max_strands = RNA_int_get(op->ptr, "max_strands");
	float max_length = RNA_float_get(op->ptr, "max_length");
	int segments = RNA_int_get(op->ptr, "segments");
	int res = RNA_int_get(op->ptr, "resolution");
	
	if (!psys || !(ob->type == OB_MESH || ob->derivedFinal))
		return OPERATOR_CANCELLED;
	
	data = BPH_strands_solve_hair_flow(scene, ob, max_length, res, NULL);
	if (data) {
		BMesh *bm = BKE_particles_to_bmesh(ob, psys);
		DerivedMesh *dm = ob->derivedFinal ? ob->derivedFinal : mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
		BMEditStrands *edit = BKE_editstrands_create(bm, dm);
		
		/* generate new hair strands */
		BPH_strands_sample_hair_flow(ob, edit, data, seed, max_strands, max_length, segments);
		
		BKE_particles_from_bmesh(ob, psys, edit);
		psys->flag |= PSYS_EDITED;
		BKE_editstrands_free(edit);
		MEM_freeN(edit);
		
		BPH_strands_free_hair_flow(data);
		
		WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_MODE_HAIR, NULL);
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	}
	
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW | NA_SELECTED, ob);
	
	return OPERATOR_FINISHED;
}

void HAIR_OT_solve_flow(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Solve Hair Flow";
	ot->idname = "HAIR_OT_solve_flow";
	ot->description = "Generate hair strands based on flow editing";

	/* api callbacks */
	ot->exec = hair_solve_flow_exec;
	ot->poll = hair_flow_poll; /* uses temporary edit data */

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_int(ot->srna, "max_strands", 1, 1, INT_MAX, "Strands", "Maximum number of strands to generate", 1, 100000);
	RNA_def_float(ot->srna, "max_length", 1.0f, 0.0f, FLT_MAX, "Length", "Maximum length of strands", 0.0001f, 10000.0f);
	RNA_def_int(ot->srna, "segments", 5, 1, INT_MAX, "Segments", "Number of segments per strand", 1, 100);
	RNA_def_int(ot->srna, "resolution", 10, 1, INT_MAX, "Resolution", "Resolution of the hair flow grid", 1, 100);
}

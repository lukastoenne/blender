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

/** \file blender/editors/hair/hair_undo.c
 *  \ingroup edhair
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.h"
#include "BKE_DerivedMesh.h"
#include "BKE_editstrands.h"
#include "BKE_key.h"
#include "BKE_mesh.h"
#include "BKE_mesh_sample.h"

#include "ED_physics.h"
#include "ED_util.h"

#include "bmesh.h"

#include "hair_intern.h"

static void *strands_get_edit(bContext *C)
{
	Object *obact = CTX_data_active_object(C);
	const int mode_flag = OB_MODE_HAIR_EDIT;
	const bool is_mode_set = ((obact->mode & mode_flag) != 0);

	if (obact && is_mode_set) {
		BMEditStrands *edit = BKE_editstrands_from_object(obact);
		return edit;
	}
	return NULL;
}

typedef struct UndoStrands {
	Mesh me; /* Mesh supports all the customdata we need, easiest way to implement undo storage */
	int selectmode;

	/** \note
	 * this isn't a prefect solution, if you edit keys and change shapes this works well (fixing [#32442]),
	 * but editing shape keys, going into object mode, removing or changing their order,
	 * then go back into editmode and undo will give issues - where the old index will be out of sync
	 * with the new object index.
	 *
	 * There are a few ways this could be made to work but for now its a known limitation with mixing
	 * object and editmode operations - Campbell */
	int shapenr;
} UndoStrands;

/* undo simply makes copies of a bmesh */
static void *strands_edit_to_undo(void *editv, void *UNUSED(obdata))
{
	BMEditStrands *edit = editv;
//	Mesh *obme = obdata;
	
	UndoStrands *undo = MEM_callocN(sizeof(UndoStrands), "undo Strands");
	
	/* make sure shape keys work */
//	um->me.key = obme->key ? BKE_key_copy_nolib(obme->key) : NULL;

	/* BM_mesh_validate(em->bm); */ /* for troubleshooting */

	BM_mesh_bm_to_me_ex(edit->bm, &undo->me, CD_MASK_STRANDS, false);

	undo->selectmode = edit->bm->selectmode;
	undo->shapenr = edit->bm->shapenr;

	return undo;
}

static void strands_undo_to_edit(void *undov, void *editv, void *UNUSED(obdata))
{
	UndoStrands *undo = undov;
	BMEditStrands *edit = editv, *edit_tmp;
	Object *ob = edit->ob;
	DerivedMesh *dm = edit->root_dm;
	BMesh *bm;
//	Key *key = ((Mesh *) obdata)->key;
	
	const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(&undo->me);
	
	edit->bm->shapenr = undo->shapenr;
	
	bm = BM_mesh_create(&allocsize);
	BM_mesh_bm_from_me_ex(bm, &undo->me, CD_MASK_STRANDS_BMESH, false, false, undo->shapenr);
	
	/* note: have to create the new edit before freeing the old one,
	 * because it owns the root_dm and we have to copy it before
	 * it gets released when freeing the old edit.
	 */
	edit_tmp = BKE_editstrands_create(bm, dm);
	BKE_editstrands_free(edit);
	*edit = *edit_tmp;
	
	bm->selectmode = undo->selectmode;
	edit->ob = ob;
	
#if 0
	/* T35170: Restore the active key on the RealMesh. Otherwise 'fake' offset propagation happens
	 *         if the active is a basis for any other. */
	if (key && (key->type == KEY_RELATIVE)) {
		/* Since we can't add, remove or reorder keyblocks in editmode, it's safe to assume
		 * shapenr from restored bmesh and keyblock indices are in sync. */
		const int kb_act_idx = ob->shapenr - 1;
		
		/* If it is, let's patch the current mesh key block to its restored value.
		 * Else, the offsets won't be computed and it won't matter. */
		if (BKE_keyblock_is_basis(key, kb_act_idx)) {
			KeyBlock *kb_act = BLI_findlink(&key->block, kb_act_idx);
			
			if (kb_act->totelem != undo->me.totvert) {
				/* The current mesh has some extra/missing verts compared to the undo, adjust. */
				MEM_SAFE_FREE(kb_act->data);
				kb_act->data = MEM_mallocN((size_t)(key->elemsize * bm->totvert), __func__);
				kb_act->totelem = undo->me.totvert;
			}
			
			BKE_keyblock_update_from_mesh(&undo->me, kb_act);
		}
	}
#endif

	ob->shapenr = undo->shapenr;
	
	MEM_freeN(edit_tmp);
}

static void strands_free_undo(void *undov)
{
	UndoStrands *undo = undov;
	
	if (undo->me.key) {
		BKE_key_free(undo->me.key);
		MEM_freeN(undo->me.key);
	}

	BKE_mesh_free(&undo->me, false);
	MEM_freeN(undo);
}

/* and this is all the undo system needs to know */
void undo_push_strands(bContext *C, const char *name)
{
	/* edit->ob gets out of date and crashes on mesh undo,
	 * this is an easy way to ensure its OK
	 * though we could investigate the matter further. */
	Object *obact = CTX_data_active_object(C);
	BMEditStrands *edit = BKE_editstrands_from_object(obact);
	edit->ob = obact;
	
	undo_editmode_push(C, name, CTX_data_active_object, strands_get_edit, strands_free_undo, strands_undo_to_edit, strands_edit_to_undo, NULL);
}

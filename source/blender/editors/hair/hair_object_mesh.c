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

/** \file blender/editors/hair/hair_object_mesh.c
 *  \ingroup edhair
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_DerivedMesh.h"
#include "BKE_editstrands.h"

#include "bmesh.h"

#include "hair_intern.h"

bool ED_hair_object_init_mesh_edit(Scene *UNUSED(scene), Object *ob)
{
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		
		if (!me->edit_strands) {
			BMesh *bm = BKE_editstrands_mesh_to_bmesh(ob, me);
			DerivedMesh *root_dm = CDDM_new(0, 0, 0, 0, 0);
			
			me->edit_strands = BKE_editstrands_create(bm, root_dm);
			
			root_dm->release(root_dm);
		}
		return true;
	}
	
	return false;
}

bool ED_hair_object_apply_mesh_edit(Object *ob)
{
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		
		if (me->edit_strands) {
			BKE_editstrands_mesh_from_bmesh(ob);
			
			BKE_editstrands_free(me->edit_strands);
			MEM_freeN(me->edit_strands);
			me->edit_strands = NULL;
		}
		
		return true;
	}
	
	return false;
}

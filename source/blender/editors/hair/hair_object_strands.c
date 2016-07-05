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

/** \file blender/editors/hair/hair_object_strands.c
 *  \ingroup edhair
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_strand_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_editstrands.h"
#include "BKE_modifier.h"
#include "BKE_strands.h"

#include "bmesh.h"

#include "hair_intern.h"

bool ED_hair_object_has_strands_data(Object *ob)
{
	StrandsModifierData *smd = (StrandsModifierData *)modifiers_findByType(ob, eModifierType_Strands);
	return (smd != NULL);
}

bool ED_hair_object_init_strands_edit(Scene *scene, Object *ob)
{
	StrandsModifierData *smd = (StrandsModifierData *)modifiers_findByType(ob, eModifierType_Strands);
	
	if (smd) {
		DerivedMesh *dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
		if (dm) {
			BMesh *bm = BKE_editstrands_strands_to_bmesh(smd->strands, dm);
			
			smd->edit = BKE_editstrands_create(bm, dm);
			
			return true;
		}
	}
	
	return false;
}

bool ED_hair_object_apply_strands_edit(Scene *scene, Object *ob)
{
	StrandsModifierData *smd = (StrandsModifierData *)modifiers_findByType(ob, eModifierType_Strands);
	
	if (smd) {
		if (smd->edit) {
			DerivedMesh *dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
			if (dm) {
				BKE_editstrands_strands_from_bmesh(smd->strands, smd->edit->base.bm, dm);
			}
			
			BKE_editstrands_free(smd->edit);
			MEM_freeN(smd->edit);
			smd->edit = NULL;
			
			/* invalidate roots */
			if (smd->roots) {
				MEM_freeN(smd->roots);
				smd->roots = NULL;
			}
		}
		
		return true;
	}
	
	return false;
}

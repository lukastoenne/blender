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

/** \file blender/editors/hair/hair_object_particles.c
 *  \ingroup edhair
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_DerivedMesh.h"
#include "BKE_editstrands.h"
#include "BKE_particle.h"

#include "bmesh.h"

#include "hair_intern.h"

bool ED_hair_object_has_hair_particle_data(Object *ob)
{
	ParticleSystem *psys = psys_get_current(ob);
	if (psys && psys->part->type == PART_HAIR)
		return true;
	
	return false;
}

bool ED_hair_object_init_particle_edit(Scene *scene, Object *ob)
{
	ParticleSystem *psys = psys_get_current(ob);
	BMesh *bm;
	DerivedMesh *dm;
	
	if (psys && psys->part->type == PART_HAIR) {
		if (!psys->hairedit) {
			bm = BKE_particles_to_bmesh(ob, psys);
			
			if (ob->type == OB_MESH || ob->derivedFinal)
				dm = ob->derivedFinal ? ob->derivedFinal : mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
			else
				dm = NULL;
			
			psys->hairedit = BKE_editstrands_create(bm, dm);
		}
		return true;
	}
	
	return false;
}

bool ED_hair_object_apply_particle_edit(Object *ob)
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

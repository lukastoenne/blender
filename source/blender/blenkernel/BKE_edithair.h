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

#ifndef __BKE_EDITHAIR_H__
#define __BKE_EDITHAIR_H__

/** \file blender/editors/hair/BKE_edithair.h
 *  \ingroup bke
 */

#include "BLI_utildefines.h"

#include "DNA_customdata_types.h"

#include "BKE_customdata.h"
#include "bmesh.h"

typedef struct BMEditStrands {
	struct BMesh *bm;
	
	/*this is for undoing failed operations*/
	struct BMEditStrands *emcopy;
	int emcopyusers;
	
	/*selection mode*/
	short selectmode;
	
	/* Object this editmesh came from (if it came from one) */
	struct Object *ob;
} BMEditStrands;

struct BMEditStrands *BKE_editstrands_create(struct BMesh *bm);
struct BMEditStrands *BKE_editstrands_copy(struct BMEditStrands *em);
void BKE_editstrands_update_linked_customdata(struct BMEditStrands *em);
void BKE_editstrands_free(struct BMEditStrands *em);

/* === particle conversion === */

struct BMesh *BKE_particles_to_bmesh(struct Object *ob, struct ParticleSystem *psys);
void BKE_particles_from_bmesh(struct Object *ob, struct ParticleSystem *psys);

#endif

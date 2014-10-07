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

/** \file blender/editors/hair/hair_intern.h
 *  \ingroup edhair
 */

#ifndef __HAIR_INTERN_H__
#define __HAIR_INTERN_H__

#include "DNA_customdata_types.h"

struct Object;
struct ParticleSystem;

/* hair curve */
typedef struct HairEditCurve {
	int start;          /* first vertex index */
} HairEditCurve;

typedef struct HairEditVertex {
	float co[3];
} HairEditVertex;

typedef struct HairEditData {
	HairEditCurve *curves;
	HairEditVertex *verts;
	
	int totcurves;
	int totverts;
	
	CustomData hdata;   /* curve data */
	CustomData vdata;   /* vertex data */
} HairEditData;

struct HairEditData *hair_edit_create(int totcurves, int totverts);
struct HairEditData *hair_edit_copy(struct HairEditData *hedit);
void hair_edit_free(struct HairEditData *hedit);

/* === particle conversion === */

struct HairEditData *hair_edit_from_particles(struct Object *ob, struct ParticleSystem *psys);
void hair_edit_to_particles(struct HairEditData *hedit, struct Object *ob, struct ParticleSystem *psys);

#endif

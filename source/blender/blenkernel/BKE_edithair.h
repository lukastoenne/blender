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

#include "DNA_customdata_types.h"

/* hair curve */
typedef struct HairEditCurve {
	int start;          /* first vertex index */
	int numverts;       /* number of vertices in the curve */
} HairEditCurve;

typedef struct HairEditVertex {
	float co[3];
} HairEditVertex;

typedef struct HairEditData {
	HairEditCurve *curves;
	HairEditVertex *verts;
	
	int totcurves, alloc_curves;
	int totverts, alloc_verts;
	
	CustomData hdata;   /* curve data */
	CustomData vdata;   /* vertex data */
} HairEditData;

struct HairEditData *BKE_edithair_create(void);
struct HairEditData *BKE_edithair_copy(struct HairEditData *hedit);
void BKE_edithair_free(struct HairEditData *hedit);

void BKE_edithair_clear(struct HairEditData *hedit);
void BKE_edithair_reserve(struct HairEditData *hedit, int alloc_curves, int alloc_verts, bool shrink);

/* === particle conversion === */

struct Object;
struct ParticleSystem;

void BKE_edithair_from_particles(struct HairEditData *hedit, struct Object *ob, struct ParticleSystem *psys);
void BKE_edithair_to_particles(struct HairEditData *hedit, struct Object *ob, struct ParticleSystem *psys);

#endif

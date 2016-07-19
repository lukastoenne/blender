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

#ifndef __BKE_EDITSTRANDS_H__
#define __BKE_EDITSTRANDS_H__

/** \file blender/blenkernel/BKE_editstrands.h
 *  \ingroup bke
 */

#include "BLI_utildefines.h"

#include "DNA_customdata_types.h"

#include "BKE_customdata.h"
#include "BKE_editmesh.h"

#include "bmesh.h"

struct BMesh;
struct DerivedMesh;
struct GPUStrandsConverter;
struct Mesh;
struct Object;
struct Strands;
struct StrandFiber;

typedef struct BMEditStrands {
	BMEditMesh base;
	
	/* Hair follicles */
	struct StrandFiber *fibers;
	/* Scalp mesh for fixing root vertices */
	struct DerivedMesh *root_dm;
	int totfibers;
	
	int flag;
} BMEditStrands;

/* BMEditStrands->flag */
typedef enum BMEditStrandsFlag {
	BM_STRANDS_DIRTY_SEGLEN     = 1,
} BMEditStrandsFlag;

struct BMEditStrands *BKE_editstrands_create(struct BMesh *bm, struct DerivedMesh *root_dm,
                                             struct StrandFiber *fibers, int num_fibers);
struct BMEditStrands *BKE_editstrands_copy(struct BMEditStrands *es);
struct BMEditStrands *BKE_editstrands_from_object(struct Object *ob);
void BKE_editstrands_update_linked_customdata(struct BMEditStrands *es);
void BKE_editstrands_free(struct BMEditStrands *es);

bool BKE_editstrands_get_location(struct BMEditStrands *edit, struct BMVert *curve, float loc[3]);
bool BKE_editstrands_get_vectors(struct BMEditStrands *edit, struct BMVert *curve, float loc[3], float nor[3], float tang[3]);
bool BKE_editstrands_get_matrix(struct BMEditStrands *edit, struct BMVert *curve, float mat[4][4]);

/* === constraints === */

/* Stores vertex locations for temporary reference:
 * Vertex locations get modified by tools, but then need to be corrected
 * by calculating a smooth solution based on the difference to original pre-tool locations.
 */
typedef float (*BMEditStrandsLocations)[3];
BMEditStrandsLocations BKE_editstrands_get_locations(struct BMEditStrands *edit);
void BKE_editstrands_free_locations(BMEditStrandsLocations locs);

void BKE_editstrands_solve_constraints(struct Object *ob, struct BMEditStrands *es, BMEditStrandsLocations orig);
void BKE_editstrands_ensure(struct BMEditStrands *es);

/* === particle conversion === */
struct BMesh *BKE_editstrands_particles_to_bmesh(struct Object *ob, struct ParticleSystem *psys);
void BKE_editstrands_particles_from_bmesh(struct Object *ob, struct ParticleSystem *psys);

/* === mesh conversion === */
struct BMesh *BKE_editstrands_mesh_to_bmesh(struct Object *ob, struct Mesh *me);
void BKE_editstrands_mesh_from_bmesh(struct Object *ob);

/* === strands conversion === */
struct BMesh *BKE_editstrands_strands_to_bmesh(struct Strands *strands, struct DerivedMesh *root_dm);
void BKE_editstrands_strands_from_bmesh(struct Strands *strands, struct BMesh *bm, struct DerivedMesh *root_dm);

/* === gpu buffer conversion === */
struct GPUStrandsConverter *BKE_editstrands_get_gpu_converter(struct BMEditStrands *edit, struct DerivedMesh *root_dm,
                                                              int subdiv, int fiber_primitive, bool use_geomshader);

#endif

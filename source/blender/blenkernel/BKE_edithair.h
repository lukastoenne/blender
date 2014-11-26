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

/* hair curve */
typedef struct HairEditCurve {
	void *data;
	
	struct HairEditVertex *v;
} HairEditCurve;

typedef struct HairEditVertex {
	struct HairEditVertex *next, *prev; /* next/prev verts on the strand */
	
	void *data;
	
	float co[3];
} HairEditVertex;

typedef struct HairEditData {
	int totcurves, totverts;
	
	/* element pools */
	struct BLI_mempool *cpool, *vpool;
	
	CustomData cdata, vdata;
} HairEditData;

/* ==== Iterators ==== */

/* these iterate over all elements of a specific
 * type in the mesh.
 *
 * be sure to keep 'bm_iter_itype_htype_map' in sync with any changes
 */
typedef enum HairEditIterType {
	HAIREDIT_CURVES_OF_MESH = 1,
	HAIREDIT_VERTS_OF_MESH = 2,
	HAIREDIT_VERTS_OF_CURVE = 3,
} HairEditIterType;

#define HAIREDIT_ITYPE_MAX 4

#define HAIREDIT_ITER(ele, iter, data, itype) \
	for (ele = HairEdit_iter_new(iter, data, itype, NULL); ele; ele = HairEdit_iter_step(iter))

#define HAIREDIT_ITER_INDEX(ele, iter, data, itype, indexvar) \
	for (ele = HairEdit_iter_new(iter, data, itype, NULL), indexvar = 0; ele; ele = HairEdit_iter_step(iter), (indexvar)++)

/* a version of HAIREDIT_ITER which keeps the next item in storage
 * so we can delete the current item */
#ifdef DEBUG
#  define HAIREDIT_ITER_MUTABLE(ele, ele_next, iter, data, itype) \
	for (ele = HairEdit_iter_new(iter, data, itype, NULL); \
	ele ? ((void)((iter)->count = HairEdit_iter_mesh_count(itype, data)), \
	       (void)(ele_next = HairEdit_iter_step(iter)), 1) : 0; \
	ele = ele_next)
#else
#  define HAIREDIT_ITER_MUTABLE(ele, ele_next, iter, data, itype) \
	for (ele = HairEdit_iter_new(iter, data, itype, NULL); ele ? ((ele_next = HairEdit_iter_step(iter)), 1) : 0; ele = ele_next)
#endif


#define HAIREDIT_ITER_ELEM(ele, iter, data, itype) \
	for (ele = HairEdit_iter_new(iter, NULL, itype, data); ele; ele = HairEdit_iter_step(iter))

#define HAIREDIT_ITER_ELEM_INDEX(ele, iter, data, itype, indexvar) \
	for (ele = HairEdit_iter_new(iter, NULL, itype, data), indexvar = 0; ele; ele = HairEdit_iter_step(iter), (indexvar)++)

#include "intern/edithair_inline.h"

/* =================== */

struct HairEditData *BKE_edithair_create(void);

void BKE_edithair_data_free(struct HairEditData *hedit);
void BKE_edithair_free(struct HairEditData *hedit);

void BKE_edithair_clear(struct HairEditData *hedit);

void BKE_edithair_get_min_max(struct HairEditData *hedit, float r_min[3], float r_max[3]);

struct HairEditCurve *BKE_edithair_curve_create(struct HairEditData *hedit, struct HairEditCurve *example);

int BKE_edithair_curve_vertex_count(struct HairEditData *hedit, struct HairEditCurve *c);
struct HairEditVertex *BKE_edithair_curve_extend(struct HairEditData *hedit, struct HairEditCurve *c, struct HairEditVertex *example, int num);

/* === particle conversion === */

struct Object;
struct ParticleSystem;

void BKE_edithair_from_particles(struct HairEditData *hedit, struct Object *ob, struct ParticleSystem *psys);
void BKE_edithair_to_particles(struct HairEditData *hedit, struct Object *ob, struct ParticleSystem *psys);

#endif

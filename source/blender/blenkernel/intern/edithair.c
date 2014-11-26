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

/** \file blender/editors/hair/edithair.c
 *  \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_mempool.h"

#include "DNA_customdata_types.h"

#include "BKE_customdata.h"
#include "BKE_edithair.h"

static const int chunksize_default_totcurve = 512;
static const int allocsize_default_totcurve = 512;

static const int chunksize_default_totvert = 1024;
static const int allocsize_default_totvert = 1024;

static void edithair_mempool_init(HairEditData *hedit)
{
	hedit->cpool = BLI_mempool_create(sizeof(HairEditCurve), allocsize_default_totcurve,
	                                  chunksize_default_totcurve, BLI_MEMPOOL_ALLOW_ITER);
	hedit->vpool = BLI_mempool_create(sizeof(HairEditVertex), allocsize_default_totvert,
	                                  chunksize_default_totvert, BLI_MEMPOOL_ALLOW_ITER);
}

HairEditData *BKE_edithair_create(void)
{
	HairEditData *hedit = MEM_callocN(sizeof(HairEditData), "hair edit data");
	
	edithair_mempool_init(hedit);
	
	CustomData_reset(&hedit->cdata);
	CustomData_reset(&hedit->vdata);
	
	return hedit;
}

void BKE_edithair_data_free(HairEditData *hedit)
{
	if (CustomData_bmesh_has_free(&(hedit->cdata))) {
		HairEditCurve *curve;
		HairEditIter iter;
		HAIREDIT_ITER(curve, &iter, hedit, HAIREDIT_CURVES_OF_MESH) {
			CustomData_bmesh_free_block(&hedit->cdata, &curve->data);
		}
	}
	
	if (CustomData_bmesh_has_free(&(hedit->vdata))) {
		HairEditVertex *vert;
		HairEditIter iter;
		HAIREDIT_ITER(vert, &iter, hedit, HAIREDIT_VERTS_OF_MESH) {
			CustomData_bmesh_free_block(&hedit->vdata, &vert->data);
		}
	}
	
	/* Free custom data pools, This should probably go in CustomData_free? */
	if (hedit->cdata.totlayer) BLI_mempool_destroy(hedit->cdata.pool);
	if (hedit->vdata.totlayer) BLI_mempool_destroy(hedit->vdata.pool);
	
	/* free custom data */
	CustomData_free(&hedit->cdata, 0);
	CustomData_free(&hedit->vdata, 0);

	/* destroy element pools */
	BLI_mempool_destroy(hedit->cpool);
	BLI_mempool_destroy(hedit->vpool);
}

void BKE_edithair_clear(HairEditData *hedit)
{
	/* free old data */
	BKE_edithair_data_free(hedit);
	memset(hedit, 0, sizeof(HairEditData));

	/* allocate the memory pools for the hair elements */
	edithair_mempool_init(hedit);

	CustomData_reset(&hedit->cdata);
	CustomData_reset(&hedit->vdata);
}

void BKE_edithair_free(HairEditData *hedit)
{
	BKE_edithair_data_free(hedit);
	
	MEM_freeN(hedit);
}

void BKE_edithair_get_min_max(HairEditData *hedit, float r_min[3], float r_max[3])
{
	if (hedit->totverts) {
		HairEditVertex *vert;
		HairEditIter iter;
		
		INIT_MINMAX(r_min, r_max);
		
		HAIREDIT_ITER(vert, &iter, hedit, HAIREDIT_VERTS_OF_MESH) {
			minmax_v3v3_v3(r_min, r_max, vert->co);
		}
	}
	else {
		zero_v3(r_min);
		zero_v3(r_max);
	}
}

HairEditCurve *BKE_edithair_curve_create(HairEditData *hedit, HairEditCurve *example)
{
	HairEditCurve *c = NULL;

	c = BLI_mempool_alloc(hedit->cpool);

	/* --- assign all members --- */
	c->data = NULL;

	c->v = NULL;
	/* --- done --- */

	hedit->totcurves++;

	if (example) {
		CustomData_bmesh_copy_data(&hedit->cdata, &hedit->cdata, example->data, &c->data);
	}
	else {
		CustomData_bmesh_set_default(&hedit->cdata, &c->data);
	}

	return c;
}

int BKE_edithair_curve_vertex_count(HairEditData *UNUSED(hedit), HairEditCurve *c)
{
	const HairEditVertex *v;
	int count = 0;
	for (v = c->v; v; v = v->next) {
		++count;
	}
	return count;
}

static HairEditVertex *edithair_vertex_create(HairEditData *hedit, HairEditVertex *example)
{
	HairEditVertex *v = NULL;

	v = BLI_mempool_alloc(hedit->vpool);

	/* --- assign all members --- */
	v->data = NULL;

	v->next = v->prev = NULL;
	/* --- done --- */

	hedit->totverts++;

	if (example) {
		CustomData_bmesh_copy_data(&hedit->vdata, &hedit->vdata, example->data, &v->data);
	}
	else {
		CustomData_bmesh_set_default(&hedit->vdata, &v->data);
	}

	return v;
}

HairEditVertex *BKE_edithair_curve_extend(HairEditData *hedit, HairEditCurve *c, HairEditVertex *example, int num)
{
	HairEditVertex *v_first = NULL;
	int i;
	
	for (i = 0; i < num; ++i) {
		HairEditVertex *v = edithair_vertex_create(hedit, example);
		if (i == 0)
			v_first = v;
		
		v->prev = c->v;
		if (c->v)
			c->v->next = v;
		c->v = v;
	}
	
	return v_first;
}

/* ==== Iterators ==== */

/*
 * CURVE OF MESH CALLBACKS
 */

void hairedit_iter__elem_of_mesh_begin(struct HairEditIter__elem_of_mesh *iter)
{
	BLI_mempool_iternew(iter->pooliter.pool, &iter->pooliter);
}

void *hairedit_iter__elem_of_mesh_step(struct HairEditIter__elem_of_mesh *iter)
{
	return BLI_mempool_iterstep(&iter->pooliter);
}

/*
 * VERT OF CURVE CALLBACKS
 */

void  hairedit_iter__vert_of_curve_begin(struct HairEditIter__vert_of_curve *iter)
{
	if (iter->cdata->v) {
		iter->v_first = iter->cdata->v;
		iter->v_next = iter->cdata->v;
	}
	else {
		iter->v_first = NULL;
		iter->v_next = NULL;
	}
}

void  *hairedit_iter__vert_of_curve_step(struct HairEditIter__vert_of_curve *iter)
{
	HairEditVertex *v_curr = iter->v_next;

	if (iter->v_next) {
		iter->v_next = iter->v_next->next;
	}

	return v_curr;
}

/* =================== */

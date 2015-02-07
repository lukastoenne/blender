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

/** \file blender/editors/hair/hair_mirror.c
 *  \ingroup edhair
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_editmesh_bvh.h"
#include "BKE_editstrands.h"

#include "ED_physics.h"
#include "ED_util.h"

#include "bmesh.h"

#include "hair_intern.h"

/* TODO use_topology is not yet implemented for strands.
 * Native strand topology is not very useful for this.
 * Instead, the topology of the scalp mesh should be used for finding mirrored strand roots,
 * then the arc- or parametric length of a vertex from the root to find mirrored verts.
 */

#define BM_SEARCH_MAXDIST_MIRR 0.00002f
#define BM_CD_LAYER_ID "__mirror_index"
/**
 * \param edit  Edit strands.
 * \param use_self  Allow a vertex to point to its self (middle verts).
 * \param use_select  Restrict to selected verts.
 * \param use_topology  Use topology mirror.
 * \param maxdist  Distance for close point test.
 * \param r_index  Optional array to write into, as an alternative to a customdata layer (length of total verts).
 */
void ED_strands_mirror_cache_begin_ex(BMEditStrands *edit, const int axis, const bool use_self, const bool use_select,
                                      /* extra args */
                                      const bool UNUSED(use_topology), float maxdist, int *r_index)
{
	BMesh *bm = edit->bm;
	BMIter iter;
	BMVert *v;
	int cd_vmirr_offset;
	int i;

	/* one or the other is used depending if topo is enabled */
	struct BMBVHTree *tree = NULL;

	BM_mesh_elem_table_ensure(bm, BM_VERT);

	if (r_index == NULL) {
		const char *layer_id = BM_CD_LAYER_ID;
		edit->mirror_cdlayer = CustomData_get_named_layer_index(&bm->vdata, CD_PROP_INT, layer_id);
		if (edit->mirror_cdlayer == -1) {
			BM_data_layer_add_named(bm, &bm->vdata, CD_PROP_INT, layer_id);
			edit->mirror_cdlayer = CustomData_get_named_layer_index(&bm->vdata, CD_PROP_INT, layer_id);
		}

		cd_vmirr_offset = CustomData_get_n_offset(&bm->vdata, CD_PROP_INT,
		                                          edit->mirror_cdlayer - CustomData_get_layer_index(&bm->vdata, CD_PROP_INT));

		bm->vdata.layers[edit->mirror_cdlayer].flag |= CD_FLAG_TEMPORARY;
	}

	BM_mesh_elem_index_ensure(bm, BM_VERT);

	tree = BKE_bmbvh_new(edit->bm, NULL, 0, 0, NULL, false);

#define VERT_INTPTR(_v, _i) r_index ? &r_index[_i] : BM_ELEM_CD_GET_VOID_P(_v, cd_vmirr_offset);

	BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
		BLI_assert(BM_elem_index_get(v) == i);

		/* temporary for testing, check for selection */
		if (use_select && !BM_elem_flag_test(v, BM_ELEM_SELECT)) {
			/* do nothing */
		}
		else {
			BMVert *v_mirr;
			float co[3];
			int *idx = VERT_INTPTR(v, i);

			copy_v3_v3(co, v->co);
			co[axis] *= -1.0f;
			v_mirr = BKE_bmbvh_find_vert_closest(tree, co, maxdist);

			if (v_mirr && (use_self || (v_mirr != v))) {
				const int i_mirr = BM_elem_index_get(v_mirr);
				*idx = i_mirr;
				idx = VERT_INTPTR(v_mirr, i_mirr);
				*idx = i;
			}
			else {
				*idx = -1;
			}
		}

	}

#undef VERT_INTPTR

	BKE_bmbvh_free(tree);
}

void ED_strands_mirror_cache_begin(BMEditStrands *edit, const int axis,
                                   const bool use_self, const bool use_select,
                                   const bool use_topology)
{
	ED_strands_mirror_cache_begin_ex(edit, axis,
	                                 use_self, use_select,
	                                 /* extra args */
	                                 use_topology, BM_SEARCH_MAXDIST_MIRR, NULL);
}

BMVert *ED_strands_mirror_get(BMEditStrands *edit, BMVert *v)
{
	const int *mirr = CustomData_bmesh_get_layer_n(&edit->bm->vdata, v->head.data, edit->mirror_cdlayer);

	BLI_assert(edit->mirror_cdlayer != -1); /* invalid use */

	if (mirr && *mirr >= 0 && *mirr < edit->bm->totvert) {
		if (!edit->bm->vtable) {
			printf("err: should only be called between "
			       "ED_strands_mirror_cache_begin and ED_strands_mirror_cache_end");
			return NULL;
		}

		return edit->bm->vtable[*mirr];
	}

	return NULL;
}

BMEdge *ED_strands_mirror_get_edge(BMEditStrands *edit, BMEdge *e)
{
	BMVert *v1_mirr = ED_strands_mirror_get(edit, e->v1);
	if (v1_mirr) {
		BMVert *v2_mirr = ED_strands_mirror_get(edit, e->v2);
		if (v2_mirr) {
			return BM_edge_exists(v1_mirr, v2_mirr);
		}
	}

	return NULL;
}

void ED_strands_mirror_cache_clear(BMEditStrands *edit, BMVert *v)
{
	int *mirr = CustomData_bmesh_get_layer_n(&edit->bm->vdata, v->head.data, edit->mirror_cdlayer);

	BLI_assert(edit->mirror_cdlayer != -1); /* invalid use */

	if (mirr) {
		*mirr = -1;
	}
}

void ED_strands_mirror_cache_end(BMEditStrands *edit)
{
	edit->mirror_cdlayer = -1;
}

void ED_strands_mirror_apply(BMEditStrands *edit, const int sel_from, const int sel_to)
{
	BMIter iter;
	BMVert *v;

	BLI_assert((edit->bm->vtable != NULL) && ((edit->bm->elem_table_dirty & BM_VERT) == 0));

	BM_ITER_MESH (v, &iter, edit->bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(v, BM_ELEM_SELECT) == sel_from) {
			BMVert *mirr = ED_strands_mirror_get(edit, v);
			if (mirr) {
				if (BM_elem_flag_test(mirr, BM_ELEM_SELECT) == sel_to) {
					copy_v3_v3(mirr->co, v->co);
					mirr->co[0] *= -1.0f;
				}
			}
		}
	}
}

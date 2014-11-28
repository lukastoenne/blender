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
 * Contributor(s): Lukas Toenne.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_STRANDS_H__
#define __BMESH_STRANDS_H__

/** \file blender/bmesh/intern/bmesh_strands.h
 *  \ingroup bmesh
 */

#include "BLI_utildefines.h"

#include "bmesh.h"
#include "bmesh_queries.h"
#include "bmesh_structure.h"

/* True if v is the root of a strand */
BLI_INLINE bool BM_strands_vert_is_root(BMVert *v)
{
	BMEdge *e_first = v->e;
	BMEdge *e_next;
	
	e_next = bmesh_disk_edge_next(e_first, v);
	
	/* with a single edge, the vertex is either first or last of the curve;
	 * first vertex is defined as the root
	 */
	if (e_next == e_first) {
		if (e_first->v1 == v)
			return true;
	}
	return false;
}

int BM_strands_count(BMesh *bm);
int BM_strands_keys_count(BMVert *root);

/* ==== Iterators ==== */

typedef enum BMStrandsIterType {
	BM_STRANDS_OF_MESH,
	BM_VERTS_OF_STRAND,
} BMStrandsIterType;

#define BM_ITER_STRANDS(ele, iter, bm, itype) \
	for (ele = BM_strand_iter_new(iter, bm, itype, NULL); ele; ele = BM_iter_step(iter))

#define BM_ITER_STRANDS_ELEM(ele, iter, data, itype) \
	for (ele = BM_strand_iter_new(iter, NULL, itype, data); ele; ele = BM_iter_step(iter))

typedef struct BMIter__vert_of_strand {
	BMVert *v_next;
	BMEdge *e_next;
} BMIter__vert_of_strand;

void  bmstranditer__strands_of_mesh_begin(struct BMIter__elem_of_mesh *iter);
void *bmstranditer__strands_of_mesh_step(struct BMIter__elem_of_mesh *iter);

void  bmstranditer__verts_of_strand_begin(struct BMIter__vert_of_strand *iter);
void *bmstranditer__verts_of_strand_step(struct BMIter__vert_of_strand *iter);

BLI_INLINE bool BM_strand_iter_init(BMIter *iter, BMesh *bm, const char itype, void *data)
{
	/* int argtype; */
	iter->itype = itype;

	/* inlining optimizes out this switch when called with the defined type */
	switch ((BMStrandsIterType)itype) {
		case BM_STRANDS_OF_MESH:
			BLI_assert(bm != NULL);
			BLI_assert(data == NULL);
			iter->begin = (BMIter__begin_cb)bmstranditer__strands_of_mesh_begin;
			iter->step  = (BMIter__step_cb)bmstranditer__strands_of_mesh_step;
			iter->data.elem_of_mesh.pooliter.pool = bm->vpool;
			break;
		case BM_VERTS_OF_STRAND: {
			BMVert *root;
			
			BLI_assert(data != NULL);
			BLI_assert(((BMElem *)data)->head.htype == BM_VERT);
			root = (BMVert *)data;
			BLI_assert(BM_strands_vert_is_root(root));
			iter->begin = (BMIter__begin_cb)bmstranditer__verts_of_strand_begin;
			iter->step  = (BMIter__step_cb)bmstranditer__verts_of_strand_step;
			((BMIter__vert_of_strand *)(&iter->data))->v_next = root;
			break;
		}
		default:
			/* fallback to regular bmesh iterator */
			return BM_iter_init(iter, bm, itype, data);
			break;
	}
	
	iter->begin(iter);

	return true;
}

/**
 * \brief Iterator New
 *
 * Takes a bmesh iterator structure and fills
 * it with the appropriate function pointers based
 * upon its type and then calls BMeshIter_step()
 * to return the first element of the iterator.
 *
 */
BLI_INLINE void *BM_strand_iter_new(BMIter *iter, BMesh *bm, const char itype, void *data)
{
	if (LIKELY(BM_strand_iter_init(iter, bm, itype, data))) {
		return BM_iter_step(iter);
	}
	else {
		return NULL;
	}
}

#define BM_strand_iter_new(iter, bm, itype, data) \
	(BM_ITER_CHECK_TYPE_DATA(data), BM_strand_iter_new(iter, bm, itype, data))

#endif /* __BMESH_STRANDS_H__ */

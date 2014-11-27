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

/** \file blender/bmesh/intern/bmesh_strands.c
 *  \ingroup bmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_mempool.h"

#include "bmesh.h"
#include "bmesh_private.h"

/*
 * STRANDS OF MESH CALLBACKS
 */

void  bmstranditer__strands_of_mesh_begin(struct BMIter__elem_of_mesh *iter)
{
	BLI_mempool_iternew(iter->pooliter.pool, &iter->pooliter);
}

void *bmstranditer__strands_of_mesh_step(struct BMIter__elem_of_mesh *iter)
{
	BMVert *v;
	
	do {
		v = BLI_mempool_iterstep(&iter->pooliter);
	} while (v && !BM_strands_vert_is_root(v));
	
	return v;
}

/*
 * VERTS OF STRAND CALLBACKS
 */

void  bmstranditer__verts_of_strand_begin(struct BMIter__vert_of_edge *UNUSED(iter))
{
}

void *bmstranditer__verts_of_strand_step(struct BMIter__vert_of_edge *iter)
{
	if (iter->edata) {
		/* by definition the all strand edges run in the same direction,
		 * with the root being v1 of the first edge.
		 */
		BMVert *v_curr = iter->edata->v1;
		BMEdge *e_first = iter->edata;
		
		iter->edata = bmesh_disk_edge_next(iter->edata, iter->edata->v2);
		if (iter->edata == e_first) {
			/* only one edge means the last segment, terminate */
			iter->edata = NULL;
		}
		
		return v_curr;
	}
	else
		return NULL;
}

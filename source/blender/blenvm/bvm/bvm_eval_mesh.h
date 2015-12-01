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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BVM_EVAL_MESH_H__
#define __BVM_EVAL_MESH_H__

/** \file bvm_eval_mesh.h
 *  \ingroup bvm
 */

extern "C" {
#include "BLI_math.h"
}

#include "bvm_eval_common.h"

namespace bvm {

static void eval_op_mesh_load(const EvalData *data, float *stack, StackIndex offset)
{
	DerivedMesh *dm = CDDM_from_mesh(data->modifier.base_mesh);
	stack_store_mesh(stack, offset, dm);
}

static DerivedMesh *do_array(const EvalGlobals *globals, const EvalData *data, const EvalKernelData *kernel_data, float *stack,
                             DerivedMesh *dm, int count, int fn_transform, StackIndex offset_transform)
{
	const bool use_recalc_normals = (dm->dirty & DM_DIRTY_NORMALS);
	EvalData iter_data = *data;
	
	int chunk_nverts = dm->getNumVerts(dm);
	int chunk_nedges = dm->getNumEdges(dm);
	int chunk_nloops = dm->getNumLoops(dm);
	int chunk_npolys = dm->getNumPolys(dm);
	
	/* The number of verts, edges, loops, polys, before eventually merging doubles */
	int result_nverts = chunk_nverts * count;
	int result_nedges = chunk_nedges * count;
	int result_nloops = chunk_nloops * count;
	int result_npolys = chunk_npolys * count;

	/* Initialize a result dm */
	MVert *orig_dm_verts = dm->getVertArray(dm);
	DerivedMesh *result = CDDM_from_template(dm, result_nverts, result_nedges, 0, result_nloops, result_npolys);
	MVert *result_dm_verts = CDDM_get_verts(result);

	/* copy customdata to original geometry */
	DM_copy_vert_data(dm, result, 0, 0, chunk_nverts);
	DM_copy_edge_data(dm, result, 0, 0, chunk_nedges);
	DM_copy_loop_data(dm, result, 0, 0, chunk_nloops);
	DM_copy_poly_data(dm, result, 0, 0, chunk_npolys);

#if 0 /* XXX is this needed? comment is unintelligible */
	/* subsurf for eg wont have mesh data in the
	 * now add mvert/medge/mface layers */

	if (!CustomData_has_layer(&dm->vertData, CD_MVERT)) {
		dm->copyVertArray(dm, result_dm_verts);
	}
	if (!CustomData_has_layer(&dm->edgeData, CD_MEDGE)) {
		dm->copyEdgeArray(dm, CDDM_get_edges(result));
	}
	if (!CustomData_has_layer(&dm->polyData, CD_MPOLY)) {
		dm->copyLoopArray(dm, CDDM_get_loops(result));
		dm->copyPolyArray(dm, CDDM_get_polys(result));
	}
#endif

	for (int c = 0; c < count; c++) {
		/* copy customdata to new geometry */
		DM_copy_vert_data(result, result, 0, c * chunk_nverts, chunk_nverts);
		DM_copy_edge_data(result, result, 0, c * chunk_nedges, chunk_nedges);
		DM_copy_loop_data(result, result, 0, c * chunk_nloops, chunk_nloops);
		DM_copy_poly_data(result, result, 0, c * chunk_npolys, chunk_npolys);

		/* calculate transform for the copy */
		iter_data.iteration = c;
		kernel_data->context->eval_expression(globals, &iter_data, kernel_data->function, fn_transform, stack);
		matrix44 tfm = stack_load_matrix44(stack, offset_transform);
		float mat[4][4];
		transpose_m4_m4(mat, (float (*)[4])tfm.data);

		/* apply offset to all new verts */
		MVert *mv_orig = orig_dm_verts;
		MVert *mv = result_dm_verts + c * chunk_nverts;
		for (int i = 0; i < chunk_nverts; i++, mv++, mv_orig++) {
			mul_v3_m4v3(mv->co, mat, mv_orig->co);

			/* We have to correct normals too, if we do not tag them as dirty! */
			if (!use_recalc_normals) {
				float no[3];
				normal_short_to_float_v3(no, mv->no);
				mul_mat3_m4_v3(mat, no);
				normalize_v3(no);
				normal_float_to_short_v3(mv->no, no);
			}
		}

		/* adjust edge vertex indices */
		MEdge *me = CDDM_get_edges(result) + c * chunk_nedges;
		for (int i = 0; i < chunk_nedges; i++, me++) {
			me->v1 += c * chunk_nverts;
			me->v2 += c * chunk_nverts;
		}

		MPoly *mp = CDDM_get_polys(result) + c * chunk_npolys;
		for (int i = 0; i < chunk_npolys; i++, mp++) {
			mp->loopstart += c * chunk_nloops;
		}

		/* adjust loop vertex and edge indices */
		MLoop *ml = CDDM_get_loops(result) + c * chunk_nloops;
		for (int i = 0; i < chunk_nloops; i++, ml++) {
			ml->v += c * chunk_nverts;
			ml->e += c * chunk_nedges;
		}
	}

	/* In case org dm has dirty normals, or we made some merging, mark normals as dirty in new dm!
	 * TODO: we may need to set other dirty flags as well?
	 */
	if (use_recalc_normals) {
		result->dirty = (DMDirtyFlag)(result->dirty | (int)DM_DIRTY_NORMALS);
	}
	
	return result;
}

static void eval_op_mesh_array(const EvalGlobals *globals, const EvalData *data, const EvalKernelData *kernel_data, float *stack,
                               StackIndex offset_mesh_in, StackIndex offset_mesh_out,
                               StackIndex offset_count, int fn_transform, StackIndex offset_transform)
{
	DerivedMesh *dm = stack_load_mesh(stack, offset_mesh_in);
	int count = stack_load_int(stack, offset_count);
	CLAMP_MIN(count, 0);
	
	DerivedMesh *result = do_array(globals, data, kernel_data, stack, dm, count, fn_transform, offset_transform);
	
	stack_store_mesh(stack, offset_mesh_out, result);
}

} /* namespace bvm */

#endif /* __BVM_EVAL_MESH_H__ */

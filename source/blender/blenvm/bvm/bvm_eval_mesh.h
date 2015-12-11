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

#include "DNA_object_types.h"

#include "bmesh.h"
#include "bmesh_tools.h"
#include "tools/bmesh_intersect.h"
}

#include "bvm_eval_common.h"

namespace bvm {

static void eval_op_mesh_load(float *stack, StackIndex offset_base_mesh, StackIndex offset_mesh)
{
	PointerRNA ptr = stack_load_pointer(stack, offset_base_mesh);
	DerivedMesh *dm;
	if (ptr.data && RNA_struct_is_a(&RNA_Mesh, ptr.type)) {
		dm = CDDM_from_mesh((Mesh *)ptr.data);
	}
	else {
		dm = CDDM_new(0, 0, 0, 0, 0);
	}
	stack_store_mesh(stack, offset_mesh, dm);
}

static void dm_insert(
        DerivedMesh *result, DerivedMesh *dm,
        int ofs_verts, int ofs_edges, int ofs_loops, int ofs_polys)
{
	int *index_orig;
	int i;
	MVert *mv;
	MEdge *me;
	MLoop *ml;
	MPoly *mp;

	/* needed for subsurf so arrays are allocated */
	dm->getVertArray(dm);
	dm->getEdgeArray(dm);
	dm->getLoopArray(dm);
	dm->getPolyArray(dm);

	int cap_nverts = dm->getNumVerts(dm);
	int cap_nedges = dm->getNumEdges(dm);
	int cap_nloops = dm->getNumLoops(dm);
	int cap_npolys = dm->getNumPolys(dm);

	DM_copy_vert_data(dm, result, 0, ofs_verts, cap_nverts);
	DM_copy_edge_data(dm, result, 0, ofs_edges, cap_nedges);
	DM_copy_loop_data(dm, result, 0, ofs_loops, cap_nloops);
	DM_copy_poly_data(dm, result, 0, ofs_polys, cap_npolys);

	mv = CDDM_get_verts(result) + ofs_verts;

	for (i = 0; i < cap_nverts; i++, mv++) {
		/* Reset MVert flags for caps */
		mv->flag = mv->bweight = 0;
	}

	/* adjust cap edge vertex indices */
	me = CDDM_get_edges(result) + ofs_edges;
	for (i = 0; i < cap_nedges; i++, me++) {
		me->v1 += ofs_verts;
		me->v2 += ofs_verts;
	}

	/* adjust cap poly loopstart indices */
	mp = CDDM_get_polys(result) + ofs_polys;
	for (i = 0; i < cap_npolys; i++, mp++) {
		mp->loopstart += ofs_loops;
	}

	/* adjust cap loop vertex and edge indices */
	ml = CDDM_get_loops(result) + ofs_loops;
	for (i = 0; i < cap_nloops; i++, ml++) {
		ml->v += ofs_verts;
		ml->e += ofs_edges;
	}

	/* set origindex */
	index_orig = (int *)result->getVertDataArray(result, CD_ORIGINDEX);
	if (index_orig) {
		copy_vn_i(index_orig + ofs_verts, cap_nverts, ORIGINDEX_NONE);
	}

	index_orig = (int *)result->getEdgeDataArray(result, CD_ORIGINDEX);
	if (index_orig) {
		copy_vn_i(index_orig + ofs_edges, cap_nedges, ORIGINDEX_NONE);
	}

	index_orig = (int *)result->getPolyDataArray(result, CD_ORIGINDEX);
	if (index_orig) {
		copy_vn_i(index_orig + ofs_polys, cap_npolys, ORIGINDEX_NONE);
	}

	index_orig = (int *)result->getLoopDataArray(result, CD_ORIGINDEX);
	if (index_orig) {
		copy_vn_i(index_orig + ofs_loops, cap_nloops, ORIGINDEX_NONE);
	}
}

static void eval_op_mesh_combine(const EvalKernelData */*kernel_data*/, float *stack,
                                 StackIndex offset_mesh_a, StackIndex offset_mesh_b, StackIndex offset_mesh_out)
{
	DerivedMesh *dm_a = stack_load_mesh(stack, offset_mesh_a);
	DerivedMesh *dm_b = stack_load_mesh(stack, offset_mesh_b);
	
	int numVertsA = dm_a->getNumVerts(dm_a);
	int numEdgesA = dm_a->getNumEdges(dm_a);
	int numTessFacesA = dm_a->getNumTessFaces(dm_a);
	int numLoopsA = dm_a->getNumLoops(dm_a);
	int numPolysA = dm_a->getNumPolys(dm_a);
	int numVertsB = dm_b->getNumVerts(dm_b);
	int numEdgesB = dm_b->getNumEdges(dm_b);
	int numTessFacesB = dm_b->getNumTessFaces(dm_b);
	int numLoopsB = dm_b->getNumLoops(dm_b);
	int numPolysB = dm_b->getNumPolys(dm_b);
	
	DerivedMesh *result = CDDM_new(numVertsA + numVertsB,
	                               numEdgesA + numEdgesB,
	                               numTessFacesA + numTessFacesB,
	                               numLoopsA + numLoopsB,
	                               numPolysA + numPolysB);
	
	dm_insert(result, dm_a, 0, 0, 0, 0);
	dm_insert(result, dm_b, numVertsA, numEdgesA, numLoopsA, numPolysA);
	
	stack_store_mesh(stack, offset_mesh_out, result);
}

static DerivedMesh *do_array(const EvalGlobals *globals, const EvalKernelData *kernel_data, float *stack,
                             DerivedMesh *dm, int count,
                             int fn_transform, StackIndex offset_transform, StackIndex offset_iteration)
{
	const bool use_recalc_normals = (dm->dirty & DM_DIRTY_NORMALS);
	
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
		stack_store_int(stack, offset_iteration, c);
		kernel_data->context->eval_expression(globals, kernel_data->function, fn_transform, stack);
		matrix44 tfm = stack_load_matrix44(stack, offset_transform);

		/* apply offset to all new verts */
		MVert *mv_orig = orig_dm_verts;
		MVert *mv = result_dm_verts + c * chunk_nverts;
		for (int i = 0; i < chunk_nverts; i++, mv++, mv_orig++) {
			mul_v3_m4v3(mv->co, tfm.data, mv_orig->co);

			/* We have to correct normals too, if we do not tag them as dirty! */
			if (!use_recalc_normals) {
				float no[3];
				normal_short_to_float_v3(no, mv->no);
				mul_mat3_m4_v3(tfm.data, no);
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

static void eval_op_mesh_array(const EvalGlobals *globals, const EvalKernelData *kernel_data, float *stack,
                               StackIndex offset_mesh_in, StackIndex offset_mesh_out, StackIndex offset_count,
                               int fn_transform, StackIndex offset_transform, StackIndex offset_iteration)
{
	DerivedMesh *dm = stack_load_mesh(stack, offset_mesh_in);
	int count = stack_load_int(stack, offset_count);
	
	DerivedMesh *result = (count > 0) ?
	                          do_array(globals, kernel_data, stack, dm, count, fn_transform, offset_transform, offset_iteration) :
	                          CDDM_new(0, 0, 0, 0, 0);
	
	stack_store_mesh(stack, offset_mesh_out, result);
}

static DerivedMesh *do_displace(const EvalGlobals *globals, const EvalKernelData *kernel_data, float *stack,
                        DerivedMesh *dm, int fn_vector, StackIndex offset_vector,
                        StackIndex offset_elem_index, StackIndex offset_elem_loc)
{
	DerivedMesh *result = CDDM_copy(dm);
	MVert *orig_mv, *orig_mverts = dm->getVertArray(dm);
	MVert *mv, *mverts = result->getVertArray(result);
	int i, numverts = result->getNumVerts(result);
	
	for (i = 0, mv = mverts, orig_mv = orig_mverts; i < numverts; ++i, ++mv, ++orig_mv) {
		stack_store_int(stack, offset_elem_index, i);
		stack_store_float3(stack, offset_elem_loc, float3::from_data(orig_mv->co));
		
		kernel_data->context->eval_expression(globals, kernel_data->function, fn_vector, stack);
		float3 dco = stack_load_float3(stack, offset_vector);
		
		add_v3_v3v3(mv->co, orig_mv->co, dco.data());
	}
	
	result->dirty = (DMDirtyFlag)(result->dirty | (int)DM_DIRTY_NORMALS);
	
	return result;
}

static void eval_op_mesh_displace(const EvalGlobals *globals, const EvalKernelData *kernel_data, float *stack,
                                  StackIndex offset_mesh_in, StackIndex offset_mesh_out,
                                  int fn_vector, StackIndex offset_vector,
                                  StackIndex offset_elem_index, StackIndex offset_elem_loc)
{
	DerivedMesh *dm = stack_load_mesh(stack, offset_mesh_in);
	
	DerivedMesh *result = do_displace(globals, kernel_data, stack,
	                                  dm, fn_vector, offset_vector,
	                                  offset_elem_index, offset_elem_loc);
	
	stack_store_mesh(stack, offset_mesh_out, result);
}

#if 0
static DerivedMesh *boolean_get_quick_derivedMesh(DerivedMesh *derivedData, DerivedMesh *dm, int operation)
{
	DerivedMesh *result = NULL;

	if (derivedData->getNumPolys(derivedData) == 0 || dm->getNumPolys(dm) == 0) {
		switch (operation) {
			case eBooleanModifierOp_Intersect:
				result = CDDM_new(0, 0, 0, 0, 0);
				break;

			case eBooleanModifierOp_Union:
				if (derivedData->getNumPolys(derivedData)) result = derivedData;
				else result = CDDM_copy(dm);

				break;

			case eBooleanModifierOp_Difference:
				result = derivedData;
				break;
		}
	}

	return result;
}
#endif

/* has no meaning for faces, do this so we can tell which face is which */
#define BM_FACE_TAG BM_ELEM_DRAW

/**
 * Compare selected/unselected.
 */
static int bm_face_isect_pair(BMFace *f, void *UNUSED(user_data))
{
	return BM_elem_flag_test(f, BM_FACE_TAG) ? 1 : 0;
}

static DerivedMesh *do_boolean(DerivedMesh *dm, DerivedMesh *dm_other, matrix44 &omat,
                               bool separate, bool dissolve, bool connect_regions,
                               int boolean_mode, float threshold)
{
	DerivedMesh *result = NULL;
	BMesh *bm;
	const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_DM(dm, dm_other);

//	TIMEIT_START(boolean_bmesh);
	bm = BM_mesh_create(&allocsize);

	DM_to_bmesh_ex(dm_other, bm, true);
	DM_to_bmesh_ex(dm, bm, true);

	if (1) {
		/* create tessface & intersect */
		const int looptris_tot = poly_to_tri_count(bm->totface, bm->totloop);
		int tottri;
		BMLoop *(*looptris)[3];

		looptris = (BMLoop *(*)[3])MEM_mallocN(sizeof(*looptris) * looptris_tot, __func__);

		BM_bmesh_calc_tessellation(bm, looptris, &tottri);

		/* postpone this until after tessellating
		 * so we can use the original normals before the vertex are moved */
		{
			BMIter iter;
			int i;
			const int i_verts_end = dm_other->getNumVerts(dm_other);
			const int i_faces_end = dm_other->getNumPolys(dm_other);

			BMVert *eve;
			i = 0;
			BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
				mul_m4_v3(omat.data, eve->co);
				if (++i == i_verts_end) {
					break;
				}
			}

			/* we need face normals because of 'BM_face_split_edgenet'
			 * we could calculate on the fly too (before calling split). */
			float nmat[4][4];
			invert_m4_m4(nmat, omat.data);

			BMFace *efa;
			i = 0;
			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				mul_transposed_mat3_m4_v3(nmat, efa->no);
				normalize_v3(efa->no);
				BM_elem_flag_enable(efa, BM_FACE_TAG);  /* temp tag to test which side split faces are from */
				if (++i == i_faces_end) {
					break;
				}
			}
		}

		/* not needed, but normals for 'dm' will be invalid,
		 * currently this is ok for 'BM_mesh_intersect' */
		// BM_mesh_normals_update(bm);

		BM_mesh_intersect(
		        bm,
		        looptris, tottri,
		        bm_face_isect_pair, NULL,
		        false,
		        separate,
		        dissolve,
		        connect_regions,
		        boolean_mode,
		        threshold);

		MEM_freeN(looptris);
	}

	result = CDDM_from_bmesh(bm, true);

	BM_mesh_free(bm);

	result->dirty = (DMDirtyFlag)((int)result->dirty | DM_DIRTY_NORMALS);

//	TIMEIT_END(boolean_bmesh);

	return result;
}

#undef BM_FACE_TAG

static void eval_op_mesh_boolean(const EvalGlobals *UNUSED(globals), const EvalKernelData *UNUSED(kernel_data), float *stack,
                                 StackIndex offset_mesh_in, StackIndex offset_object, StackIndex offset_operation,
                                 StackIndex offset_separate, StackIndex offset_dissolve,
                                 StackIndex offset_connect_regions, StackIndex offset_threshold,
                                 StackIndex offset_mesh_out)
{
	PointerRNA ptr = stack_load_pointer(stack, offset_object);
	DerivedMesh *dm = stack_load_mesh(stack, offset_mesh_in);
	
	DerivedMesh *result = NULL;
	if (ptr.data && RNA_struct_is_a(&RNA_Object, ptr.type)) {
		Object *ob = (Object *)ptr.data;
		DerivedMesh *dm_other = ob->derivedFinal;
		if (dm_other && dm_other->getNumPolys(dm_other) > 0) {
			int operation = stack_load_int(stack, offset_operation);
			bool separate = stack_load_int(stack, offset_separate);
			bool dissolve = stack_load_int(stack, offset_dissolve);
			bool connect_regions = stack_load_int(stack, offset_connect_regions);
			float threshold = stack_load_float(stack, offset_threshold);
			
			matrix44 omat = matrix44::identity(); // TODO
//			float imat[4][4];
//			float omat[4][4];
//			invert_m4_m4(imat, ob->obmat);
//			mul_m4_m4m4(omat, imat, bmd->object->obmat);
			
			result = do_boolean(dm, dm_other, omat, separate, dissolve, connect_regions, operation, threshold);
		}
	}
	if (!result)
		result = CDDM_new(0, 0, 0, 0, 0);
	
	stack_store_mesh(stack, offset_mesh_out, result);
}

} /* namespace bvm */

#endif /* __BVM_EVAL_MESH_H__ */

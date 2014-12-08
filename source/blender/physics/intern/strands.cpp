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

/** \file blender/physics/intern/strands.c
 *  \ingroup bke
 */

extern "C" {
#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "DNA_customdata_types.h"
#include "DNA_object_types.h"

#include "BKE_bvhutils.h"
#include "BKE_customdata.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_DerivedMesh.h"
#include "BKE_editstrands.h"
#include "BKE_effect.h"
#include "BKE_mesh_sample.h"

#include "bmesh.h"
}

#include "BPH_strands.h"

#include "eigen_utils.h"

/* === constraints === */

static bool editstrands_get_root(BMEditStrands *edit, BMVert *root, float loc[3], float nor[3], float tang[3])
{
	BMesh *bm = edit->bm;
	DerivedMesh *root_dm = edit->root_dm;
	MSurfaceSample root_sample;
	
	BM_elem_meshsample_data_named_get(&bm->vdata, root, CD_MSURFACE_SAMPLE, CD_HAIR_ROOT_LOCATION, &root_sample);
	return BKE_mesh_sample_eval(root_dm, &root_sample, loc, nor, tang);
}

static int strand_count(BMVert *root)
{
	BMVert *v;
	BMIter iter;
	
	int len = 0;
	BM_ITER_STRANDS_ELEM(v, &iter, root, BM_VERTS_OF_STRAND) {
		++len;
	}
	return len;
}

static int strands_get_max_length(BMEditStrands *edit)
{
	BMVert *root;
	BMIter iter;
	int maxlen = 0;
	
	BM_ITER_STRANDS(root, &iter, edit->bm, BM_STRANDS_OF_MESH) {
		int len = strand_count(root);
		if (len > maxlen)
			maxlen = len;
	}
	return maxlen;
}

static void editstrands_apply_root_locations(BMEditStrands *edit)
{
	BMVert *root;
	BMIter iter;
	
	if (!edit->root_dm)
		return;
	
	BM_ITER_STRANDS(root, &iter, edit->bm, BM_STRANDS_OF_MESH) {
		float loc[3], nor[3], tang[3];
		
		if (editstrands_get_root(edit, root, loc, nor, tang))
			copy_v3_v3(root->co, loc);
	}
}

static void editstrands_solve_segment_lengths(BMesh *bm)
{
	BMVert *root, *v, *vprev;
	BMIter iter, iter_strand;
	int k;
	
	/* XXX Simplistic implementation from particles:
	 * adjust segment lengths starting from the root.
	 * This should be replaced by a more advanced method using a least-squares
	 * error metric with length and root location constraints
	 */
	BM_ITER_STRANDS(root, &iter, bm, BM_STRANDS_OF_MESH) {
		BM_ITER_STRANDS_ELEM_INDEX(v, &iter_strand, root, BM_VERTS_OF_STRAND, k) {
			if (k > 0) {
				float base_length = BM_elem_float_data_named_get(&bm->vdata, v, CD_PROP_FLT, CD_HAIR_SEGMENT_LENGTH);
				float dist[3];
				float length;
				
				sub_v3_v3v3(dist, v->co, vprev->co);
				length = len_v3(dist);
				if (length > 0.0f)
					madd_v3_v3v3fl(v->co, vprev->co, dist, base_length / length);
			}
			vprev = v;
		}
	}
}

typedef struct IKTarget {
	BMVert *vertex;
	float weight;
} IKTarget;

static int find_ik_targets(BMVert *root, IKTarget *targets)
{
	BMVert *v;
	BMIter iter;
	int k, index;
	
	index = 0;
	BM_ITER_STRANDS_ELEM_INDEX(v, &iter, root, BM_VERTS_OF_STRAND, k) {
		/* XXX TODO allow multiple targets and do weight calculation here */
		if (BM_strands_vert_is_tip(v)) {
			IKTarget *target = &targets[index];
			target->vertex = v;
			target->weight = 1.0f;
			++index;
		}
	}
	
	return index;
}

static void calc_jacobian_entry(Object *ob, BMEditStrands *edit, IKTarget *target, int index_target, int index_angle,
                                const float point[3], const float axis1[3], const float axis2[3], MatrixX &J)
{
	float (*obmat)[4] = ob->obmat;
	
	float dist[3], jac1[3], jac2[3];
	
	sub_v3_v3v3(dist, target->vertex->co, point);
	
	cross_v3_v3v3(jac1, axis1, dist);
	cross_v3_v3v3(jac2, axis2, dist);
	
	for (int i = 0; i < 3; ++i) {
		J.coeffRef(index_target + i, index_angle + 0) = jac1[i];
		J.coeffRef(index_target + i, index_angle + 1) = jac2[i];
	}
	
#if 1
	{
		float wco[3], wdir[3];
		
		mul_v3_m4v3(wco, obmat, point);
		
		mul_v3_m4v3(wdir, obmat, jac1);
		BKE_sim_debug_data_add_vector(edit->debug_data, wco, wdir, 1,1,0, "strands", index_angle, 1);
		mul_v3_m4v3(wdir, obmat, jac2);
		BKE_sim_debug_data_add_vector(edit->debug_data, wco, wdir, 0,1,1, "strands", index_angle + 1, 2);
	}
#endif
}

static void calc_target_jacobian(Object *ob, BMEditStrands *edit)
{
	BMesh *bm = edit->bm;
	
	BMVert *root;
	BMIter iter;
	
	BM_ITER_STRANDS(root, &iter, bm, BM_STRANDS_OF_MESH) {
		BMVert *v, *vprev;
		BMIter iter_strand;
		int k;
		
		float loc[3], axis[3], dir[3];
		IKTarget targets[1]; /* XXX placeholder, later should be allocated to max. strand length */
		int numjoints, numtargets;
		
		if (!editstrands_get_root(edit, root, loc, dir, axis))
			continue;
		
		numjoints = strand_count(root);
		if (numjoints <= 0)
			continue;
		numtargets = find_ik_targets(root, targets);
		
		MatrixX J(3 * numtargets, 2 * numjoints);
		
		BM_ITER_STRANDS_ELEM_INDEX(v, &iter_strand, root, BM_VERTS_OF_STRAND, k) {
			float dirprev[3];
			
			if (k > 0) {
				float rot[3][3];
				
				copy_v3_v3(dirprev, dir);
				sub_v3_v3v3(dir, v->co, vprev->co);
				normalize_v3(dir);
				
				rotation_between_vecs_to_mat3(rot, dirprev, dir);
				mul_m3_v3(rot, axis);
			}
			
			calc_jacobian_entry(ob, edit, &targets[0], 0, 2*k, v->co, axis, dir, J);
			
#if 0
			{
				float (*obmat)[4] = ob->obmat;
				float wco[3], wdir[3];
				
				mul_v3_m4v3(wco, obmat, v->co);
				
				mul_v3_m4v3(wdir, obmat, axis);
				BKE_sim_debug_data_add_vector(edit->debug_data, wco, wdir, 1,0,0, "strands", BM_elem_index_get(v), 1);
				mul_v3_m4v3(wdir, obmat, dir);
				BKE_sim_debug_data_add_vector(edit->debug_data, wco, wdir, 0,1,0, "strands", BM_elem_index_get(v), 2);
				cross_v3_v3v3(wdir, axis, dir);
				mul_m4_v3(obmat, wdir);
				BKE_sim_debug_data_add_vector(edit->debug_data, wco, wdir, 0,0,1, "strands", BM_elem_index_get(v), 3);
			}
#endif
			
			vprev = v;
		}
		
		pseudo_inverse(J, 1.e-6);
	}
}

void BPH_strands_solve_constraints(Object *ob, BMEditStrands *es)
{
	editstrands_apply_root_locations(es);
	
	calc_target_jacobian(ob, es);
	editstrands_solve_segment_lengths(es->bm);
}

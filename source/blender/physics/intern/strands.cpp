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

#include "iostream"

extern "C" {
#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "DNA_customdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_bvhutils.h"
#include "BKE_collision.h"
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

//#define STRAND_CONSTRAINT_EDGERELAX
//#define STRAND_CONSTRAINT_IK
#define STRAND_CONSTRAINT_LAGRANGEMULT

static int strand_count_vertices(BMVert *root)
{
	BMVert *v;
	BMIter iter;
	
	int len = 0;
	BM_ITER_STRANDS_ELEM(v, &iter, root, BM_VERTS_OF_STRAND) {
		++len;
	}
	return len;
}

static int UNUSED_FUNCTION(strands_get_max_length)(BMEditStrands *edit)
{
	BMesh *bm = edit->base.bm;
	BMVert *root;
	BMIter iter;
	int maxlen = 0;
	
	BM_ITER_STRANDS(root, &iter, bm, BM_STRANDS_OF_MESH) {
		int len = strand_count_vertices(root);
		if (len > maxlen)
			maxlen = len;
	}
	return maxlen;
}

static void strands_apply_root_locations(BMEditStrands *edit)
{
	BMesh *bm = edit->base.bm;
	BMVert *root;
	BMIter iter;
	
	if (!edit->root_dm)
		return;
	
	BM_ITER_STRANDS(root, &iter, bm, BM_STRANDS_OF_MESH) {
		float loc[3], nor[3], tang[3];
		
		if (BKE_editstrands_get_vectors(edit, root, loc, nor, tang))
			copy_v3_v3(root->co, loc);
	}
}

#ifdef STRAND_CONSTRAINT_EDGERELAX
static void strands_adjust_segment_lengths(BMesh *bm)
{
	BMVert *root, *v, *vprev;
	BMIter iter, iter_strand;
	int k;
	
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

/* try to find a nice solution to keep distances between neighboring keys */
/* XXX Stub implementation ported from particles:
 * Successively relax each segment starting from the root,
 * repeat this for every vertex (O(n^2) !!)
 * This should be replaced by a more advanced method using a least-squares
 * error metric with length and root location constraints (IK solver)
 */
static void strands_solve_edge_relaxation(BMEditStrands *edit)
{
	BMesh *bm = edit->base.bm;
	const int Nmax = BM_strand_verts_count_max(bm);
	/* cache for vertex positions and segment lengths, for easier indexing */
	float **co = (float **)MEM_mallocN(sizeof(float*) * Nmax, "strand positions");
	float *target_length = (float *)MEM_mallocN(sizeof(float) * Nmax, "strand segment lengths");
	
	BMVert *root;
	BMIter iter;
	BM_ITER_STRANDS(root, &iter, bm, BM_STRANDS_OF_MESH) {
		const int S = 1; /* TODO particles use PE_LOCK_FIRST option */
		const int N = BM_strand_verts_count(root);
		const float divN = 1.0f / (float)N;
		
		/* setup positions cache */
		{
			BMVert *v;
			BMIter viter;
			int k;
			BM_ITER_STRANDS_ELEM_INDEX(v, &viter, root, BM_VERTS_OF_STRAND, k) {
				co[k] = v->co;
				target_length[k] = BM_elem_float_data_named_get(&bm->vdata, v, CD_PROP_FLT, CD_HAIR_SEGMENT_LENGTH);
			}
		}
		
		for (int iter = 1; iter < N; iter++) {
			float correct_first[3] = {0.0f, 0.0f, 0.0f};
			float correct_second[3] = {0.0f, 0.0f, 0.0f};
			
			for (int k = S; k < N; k++) {
				if (k > 0) {
					/* calculate correction for the first vertex */
					float dir[3];
					sub_v3_v3v3(dir, co[k-1], co[k]);
					float length = normalize_v3(dir);
					
					mul_v3_v3fl(correct_first, dir, divN * (length - target_length[k]));
				}
				
				if (k < N-1) {
					/* calculate correction for the second vertex */
					float dir[3];
					sub_v3_v3v3(dir, co[k+1], co[k]);
					float length_next = normalize_v3(dir);
					
					mul_v3_v3fl(correct_second, dir, divN * (length_next - target_length[k+1]));
				}
				
				/* apply both corrections (try to satisfy both sides equally) */
				add_v3_v3(co[k], correct_first);
				add_v3_v3(co[k], correct_second);
			}
		}
	}
	
	if (co)
		MEM_freeN(co);
	if (target_length)
		MEM_freeN(target_length);
	
	strands_adjust_segment_lengths(bm);
}
#endif

#ifdef STRAND_CONSTRAINT_IK
typedef struct IKTarget {
	BMVert *vertex;
	float weight;
} IKTarget;

static int strand_find_ik_targets(BMVert *root, IKTarget *targets)
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

static void calc_jacobian_entry(Object *ob, BMEditStrands *UNUSED(edit), IKTarget *target, int index_target, int index_angle,
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
		BKE_sim_debug_data_add_vector(wco, wdir, 1,1,0, "strands", index_angle, 1);
		mul_v3_m4v3(wdir, obmat, jac2);
		BKE_sim_debug_data_add_vector(wco, wdir, 0,1,1, "strands", index_angle + 1, 2);
	}
#endif
}

static MatrixX strand_calc_target_jacobian(Object *ob, BMEditStrands *edit, BMVert *root, int numjoints, IKTarget *targets, int numtargets)
{
	BMVert *v, *vprev;
	BMIter iter_strand;
	int k;
	
	float loc[3], axis[3], dir[3];
	
	MatrixX J(3 * numtargets, 2 * numjoints);
	if (!BKE_editstrands_get_vectors(edit, root, loc, dir, axis)) {
		return J;
	}
	
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
	
	return J;
}

static VectorX strand_angles_to_loc(Object *UNUSED(ob), BMEditStrands *edit, BMVert *root, int numjoints, const VectorX &angles)
{
	BMesh *bm = edit->base.bm;
	BMVert *v, *vprev;
	BMIter iter_strand;
	int k;
	
	float loc[3], axis[3], dir[3];
	float mat_theta[3][3], mat_phi[3][3];
	
	if (!BKE_editstrands_get_vectors(edit, root, loc, dir, axis))
		return VectorX();
	
	VectorX result(3*numjoints);
	
	BM_ITER_STRANDS_ELEM_INDEX(v, &iter_strand, root, BM_VERTS_OF_STRAND, k) {
		float dirprev[3];
		
		if (k > 0) {
			const float base_length = BM_elem_float_data_named_get(&bm->vdata, v, CD_PROP_FLT, CD_HAIR_SEGMENT_LENGTH);
			float rot[3][3];
			
			copy_v3_v3(dirprev, dir);
			sub_v3_v3v3(dir, v->co, vprev->co);
			normalize_v3(dir);
			
			rotation_between_vecs_to_mat3(rot, dirprev, dir);
			mul_m3_v3(rot, axis);
			
			/* apply rotations from previous joint on the vertex */
			float vec[3];
			mul_v3_v3fl(vec, dir, base_length);
			
			mul_m3_v3(mat_theta, vec);
			mul_m3_v3(mat_phi, vec);
			add_v3_v3v3(&result.coeffRef(3*k), &result.coeff(3*(k-1)), vec);
		}
		else {
			copy_v3_v3(&result.coeffRef(3*k), v->co);
		}
		
		float theta = angles[2*k + 0];
		float phi = angles[2*k + 1];
		axis_angle_normalized_to_mat3(mat_theta, axis, theta);
		axis_angle_normalized_to_mat3(mat_phi, dir, phi);
		
		vprev = v;
	}
	
	return result;
}

static void UNUSED_FUNCTION(strand_apply_ik_result)(Object *UNUSED(ob), BMEditStrands *UNUSED(edit), BMVert *root, const VectorX &solution)
{
	BMVert *v;
	BMIter iter_strand;
	int k;
	
	BM_ITER_STRANDS_ELEM_INDEX(v, &iter_strand, root, BM_VERTS_OF_STRAND, k) {
		copy_v3_v3(v->co, &solution.coeff(3*k));
	}
}

static void strands_solve_inverse_kinematics(Object *ob, BMEditStrands *edit, float (*orig)[3])
{
	BMesh *bm = edit->base.bm;
	
	BMVert *root;
	BMIter iter;
	
	BM_ITER_STRANDS(root, &iter, bm, BM_STRANDS_OF_MESH) {
		int numjoints = strand_count_vertices(root);
		if (numjoints <= 0)
			continue;
		
		IKTarget targets[1]; /* XXX placeholder, later should be allocated to max. strand length */
		int numtargets = strand_find_ik_targets(root, targets);
		
		MatrixX J = strand_calc_target_jacobian(ob, edit, root, numjoints, targets, numtargets);
		MatrixX Jinv = pseudo_inverse(J, 1.e-6);
		
		VectorX x(3 * numtargets);
		for (int i = 0; i < numtargets; ++i) {
			sub_v3_v3v3(&x.coeffRef(3*i), targets[i].vertex->co, orig[i]);
			/* TODO calculate deviation of vertices from their origin (whatever that is) */
//			x[3*i + 0] = 0.0f;
//			x[3*i + 1] = 0.0f;
//			x[3*i + 2] = 0.0f;
		}
		VectorX angles = Jinv * x;
		VectorX solution = strand_angles_to_loc(ob, edit, root, numjoints, angles);
		
//		strand_apply_ik_result(ob, edit, root, solution);

#if 1
		{
			BMVert *v;
			BMIter iter_strand;
			int k;
			float wco[3];
			
			BM_ITER_STRANDS_ELEM_INDEX(v, &iter_strand, root, BM_VERTS_OF_STRAND, k) {
				mul_v3_m4v3(wco, ob->obmat, &solution.coeff(3*k));
				BKE_sim_debug_data_add_circle(wco, 0.05f, 1,0,1, "strands", k, BM_elem_index_get(root), 2344);
			}
		}
#endif
	}
}
#endif

#ifdef STRAND_CONSTRAINT_LAGRANGEMULT

//#define DO_DEBUG

/* Solve edge constraints and collisions for a single strand based on
 * "Linear-Time Dynamics using Lagrange Multipliers" (Baraff, 1996)
 * 
 * M, M_inv: mass matrix and its inverse
 * L: target lengths for each segment (L[0] == 0)
 * x0: original starting locations
 * x1: unconstrained locations
 * x: constrained location result
 */

inline static VectorX get_constraints(int numverts, const VectorX &x, const Eigen::Vector3f &root)
{
	using Eigen::Vector3f;
	using Eigen::Matrix3f;
	
	BLI_assert(x.rows() == numverts * 3);
	
	int numcons_root = 3; /* root velocity constraint */
	int numcons_edges = numverts - 1; /* distance constraints */
	int numcons = numcons_edges + numcons_root;
	
	VectorX S = VectorX::Zero(numcons);
	S.block<3,1>(0, 0) = root;
	for (int k = 0; k < numcons_edges; ++k) {
		int ia = k*3;
		int ib = (k+1)*3;
		int kcon = numcons_root + k;
		
		Vector3f xa = x.block<3,1>(ia,0);
		Vector3f xb = x.block<3,1>(ib,0);
		Vector3f dcon = (xb - xa);
		
		S.coeffRef(kcon) = dcon.dot(dcon);
	}
	
	return S;
}

inline static MatrixX get_constraints_dx(int numverts, const VectorX &x)
{
	using Eigen::Vector3f;
	using Eigen::Matrix3f;
	
	BLI_assert(x.rows() == numverts * 3);
	
	int numcons_root = 3; /* root velocity constraint */
	int numcons_edges = numverts - 1; /* distance constraints */
	int numcons = numcons_edges + numcons_root;
	
	MatrixX J = MatrixX::Zero(numcons, 3 * numverts);
	for (int k = 0; k < numcons_edges; ++k) {
		int ia = k*3;
		int ib = (k+1)*3;
		int kcon = numcons_root + k;
		
		Vector3f xa = x.block<3,1>(ia,0);
		Vector3f xb = x.block<3,1>(ib,0);
		Vector3f dcon = 2.0f * (xb - xa);
		
		J.block<1,3>(kcon, ia) = -dcon;
		J.block<1,3>(kcon, ib) = dcon;
	}
	
	return J;
}

inline static MatrixX get_constraints_dlambda(int numverts, const VectorX &x, const MatrixX &dSdx, const MatrixX &M_inv)
{
	using Eigen::Vector3f;
	using Eigen::Matrix3f;
	
	BLI_assert(x.rows() == numverts * 3);
	
	int numcons_root = 3; /* root velocity constraint */
	int numcons_edges = numverts - 1; /* distance constraints */
	int numcons = numcons_edges + numcons_root;
	
	MatrixX U = M_inv * dSdx.transpose();
	
	MatrixX J = MatrixX::Zero(numcons, numcons);
	for (int k = 0; k < numcons_edges; ++k) {
		int ia = k*3;
		int ib = (k+1)*3;
		int kcon = numcons_root + k;
		
		Vector3f xa = x.block<3,1>(ia,0);
		Vector3f xb = x.block<3,1>(ib,0);
		J.coeffRef(kcon) = 2.0f * (xb - xa).dot(U.block<3,1>(ia, kcon) - U.block<3,1>(ib, kcon));
	}
	
	return J;
}

static VectorX strand_solve_step(int numverts, const Eigen::Vector3f &root_v,
                                 const MatrixX &M, const MatrixX &M_inv, const VectorX &L,
                                 const VectorX &x0, const VectorX &x1,
                                 int debug_root, int debug_step)
{
	using Eigen::Vector3f;
	using Eigen::Matrix3f;
	
#if 0
	int numcons_root = 3; /* root velocity constraint */
	int numcons_edges = numverts - 1; /* distance constraints */
	int numcons = numcons_edges + numcons_root;
	
	MatrixX dS0dx = get_constraints_dx(numverts, x0);
	
	float res;
	VectorX x = x1;
	do {
//		VectorX lambda = VectorX::Zero(numcons);
		
//		VectorX dSdlambda(numcons);
//		for (int k = 0; k < numcons_edges; ++k) {
//			int ia = k*3;
//			int ib = (k+1)*3;
//			dSdlambda.block<3,1>(k, 0) = x.block<3,1>(ib, 0) - x.block<3,1>(ia, 0);
//		}
		
//		MatrixX J = MatrixX::Zero(numcons, 3 * numverts);
//		MatrixX J = 2.0f * dSdlambda * dS0dx.transpose() * M_inv;
		
		VectorX S = get_constraints(numverts, x, root_v);
		MatrixX dSdx = get_constraints_dx(numverts, x);
		MatrixX dSdlambda = get_constraints_dlambda(numverts, x, dSdx, M_inv);
		
		/* Lagrange multipliers are the solution to dSdlambda * lambda = S */
		VectorX lambda = dSdlambda.ldlt().solve(S);
		BLI_assert((dSdlambda * lambda).isApprox(S, 0.001f));
		
		x += dSdx.transpose() * lambda;
		
		res = 0.0f; /* TODO */
	} while (res > 0.05f);
	
	return x;
#endif
	
#if 1
	/* Constraint matrix */
	int numcons_roots = 3; /* root velocity constraint */
	int numcons_edges = numverts - 1; /* distance constraints */
	int numcons = numcons_edges + numcons_roots;
	MatrixX J = MatrixX::Zero(numcons, 3 * numverts);
	/* Constraint velocities */
	VectorX c = VectorX::Zero(numcons);
	
	VectorX x = x0;
	float res = 4;
	do {
		/* root velocity constraint */
		J.block<3,3>(0, 0) = Matrix3f::Identity();
		c.block<3,1>(0, 0) = -root_v;
		
		/* distance constraints */
		for (int i = 0; i < numcons_edges; ++i) {
			int ka = i * 3;
			int kb = (i+1) * 3;
			Vector3f xa(x.block<3,1>(ka, 0));
			Vector3f xb(x.block<3,1>(kb, 0));
			Vector3f j = (xb - xa);
			//		float length = j.norm();
			//		float target_length = L[i+1];
			//		if (j.norm() > 0.0f)
			//			j.normalize();
			
			int con = numcons_roots + i;
			J.block<1,3>(con, ka) = -j.transpose();
			J.block<1,3>(con, kb) =  j.transpose();
			
			/* counteract drift with velocity along the edge */
			//		float drift = length - target_length;
			//		c.coeffRef(con) = drift;
			
#ifdef DO_DEBUG
#if 0
			{
				Vector3f vj = -j;
				Vector3f xd = xa + Vector3f(0, 0, 0.03f);
				BKE_sim_debug_data_add_vector(xd.data(), vj.data(), 0.3,1,0, "hair solve", 3274, i, debug_root, debug_step);
				vj = -vj;
				xd = xb + Vector3f(0, 0, 0.03f);
				BKE_sim_debug_data_add_vector(xd.data(), vj.data(), 0,1,0.3, "hair solve", 32799, i, debug_root, debug_step);
			}
#endif
#endif
		}
		/* A = J * M^-1 * J^T */
		MatrixX A = J * M_inv * J.transpose();
	
		/* force vector */
		VectorX F = M * (x1 - x);
#if 0
		/* length spring: correct for drift of length constraints */
		for (int i = 0; i < numverts - 1; ++i) {
			int ka = i * 3;
			int kb = (i+1) * 3;
			Vector3f xa(x1.block<3,1>(ka, 0));
			Vector3f xb(x1.block<3,1>(kb, 0));
			Vector3f d = (xb - xa);
			float length = d.norm();
			Vector3f f;
			if (length > 0.0f)
				f = d * (1.0f - L[i+1] / length);
			else
				f = Vector3f(0.0f, 0.0f, 0.0f);
			F.block<3,1>(ka, 0) += f;
			F.block<3,1>(kb, 0) += -f;
		}
#endif
#if 0
		/* bending force: smoothes the hair */
		float stiffness = 0.0;
		for (int i = 1; i < numverts - 1; ++i) {
			int ka = (i-1) * 3;
			int kb = i * 3;
			int kc = (i+1) * 3;
			Vector3f xa(x1.block<3,1>(ka, 0));
			Vector3f xb(x1.block<3,1>(kb, 0));
			Vector3f xc(x1.block<3,1>(kc, 0));
			Vector3f target = xb + (xb - xa).normalized() * (xc - xb).norm();
			Vector3f f = stiffness * (target - xc);
			F.block<3,1>(kc, 0) += f;
			F.block<3,1>(kb, 0) += -f;
		}
#endif
		
		/* b = -(J * M^-1 * F + c) */
		VectorX b = -(J * M_inv * F + c);
		
		/* Lagrange multipliers are the solution to A * lambda = b */
		VectorX lambda = A.ldlt().solve(b);
		BLI_assert((A * lambda).isApprox(b, 0.001f));
		
		/* calculate velocity correction by constraint forces */
		VectorX v = M_inv * (J.transpose() * lambda + F);
		
		/* corrected position update */
		x = x + v;
		
#ifdef DO_DEBUG
		{
			std::cout << "J = " << std::endl << J << std::endl;
			std::cout << "A = " << std::endl << A << std::endl;
			std::cout << "v = " << std::endl << v << std::endl;
			std::cout << "b = " << std::endl << b << std::endl;
			std::cout << "lambda = " << std::endl << lambda << std::endl;
			
			for (int k = 0; k < numverts-1; ++k) {
				Vector3f xa = x0.block<3,1>(3*k, 0);
				Vector3f xb = x0.block<3,1>(3*(k+1), 0);
				BKE_sim_debug_data_add_line(xa.data(), xb.data(), 0.4,0.4,0.4, "hair solve", 3800, k, debug_root, debug_step);
				xa = x1.block<3,1>(3*k, 0);
				xb = x1.block<3,1>(3*(k+1), 0);
				BKE_sim_debug_data_add_line(xa.data(), xb.data(), 0.7,0.7,0.7, "hair solve", 3801, k, debug_root, debug_step);
			}
			
			VectorX dv = v - (x1 - x0);
			for (int k = 0; k < numverts; ++k) {
				Vector3f xd = x1.block<3,1>(3*k, 0) + Vector3f(0, 0, 0.01f);
				BKE_sim_debug_data_add_vector(xd.data(), &dv.coeff(3*k), 0.4,0.4,1, "hair solve", 3833, k, debug_root, debug_step);
				xd = x0.block<3,1>(3*k, 0) + Vector3f(0, 0, 0.02f);
				BKE_sim_debug_data_add_vector(xd.data(), &v.coeff(3*k), 0,0,1, "hair solve", 3811, k, debug_root, debug_step);
				
				xd = x1.block<3,1>(3*k, 0) + Vector3f(0, 0, 0.04f);
				BKE_sim_debug_data_add_vector(xd.data(), &F.coeff(3*k), 1,0,0, "hair solve", 32789, k, debug_root, debug_step);
			}
			
			VectorX Fj = J.transpose() * lambda;
			for (int i = 0; i < numcons_edges; ++i) {
				int ka = i * 3;
				Vector3f xa(x1.block<3,1>(ka, 0));
				Vector3f j = Fj.block<3,1>(ka, 0);
				
				Vector3f xd = xa + Vector3f(0, 0, 0.03f);
				BKE_sim_debug_data_add_vector(xd.data(), j.data(), 0.3,1,0, "hair solve", 3274, i, debug_root, debug_step);
			}
		}
#endif
		
		res = res - 1;
	} while (res > 0.05f);
	
	return x;
#endif
}

static void strand_solve(BMesh *bm, BMVert *root, float (*orig)[3], int numverts,
                         const Eigen::Vector3f &root_v, int substeps)
{
	using Eigen::Vector3f;
	using Eigen::Matrix3f;
	
	/* compute unconstrained velocities by 1st order differencing */
	VectorX x0(3 * numverts);
	VectorX xN(3 * numverts);
	VectorX L(numverts);
	/* "Mass" matrix can be understood as resistance to editing changes.
	 * XXX For now just using identity, in future more interesting things could be done here.
	 */
	MatrixX M = MatrixX::Identity(3 * numverts, 3 * numverts);
	{
		BMIter iter;
		BMVert *vert;
		int k;
		BM_ITER_STRANDS_ELEM_INDEX(vert, &iter, root, BM_VERTS_OF_STRAND, k) {
			copy_v3_v3(&x0.coeffRef(3*k), orig[k]);
			copy_v3_v3(&xN.coeffRef(3*k), vert->co);
			L.coeffRef(k) = BM_elem_float_data_named_get(&bm->vdata, vert, CD_PROP_FLT, CD_HAIR_SEGMENT_LENGTH);
#ifdef DO_DEBUG
			BKE_sim_debug_data_add_line(orig[k], vert->co, 0,0,1, "hair solve", 3874, BLI_ghashutil_ptrhash(root), k);
#endif
		}
	}
	/* XXX here just skip a pointless solve step */
	MatrixX M_inv = MatrixX::Identity(3 * numverts, 3 * numverts);
	
	BKE_sim_debug_data_clear_category("hair solve");
	for (int s = 0; s < substeps; ++s) {
		VectorX x1 = x0 + (xN - x0) / (float)(substeps - s);
		{
			BMIter iter;
			BMVert *vert;
			int k;
			BM_ITER_STRANDS_ELEM_INDEX(vert, &iter, root, BM_VERTS_OF_STRAND, k) {
//				BKE_sim_debug_data_add_line(&x0.coeff(k*3), &x1.coeff(k*3), 1, s%2, 0, "hair solve", 232, BLI_ghashutil_ptrhash(root), k, s);
			}
		}
		x0 = strand_solve_step(numverts, root_v, M, M_inv, L, x0, x1, BLI_ghashutil_ptrhash(root), s);
	}
	
	{
		BMIter iter;
		BMVert *vert;
		int k;
		BM_ITER_STRANDS_ELEM_INDEX(vert, &iter, root, BM_VERTS_OF_STRAND, k) {
			copy_v3_v3(vert->co, &x0.coeff(3*k));
		}
	}
}

static void strands_solve_lagrange_multipliers(Object *ob, BMEditStrands *edit, float (*orig)[3], int substeps)
{
	using Eigen::Vector3f;
	
	if (substeps <= 0)
		return;
	
	BMesh *bm = edit->base.bm;
	
	BMIter iter;
	BMVert *root;
	BM_ITER_STRANDS(root, &iter, bm, BM_STRANDS_OF_MESH) {
		int numverts = BM_strand_verts_count(root);
		/* TODO if the root gets moved this would be non-zero */
		Vector3f root_v = Vector3f(0.0f, 0.0f, 0.0f);
		
		strand_solve(bm, root, orig, numverts, root_v, substeps);
		
		orig += numverts;
	}
}
#endif

void BPH_strands_solve_constraints(Scene *scene, Object *ob, BMEditStrands *edit,
                                   float (*orig)[3], int substeps)
{
	HairEditSettings *settings = &scene->toolsettings->hair_edit;
	BLI_assert(orig);
	
	/* Deflection */
	if (settings->flag & HAIR_EDIT_USE_DEFLECT) {
		CollisionContactCache *contacts = BKE_collision_cache_create();
		
		BKE_editstrands_get_collision_contacts(scene, ob, edit, contacts);
		if (settings->flag & HAIR_EDIT_SHOW_DEBUG) {
			if (settings->flag & HAIR_EDIT_SHOW_DEBUG_CONTACTS) {
				BKE_sim_debug_data_clear_category("hair collision");
				
				CollisionContactIterator iter;
				CollisionContactPoint *pt;
				BKE_COLLISION_ITER_CONTACTS(pt, &iter, contacts) {
					BKE_sim_debug_data_add_line(pt->point_world_a, pt->point_world_b, 0.95, 0.9, 0.1, "hair collision",
					                            pt->index_a, pt->index_b, pt->part_id_a, pt->part_id_b);
				}
			}
		}
		
		BKE_collision_cache_free(contacts);
	}
	
#ifdef STRAND_CONSTRAINT_EDGERELAX
	strands_apply_root_locations(edit);
	strands_solve_edge_relaxation(edit);
#endif
#ifdef STRAND_CONSTRAINT_IK
	strands_apply_root_locations(edit);
	strands_solve_inverse_kinematics(ob, edit, orig);
#endif
#ifdef STRAND_CONSTRAINT_LAGRANGEMULT
	strands_solve_lagrange_multipliers(ob, edit, orig, substeps);
#endif
}

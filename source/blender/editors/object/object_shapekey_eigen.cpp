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
 * Contributor(s): Blender Foundation, Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/object/object_shapekey_eigen.cpp
 *  \ingroup edobj
 */

#ifdef __GNUC__
# pragma GCC diagnostic push
/* XXX suppress verbose warnings in eigen */
# pragma GCC diagnostic ignored "-Wlogical-op"
#endif

#include <Eigen/SparseQR>
#include <Eigen/src/Core/util/DisableStupidWarnings.h>

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

extern "C" {
#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"

#include "DNA_key_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_mesh_sample.h"

#include "object_intern.h"
}

typedef float Scalar;

typedef Eigen::Triplet<Scalar> Triplet;
typedef std::vector<Triplet> TripletList;

typedef Eigen::VectorXf lVector;

typedef Eigen::SparseMatrix<Scalar> lMatrix;

//typedef Eigen::ConjugateGradient<lMatrix, Eigen::Lower, Eigen::DiagonalPreconditioner<Scalar> > ConjugateGradient;
typedef Eigen::SparseQR<lMatrix, Eigen::COLAMDOrdering<int> > QRSolver;

using Eigen::ComputationInfo;

BLI_INLINE void implicit_print_matrix_elem(float v)
{
	printf("%-8.3f", v);
}

BLI_INLINE void print_lvector(const lVector &v)
{
	for (int i = 0; i < v.rows(); ++i) {
		if (i > 0 && i % 3 == 0)
			printf("\n");
		
		printf("%f,\n", v[i]);
	}
}

BLI_INLINE void print_lmatrix(const lMatrix &m)
{
	for (int j = 0; j < m.rows(); ++j) {
		if (j > 0 && j % 3 == 0)
			printf("\n");
		
		for (int i = 0; i < m.cols(); ++i) {
			if (i > 0 && i % 3 == 0)
				printf(" ");
			
			implicit_print_matrix_elem(m.coeff(j, i));
		}
		printf("\n");
	}
}

static void make_least_square_input(Key *key, MSurfaceSample *samples, float (*goals)[3], int totgoals, lMatrix &A, lVector &b)
{
	float refloc[3], loc[3];
	int i, j, k;
	TripletList trips;
	
	A = lMatrix(3 * totgoals, key->totkey - 1);
	b = lVector(3 * totgoals);
	
	/* goals */
	for (i = 0; i < totgoals; ++i) {
		MSurfaceSample *sample = &samples[i];
		const float *goal = goals[i];
		
		BKE_mesh_sample_shapekey(key, key->refkey, sample, refloc);
		
		/* shape keys (except basis key) */
		for (j = 0; j < key->totkey - 1; ++j) {
			/* +1 because first key block is the refkey */
			KeyBlock *kb = (KeyBlock *)BLI_findlink(&key->block, j + 1);
			
			BKE_mesh_sample_shapekey(key, kb, sample, loc);
			
			/* vector components */
			for (k = 0; k < 3; ++k) {
				int row = 3*i + k;
				int col = j;
				float val = loc[k] - refloc[k];
				
				trips.push_back(Triplet(row, col, val));
				
				b[row] = goal[k] - refloc[k];
			}
		}
	}
	
	A.setFromTriplets(trips.begin(), trips.end());
}

float *shape_key_goal_weights_solve(Key *key, MSurfaceSample *samples, float (*goals)[3], int totgoals)
{
	if (!key->refkey || !key->block.first)
		return NULL;
	
	lMatrix A;
	lVector b;
	
	make_least_square_input(key, samples, goals, totgoals, A, b);
	
	QRSolver solver;
	solver.compute(A);
	if (solver.info() != Eigen::Success)
		return NULL;
	
	lVector x = solver.solve(b);
	if (solver.info() != Eigen::Success)
		return NULL;
	
	/* note: weights has one more value than x at the beginning for the refkey = 1 - sum(x) */
	float *weights = (float *)MEM_mallocN(key->totkey * sizeof(float), "shape key goal weights");
	float totweight = 0.0f;
	for (int i = 0; i < key->totkey - 1; ++i) {
		weights[i + 1] = x[i];
		totweight += x[i];
	}
	weights[0] = 1.0f - totweight;
	
	return weights;
}

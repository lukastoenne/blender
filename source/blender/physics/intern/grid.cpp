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

/** \file grid.cpp
 *  \ingroup bph
 */

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_kdopbvh.h"
#include "BLI_math.h"

#include "DNA_object_types.h"

#include "BKE_bvhutils.h"
#include "BKE_cdderivedmesh.h"
}

#include "grid.h"

namespace BPH {

Grid::Grid() :
    cellsize(0.0f),
    inv_cellsize(0.0f),
    num_cells(0)
{
	zero_v3(offset);
	zero_v3_int(res);
}

Grid::~Grid()
{
}

void Grid::resize(float _cellsize, const float _offset[3], const int _res[3])
{
	BLI_assert(_cellsize >= 0.0f);
	cellsize = _cellsize;
	inv_cellsize = 1.0f / _cellsize;
	copy_v3_v3(offset, _offset);
	copy_v3_v3_int(res, _res);
	num_cells = _res[0] * _res[1] * _res[2];
}

void Grid::init()
{
}

void Grid::clear()
{
}

int Grid::set_inner_cells(GridHash<bool> &bounds, GridHash<float3> &normal, Object *ob) const
{
	DerivedMesh *ob_dm = ob->derivedFinal;
	if (!ob_dm)
		return 0;
	
	DerivedMesh *dm = NULL;
	MVert *mvert;
//	MFace *mface;
	BVHTreeFromMesh treeData = {NULL};
	int numverts, i;
	float gridmat[4][4], mat[4][4];
	
	float surface_distance = 0.6;
	int tot_inner = 0;
	
	/* grid->world space conversion */
	zero_m4(gridmat);
	gridmat[0][0] = cellsize;
	gridmat[1][1] = cellsize;
	gridmat[2][2] = cellsize;
	gridmat[3][3] = 1.0f;
	copy_v3_v3(gridmat[3], offset);
	/* object->grid space conversion */
	invert_m4_m4(mat, gridmat);
	mul_m4_m4m4(mat, mat, ob->obmat);
	
	/* local copy of dm for storing grid space mesh */
	dm = CDDM_copy(ob_dm);
	CDDM_calc_normals(dm);
	mvert = dm->getVertArray(dm);
//	mface = dm->getTessFaceArray(dm);
	numverts = dm->getNumVerts(dm);
	
	/* transform vertices to grid space for fast lookups */
	for (i = 0; i < numverts; i++) {
		float n[3];
		
		/* vert pos */
		mul_m4_v3(mat, mvert[i].co);
		
		/* vert normal */
		normal_short_to_float_v3(n, mvert[i].no);
		mul_mat3_m4_v3(mat, n);
		normalize_v3(n);
		normal_float_to_short_v3(mvert[i].no, n);
	}
	
	if (bvhtree_from_mesh_faces(&treeData, dm, 0.0f, 4, 6)) {
		int xmax = res[0];
		int ymax = res[1];
		int zmax = res[2];
//#pragma omp parallel for schedule(static)
		for (int z = 0; z < zmax; z++) {
			for (int x = 0; x < xmax; x++) {
				for (int y = 0; y < ymax; y++) {
					float ray_start[3] = {(float)x + 0.5f, (float)y + 0.5f, (float)z + 0.5f};
					BVHTreeNearest nearest = {0};
					nearest.index = -1;
					nearest.dist_sq = surface_distance * surface_distance; /* find_nearest uses squared distance */
					
					/* find the nearest point on the mesh */
					if (BLI_bvhtree_find_nearest(treeData.tree, ray_start, &nearest, treeData.nearest_callback, &treeData) != -1) {
#if 0
						float weights[4];
						int v1, v2, v3, f_index = nearest.index;
						
						/* calculate barycentric weights for nearest point */
						v1 = mface[f_index].v1;
						v2 = (nearest.flags & BVH_ONQUAD) ? mface[f_index].v3 : mface[f_index].v2;
						v3 = (nearest.flags & BVH_ONQUAD) ? mface[f_index].v4 : mface[f_index].v3;
						interp_weights_face_v3(weights, mvert[v1].co, mvert[v2].co, mvert[v3].co, NULL, nearest.co);
#endif
						
						/* tag obstacle cells */
						*bounds.get(x, y, z) = true;
						copy_v3_v3(*normal.get(x, y, z), nearest.no);
						++tot_inner;
					}
				}
			}
		}
	}
	
	/* free bvh tree */
	free_bvhtree_from_mesh(&treeData);
	
	/* free world space dm copy */
	dm->release(dm);
	
	return tot_inner;
}

void Grid::calc_divergence(GridHash<float> &divergence, const GridHash<bool> &source, const GridHash<float3> &source_normal) const
{
	const float flowfac = cellsize;
	
	divergence.clear();
	
	int xmax = res[0];
	int ymax = res[1];
	int zmax = res[2];
	for (int z = 0; z < zmax; z++) {
		for (int x = 0; x < xmax; x++) {
			for (int y = 0; y < ymax; y++) {
				const bool *sl = source.get(x-1, y, z);
				const bool *sr = source.get(x+1, y, z);
				const bool *sb = source.get(x, y-1, z);
				const bool *st = source.get(x, y+1, z);
				const bool *sd = source.get(x, y, z-1);
				const bool *su = source.get(x, y, z+1);
				const bool is_margin = !(sl && sr && sb && st && sd && su);
				if (is_margin)
					continue;
				
				const float3 n = *source_normal.get(x, y, z);
				const float3 *nl = sl && *sl ? source_normal.get(x-1, y, z) : NULL;
				const float3 *nr = sr && *sr ? source_normal.get(x+1, y, z) : NULL;
				const float3 *nb = sb && *sb ? source_normal.get(x, y-1, z) : NULL;
				const float3 *nt = st && *st ? source_normal.get(x, y+1, z) : NULL;
				const float3 *nd = sd && *sd ? source_normal.get(x, y, z-1) : NULL;
				const float3 *nu = su && *su ? source_normal.get(x, y, z+1) : NULL;
				
				float dx = 0.0f, dy = 0.0f, dz = 0.0f;
				if (nl)
					dx += n[0] - (*nl)[0];
				if (nr)
					dx += (*nr)[0] - n[0];
				if (nb)
					dy += n[1] - (*nb)[1];
				if (nt)
					dy += (*nt)[1] - n[1];
				if (nd)
					dz += n[2] - (*nd)[2];
				if (nu)
					dz += (*nu)[2] - n[2];
				
				divergence.add(x, y, z) -= 0.5f * flowfac * (dx + dy + dz);
			}
		}
	}
}

/* Calculate velocity = grad(p) */
void Grid::calc_gradient(GridHash<float3> &velocity, const GridHash<float> &pressure) const
{
	const float inv_flowfac = 1.0f / cellsize;
	
	velocity.clear();
	
	for (int z = 0; z < res[2]; ++z) {
		for (int y = 0; y < res[1]; ++y) {
			for (int x = 0; x < res[0]; ++x) {
				bool is_margin = !(x > 0 && x < res[0]-1 && y > 0 && y < res[1]-1 && z > 0 && z < res[2]-1);
				if (is_margin)
					continue;
				
				/*const float *p  = pressure.get(x, y, z);*/
				const float *pl = pressure.get(x-1, y, z);
				const float *pr = pressure.get(x+1, y, z);
				const float *pb = pressure.get(x, y-1, z);
				const float *pt = pressure.get(x, y+1, z);
				const float *pd = pressure.get(x, y, z-1);
				const float *pu = pressure.get(x, y, z+1);
				
				/* finite difference estimate of pressure gradient */
				float dvel[3];
				dvel[0] = *pr - *pl;
				dvel[1] = *pt - *pb;
				dvel[2] = *pu - *pd;
				mul_v3_fl(dvel, -0.5f * inv_flowfac);
				
				velocity.add(x, y, z) = float3(dvel);
			}
		}
	}
}

void Grid::normalize(GridHash<float3> &velocity) const
{
	for (int z = 0; z < res[2]; ++z) {
		for (int y = 0; y < res[1]; ++y) {
			for (int x = 0; x < res[0]; ++x) {
				float3 *v = velocity.get(x, y, z);
				if (v)
					normalize_v3(*v);
			}
		}
	}
}

/* Main Poisson equation system:
 * This is derived from the discretezation of the Poisson equation
 *   div(grad(p)) = div(v)
 * 
 * The finite difference approximation yields the linear equation system described here:
 * http://en.wikipedia.org/wiki/Discrete_Poisson_equation
 * 
 * For a good overview of eulerian fluid sim methods, see
 * http://www.proxyarch.com/util/techpapers/papers/Fluid%20flow%20for%20the%20rest%20of%20us.pdf
 */
void Grid::solve_pressure(GridHash<float> &pressure, const GridHash<float> &divergence) const
{
	int stride[3] = { 1, res[0], res[0] * res[1] };
	
	lMatrix A(num_cells, num_cells);
	
	/* Reserve space for the base equation system (without boundary conditions).
	 * Each column contains a factor 6 on the diagonal
	 * and up to 6 factors -1 on other places.
	 */
	A.reserve(Eigen::VectorXi::Constant(num_cells, 7));
	
	for (int x = 0; x < res[0]; ++x) {
		for (int y = 0; y < res[1]; ++y) {
			for (int z = 0; z < res[2]; ++z) {
				int u = x * stride[0] + y * stride[1] + z * stride[2];
				bool is_margin = !(x > 0 && x < res[0]-1 && y > 0 && y < res[1]-1 && z > 0 && z < res[2]-1);
				if (is_margin) {
					A.insert(u, u) = 1.0f;
					continue;
				}
				
				/* check for upper bounds in advance
				 * to get the correct number of neighbors,
				 * needed for the diagonal element
				 */
				int neighbors_lo = 0;
				int neighbors_hi = 0;
				int non_solid_neighbors = 0;
				int neighbor_lo_index[3];
				int neighbor_hi_index[3];
				if (z > 1)
					neighbor_lo_index[neighbors_lo++] = u - stride[2];
				if (y > 1)
					neighbor_lo_index[neighbors_lo++] = u - stride[1];
				if (x > 1)
					neighbor_lo_index[neighbors_lo++] = u - stride[0];
				if (x < res[0]-2)
					neighbor_hi_index[neighbors_hi++] = u + stride[0];
				if (y < res[1]-2)
					neighbor_hi_index[neighbors_hi++] = u + stride[1];
				if (z < res[2]-2)
					neighbor_hi_index[neighbors_hi++] = u + stride[2];
				
				/*int liquid_neighbors = neighbors_lo + neighbors_hi;*/
				non_solid_neighbors = 6;
				
				for (int n = 0; n < neighbors_lo; ++n)
					A.insert(neighbor_lo_index[n], u) = -1.0f;
				A.insert(u, u) = (float)non_solid_neighbors;
				for (int n = 0; n < neighbors_hi; ++n)
					A.insert(neighbor_hi_index[n], u) = -1.0f;
			}
		}
	}
	
	ConjugateGradient cg;
	cg.setMaxIterations(100);
	cg.setTolerance(0.01f);
	
	cg.compute(A);
	
	lVector B(num_cells);
	divergence.to_eigen(B);
	
	lVector p = cg.solve(B);
	
	if (cg.info() == Eigen::Success) {
		pressure.from_eigen(p);
	}
	else
		pressure.clear();
}

} /* namespace BPH */

#include "implicit.h"


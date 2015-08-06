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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Kevin Dietrich, Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>

#include <openvdb/tools/ValueTransformer.h>  /* for tools::foreach */
#include <openvdb/tools/LevelSetUtil.h>
#include <openvdb/tools/Composite.h>
#include <openvdb/tools/GridOperators.h>
#include <openvdb/tools/Morphology.h> // for tools::erodeVoxels()
#include <openvdb/tools/PoissonSolver.h>

#include "openvdb_smoke.h"
#include "openvdb_util.h"

namespace internal {

using namespace openvdb;
using namespace openvdb::math;
using tools::poisson::VIndex;

static const VIndex VINDEX_INVALID = (VIndex)(-1);

struct GridScale {
	float factor;
	GridScale(float factor) : factor(factor) {}
	
	inline void operator()(const FloatGrid::ValueOnIter& iter) const {
		iter.setValue((*iter) * factor);
	}
	inline void operator()(const VectorGrid::ValueOnIter& iter) const {
		iter.setValue((*iter) * factor);
	}
};

template <typename GridType>
inline static void mul_grid_fl(GridType &grid, float f)
{
	tools::foreach(grid.beginValueOn(), GridScale(f));
}

inline static void mul_vgrid_fgrid(VectorGrid &R, const VectorGrid &a, const ScalarGrid &b)
{
	struct Local {
		static void mul_v3fl(const Vec3f& a, const float& b, Vec3f& result)
		    { result = a * b; }
	};
	
	R.tree().combine2(a.tree(), b.tree(), Local::mul_v3fl);
}

inline static void div_vgrid_fgrid(VectorGrid &R, const VectorGrid &a, const ScalarGrid &b)
{
	struct Local {
		static void div_v3fl(const Vec3f& a, const float& b, Vec3f& result)
		    { result = (b > 0.0f) ? a / b : Vec3f(0.0f, 0.0f, 0.0f); }
	};
	
	R.tree().combine2(a.tree(), b.tree(), Local::div_v3fl);
}

OpenVDBSmokeData::OpenVDBSmokeData(const Mat4R &cell_transform) :
    cell_transform(Transform::createLinearTransform(cell_transform))
{
	density = ScalarGrid::create(0.0f);
	density->setTransform(this->cell_transform);
	velocity = VectorGrid::create(Vec3f(0.0f, 0.0f, 0.0f));
	velocity->setTransform(this->cell_transform);
	pressure = ScalarGrid::create(0.0f);
	pressure->setTransform(this->cell_transform);
	force = VectorGrid::create(Vec3f(0.0f, 0.0f, 0.0f));
	force->setTransform(this->cell_transform);
}

OpenVDBSmokeData::~OpenVDBSmokeData()
{
}

float OpenVDBSmokeData::cell_size() const
{
	return cell_transform->voxelSize().x();
}

void OpenVDBSmokeData::add_inflow(const std::vector<Vec3s> &vertices, const std::vector<Vec3I> &triangles,
                                  float flow_density, bool incremental)
{
	float bandwidth_ex = (float)LEVEL_SET_HALF_WIDTH;
	float bandwidth_in = (float)LEVEL_SET_HALF_WIDTH;
	
	FloatGrid::Ptr emission = tools::meshToSignedDistanceField<FloatGrid>(*cell_transform, vertices, triangles, std::vector<Vec4I>(), bandwidth_ex, bandwidth_in);
	tools::sdfToFogVolume(*emission, 0.0f);
	mul_grid_fl(*emission, flow_density);
	
	if (incremental) {
		tools::compSum(*density, *emission);
	}
	else {
		tools::compReplace(*density, *emission);
	}
	
//	VectorGrid::Ptr grad = tools::gradient(*sdf);
	
//	tools::compSum(*density, *sdf);
//	tools::compSum(*velocity, *grad);
	
//	BoolGrid::Ptr mask = tools::sdfInteriorMask(*sdf, 0.0f);
//	density->topologyIntersection(*mask);
}

void OpenVDBSmokeData::add_obstacle(const std::vector<Vec3s> &vertices, const std::vector<Vec3I> &triangles)
{
	float bandwidth_ex = (float)LEVEL_SET_HALF_WIDTH;
	float bandwidth_in = (float)LEVEL_SET_HALF_WIDTH;
	FloatGrid::Ptr sdf = tools::meshToSignedDistanceField<FloatGrid>(*cell_transform, vertices, triangles, std::vector<Vec4I>(), bandwidth_ex, bandwidth_in);
//	VectorGrid::Ptr grad = tools::gradient(*sdf);
	
	tools::compSum(*density, *sdf);
//	tools::compSum(*velocity, *grad);
	
//	BoolGrid::Ptr mask = tools::sdfInteriorMask(*sdf, 0.0f);
//	density->topologyIntersection(*mask);
}

void OpenVDBSmokeData::clear_obstacles()
{
//	if (density)
//		density->clear();
}

void OpenVDBSmokeData::add_gravity_force(const Vec3f &g)
{
	tools::compSum(*force, *VectorGrid::create(g));
}

void OpenVDBSmokeData::add_pressure_force(float dt, float bg_pressure)
{
	calculate_pressure(dt, bg_pressure);
	
	VectorGrid::Ptr f = tools::gradient(*pressure);
	div_vgrid_fgrid(*f, *f, *density);
	mul_grid_fl(*f, -dt/cell_size());
	tools::compSum(*force, *f);
}

bool OpenVDBSmokeData::step(float dt, int /*num_substeps*/)
{
	force->clear();
	add_gravity_force(Vec3f(0,0,-1));
	add_pressure_force(dt, 0.0f);
	
	mul_grid_fl(*force, dt);
	tools::compSum(*velocity, *force);
	
	return true;
	
#if 0
	ScalarGrid::Ptr divergence = tools::Divergence<VectorGrid>(*velocity).process();
	
	if (divergence->cbeginValueOn())
		pressure->setTree(tools::poisson::solve(divergence->tree(), result));
	else
		pressure->clear();
	
	return true;
#endif
}

pcg::State OpenVDBSmokeData::calculate_pressure(float dt, float bg_pressure)
{
	typedef FloatTree::ValueConverter<VIndex>::Type VIdxTree;
	typedef pcg::SparseStencilMatrix<float, 7> MatrixType;
	typedef MatrixType::VectorType VectorType;
	
	ScalarGrid::Ptr divergence = tools::Divergence<VectorGrid>(*velocity).process();
	tools::compMul(*divergence, *density->copy());
	mul_grid_fl(*divergence, cell_size() / dt);
	
	VIdxTree::Ptr index_tree = tools::poisson::createIndexTree(divergence->tree());
	VectorType::Ptr B = tools::poisson::createVectorFromTree<float>(divergence->tree(), *index_tree);
	
	const pcg::SizeType rows = B->size();
	MatrixType A(rows);
	
	for (ScalarGrid::ValueOnCIter it = divergence->cbeginValueOn(); it; ++it) {
		Coord c = it.getCoord();
		VIndex index = index_tree->getValue(c);
		
		// TODO probably this can be optimized significantly
		// by shifting grids as a whole and encode neighbors
		// as bit flags or so ...
		
		Coord neighbors[6] = {
		    Coord(c[0]-1, c[1], c[2]),
		    Coord(c[0]+1, c[1], c[2]),
		    Coord(c[0], c[1]-1, c[2]),
		    Coord(c[0], c[1]+1, c[2]),
		    Coord(c[0], c[1], c[2]-1),
		    Coord(c[0], c[1], c[2]+1)
		};
		
		float diag = 0.0f;
		float bg = 0.0f;
		FloatGrid::ConstAccessor acc(density->tree());
		for (int i = 0; i < 6; ++i) {
			const Coord &c = neighbors[i];
			VIndex n = index_tree->getValue(c);
			if (n != VINDEX_INVALID) {
				bool is_solid = false;
				bool is_empty = acc.isValueOn(c);
				
				/* add matrix entries for interacting cells (non-solid neighbors) */
				if (!is_solid) {
					A.setValue(index, n, 1.0f);
					diag -= 1.0f;
				}
				
				/* add background pressure terms */
				if (is_empty) {
					bg -= bg_pressure;
				}
			}
		}
		
		A.setValue(index, index, diag);
		(*B)[index] += bg;
	}
	
	/* preconditioner for faster convergence */
	pcg::JacobiPreconditioner<MatrixType> precond(A);
	
	/* solve A * x = B for x */
	MatrixType::VectorType x(rows, 0.0f);
	
	pcg::State terminator = pcg::terminationDefaults<float>();
	terminator.iterations = 100;
	terminator.relativeError = terminator.absoluteError = 1.0e-4;
	
	util::NullInterrupter interrupter;
	pcg::State result = math::pcg::solve(A, *B, x, precond, interrupter, terminator);
	
	if (result.success) {
		pressure->setTree(tools::poisson::createTreeFromVector<float>(x, *index_tree, 0.0f));
	}
	else {
		pressure->clear();
	}
	
	return result;
}

}  /* namespace internal */

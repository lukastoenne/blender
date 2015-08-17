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
#include <openvdb/tools/ParticlesToLevelSet.h>
#include <openvdb/tools/PointIndexGrid.h>
#include <openvdb/tools/PoissonSolver.h>

#include "openvdb_smoke.h"
#include "openvdb_util.h"

namespace internal {

using namespace openvdb;
using namespace openvdb::math;
using tools::poisson::VIndex;

static const VIndex VINDEX_INVALID = (VIndex)(-1);

template <typename T>
static inline void print_grid_range(Grid<T> &grid, const char *prefix, const char *name)
{
#ifndef NDEBUG
	if (grid.empty())
		printf("%s: %s = 0, min=?, max=?\n", prefix, name);
	else {
		float min = FLT_MAX, max = -FLT_MAX;
		for (typename T::ValueOnCIter iter = grid.cbeginValueOn(); iter; ++iter) {
			float v = FloatConverter<typename T::ValueType>::get(iter.getValue());
			if (v < min) min = v;
			if (v > max) max = v;
		}
		printf("%s: %s = %d, min=%f, max=%f\n", prefix, name, (int)grid.activeVoxelCount(), min, max);
	}
#else
	(void)grid;
	(void)prefix;
	(void)name;
#endif
}

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

inline static void mul_fgrid_fgrid(ScalarGrid &R, const ScalarGrid &a, const ScalarGrid &b)
{
	struct op {
		static inline void combine2Extended(CombineArgs<float, float> &args)
		{
			args.setResult(args.a() * args.b());
			args.setResultIsActive(args.aIsActive() || args.bIsActive());
		}
	};
	
	R.tree().combine2Extended(a.tree(), b.tree(), op::combine2Extended);
}

/* XXX essentially the same as, but probably faster than
 *   a.topologyUnion(b);
 *   a.topologyIntersection(b);
 */
template <typename TA, typename TB>
inline static void topology_copy(Grid<TA> &a, Grid<TB> &b)
{
	typedef typename TA::ValueType VA;
	typedef typename TB::ValueType VB;
	
	struct op {
		inline void operator() (CombineArgs<VA, VB>& args) {
			args.setResultIsActive(args.bIsActive());
		}
		inline static void test(CombineArgs<VA, VB>& args) {
			args.setResultIsActive(args.bIsActive());
		}
	};
	
	a.tree().combine2Extended(a.tree(), b.tree(), op::test);
}

struct add_v3v3 {
	Vec3f v;
	add_v3v3(const Vec3f& v) : v(v) {}
	inline void operator() (const VectorGrid::ValueOnIter& iter) const {
		iter.setValue((*iter) + v);
	}
};

inline static void add_vgrid_v3(VectorGrid &a, const Vec3f &b)
{
	tools::foreach(a.beginValueOn(), add_v3v3(b));
}

inline static void mul_vgrid_fgrid(VectorGrid &R, const VectorGrid &a, const ScalarGrid &b)
{
	struct op {
		static inline void combine2Extended(CombineArgs<Vec3f, float> &args)
		{
			args.setResult(args.a() * args.b());
			args.setResultIsActive(args.aIsActive() || args.bIsActive());
		}
	};
	
	R.tree().combine2Extended(a.tree(), b.tree(), op::combine2Extended);
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

template <int> struct util_weight;
template <> struct util_weight<0> {
	inline static Real weight(Real x) { return x; }
};
template <> struct util_weight<1> {
	inline static Real weight(Real x) { return 1.0 - x; }
};

struct util_gather {
	template <int i, int j, int k>
	inline static void add_corner(BoxStencil<FloatGrid> &stencil, const Vec3R &offset)
	{
		float current = stencil.getValue<i, j, k>();
		
		float weight = util_weight<i>::weight(offset.x());
		
//		stencil.setValue<i, j, k>(current + weight);
		stencil.setValue<i, j, k>(1.0f);
	}
};

void OpenVDBSmokeData::init_grids()
{
#if 0
	const float voxel_size = cell_size();
	FloatGrid::Ptr ls = createLevelSet<FloatGrid>(voxel_size);
	ls->setTransform(cell_transform);

	tools::ParticlesToLevelSet<FloatGrid, Vec3f> raster(*ls);
	raster.setGrainSize(1); /* a value of zero disables threading */
	raster.rasterizeSpheres(points);
	raster.finalize();
	VectorGrid::Ptr vel = raster.attributeGrid();
	vel->setGridClass(GRID_STAGGERED);
	
	tools::sdfToFogVolume(*ls);
	
	density = ls;
	velocity = vel;
#elif 0
	typedef tools::PointIndexTree PointIndexTree;
	typedef tools::PointIndexGrid PointIndexGrid;
	typedef FloatGrid::Accessor FloatAccessor;
	typedef PointIndexGrid::ConstAccessor PointIndexAccessor;
	typedef openvdb::tools::PointIndexIterator<> PointIndexIterator;
	
	PointIndexGrid::Ptr point_grid =
	        tools::createPointIndexGrid<PointIndexGrid>(points, *cell_transform);
	
	PointIndexAccessor acc_point_grid = point_grid->getConstAccessor();
	FloatAccessor acc_density = density->getAccessor();
	PointIndexIterator pit;
	
	PointIndexTree::ValueOnCIter it = point_grid->cbeginValueOn();
	for (; it; ++it) {
		Coord ijk = it.getCoord();
		CoordBBox bbox(ijk, ijk);
		bbox.expand(2);
		Vec3R center = cell_transform->indexToWorld(ijk);
		
		Real weight = 0.0f;
		pit.searchAndUpdate(bbox, acc_point_grid);
		for (; pit; ++pit) {
			Index32 index = *pit;
			Vec3R pos, vel;
			Real rad;
			points.getPosRadVel(index, pos, rad, vel);
			
			Real wx = 1.0 - fabs(pos.x() - center.x());
			Real wy = 1.0 - fabs(pos.y() - center.y());
			Real wz = 1.0 - fabs(pos.z() - center.z());
			weight += wx * wy * wz;
		}
		
		acc_density.setValueOn(ijk, weight);
	}
#elif 1
	typedef tools::PointIndexTree PointIndexTree;
	typedef tools::PointIndexGrid PointIndexGrid;
	typedef FloatGrid::Accessor FloatAccessor;
	typedef PointIndexGrid::ConstAccessor PointIndexAccessor;
	typedef openvdb::tools::PointIndexIterator<> PointIndexIterator;
	
	density->clear();
	
	PointIndexGrid::Ptr point_grid =
	        tools::createPointIndexGrid<PointIndexGrid>(points, *cell_transform);
	FloatAccessor acc_density = density->getAccessor();
	
	for (size_t n = 0; n < points.size(); ++n) {
		Vec3R pos, vel;
		Real rad;
		points.getPosRadVel(n, pos, rad, vel);
		
		Vec3R cpos = cell_transform->worldToIndex(pos);
		float wx1 = fabs(cpos.x() - floor(cpos.x()));
		float wy1 = fabs(cpos.y() - floor(cpos.y()));
		float wz1 = fabs(cpos.z() - floor(cpos.z()));
		float wx0 = 1.0f - wx1;
		float wy0 = 1.0f - wy1;
		float wz0 = 1.0f - wz1;
		
		Coord ijk = cell_transform->worldToIndexCellCentered(pos);
#define ADD_VALUE(i, j, k, w) acc_density.setValueOn(ijk+Coord(i,j,k), acc_density.getValue(ijk+Coord(i,j,k)) + w)
		ADD_VALUE(0,0,0, wx0 * wy0 * wz0);
		ADD_VALUE(1,0,0, wx1 * wy0 * wz0);
		ADD_VALUE(0,1,0, wx0 * wy1 * wz0);
		ADD_VALUE(1,1,0, wx1 * wy1 * wz0);
		ADD_VALUE(0,0,1, wx0 * wy0 * wz1);
		ADD_VALUE(1,0,1, wx1 * wy0 * wz1);
		ADD_VALUE(0,1,1, wx0 * wy1 * wz1);
		ADD_VALUE(1,1,1, wx1 * wy1 * wz1);
#undef ADD_VALUE
	}
	
#else
	typedef tools::UInt32PointPartitioner PointPartitioner;
	PointPartitioner::Ptr partitioner =
	        PointPartitioner::create(points, *cell_transform);
	
//	FloatTree &tree = density->tree;
//	BoxStencil<FloatGrid> stencil(*density);
//	FloatGrid::Accessor acc(density->tree());
//	tree.beginLeaf()
	
	size_t num_buckets = partitioner->size();
	for (size_t i = 0; i < num_buckets; ++i) {
//		CoordBBox leaf_bbox = partitioner->getBBox(i);
		
//		acc.setActiveState(coords.getStart(), true);
		
		PointPartitioner::IndexIterator it = partitioner->indices(i);
		for (; it; ++it) {
			unsigned int index = *it;
			
			Vec3R pos, vel;
			Real rad;
			points.getPosRadVel(index, pos, rad, vel);
//			Vec3R cpos = cell_transform->worldToIndex(pos);
			Vec3R cpos = pos;
			
			stencil.moveTo(cpos);
//			stencil.moveTo(Coord((int)cpos.x(), (int)cpos.y(), (int)cpos.z()));
			
			util_gather::add_corner<0, 0, 0>(stencil, cpos);
			util_gather::add_corner<1, 0, 0>(stencil, cpos);
			util_gather::add_corner<0, 1, 0>(stencil, cpos);
			util_gather::add_corner<1, 1, 0>(stencil, cpos);
			util_gather::add_corner<0, 0, 1>(stencil, cpos);
			util_gather::add_corner<1, 0, 1>(stencil, cpos);
			util_gather::add_corner<0, 1, 1>(stencil, cpos);
			util_gather::add_corner<1, 1, 1>(stencil, cpos);
		}
	}
#endif
}

void OpenVDBSmokeData::update_points(float dt)
{
	typedef VectorGrid::ConstAccessor AccessorType;
	typedef tools::GridSampler<AccessorType, tools::BoxSampler> SamplerType;
	
	AccessorType acc_vel(velocity->tree());
	SamplerType sampler(acc_vel, velocity->transform());
	
	/* use RK2 integration to move points through the velocity field */
	for (SmokeParticleList::iterator it = points.begin(); it != points.end(); ++it) {
		SmokeParticleList::Point &pt = *it;
		
		Vec3f loc1 = pt.loc;
		// note: velocity from particles is ignored, only grid velocities are used
		Vec3f vel1 = sampler.wsSample(loc1);
		
		Vec3f loc2 = loc1 + 0.5f*dt * vel1;
		Vec3f vel2 = sampler.wsSample(loc2);
		
		Vec3f loc3 = loc2 + dt * vel2;
		Vec3f vel3 = sampler.wsSample(loc3);
		
		pt.loc = loc3;
		pt.vel = vel3;
	}
}

#if 0
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
#endif

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

void OpenVDBSmokeData::set_gravity(const Vec3f &g)
{
	gravity = g;
}

void OpenVDBSmokeData::add_gravity_force()
{
	/* density defines which cells gravity acts on */
	force->topologyUnion(*density);
	
	add_vgrid_v3(*force, gravity);
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
	/* keep old velocity */
	velocity_old = velocity;
	
	init_grids();
	
	density->pruneGrid(1e-4f);
	
	/* only cells with some density can be active */
	// XXX implicitly true through the point rasterizer
//	velocity->topologyIntersection(*density);
	
	print_grid_range(*density, "STEP", "density");
	print_grid_range(*velocity, "STEP", "velocity");
	
//	advect_backwards_trace(dt);
	print_grid_range(*velocity, "V1", "velocity");
	
	force->clear();
	add_gravity_force();
	add_pressure_force(dt, 0.0f);
	print_grid_range(*velocity, "V2", "velocity");
	
	printf("  (pressure: ");
	if (pressure_result.success) {
		print_grid_range(*pressure, "INPUT", "pressure");
		printf(" ok)\n");
	}
	else
		printf(" FAIL! %d iterations, error=%f%%=%f)\n",
		       pressure_result.iterations, pressure_result.relativeError, pressure_result.absoluteError);
	
	mul_grid_fl(*force, dt);
	tools::compSum(*velocity, *force);
	print_grid_range(*velocity, "V3", "velocity");
	
	update_points(dt);
	
	return true;
}

struct advect_v3 {
	typedef VectorGrid::ConstAccessor AccessorType;
	typedef tools::GridSampler<AccessorType, tools::StaggeredBoxSampler> SamplerType;
	
	Transform::ConstPtr transform;
	AccessorType acc_vel;
	SamplerType sampler;
	float dt;
	
	advect_v3(const VectorGrid &velocity, float dt) :
	    transform(velocity.transformPtr()),
	    acc_vel(velocity.tree()),
	    sampler(SamplerType(acc_vel, velocity.transform())),
	    dt(dt)
	{
	}
	
	inline void operator() (const VectorGrid::ValueOnIter& iter) const {
		Coord ijk = iter.getCoord();
		
		Vec3f v0 = acc_vel.getValue(ijk);
		Vec3f p0 = transform->indexToWorld(ijk);
		
		Vec3f p1 = p0 - dt * v0;
		Vec3f v1 = sampler.sampleVoxel(p1.x(), p1.y(), p1.z());
		
		iter.setValue(v1);
	}
};

void OpenVDBSmokeData::advect_backwards_trace(float dt)
{
	VectorGrid::Ptr nvel = VectorGrid::create(Vec3f(0.0f, 0.0f, 0.0f));
	nvel->setTransform(velocity->transformPtr());
	nvel->topologyUnion(*velocity);
	
	tools::foreach(nvel->beginValueOn(), advect_v3(*velocity, dt));
	
	velocity = nvel;
}

void OpenVDBSmokeData::calculate_pressure(float dt, float bg_pressure)
{
	typedef FloatTree::ValueConverter<VIndex>::Type VIndexTree;
	typedef pcg::SparseStencilMatrix<float, 7> MatrixType;
	typedef MatrixType::VectorType VectorType;
	
	pressure_result.success = false;
	pressure_result.iterations = 0;
	pressure_result.absoluteError = 0.0f;
	pressure_result.relativeError = 0.0f;
	
	printf("=======================================\n");
	print_grid_range(*density, "PRESSURE", "density");
	print_grid_range(*velocity, "PRESSURE", "velocity");
	
	/* add a 1-cell padding to allow flow into empty cells */
	tools::dilateVoxels(velocity->tree(), 1, tools::NN_FACE);
	
	ScalarGrid::Ptr divergence = tools::Divergence<VectorGrid>(*velocity).process();
	print_grid_range(*divergence, "PRESSURE", "divergence");
	
	mul_fgrid_fgrid(*divergence, *divergence, *density);
	mul_grid_fl(*divergence, cell_size() / dt);
	tmp_divergence = divergence;
	if (divergence->empty())
		return;
	print_grid_range(*divergence, "PRESSURE", "divergence * density");
	
	VIndexTree::Ptr index_tree = tools::poisson::createIndexTree(divergence->tree());
	VectorType::Ptr B = tools::poisson::createVectorFromTree<float>(divergence->tree(), *index_tree);
	
	const pcg::SizeType rows = B->size();
	MatrixType A(rows);
	
	for (ScalarGrid::ValueOnCIter it = divergence->cbeginValueOn(); it; ++it) {
		Coord c = it.getCoord();
		VIndex irow = index_tree->getValue(c);
		
		// TODO probably this can be optimized significantly
		// by shifting grids as a whole and encode neighbors
		// as bit flags or so ...
		// XXX look for openvdb stencils? div operator works similar?
		
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
			VIndex icol = index_tree->getValue(c);
			if (icol != VINDEX_INVALID) {
				const bool is_solid = false; // TODO needs obstacle grids
				/* no need to check actual density threshold, since we prune in advance */
				const bool is_empty = !acc.isValueOn(c);
				const bool is_fluid = !is_solid && !is_empty;
				
				/* add matrix entries for interacting cells (non-solid neighbors) */
				if (!is_solid) {
					diag -= 1.0f;
				}
				
				if (is_fluid) {
					A.setValue(irow, icol, 1.0f);
				}
				
				/* add background pressure terms */
				if (is_empty) {
					bg -= bg_pressure;
				}
			}
		}
		
		/* XXX degenerate case (only solid neighbors), how to handle? */
		if (diag == 0.0f)
			diag = 1.0f;
		
		A.setValue(irow, irow, diag);
		(*B)[irow] += bg;
	}
	
	assert(A.isFinite());
	
#if 0
	{
		struct Local {
			static Coord get_index_coords(VIndex index, VIndexTree &index_tree)
			{
				Grid<VIndexTree>::ValueOnCIter iter = index_tree.cbeginValueOn();
				for (int i = 0; iter; ++iter, ++i)
					if (i == (int)index)
						return iter.getCoord();
				return Coord();
			}
		};
		
		printf("A[%d][X] = \n", (int)A.numRows());
		int irow;
		for (irow = 0; irow < A.numRows(); ++irow) {
			
			MatrixType::ConstRow row = A.getConstRow(irow);
			MatrixType::ConstValueIter row_iter = row.cbegin();
			int k;
//			Coord c = Local::get_index_coords((VIndex)i, *index_tree);
//			printf("  %d (%d, %d, %d) [%d] ", i, c.x(), c.y(), c.z(), row.size());
			printf("  %d ", irow);
			for (k = 0; row_iter; ++row_iter, ++k) {
				VIndex icol = row_iter.column();
				printf("%8.3f | ", A.getValue(irow, icol));
//				printf("[%d]=%.3f | ", (int)icol, A.getValue(irow, icol));
//				Coord cr = Local::get_index_coords(ir, *index_tree);
//				printf("(%d,%d,%d)=%.3f | ", cr.x(), cr.y(), cr.z(), A.getValue(i, k));
			}
			printf("\n");
		}
		
		printf("\n");
		
		printf("B[%d] = \n", (int)B->size());
		for (int i = 0; i < B->size(); ++i) {
			printf("  %d %.5f\n", i, (*B)[i]);
		}
		
		fflush(stdout);
	}
#endif
	
	/* preconditioner for faster convergence */
	pcg::JacobiPreconditioner<MatrixType> precond(A);
	
	/* solve A * x = B for x */
	MatrixType::VectorType x(rows, 0.0f);
	
	pcg::State terminator = pcg::terminationDefaults<float>();
	terminator.iterations = 100;
	terminator.relativeError = terminator.absoluteError = 1.0e-4;
	
	util::NullInterrupter interrupter;
	pressure_result = math::pcg::solve(A, *B, x, precond, interrupter, terminator);
	
	if (pressure_result.success) {
		pressure->setTree(tools::poisson::createTreeFromVector<float>(x, *index_tree, 0.0f));
	}
	else {
		pressure->clear();
	}
}

}  /* namespace internal */

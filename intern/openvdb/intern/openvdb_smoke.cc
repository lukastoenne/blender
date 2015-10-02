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
#include <openvdb/tools/PointScatter.h>
#include <openvdb/tools/PoissonSolver.h>

#include "openvdb_smoke.h"
#include "openvdb_util.h"

#define DEBUG_PRESSURE_SOLVE

namespace internal {

using namespace openvdb;
using namespace openvdb::math;

static const SmokeData::VIndex VINDEX_INVALID = (SmokeData::VIndex)(-1);

/* ------------------------------------------------------------------------- */

template <typename T>
static inline void print_grid_range(Grid<T> &grid, const char *prefix, const char *name)
{
#if 0
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
#else
	(void)grid;
	(void)prefix;
	(void)name;
#endif
}

#ifdef DEBUG_PRESSURE_SOLVE
template <typename MatrixType, typename VectorType>
inline static void debug_print_poisson_matrix(const MatrixType &A, typename VectorType::Ptr b)
{
	typedef SmokeData::VIndex VIndex;
	typedef ScalarTree::ValueConverter<VIndex>::Type VIndexTree;
	
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
		
		typename MatrixType::ConstRow row = A.getConstRow(irow);
		typename MatrixType::ConstValueIter row_iter = row.cbegin();
		int k;
//		Coord c = Local::get_index_coords((VIndex)i, *index_tree);
//		printf("  %d (%d, %d, %d) [%d] ", i, c.x(), c.y(), c.z(), row.size());
		printf("  %d ", irow);
		for (k = 0; row_iter; ++row_iter, ++k) {
			VIndex icol = row_iter.column();
			printf("%8.3f | ", A.getValue(irow, icol));
//			printf("[%d]=%.3f | ", (int)icol, A.getValue(irow, icol));
//			Coord cr = Local::get_index_coords(ir, *index_tree);
//			printf("(%d,%d,%d)=%.3f | ", cr.x(), cr.y(), cr.z(), A.getValue(i, k));
		}
		printf("\n");
	}
	
	printf("\n");
	
	printf("B[%d] = \n", (int)b->size());
	for (int i = 0; i < b->size(); ++i) {
		printf("  %d %.5f\n", i, (*b)[i]);
	}
	
	fflush(stdout);
}
#else
template <typename MatrixType, typename VectorType>
inline static void debug_print_poisson_matrix(const MatrixType &A, typename VectorType::Ptr b)
{ (void)A; (void)b; }
#endif

static unsigned int hash_combine(unsigned int kx, unsigned int ky)
{
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

	unsigned int a, b, c;

	a = b = c = 0xdeadbeef + (2 << 2) + 13;
	a += kx;
	b += ky;

	c ^= b; c -= rot(b,14);
	a ^= c; a -= rot(c,11);
	b ^= a; b -= rot(a,25);
	c ^= b; c -= rot(b,16);
	a ^= c; a -= rot(c,4);
	b ^= a; b -= rot(a,14);
	c ^= b; c -= rot(b,24);

	return c;

#undef rot
}

static unsigned int coord_hash(const Coord &ijk)
{
	return hash_combine(hash_combine(ijk.x(), ijk.y()), ijk.z());
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

inline static void velocity_normalize(VectorGrid &vel, const VectorGrid &weight)
{
	struct Local {
		static void div_v3v3(const Vec3f& v, const Vec3f& w, Vec3f& result)
		{
			result = Vec3f((w.x() > 0.0f) ? v.x() / w.x() : 0.0f,
			               (w.y() > 0.0f) ? v.y() / w.y() : 0.0f,
			               (w.z() > 0.0f) ? v.z() / w.z() : 0.0f);
		}
	};
	
	vel.tree().combine2(vel.tree(), weight.tree(), Local::div_v3v3);
}

void SmokeParticleList::from_stream(OpenVDBPointInputStream *stream)
{
	m_points.clear();
	
	for (; stream->has_points(stream); stream->next_point(stream)) {
		Vec3f locf, velf;
		float rad;
		stream->get_point(stream, locf.asPointer(), &rad, velf.asPointer());
		
		Point pt(locf, rad * m_radius_scale, velf * m_velocity_scale);
		m_points.push_back(pt);
	}
}

void SmokeParticleList::to_stream(OpenVDBPointOutputStream *stream) const
{
	stream->create_points(stream, (int)m_points.size());
	
	PointList::const_iterator it;
	for (it = m_points.begin(); stream->has_points(stream) && it != m_points.end(); stream->next_point(stream), ++it) {
		const Point &pt = *it;
		Vec3f locf = pt.loc;
		Vec3f velf = pt.vel;
		stream->set_point(stream, locf.asPointer(), velf.asPointer());
	}
}

void SmokeParticleList::add_source(const Transform &cell_transform, const std::vector<Vec3s> &vertices, const std::vector<Vec3I> &triangles,
                                   unsigned int seed, float points_per_voxel, const Vec3f &velocity)
{
	FloatGrid::Ptr source = tools::meshToLevelSet<FloatGrid>(cell_transform, vertices, triangles, std::vector<Vec4I>(), 0.5f);
	
	typedef boost::mt19937 RngType;
	typedef tools::DenseUniformPointScatter<PointAccessor, RngType> ScatterType;
	
	PointAccessor point_acc(this, velocity);
	RngType rng = RngType(seed);
	ScatterType scatter(point_acc, points_per_voxel, rng);
	
	// XXX TODO disabled temporarily
//	scatter(*source);
}

SmokeData::SmokeData(const Mat4R &cell_transform) :
    cell_transform(Transform::createLinearTransform(cell_transform))
{
	density = ScalarGrid::create(0.0f);
	density->setTransform(this->cell_transform);
	velocity = VectorGrid::create(Vec3f(0.0f, 0.0f, 0.0f));
	velocity->setTransform(this->cell_transform);
	velocity->setGridClass(GRID_STAGGERED);
	obstacle = ScalarGrid::create(0.0f);
	obstacle->setTransform(this->cell_transform);
}

SmokeData::~SmokeData()
{
}

float SmokeData::cell_size() const
{
	return cell_transform->voxelSize().x();
}

void SmokeData::init_grids()
{
#if 0
	/* simple particle loop
	 * (does not support averaging and can lead to large density differences)
	 */
	typedef FloatGrid::Accessor FloatAccessor;
	typedef VectorGrid::Accessor VectorAccessor;
	
	density->clear();
	velocity->clear();
	
	/* Temp grid to store accumulated velocity weight for normalization.
	 * Velocity is a staggered grid, so these weights are not the same as
	 * the regular density! For more detailed description of weighting functions, see e.g.
	 * 
	 * Gerszewski, Dan, and Adam W. Bargteil.
	 * "Physics-based animation of large-scale splashing liquids."
	 * ACM Trans. Graph. 32.6 (2013): 185.
	 */
	VectorGrid::Ptr velocity_weight = VectorGrid::create(Vec3f(0.0f, 0.0f, 0.0f));
	
	FloatAccessor acc_density = density->getAccessor();
	VectorAccessor acc_velocity = velocity->getAccessor();
	VectorAccessor acc_velweight = velocity_weight->getAccessor();
	
	for (size_t n = 0; n < points.size(); ++n) {
		Vec3R pos, vel;
		Real rad;
		points.getPosRadVel(n, pos, rad, vel);
		
		Vec3R pos_wall = cell_transform->worldToIndex(pos) + Vec3R(0.5d, 0.5d, 0.5d);
		Vec3R pos_cell = cell_transform->worldToIndex(pos);
		Coord ijk = Coord(floor(pos_cell[0]), floor(pos_cell[1]), floor(pos_cell[2]));
		Coord ijk_x = Coord(floor(pos_wall[0]), floor(pos_cell[1]), floor(pos_cell[2]));;
		Coord ijk_y = Coord(floor(pos_cell[0]), floor(pos_wall[1]), floor(pos_cell[2]));;
		Coord ijk_z = Coord(floor(pos_cell[0]), floor(pos_cell[1]), floor(pos_wall[2]));;
		/* cell center weights (for density) */
		float cx[2], cy[2], cz[2];
		cx[1] = fabs(pos_cell.x() - round(pos_cell.x()));
		cy[1] = fabs(pos_cell.y() - round(pos_cell.y()));
		cz[1] = fabs(pos_cell.z() - round(pos_cell.z()));
		cx[0] = 1.0f - cx[1];
		cy[0] = 1.0f - cy[1];
		cz[0] = 1.0f - cz[1];
		/* face center weights (for velocity) */
		float fx[2], fy[2], fz[2];
		fx[1] = fabs(pos_wall.x() - floor(pos_wall.x()));
		fy[1] = fabs(pos_wall.y() - floor(pos_wall.y()));
		fz[1] = fabs(pos_wall.z() - floor(pos_wall.z()));
		fx[0] = 1.0f - fx[1];
		fy[0] = 1.0f - fy[1];
		fz[0] = 1.0f - fz[1];
		
#define ADD_DENSITY(di, dj, dk) { \
	Coord c = ijk.offsetBy(di, dj, dk); \
	acc_density.setValueOn(c, acc_density.getValue(c) + cx[di] * cy[dj] * cz[dk]); \
}
#define ADD_VELOCITY_X(di, dj, dk) { \
	Coord c = ijk_x.offsetBy(di, 0, 0); \
	float w = fx[di] * cy[dj] * cz[dk]; \
	acc_velocity.setValueOn(c, acc_velocity.getValue(c) + Vec3f(vel.x() * w, 0.0f, 0.0f)); \
	acc_velweight.setValueOn(c, acc_velweight.getValue(c) + Vec3f(w, 0.0f, 0.0f)); \
}
#define ADD_VELOCITY_Y(di, dj, dk) { \
	Coord c = ijk_y.offsetBy(di, 0, 0); \
	float w = cx[di] * fy[dj] * cz[dk]; \
	acc_velocity.setValueOn(c, acc_velocity.getValue(c) + Vec3f(0.0f, vel.y() * w, 0.0f)); \
	acc_velweight.setValueOn(c, acc_velweight.getValue(c) + Vec3f(0.0f, w, 0.0f)); \
}
#define ADD_VELOCITY_Z(di, dj, dk) { \
	Coord c = ijk_z.offsetBy(di, 0, 0); \
	float w = cx[di] * cy[dj] * fz[dk]; \
	acc_velocity.setValueOn(c, acc_velocity.getValue(c) + Vec3f(0.0f, 0.0f, vel.z() * w)); \
	acc_velweight.setValueOn(c, acc_velweight.getValue(c) + Vec3f(0.0f, 0.0f, w)); \
}
#define ADD_VELOCITY(di, dj, dk) \
	ADD_VELOCITY_X(di, dj, dk); \
	ADD_VELOCITY_Y(di, dj, dk); \
	ADD_VELOCITY_Z(di, dj, dk);
		
		ADD_DENSITY(0,0,0);
		ADD_DENSITY(0,0,1);
		ADD_DENSITY(0,1,0);
		ADD_DENSITY(0,1,1);
		ADD_DENSITY(1,0,0);
		ADD_DENSITY(1,0,1);
		ADD_DENSITY(1,1,0);
		ADD_DENSITY(1,1,1);
		
		ADD_VELOCITY(0,0,0);
		ADD_VELOCITY(0,0,1);
		ADD_VELOCITY(0,1,0);
		ADD_VELOCITY(0,1,1);
		ADD_VELOCITY(1,0,0);
		ADD_VELOCITY(1,0,1);
		ADD_VELOCITY(1,1,0);
		ADD_VELOCITY(1,1,1);
		
#undef ADD_DENSITY
#undef ADD_VELOCITY
	}
	
	/* normalize velocity vectors */
	velocity_normalize(*velocity, *velocity_weight);
	
	velocity_weight.reset(); // release
#elif 0
	/* using the ParticlesToLevelSet tool */
	
	typedef openvdb::tools::ParticlesToLevelSet<ScalarGrid, Vec3f> RasterT;
	const float halfWidth = 2.0f;
	openvdb::FloatGrid::Ptr ls = openvdb::createLevelSet<openvdb::FloatGrid>(cell_size(), halfWidth);
	
	RasterT raster(*ls);
	raster.setGrainSize(1); //a value of zero disables threading
	
	{
		ScopeTimer prof("----Particles to Level Set");
		raster.rasterizeSpheres(points);
	}
	{
		ScopeTimer prof("----Finalize Attribute Grid");
		raster.finalize();
	}
	{
		ScopeTimer prof("----Density Fog Volume");
		tools::sdfToFogVolume(*ls, 0.0f);
	}
	
	density = ls;
	velocity = raster.attributeGrid();
#else
	/* TEST DISTRIBUTION */
	
	if (density->empty()) {
		// dam break
		BBoxd bbox(Vec3d(-1, -0.1, -0.5), Vec3d(1, 0.1, 0.5));
		Vec3f vel0(0.4, 0.18, -0.8);
		
		vel0 *= 1.0f;
		
		CoordBBox cbox = cell_transform->worldToIndexCellCentered(bbox);
		/* need to extend cbox by 1 in positive direction,
		 * to include neighboring cells for staggered velocity
		 */
		cbox.max().offset(1, 1, 1);
		
		ScalarGrid::Accessor dacc = density->getAccessor();
		VectorGrid::Accessor vacc = velocity->getAccessor();
		
		for (int i = cbox.min().x(); i <= cbox.max().x(); ++i) {
			for (int j = cbox.min().y(); j <= cbox.max().y(); ++j) {
				for (int k = cbox.min().z(); k <= cbox.max().z(); ++k) {
					bool xmax = i == cbox.max().x();
					bool ymax = j == cbox.max().y();
					bool zmax = k == cbox.max().z();
					
					if (!xmax && !ymax && !zmax)
						dacc.setValueOn(Coord(i, j, k), 1.0f);
					
					float vx = ymax || zmax ? 0.0f : vel0.x();
					float vy = zmax || xmax ? 0.0f : vel0.y();
					float vz = xmax || ymax ? 0.0f : vel0.z();
					vacc.setValueOn(Coord(i, j, k), Vec3f(vx, vy, vz));
				}
			}
		}
	}
#endif
}

void SmokeData::update_points(float dt)
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

void SmokeData::add_obstacle(const std::vector<Vec3s> &vertices, const std::vector<Vec3I> &triangles)
{
	float bandwidth_ex = (float)LEVEL_SET_HALF_WIDTH;
	float bandwidth_in = (float)LEVEL_SET_HALF_WIDTH;
	FloatGrid::Ptr obs = tools::meshToSignedDistanceField<FloatGrid>(*cell_transform, vertices, triangles, std::vector<Vec4I>(), bandwidth_ex, bandwidth_in);
	BoolGrid::Ptr mask = tools::sdfInteriorMask(*obs, 0.0f);
	obs->topologyIntersection(*mask);
//	FloatGrid::Ptr obs = tools::meshToLevelSet<FloatGrid>(*cell_transform, vertices, triangles, std::vector<Vec4I>(), LEVEL_SET_HALF_WIDTH);
	
	tools::compMax(*obstacle, *obs);
}

void SmokeData::clear_obstacles()
{
	obstacle->clear();
}

void SmokeData::set_gravity(const Vec3f &g)
{
	gravity = g;
}

void SmokeData::add_gravity_force(VectorGrid &force)
{
	add_vgrid_v3(force, gravity);
}

/* calculates gradient as a staggered grid */
template<
    typename InGridT,
    typename MaskGridType = typename tools::gridop::ToBoolGrid<InGridT>::Type,
    typename InterruptT = util::NullInterrupter>
struct StaggeredGradientFunctor
{
	typedef InGridT                                         InGridType;
	typedef typename tools::ScalarToVectorConverter<InGridT>::Type OutGridType;
	
	StaggeredGradientFunctor(const InGridT& grid, const MaskGridType* mask,
	        bool threaded, InterruptT* interrupt):
	    mThreaded(threaded), mInputGrid(grid), mInterrupt(interrupt), mMask(mask) {}
	
	template<typename MapT>
	void operator()(const MapT& map)
	{
		typedef math::Gradient<MapT, math::BD_1ST> OpT;
		tools::gridop::GridOperator<InGridType, MaskGridType, OutGridType, MapT, OpT, InterruptT>
		        op(mInputGrid, mMask, map, mInterrupt);
		mOutputGrid = op.process(mThreaded); // cache the result
	}
	
	const bool                 mThreaded;
	const InGridT&             mInputGrid;
	typename OutGridType::Ptr  mOutputGrid;
	InterruptT*                mInterrupt;
	const MaskGridType*        mMask;
};

template <typename ObstacleGridType>
struct centered_to_staggered {
	typedef typename VectorGrid::ValueType ValueType;
	
	typedef VectorGrid::ConstAccessor AccessorType;
	typedef typename ObstacleGridType::ConstAccessor ObstacleAccessorType;
	
	typedef tools::GridSampler<AccessorType, tools::BoxSampler> SamplerType;
	
	AccessorType acc;
	ObstacleAccessorType obs_acc;
	SamplerType sampler;
	
	centered_to_staggered(const VectorGrid &grid, const ObstacleGridType &obs) :
	    acc(grid.getConstAccessor()),
	    obs_acc(obs.getConstAccessor()),
	    sampler(SamplerType(acc, grid.transform()))
	{
	}
	
	inline Vec3f get_value(const Coord &ijk) const
	{
		#define V(di,dj,dk) acc.getValue(ijk.offsetBy((di),(dj),(dk)))
		#define FLUID(di,dj,dk) acc.isValueOn(ijk.offsetBy((di),(dj),(dk)))
		#define SOLID(di,dj,dk) obs_acc.isValueOn(ijk.offsetBy((di),(dj),(dk)))
		return Vec3f(0.5f * (V(0,0,0).x() + V(-1,0,0).x()),
		             0.5f * (V(0,0,0).y() + V(0,-1,0).y()),
		             0.5f * (V(0,0,0).z() + V(0,0,-1).z()));
		#undef V
	}
	
	inline void operator() (const typename VectorGrid::ValueOnIter& iter) const
	{
#if 0
		Coord ijk = iter.getCoord();
		
		Vec3f v0x = get_velocity_x(ijk);
		Vec3f v0y = get_velocity_y(ijk);
		Vec3f v0z = get_velocity_z(ijk);
		Vec3f p0 = transform->indexToWorld(ijk);
		
		Vec3f p1x = p0 - timestep * v0x;
		Vec3f p1y = p0 - timestep * v0y;
		Vec3f p1z = p0 - timestep * v0z;
		/* transform to index space for shifting */
		p1x = transform->worldToIndex(p1x);
		p1y = transform->worldToIndex(p1y);
		p1z = transform->worldToIndex(p1z);
		
		ValueType value = Vec3f(sampler.isSample(p1x).x(),
		                        sampler.isSample(p1y).y(),
		                        sampler.isSample(p1z).z());
		iter.setValue(value);
#endif
	}
};

/* Note: this operator must not be used shared, it needs per-thread accessors */
struct prune_velocity {
	typedef ScalarGrid::ConstAccessor DensityAccessorType;
	
	DensityAccessorType acc;
	
	prune_velocity(const ScalarGrid &density) :
	    acc(density.getConstAccessor())
	{
	}
	
	inline void operator() (const typename VectorGrid::ValueOnIter& iter) const
	{
		Coord ijk = iter.getCoord();
		
		bool on_center = acc.isValueOn(ijk);
		bool on_x = acc.isValueOn(ijk.offsetBy(-1, 0, 0)) || on_center;
		bool on_y = acc.isValueOn(ijk.offsetBy(0, -1, 0)) || on_center;
		bool on_z = acc.isValueOn(ijk.offsetBy(0, 0, -1)) || on_center;
		
		if (!(on_x || on_y || on_z))
			iter.setValueOff();
		else if (!(on_x && on_y && on_z)) {
			Vec3f value = iter.getValue();
			
			if (!on_x)
				value.x() = 0.0f;
			if (!on_y)
				value.y() = 0.0f;
			if (!on_z)
				value.z() = 0.0f;
			
			iter.setValue(value);
		}
	}
};

bool SmokeData::step(float dt)
{
	ScopeTimer prof("Smoke timestep");
	
	if (debug.stage >= 1)
	{
		ScopeTimer prof("--Init grids");
		init_grids();
		
		density->pruneGrid(1e-4f);
		
		/* only cells with some density can be active */
		// XXX implicitly true through the point rasterizer
//		velocity->topologyIntersection(*density);
		
		/* add a 1-cell padding to allow flow into empty cells */
//		tools::dilateVoxels(velocity->tree(), 1, tools::NN_FACE);
		
		/* disable obstacle cells */
#if 0
		density->topologyDifference(*obstacle);
		density->pruneGrid();
		velocity->topologyDifference(*obstacle);
		velocity->pruneGrid();
#endif
	}
	
#if 0
	if (debug.stage >= 2)
	{
		ScopeTimer prof("--Apply External Forces");
		
		VectorGrid::Ptr force = VectorGrid::create(Vec3f(0.0f, 0.0f, 0.0f));
		force->setTransform(this->cell_transform);
		force->setGridClass(GRID_STAGGERED);
		
		/* density defines which cells forces act on */
		force->topologyUnion(*density);
		
		add_gravity_force(*force);
		
		tmp_force = force->deepCopy();
		
		mul_grid_fl(*force, dt);
//		tools::compSum(*velocity, *force);
	}
#endif
	
#if 1
	if (debug.stage >= 3)
	{
		ScopeTimer prof("--Advect Velocity Field");
		advect_velocity(dt, ADVECT_MAC_CORMACK);
	}
#endif
	
	{
#if 0
		ScopeTimer prof("--Divergence-Free Projection");
		
		const float bg_pressure = 1.0f;
		
		ScalarGrid::Ptr pressure = ScalarGrid::create(0.0f);
		pressure->setTransform(cell_transform);
		pressure_result = solve_pressure_equation(*velocity, *density, *obstacle, bg_pressure, *pressure);
		if (!pressure_result.success) {
			printf(" FAIL! %d iterations, error=%f%%=%f)\n",
			       pressure_result.iterations, pressure_result.relativeError, pressure_result.absoluteError);
		}
#endif
		
#if 0
		/* NB: the default gradient function uses 2nd order central differencing,
		 * but 1st order backward differencing should be used instead for staggered grids.
		 * Not sure why the gradient does not do this automatically like the divergence op...
		 * - lukas
		 */
//		tools::dilateVoxels(pressure->tree(), 1, tools::NN_FACE);
	#if 0
		VectorGrid::Ptr g = tools::gradient(*pressure);
		g->setGridClass(GRID_STAGGERED);
	#else
		StaggeredGradientFunctor<FloatGrid> functor(*pressure, NULL, true, NULL);
		processTypedMap(pressure->transform(), functor);
		if (functor.mOutputGrid)
			functor.mOutputGrid->setVectorType(openvdb::VEC_COVARIANT);
		VectorGrid::Ptr g = functor.mOutputGrid;
		g->setGridClass(GRID_STAGGERED);
	#endif
		
//		mul_grid_fl(*g, -1.0f);
//		mul_grid_fl(*g, -dt/cell_size());
//		mul_grid_fl(*g, -1.0f/cell_size());
//		mul_grid_fl(*g, -cell_size());
		mul_grid_fl(*g, -debug_scale);
		
		tmp_pressure_gradient = g->deepCopy();
		
		tools::compSum(*velocity, *g);
		
		tmp_divergence_new = tools::Divergence<VectorGrid>(*velocity).process();
#endif
	}
	
#if 1
	if (debug.stage >= 4)
	{
		ScopeTimer prof("--Advect Density");
		advect_density_field(dt, ADVECT_MAC_CORMACK);
	}
#endif
	
#if 1
	if (debug.stage >= 5)
	{
		ScopeTimer prof("--Deactivate Density Threshold");
		/* deactivate cells below threshold density */
		tools::deactivate(density->tree(), 0.0f, 1e-3f);
	}
#endif
#if 1
	if (debug.stage >= 6)
	{
		ScopeTimer prof("--Clamp Velocity Cells to Density");
		prune_velocity op(*density);
		tools::foreach(velocity->beginValueOn(), op, false);
	}
	
	if (debug.stage >= 7)
	{
		ScopeTimer prof("--Prune Cells");
		/* remove unused memory */
		tools::pruneInactive(density->tree());
		tools::pruneInactive(velocity->tree());
	}
#endif
	
#if 0
	{
		ScopeTimer prof("--Update particles");
		if (dt > 0.0f) {
			update_points(dt);
		}
	}
#endif
	
	return true;
}

/* Note: this operator must not be used shared, it needs per-thread accessors */
template <typename GridType>
struct advect_semi_lagrange {
	typedef typename GridType::ValueType ValueType;
	
	typedef typename GridType::ConstAccessor AccessorType;
	typedef VectorGrid::ConstAccessor VelocityAccessorType;
	typedef ScalarGrid::ConstAccessor ObstacleAccessorType;
	
	/* no need to use a staggered box sampler, since we use the center point
	 * for backtracing, with velocity components from from cell walls.
	 */
	typedef tools::GridSampler<AccessorType, tools::BoxSampler> SamplerType;
	
	Transform::ConstPtr transform;
	AccessorType acc;
	SamplerType sampler;
	VelocityAccessorType acc_vel;
	ObstacleAccessorType acc_obs;
	float timestep;
	SmokeDebug *debug;
	
	advect_semi_lagrange(const GridType &grid, const VectorGrid &velocity, const ScalarGrid &obstacle, float timestep, SmokeDebug &debug) :
	    transform(velocity.transformPtr()),
	    acc(grid.getConstAccessor()),
	    sampler(SamplerType(acc, grid.transform())),
	    acc_vel(velocity.tree()),
	    acc_obs(obstacle.tree()),
	    timestep(timestep),
	    debug(&debug)
	{
	}
	
	inline Vec3f get_velocity_centered(const Coord &ijk) const
	{
		#define V(di,dj,dk) acc_vel.getValue(ijk.offsetBy((di),(dj),(dk)))
		return Vec3f(0.5f * (V(0,0,0).x() + V(1,0,0).x()),
		             0.5f * (V(0,0,0).y() + V(0,1,0).y()),
		             0.5f * (V(0,0,0).z() + V(0,0,1).z()));
		#undef V
	}
	
	inline Vec3f get_velocity_x(const Coord &ijk) const
	{
		#define V(di,dj,dk) acc_vel.getValue(ijk.offsetBy((di),(dj),(dk)))
		return Vec3f(V(0,0,0).x(),
		             0.25f * (V(0,0,0).y() + V(-1,0,0).y() + V(0,1,0).y() + V(-1,1,0).y()),
		             0.25f * (V(0,0,0).z() + V(-1,0,0).z() + V(0,0,1).z() + V(-1,0,1).z()));
		#undef V
	}
	
	inline Vec3f get_velocity_y(const Coord &ijk) const
	{
		#define V(di,dj,dk) acc_vel.getValue(ijk.offsetBy((di),(dj),(dk)))
		return Vec3f(0.25f * (V(0,0,0).x() + V(0,-1,0).x() + V(1,0,0).x() + V(1,-1,0).x()),
		             V(0,0,0).y(),
		             0.25f * (V(0,0,0).z() + V(0,-1,0).z() + V(0,0,1).z() + V(0,-1,1).z()));
		#undef V
	}
	
	inline Vec3f get_velocity_z(const Coord &ijk) const
	{
		#define V(di,dj,dk) acc_vel.getValue(ijk.offsetBy((di),(dj),(dk)))
		return Vec3f(0.25f * (V(0,0,0).x() + V(0,0,-1).x() + V(1,0,0).x() + V(1,0,-1).x()),
		             0.25f * (V(0,0,0).y() + V(0,0,-1).y() + V(0,1,0).y() + V(0,1,-1).y()),
		             V(0,0,0).z());
		#undef V
	}
	
	inline void operator() (const typename ScalarGrid::ValueOnIter& iter) const
	{
		Coord ijk = iter.getCoord();
		
		Vec3f v0 = get_velocity_centered(ijk);
		Vec3f p0 = transform->indexToWorld(ijk);
		
		/* traceback */
		Vec3f p1 = p0 - timestep * v0;
		
#if 0
		float r = timestep > 0.0f ? 0 : 1;
		float g = timestep > 0.0f ? 1 : 0;
		float b = timestep > 0.0f ? 0 : 0;
		debug->draw_vector(p0, -v0, r, g, b, 387232, coord_hash(ijk));
		debug->draw_dot(p1, 1, 1, 0, 387237, coord_hash(ijk));
#endif
		
		/* transform to index space */
		p1 = transform->worldToIndex(p1);
		
		ValueType value = sampler.isSample(p1);
		iter.setValue(value);
	}
	
	inline void operator() (const typename VectorGrid::ValueOnIter& iter) const
	{
		Coord ijk = iter.getCoord();
		
		Vec3f v0x = get_velocity_x(ijk);
		Vec3f v0y = get_velocity_y(ijk);
		Vec3f v0z = get_velocity_z(ijk);
		Vec3f p0 = transform->indexToWorld(ijk);
		
		Vec3f p1x = p0 - timestep * v0x;
		Vec3f p1y = p0 - timestep * v0y;
		Vec3f p1z = p0 - timestep * v0z;
		
#if 1
		debug->draw_vector(p0, v0x, 1, 0, 0, 38721, coord_hash(ijk));
//		debug->draw_vector(p0, v0y, 0, 1, 0, 38722, coord_hash(ijk));
		debug->draw_vector(p0, v0z, 0, 0, 1, 38723, coord_hash(ijk));
#endif
		
		/* transform to index space */
		p1x = transform->worldToIndex(p1x);
		p1y = transform->worldToIndex(p1y);
		p1z = transform->worldToIndex(p1z);
		
		ValueType value = Vec3f(sampler.isSample(p1x).x(),
		                        sampler.isSample(p1y).y(),
		                        sampler.isSample(p1z).z());
		iter.setValue(value);
	}
};

template <typename GridType>
struct advect_mac_cormack_correct {
	typedef typename GridType::ValueType ValueType;
	typedef typename GridType::ConstAccessor AccessorType;
	typedef ScalarGrid::ConstAccessor FluidAccessorType;
	
	float strength;
	AccessorType acc_orig;
	AccessorType acc_bwd;
	FluidAccessorType acc_fluid;
	
	advect_mac_cormack_correct(float strength, const ScalarGrid &fluid, const GridType &orig, const GridType &bwd) :
	    strength(strength),
	    acc_orig(orig.tree()),
	    acc_bwd(bwd.tree()),
	    acc_fluid(fluid.tree())
	{
	}
	
	inline void operator() (const typename ScalarGrid::ValueOnIter& iter) const
	{
		Coord ijk = iter.getCoord();
		float value = iter.getValue();
		
		iter.setValue(value + strength * 0.5f * (acc_orig.getValue(ijk) - acc_bwd.getValue(ijk)));
	}
	
	inline void operator() (const typename VectorGrid::ValueOnIter& iter) const
	{
		Coord ijk = iter.getCoord();
		Vec3f value = iter.getValue();
		
		if (acc_fluid.isValueOn(ijk)) {
			Vec3f correction = strength * 0.5f * (acc_orig.getValue(ijk) - acc_bwd.getValue(ijk));
			
			if (acc_fluid.isValueOn(ijk.offsetBy(-1,0,0)))
				value.x() += correction.x();
			if (acc_fluid.isValueOn(ijk.offsetBy(0,-1,0)))
				value.y() += correction.y();
			if (acc_fluid.isValueOn(ijk.offsetBy(0,0,-1)))
				value.z() += correction.z();
		}
		
		iter.setValue(value);
	}
};

template <typename GridType>
static typename GridType::Ptr advect_field(const GridType &grid, const VectorGrid &density, const VectorGrid &velocity, const ScalarGrid &obstacle,
                                           float dt, SmokeData::AdvectionMode mode, SmokeDebug &debug)
{
	typedef typename GridType::Ptr GridTypePtr;
	
	/* correction factor for MacCormack advection */
	float correction = 1.0f;
	
	/* forward step */
	GridTypePtr forward = grid.copy(CP_COPY);
	/* pad grid to allow advection into empty cells */
	tools::dilateVoxels(forward->tree(), 1, tools::NN_FACE);
	
	advect_semi_lagrange<GridType> op_forward(grid, velocity, obstacle, dt, debug);
	tools::foreach(forward->beginValueOn(), op_forward, false);
	
	switch (mode) {
		case SmokeData::ADVECT_SEMI_LAGRANGE:
			return forward;
			break;
		
		case SmokeData::ADVECT_MAC_CORMACK:
			/* backward step */
			GridTypePtr backward = forward->copy(CP_COPY);
			advect_semi_lagrange<GridType> op_backward(*forward, velocity, obstacle, -dt, debug);
//			tools::foreach(backward->beginValueOn(), op_backward, false);
			
			GridTypePtr corrected = forward->copy(CP_COPY);
			advect_mac_cormack_correct<GridType> op_correct(correction, density, grid, *backward);
//			tools::foreach(corrected->beginValueOn(), op_correct, false);
			
			return corrected;
			break;
	}
	
	return GridTypePtr();
}

void SmokeData::advect_velocity(float dt, AdvectionMode mode)
{
	velocity = advect_field<VectorGrid>(*velocity, *density, *velocity, *obstacle, dt, mode, debug);
}

void SmokeData::advect_density_field(float dt, AdvectionMode mode)
{
	density = advect_field<ScalarGrid>(*density, *density, *velocity, *obstacle, dt, mode, debug);
}

ScalarGrid::Ptr SmokeData::calc_divergence()
{
	ScalarGrid::Ptr div_u = tools::Divergence<VectorGrid>(*velocity).process();
		
#ifdef DEBUG_PRESSURE_SOLVE
	tmp_divergence = div_u->deepCopy();
#endif
	
	return div_u;
}

pcg::State SmokeData::solve_pressure_equation(const VectorGrid &u,
                                              const ScalarGrid &mask_fluid,
                                              const ScalarGrid &mask_solid,
                                              float bg_pressure,
                                              ScalarGrid &q)
{
	pcg::State result;
	result.success = false;
	result.iterations = 0;
	result.absoluteError = 0.0f;
	result.relativeError = 0.0f;
	
	ScalarGrid::Ptr div_u = calc_divergence();
	
	VIndexTree::Ptr index_tree = tools::poisson::createIndexTree(div_u->tree());
	VectorType::Ptr b = tools::poisson::createVectorFromTree<float>(div_u->tree(), *index_tree);
	
	const pcg::SizeType rows = b->size();
	MatrixType A(rows);
	
	ScalarGrid::ConstAccessor acc_solid(mask_solid.tree());
	ScalarGrid::ConstAccessor acc_fluid(mask_fluid.tree());
	
#ifdef DEBUG_PRESSURE_SOLVE
	for (int i = 0; i < 6; ++i) {
		tmp_neighbor_solid[i] = ScalarGrid::create(0.0f);
		tmp_neighbor_solid[i]->setTransform(cell_transform);
		tmp_neighbor_fluid[i] = ScalarGrid::create(0.0f);
		tmp_neighbor_fluid[i]->setTransform(cell_transform);
		tmp_neighbor_empty[i] = ScalarGrid::create(0.0f);
		tmp_neighbor_empty[i]->setTransform(cell_transform);
	}
	
	ScalarGrid::Accessor acc_neighbor_solid[6] = {
	    tmp_neighbor_solid[0]->getAccessor(),
	    tmp_neighbor_solid[1]->getAccessor(),
	    tmp_neighbor_solid[2]->getAccessor(),
	    tmp_neighbor_solid[3]->getAccessor(),
	    tmp_neighbor_solid[4]->getAccessor(),
	    tmp_neighbor_solid[5]->getAccessor(),
	};
	ScalarGrid::Accessor acc_neighbor_fluid[6] = {
	    tmp_neighbor_fluid[0]->getAccessor(),
	    tmp_neighbor_fluid[1]->getAccessor(),
	    tmp_neighbor_fluid[2]->getAccessor(),
	    tmp_neighbor_fluid[3]->getAccessor(),
	    tmp_neighbor_fluid[4]->getAccessor(),
	    tmp_neighbor_fluid[5]->getAccessor(),
	};
	ScalarGrid::Accessor acc_neighbor_empty[6] = {
	    tmp_neighbor_empty[0]->getAccessor(),
	    tmp_neighbor_empty[1]->getAccessor(),
	    tmp_neighbor_empty[2]->getAccessor(),
	    tmp_neighbor_empty[3]->getAccessor(),
	    tmp_neighbor_empty[4]->getAccessor(),
	    tmp_neighbor_empty[5]->getAccessor(),
	};
#endif
	
//	float scale = 1.0f/cell_size();
	float scale = 1.0f;
	
	for (ScalarGrid::ValueOnCIter it = div_u->cbeginValueOn(); it; ++it) {
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
		for (int i = 0; i < 6; ++i) {
			const Coord &nc = neighbors[i];

			const bool is_solid = acc_solid.isValueOn(nc);
			const bool is_fluid = acc_fluid.isValueOn(nc);
			const bool is_empty = !is_solid && !is_fluid;
#ifdef DEBUG_PRESSURE_SOLVE
			acc_neighbor_solid[i].setValue(c, is_solid ? 1.0f : 0.0f);
			acc_neighbor_fluid[i].setValue(c, is_fluid ? 1.0f : 0.0f);
			acc_neighbor_empty[i].setValue(c, is_empty ? 1.0f : 0.0f);
#endif
			
			/* add matrix entries for interacting cells (non-solid neighbors) */
			if (!is_solid) {
				diag -= 1.0f;
			}
			
			if (is_fluid) {
				VIndex icol = index_tree->getValue(nc);
				if (icol != VINDEX_INVALID) {
					A.setValue(irow, icol, 1.0f);
				}
			}
			
			/* add background pressure terms */
			if (is_empty) {
				bg -= bg_pressure;
			}
		}
		
		/* XXX degenerate case (only solid neighbors), how to handle? */
		if (diag == 0.0f)
			diag = 1.0f;
		
		A.setValue(irow, irow, diag * scale);
		(*b)[irow] += bg;
	}
	assert(A.isFinite());
	
	/* preconditioner for faster convergence */
	pcg::JacobiPreconditioner<MatrixType> precond(A);
	
	/* solve A * x = B for x */
	MatrixType::VectorType x(rows, 0.0f);
	
	pcg::State terminator = pcg::terminationDefaults<float>();
	terminator.iterations = 100;
	terminator.relativeError = terminator.absoluteError = 1.0e-4;
	
	util::NullInterrupter interrupter;
	result = math::pcg::solve(A, *b, x, precond, interrupter, terminator);
	
	if (result.success) {
		q.setTree(tools::poisson::createTreeFromVector<float>(x, *index_tree, 0.0f));
	}
	else {
		q.clear();
	}
//	mul_grid_fl(q, cell_size());
	mul_grid_fl(q, scale);
	
#ifdef DEBUG_PRESSURE_SOLVE
	tmp_pressure = q.deepCopy();
#endif
	
	return result;
}

}  /* namespace internal */

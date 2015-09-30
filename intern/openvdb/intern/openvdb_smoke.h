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

#ifndef __OPENVDB_SMOKE_H__
#define __OPENVDB_SMOKE_H__

#include <openvdb/Types.h>
#include <openvdb/openvdb.h>
#include <openvdb/math/ConjGradient.h>
#include <openvdb/tools/PoissonSolver.h>

#include "openvdb_dense_convert.h"
#include "openvdb_capi.h"

namespace internal {

using openvdb::ScalarGrid;
using openvdb::VectorGrid;
using openvdb::Mat4R;
using openvdb::math::Transform;
using openvdb::math::Vec3s;
using openvdb::Vec3R;
using openvdb::Real;
using openvdb::Vec3f;
using openvdb::Vec3I;
using openvdb::Vec4I;
using openvdb::math::pcg::State;

class SmokeParticleList
{
public:
	typedef Vec3R value_type;
	struct Point {
		Point(const Vec3R &loc, const Real &rad, const Vec3R &vel) :
		    loc(loc), rad(rad), vel(vel)
		{}
		
		Vec3R loc;
		Real rad;
		Vec3R vel;
	};
	typedef std::vector<Point> PointList;
	typedef PointList::iterator iterator;
	typedef PointList::const_iterator const_iterator;
	
	struct PointAccessor {
		SmokeParticleList *list;
		Vec3f velocity;
		PointAccessor(SmokeParticleList *list, const Vec3f &velocity) : list(list), velocity(velocity) {}
		void add(const Vec3R &pos)
		{
			list->m_points.push_back(SmokeParticleList::Point(pos, 1.0f, velocity));
		}
	};

protected:
	PointList m_points;
	float m_radius_scale;
	float m_velocity_scale;

public:
	SmokeParticleList(float rscale=1, float vscale=1)
	    : m_radius_scale(rscale), m_velocity_scale(vscale)
	{
	}
	
	PointList &points() { return m_points; }
	const PointList &points() const { return m_points; }
	float radius_scale() const { return m_radius_scale; }
	void radius_scale(float radius_scale) { m_radius_scale = radius_scale; }
	float velocity_scale() const { return m_velocity_scale; }
	void velocity_scale(float velocity_scale) { m_velocity_scale = velocity_scale; }
	
	void from_stream(OpenVDBPointInputStream *stream);
	void to_stream(OpenVDBPointOutputStream *stream) const;
	
	void add_source(const Transform &cell_transform, const std::vector<Vec3s> &vertices, const std::vector<Vec3I> &triangles,
	                unsigned int seed, float points_per_voxel, const Vec3f &velocity);
	
	iterator begin() { return m_points.begin(); }
	iterator end() { return m_points.end(); }
	const_iterator begin() const { return m_points.begin(); }
	const_iterator end() const { return m_points.end(); }
	
	//////////////////////////////////////////////////////////////////////////////
	/// The methods below are the only ones required by tools::ParticleToLevelSet
	
	size_t size() const { return m_points.size(); }
	
	void getPos(size_t n, Vec3R &pos) const
	{
		pos = m_points[n].loc;
	}
	void getPosRad(size_t n, Vec3R& pos, Real& rad) const {
		pos = m_points[n].loc;
		rad = m_points[n].rad;
	}
	void getPosRadVel(size_t n, Vec3R& pos, Real& rad, Vec3R& vel) const {
		pos = m_points[n].loc;
		rad = m_points[n].rad;
		vel = m_points[n].vel;
	}
	// The method below is only required for attribute transfer
	void getAtt(size_t n, Vec3f& att) const
	{
		att = m_points[n].vel;
	}
};

struct SmokeData {
	typedef openvdb::tools::poisson::VIndex VIndex;
	typedef openvdb::ScalarTree::ValueConverter<VIndex>::Type VIndexTree;
	typedef openvdb::math::pcg::SparseStencilMatrix<float, 7> MatrixType;
	typedef MatrixType::VectorType VectorType;
	
	SmokeData(const Mat4R &cell_transform);
	~SmokeData();
	
	float cell_size() const;
	
	void add_gravity_force(VectorGrid &force);
	
	void add_obstacle(const std::vector<Vec3s> &vertices, const std::vector<Vec3I> &triangles);
	void clear_obstacles();
	void remove_border_velocity(VectorGrid &grid) const;
	void remove_obstacle_velocity(VectorGrid &grid) const;
	
	void set_gravity(const Vec3f &g);
	
	bool step(float dt);
	
	void init_grids();
	void update_points(float dt);
	
	enum AdvectionMode {
		ADVECT_1ST_ORDER = 1,
		ADVECT_2ND_ORDER = 2,
		ADVECT_SEMI_LAGRANGE = ADVECT_1ST_ORDER,
		ADVECT_MAC_CORMACK = ADVECT_2ND_ORDER,
	};
	
	void advect_velocity(float dt, AdvectionMode mode);
	void advect_density_field(float dt, AdvectionMode mode);
	
	ScalarGrid::Ptr calc_divergence();
	State solve_pressure_equation(const VectorGrid &u,
	                              const ScalarGrid &mask_fluid,
	                              const ScalarGrid &mask_solid,
	                              float bg_pressure,
	                              ScalarGrid &q);
	
	Vec3f gravity;
	
	Transform::Ptr cell_transform;
	ScalarGrid::Ptr density;
	ScalarGrid::Ptr obstacle;
	VectorGrid::Ptr velocity;
	
	SmokeParticleList points;
	
	State pressure_result;
	
	/* grid copies for display */
	ScalarGrid::Ptr tmp_divergence;
	ScalarGrid::Ptr tmp_divergence_new;
	ScalarGrid::Ptr tmp_pressure;
	VectorGrid::Ptr tmp_pressure_gradient;
	VectorGrid::Ptr tmp_force;
	ScalarGrid::Ptr tmp_neighbor_solid[6];
	ScalarGrid::Ptr tmp_neighbor_fluid[6];
	ScalarGrid::Ptr tmp_neighbor_empty[6];
	
	int debug_stage;
	float debug_scale;
};

}  /* namespace internal */

#endif /* __OPENVDB_SMOKE_H__ */

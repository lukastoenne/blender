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

#include <openvdb/openvdb.h>
#include <openvdb/math/ConjGradient.h>

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
		Vec3R loc;
		Real rad;
		Vec3R vel;
	};
	typedef std::vector<Point> PointList;
	typedef PointList::iterator iterator;
	typedef PointList::const_iterator const_iterator;

protected:
	PointList m_points;
	float m_radius_scale;
	float m_velocity_scale;

public:
	SmokeParticleList(float rscale=1, float vscale=1)
	    : m_radius_scale(rscale), m_velocity_scale(vscale)
	{
	}
	
	void from_stream(OpenVDBPointInputStream *stream)
	{
		m_points.clear();
		
		for (; stream->has_points(stream); stream->next_point(stream)) {
			Vec3f locf, velf;
			float rad;
			stream->get_point(stream, locf.asPointer(), &rad, velf.asPointer());
			
			Point pt;
			pt.loc = locf;
			pt.rad = rad * m_radius_scale;
			pt.vel = velf * m_velocity_scale;
			
			m_points.push_back(pt);
		}
	}
	
	void to_stream(OpenVDBPointOutputStream *stream) const
	{
		PointList::const_iterator it;
		for (it = m_points.begin(); stream->has_points(stream) && it != m_points.end(); stream->next_point(stream), ++it) {
			const Point &pt = *it;
			Vec3f locf = pt.loc;
			Vec3f velf = pt.vel;
			stream->set_point(stream, locf.asPointer(), velf.asPointer());
		}
	}
	
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

struct OpenVDBSmokeData {
	OpenVDBSmokeData(const Mat4R &cell_transform);
	~OpenVDBSmokeData();
	
	float cell_size() const;
	
	/* rasterize points into density and velocity grids (beginning of the time step) */
	void set_points(OpenVDBPointInputStream *stream) { points.from_stream(stream); }
	/* move particles through the velocity field (end of the time step) */
	void get_points(OpenVDBPointOutputStream *stream) { points.to_stream(stream); }
	
	void add_gravity_force(const Vec3f &g);
	void add_pressure_force(float dt, float bg_pressure);
	
//	void add_inflow(const std::vector<Vec3s> &vertices, const std::vector<Vec3I> &triangles,
//	                float flow_density, bool incremental);
	void add_obstacle(const std::vector<Vec3s> &vertices, const std::vector<Vec3I> &triangles);
	void clear_obstacles();
	
	bool step(float dt, int num_substeps);
	
	void init_grids();
	void update_points(float dt);
	void advect_backwards_trace(float dt);
	void calculate_pressure(float dt, float bg_pressure);
	
	Transform::Ptr cell_transform;
	ScalarGrid::Ptr density;
	VectorGrid::Ptr velocity;
	VectorGrid::Ptr velocity_old;
	ScalarGrid::Ptr pressure;
	State pressure_result;
	VectorGrid::Ptr force;
	SmokeParticleList points;
};

}  /* namespace internal */

#endif /* __OPENVDB_SMOKE_H__ */

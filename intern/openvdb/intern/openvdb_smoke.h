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
using openvdb::Vec3f;
using openvdb::Vec3I;
using openvdb::Vec4I;
using openvdb::math::pcg::State;

struct OpenVDBSmokeData {
	OpenVDBSmokeData(const Mat4R &cell_transform);
	~OpenVDBSmokeData();
	
	float cell_size() const;
	
	/* rasterize points into density and velocity grids (beginning of the time step) */
	void init_grids(OpenVDBPointInputStream *points);
	/* move particles through the velocity field (end of the time step) */
	void update_points(OpenVDBPointOutputStream *points);
	
	void add_gravity_force(const Vec3f &g);
	void add_pressure_force(float dt, float bg_pressure);
	
//	void add_inflow(const std::vector<Vec3s> &vertices, const std::vector<Vec3I> &triangles,
//	                float flow_density, bool incremental);
	void add_obstacle(const std::vector<Vec3s> &vertices, const std::vector<Vec3I> &triangles);
	void clear_obstacles();
	
	bool step(float dt, int num_substeps);
	
	void advect_backwards_trace(float dt);
	void calculate_pressure(float dt, float bg_pressure);
	
	Transform::Ptr cell_transform;
	ScalarGrid::Ptr density;
	VectorGrid::Ptr velocity;
	ScalarGrid::Ptr pressure;
	State pressure_result;
	VectorGrid::Ptr force;
};

}  /* namespace internal */

#endif /* __OPENVDB_SMOKE_H__ */

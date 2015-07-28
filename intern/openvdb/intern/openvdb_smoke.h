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

#include "openvdb_dense_convert.h"

namespace internal {

using openvdb::FloatGrid;
using openvdb::Mat4R;
using openvdb::math::Transform;
using openvdb::math::Vec3s;
using openvdb::Vec3I;

struct OpenVDBSmokeData {
	OpenVDBSmokeData(const Transform::Ptr &cell_transform);
	~OpenVDBSmokeData();
	
	void add_obstacle(const Mat4R &tfm, const std::vector<Vec3s> &vertices, const std::vector<Vec3I> &triangles);
	void clear_obstacles();
	
	bool step(float dt, int num_substeps);
	
	Transform cell_transform;
	FloatGrid::Ptr density;
};

}  /* namespace internal */

#endif /* __OPENVDB_SMOKE_H__ */

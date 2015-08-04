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

#include "openvdb_smoke.h"

namespace internal {

using namespace openvdb;
using namespace openvdb::math;

OpenVDBSmokeData::OpenVDBSmokeData(const Mat4R &cell_transform) :
    cell_transform(cell_transform)
{
}

OpenVDBSmokeData::~OpenVDBSmokeData()
{
}

void OpenVDBSmokeData::add_obstacle(const std::vector<Vec3s> &vertices, const std::vector<Vec3I> &triangles)
{
	Mat4R m = cell_transform;
	Transform::Ptr t = Transform::createLinearTransform(m);
	
	float bandwidth_ex = (float)LEVEL_SET_HALF_WIDTH;
	float bandwidth_in = (float)LEVEL_SET_HALF_WIDTH;
	density = tools::meshToSignedDistanceField<FloatGrid>(*t, vertices, triangles, std::vector<Vec4I>(), bandwidth_ex, bandwidth_in);
	BoolGrid::Ptr mask = tools::sdfInteriorMask<FloatGrid>(*density, 0.0f);
	density->topologyIntersection(*mask);
}

void OpenVDBSmokeData::clear_obstacles()
{
	if (density)
		density->clear();
}

bool OpenVDBSmokeData::step(float /*dt*/, int /*num_substeps*/)
{
	return true;
}

void OpenVDBSmokeData::get_bounds(float bbmin[3], float bbmax[3]) const
{
	if (!density) {
		bbmin[0] = bbmin[1] = bbmin[2] = 0.0f;
		bbmax[0] = bbmax[1] = bbmax[2] = 0.0f;
		return;
	}
	
	CoordBBox bbox = density->evalActiveVoxelBoundingBox();
	BBoxd vbox = density->transform().indexToWorld(bbox);
	vbox.min().toV(bbmin);
	vbox.max().toV(bbmax);
}

bool OpenVDBSmokeData::get_dense_texture_res(int res[3], float bbmin[3], float bbmax[3]) const
{
	if (!density)
		return false;
	
	CoordBBox bbox = density->evalActiveVoxelBoundingBox();
	res[0] = bbox.dim().x();
	res[1] = bbox.dim().y();
	res[2] = bbox.dim().z();
	
	BBoxd vbox = density->transform().indexToWorld(bbox);
	vbox.min().toV(bbmin);
	vbox.max().toV(bbmax);
	
	return res[0] > 0 && res[1] > 0 && res[2] > 0;
}

void OpenVDBSmokeData::create_dense_texture(float *buffer) const
{
	using namespace openvdb;
	using namespace openvdb::math;
	
	FloatGrid::ConstAccessor acc = density->getConstAccessor();
	
	CoordBBox bbox = density->evalActiveVoxelBoundingBox();
	
	Coord bbmin = bbox.min(), bbmax = bbox.max();
	size_t index = 0;
	Coord ijk;
	int &i = ijk[0], &j = ijk[1], &k = ijk[2];
	for (k = bbmin[2]; k <= bbmax[2]; ++k) {
		for (j = bbmin[1]; j <= bbmax[1]; ++j) {
			for (i = bbmin[0]; i <= bbmax[0]; ++i, ++index) {
				buffer[index] = acc.isValueOn(ijk) ? 1.0f : 0.0f;
			}
		}
	}
}

}  /* namespace internal */

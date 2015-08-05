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

#include "openvdb_smoke.h"
#include "openvdb_util.h"

namespace internal {

using namespace openvdb;
using namespace openvdb::math;

OpenVDBSmokeData::OpenVDBSmokeData(const Mat4R &cell_transform) :
    cell_transform(Transform::createLinearTransform(cell_transform))
{
	density = ScalarGrid::create(0.0f);
	density->setTransform(this->cell_transform);
	velocity = VectorGrid::create(Vec3f(0.0f, 0.0f, 0.0f));
	velocity->setTransform(this->cell_transform);
}

OpenVDBSmokeData::~OpenVDBSmokeData()
{
}

void OpenVDBSmokeData::add_obstacle(const std::vector<Vec3s> &vertices, const std::vector<Vec3I> &triangles)
{
	float bandwidth_ex = (float)LEVEL_SET_HALF_WIDTH;
	float bandwidth_in = (float)LEVEL_SET_HALF_WIDTH;
	FloatGrid::Ptr sdf = tools::meshToSignedDistanceField<FloatGrid>(*cell_transform, vertices, triangles, std::vector<Vec4I>(), bandwidth_ex, bandwidth_in);
	VectorGrid::Ptr grad = tools::gradient(*sdf);
	
	tools::compSum(*density, *sdf);
	tools::compSum(*velocity, *grad);
	
//	BoolGrid::Ptr mask = tools::sdfInteriorMask(*sdf, 0.0f);
//	density->topologyIntersection(*mask);
}

void OpenVDBSmokeData::clear_obstacles()
{
//	if (density)
//		density->clear();
}

bool OpenVDBSmokeData::step(float /*dt*/, int /*num_substeps*/)
{
	return true;
}

}  /* namespace internal */

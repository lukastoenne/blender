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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Kevin Dietrich, Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __OPENVDB_POINTS_CONVERT_H__
#define __OPENVDB_POINTS_CONVERT_H__

#include "openvdb_reader.h"
#include "openvdb_writer.h"
#include "openvdb_util.h"

#include <openvdb/tools/Clip.h>
#include <openvdb/tools/LevelSetUtil.h>
#include <openvdb/tools/ParticlesToLevelSet.h>

#include <cstdio>

#define TOLERANCE 1e-3f

namespace internal {

template <typename GridType, typename PointArray>
GridType *OpenVDB_export_points(
        OpenVDBWriter *writer,
        const openvdb::Name &name,
        float _mat[4][4],
        const openvdb::FloatGrid *mask,
        const PointArray &points, float voxel_size)
{
	using namespace openvdb;

	Mat4R mat = convertMatrix(_mat);
	math::Transform::Ptr transform = math::Transform::createLinearTransform(mat);

	const float half_width = openvdb::LEVEL_SET_HALF_WIDTH;
	typename GridType::Ptr grid = openvdb::createLevelSet<GridType>(voxel_size, half_width);
	tools::ParticlesToLevelSet<GridType> raster(*grid);
	
	raster.setGrainSize(1);//a value of zero disables threading
	raster.rasterizeSpheres(points);
	raster.finalize();
	
	tools::sdfToFogVolume(*grid);

	grid->setTransform(transform);

	/* Avoid clipping against an empty grid. */
	if (mask && !mask->tree().empty()) {
		grid = tools::clip(*grid, *mask);
	}

	grid->setName(name);
	grid->setIsInWorldSpace(false);
	grid->setVectorType(openvdb::VEC_INVARIANT);

	writer->insert(grid);

	return grid.get();
}

}  /* namespace internal */

#endif /* __OPENVDB_POINTS_CONVERT_H__ */

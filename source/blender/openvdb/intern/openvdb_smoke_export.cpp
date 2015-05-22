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
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <openvdb/openvdb.h>
#include <openvdb/tools/Dense.h>

#include "openvdb_capi.h"
#include "openvdb_intern.h"
#include "openvdb_writer.h"

#include "FLUID_3D.h"
#include "WTURBULENCE.h"

using namespace openvdb;

namespace internal {

static void OpenVDB_export_vector_grid(OpenVDBWriter *writer,
                                       const std::string &name,
                                       const float *data_x, const float *data_y, const float *data_z,
                                       const math::CoordBBox &bbox,
                                       const math::Transform::Ptr transform)
{
	math::Coord res = bbox.max();
	Vec3SGrid::Ptr grid = Vec3SGrid::create(Vec3s(0.0f));
	Vec3SGrid::Accessor acc = grid->getAccessor();

	int index = 0;
	for (int z = 0; z <= res.z(); ++z) {
		for (int y = 0; y <= res.y(); ++y) {
			for (int x = 0; x <= res.x(); ++x, ++index) {
				Coord xyz(x, y, z);
				Vec3s value(data_x[index], data_y[index], data_z[index]);
				acc.setValue(xyz, value);
			}
		}
	}

	grid->setName(name);
	grid->setTransform(transform->copy());
	grid->setIsInWorldSpace(false);

	writer->insert(grid);
}

template <typename GridType, typename T>
static void OpenVDB_import_grid(GridBase::Ptr &grid,
                                T *data,
                                const math::CoordBBox &bbox)
{
	typename GridType::Ptr grid_tmp = gridPtrCast<GridType>(grid);
	typename GridType::Accessor acc = grid_tmp->getAccessor();

	/* TODO(kevin): figure out why it doesn't really work */
	//tools::Dense<T, tools::LayoutXYZ> dense_grid(bbox);
	//tools::copyToDense(*grid_tmp, dense_grid);

	math::Coord res = bbox.max();
	int index = 0;
	for (int z = 0; z <= res.z(); ++z) {
		for (int y = 0; y <= res.y(); ++y) {
			for (int x = 0; x <= res.x(); ++x, ++index) {
				math::Coord xyz(x, y, z);
				data[index] = acc.getValue(xyz);
			}
		}
	}
}

static void OpenVDB_import_grid_vector(GridBase::Ptr &grid,
                                       float *data_x, float *data_y, float *data_z,
                                       const math::CoordBBox &bbox)
{
	math::Coord res = bbox.max();
	Vec3SGrid::Ptr vgrid = gridPtrCast<Vec3SGrid>(grid);
	Vec3SGrid::Accessor acc = vgrid->getAccessor();

	int index = 0;
	for (int z = 0; z <= res.z(); ++z) {
		for (int y = 0; y <= res.y(); ++y) {
			for (int x = 0; x <= res.x(); ++x, ++index) {
				math::Coord xyz(x, y, z);
				Vec3s val = acc.getValue(xyz);
				data_x[index] = val.x();
				data_y[index] = val.y();
				data_z[index] = val.z();
			}
		}
	}
}


void OpenVDB_update_fluid_transform(const char *filename, FluidDomainDescr descr)
{
	/* TODO(kevin): deduplicate this call */
	initialize();

	Mat4R fluid_mat = Mat4R(
	        descr.fluidmat[0][0], descr.fluidmat[0][1], descr.fluidmat[0][2], descr.fluidmat[0][3],
	        descr.fluidmat[1][0], descr.fluidmat[1][1], descr.fluidmat[1][2], descr.fluidmat[1][3],
	        descr.fluidmat[2][0], descr.fluidmat[2][1], descr.fluidmat[2][2], descr.fluidmat[2][3],
	        descr.fluidmat[3][0], descr.fluidmat[3][1], descr.fluidmat[3][2], descr.fluidmat[3][3]);

	Mat4R fluid_matBig = Mat4R(
	        descr.fluidmathigh[0][0], descr.fluidmathigh[0][1], descr.fluidmathigh[0][2], descr.fluidmathigh[0][3],
	        descr.fluidmathigh[1][0], descr.fluidmathigh[1][1], descr.fluidmathigh[1][2], descr.fluidmathigh[1][3],
	        descr.fluidmathigh[2][0], descr.fluidmathigh[2][1], descr.fluidmathigh[2][2], descr.fluidmathigh[2][3],
	        descr.fluidmathigh[3][0], descr.fluidmathigh[3][1], descr.fluidmathigh[3][2], descr.fluidmathigh[3][3]);

	math::Transform::Ptr transform = math::Transform::createLinearTransform(fluid_mat);
	math::Transform::Ptr transformBig = math::Transform::createLinearTransform(fluid_matBig);

	io::File file(filename);
	file.open();
	GridPtrVecPtr grids = file.getGrids();
	GridBase::Ptr grid;

	for (size_t i = 0; i < grids->size(); ++i) {
		grid = (*grids)[i];

		const std::string name = grid->getName();
		size_t found = name.find("High");

		if (found != std::string::npos) {
			grid->setTransform(transformBig);
		}
		else {
			grid->setTransform(transform);
		}
	}

	file.close();
}

} // namespace internal

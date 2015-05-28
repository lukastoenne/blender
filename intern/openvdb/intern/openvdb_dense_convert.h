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

#ifndef __OPENVDB_DENSE_CONVERT_H__
#define __OPENVDB_DENSE_CONVERT_H__

#include <openvdb/tools/Dense.h>

#include "openvdb_reader.h"
#include "openvdb_writer.h"

namespace internal {

template <typename GridType, typename T>
void OpenVDB_export_grid(OpenVDBWriter *writer,
                         const std::string &name,
                         const T *data,
                         const int res[3],
                         float fluid_mat[4][4])
{
	using namespace openvdb;

	math::CoordBBox bbox(Coord(0), Coord(res[0] - 1, res[1] - 1, res[2] - 1));

	Mat4R mat = Mat4R(
		  fluid_mat[0][0], fluid_mat[0][1], fluid_mat[0][2], fluid_mat[0][3],
          fluid_mat[1][0], fluid_mat[1][1], fluid_mat[1][2], fluid_mat[1][3],
          fluid_mat[2][0], fluid_mat[2][1], fluid_mat[2][2], fluid_mat[2][3],
          fluid_mat[3][0], fluid_mat[3][1], fluid_mat[3][2], fluid_mat[3][3]);

	math::Transform::Ptr transform = math::Transform::createLinearTransform(mat);

	typename GridType::Ptr grid = GridType::create(T(0));

	tools::Dense<const T, openvdb::tools::LayoutXYZ> dense_grid(bbox, data);
	tools::copyFromDense(dense_grid, grid->tree(), 1e-3f, true);

	grid->setName(name);
	grid->setTransform(transform);
	grid->setIsInWorldSpace(false);

	writer->insert(grid);
}

template <typename GridType, typename T>
void OpenVDB_import_grid(OpenVDBReader *reader,
                         const std::string &name,
                         T **data,
                         const int res[3])
{
	using namespace openvdb;

	typename GridType::Ptr grid_tmp = gridPtrCast<GridType>(reader->getGrid(name));
#if 0
	math::CoordBBox bbox(Coord(0), Coord(res[0] - 1, res[1] - 1, res[2] - 1));

	tools::Dense<T, tools::LayoutXYZ> dense_grid(bbox);
	tools::copyToDense(*grid_tmp, dense_grid);
	memcpy(*data, dense_grid.data(), sizeof(T) * res[0] * res[1] * res[2]);
#else
	typename GridType::Accessor acc = grid_tmp->getAccessor();
	math::Coord xyz;
	int &x = xyz[0], &y = xyz[1], &z = xyz[2];

	int index = 0;
	for (z = 0; z < res[2]; ++z) {
		for (y = 0; y < res[1]; ++y) {
			for (x = 0; x < res[0]; ++x, ++index) {
				(*data)[index] = acc.getValue(xyz);
			}
		}
	}
#endif
}

void OpenVDB_export_vector_grid(OpenVDBWriter *writer,
                                const std::string &name,
                                const float *data_x, const float *data_y, const float *data_z,
                                const int res[3],
                                float fluid_mat[4][4],
                                openvdb::VecType vec_type);


void OpenVDB_import_grid_vector(OpenVDBReader *reader,
                                const std::string &name,
                                float **data_x, float **data_y, float **data_z,
                                const int res[3]);

void OpenVDB_update_fluid_transform(const char *filename,
                                    float matrix[4][4],
                                    float matrix_high[4][4]);

}

#endif /* __OPENVDB_DENSE_CONVERT_H__ */


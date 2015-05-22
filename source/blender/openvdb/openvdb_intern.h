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
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef  __OPENVDB_INTERN_H__
#define  __OPENVDB_INTERN_H__

#include <openvdb/openvdb.h>
#include <openvdb/tools/Dense.h>

#include "openvdb_reader.h"
#include "openvdb_writer.h"

struct FLUID_3D;
struct FluidDomainDescr;
struct WTURBULENCE;
struct OpenVDBWriter;

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
//	typename GridType::Accessor acc = grid_tmp->getAccessor();

	math::CoordBBox bbox(Coord(0), Coord(res[0] - 1, res[1] - 1, res[2] - 1));

	tools::Dense<T, tools::LayoutXYZ> dense_grid(bbox);
	tools::copyToDense(*grid_tmp, dense_grid);

	*data = dense_grid.data();

//	int index = 0;
//	for (int z = 0; z <= res.z(); ++z) {
//		for (int y = 0; y <= res.y(); ++y) {
//			for (int x = 0; x <= res.x(); ++x, ++index) {
//				math::Coord xyz(x, y, z);
//				data[index] = acc.getValue(xyz);
//			}
//		}
//	}
}

void OpenVDB_update_fluid_transform(const char *filename, FluidDomainDescr descr);

}

#endif /* __OPENVDB_INTERN_H__ */


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

#include <openvdb/tools/ValueTransformer.h>

#include "openvdb_capi.h"
#include "openvdb_dense_convert.h"
#include "openvdb_writer.h"

using namespace openvdb;

namespace internal {

class MergeScalarGrids {
	tree::ValueAccessor<const FloatTree> m_acc_x, m_acc_y, m_acc_z;

public:
	MergeScalarGrids(const FloatTree *x_tree, const FloatTree *y_tree, const FloatTree *z_tree)
	    : m_acc_x(*x_tree)
	    , m_acc_y(*y_tree)
	    , m_acc_z(*z_tree)
	{}

	MergeScalarGrids(const MergeScalarGrids &other)
	    : m_acc_x(other.m_acc_x)
	    , m_acc_y(other.m_acc_y)
	    , m_acc_z(other.m_acc_z)
	{}

	void operator()(const Vec3STree::ValueOnIter &it) const
	{
		const math::Coord xyz = it.getCoord();
		float x = m_acc_x.getValue(xyz);
		float y = m_acc_y.getValue(xyz);
		float z = m_acc_z.getValue(xyz);

		it.setValue(Vec3s(x, y, z));
	}
};

GridBase *OpenVDB_export_vector_grid(OpenVDBWriter *writer,
                                     const std::string &name,
                                     const float *data_x, const float *data_y, const float *data_z,
                                     const int res[3],
                                     float fluid_mat[4][4],
                                     VecType vec_type,
                                     const bool is_color,
                                     const FloatGrid *mask)
{

	math::CoordBBox bbox(Coord(0), Coord(res[0] - 1, res[1] - 1, res[2] - 1));

	Mat4R mat = Mat4R(
	    fluid_mat[0][0], fluid_mat[0][1], fluid_mat[0][2], fluid_mat[0][3],
	    fluid_mat[1][0], fluid_mat[1][1], fluid_mat[1][2], fluid_mat[1][3],
	    fluid_mat[2][0], fluid_mat[2][1], fluid_mat[2][2], fluid_mat[2][3],
	    fluid_mat[3][0], fluid_mat[3][1], fluid_mat[3][2], fluid_mat[3][3]);

	math::Transform::Ptr transform = math::Transform::createLinearTransform(mat);

	FloatGrid::Ptr grid[3];

	grid[0] = FloatGrid::create(0.0f);
	tools::Dense<const float, tools::LayoutXYZ> dense_grid_x(bbox, data_x);
	tools::copyFromDense(dense_grid_x, grid[0]->tree(), TOLERANCE);

	grid[1] = FloatGrid::create(0.0f);
	tools::Dense<const float, tools::LayoutXYZ> dense_grid_y(bbox, data_y);
	tools::copyFromDense(dense_grid_y, grid[1]->tree(), TOLERANCE);

	grid[2] = FloatGrid::create(0.0f);
	tools::Dense<const float, tools::LayoutXYZ> dense_grid_z(bbox, data_z);
	tools::copyFromDense(dense_grid_z, grid[2]->tree(), TOLERANCE);

	Vec3SGrid::Ptr vecgrid = Vec3SGrid::create(Vec3s(0.0f));

	/* Activate voxels in the vector grid based on the scalar grids to ensure
	 * thread safety later on */
	for (int i = 0; i < 3; ++i) {
		vecgrid->tree().topologyUnion(grid[i]->tree());
	}

	MergeScalarGrids op(&(grid[0]->tree()), &(grid[1]->tree()), &(grid[2]->tree()));
	tools::foreach(vecgrid->beginValueOn(), op, true, false);

	vecgrid->setTransform(transform);

	if (mask) {
		vecgrid = tools::clip(*vecgrid, *mask);
	}

	vecgrid->setName(name);
	vecgrid->setIsInWorldSpace(false);
	vecgrid->setVectorType(vec_type);
	vecgrid->insertMeta("is_color", BoolMetadata(is_color));
	vecgrid->setGridClass(GRID_STAGGERED);

	writer->insert(vecgrid);

	return vecgrid.get();
}

void OpenVDB_import_grid_vector(OpenVDBReader *reader,
                                const std::string &name,
                                float **data_x, float **data_y, float **data_z,
                                const int res[3])
{
	Vec3SGrid::Ptr vgrid = gridPtrCast<Vec3SGrid>(reader->getGrid(name));
	Vec3SGrid::Accessor acc = vgrid->getAccessor();
	math::Coord xyz;
	int &x = xyz[0], &y = xyz[1], &z = xyz[2];

	int index = 0;
	for (z = 0; z < res[2]; ++z) {
		for (y = 0; y < res[1]; ++y) {
			for (x = 0; x < res[0]; ++x, ++index) {
				math::Vec3s value = acc.getValue(xyz);
				(*data_x)[index] = value.x();
				(*data_y)[index] = value.y();
				(*data_z)[index] = value.z();
			}
		}
	}
}

void OpenVDB_update_fluid_transform(const char *filename, float matrix[4][4], float matrix_high[4][4])
{
	/* TODO(kevin): deduplicate this call */
	initialize();

	Mat4R fluid_mat = Mat4R(
	        matrix[0][0], matrix[0][1], matrix[0][2], matrix[0][3],
	        matrix[1][0], matrix[1][1], matrix[1][2], matrix[1][3],
	        matrix[2][0], matrix[2][1], matrix[2][2], matrix[2][3],
	        matrix[3][0], matrix[3][1], matrix[3][2], matrix[3][3]);

	Mat4R fluid_matBig = Mat4R(
	        matrix_high[0][0], matrix_high[0][1], matrix_high[0][2], matrix_high[0][3],
	        matrix_high[1][0], matrix_high[1][1], matrix_high[1][2], matrix_high[1][3],
	        matrix_high[2][0], matrix_high[2][1], matrix_high[2][2], matrix_high[2][3],
	        matrix_high[3][0], matrix_high[3][1], matrix_high[3][2], matrix_high[3][3]);

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

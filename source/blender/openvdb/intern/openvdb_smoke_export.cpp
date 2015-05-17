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

#include "FLUID_3D.h"
#include "WTURBULENCE.h"

using namespace openvdb;

namespace internal {

template <typename GridType, typename T>
static void OpenVDB_export_grid(GridPtrVec &gridVec,
                                const std::string &name,
                                const T *data,
                                const T bg,
                                const math::CoordBBox &bbox,
                                const math::Transform::Ptr transform)
{
	typename GridType::Ptr grid = GridType::create(bg);

	tools::Dense<const T, tools::LayoutXYZ> dense_grid(bbox, data);
	tools::copyFromDense(dense_grid, grid->tree(), 1e-3f, true);

	grid->setName(name);
	grid->setGridClass(GRID_FOG_VOLUME);
	grid->setTransform(transform->copy());
	grid->setIsInWorldSpace(false);

	gridVec.push_back(grid);
}

static void OpenVDB_export_vector_grid(GridPtrVec &gridVec,
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
	grid->setGridClass(GRID_FOG_VOLUME);
	grid->setTransform(transform->copy());
	grid->setIsInWorldSpace(false);

	gridVec.push_back(grid);
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

static MetaMap getSimMetaMap(FLUID_3D *fluid, FluidDomainDescr descr)
{
	MetaMap sim_data;

	sim_data.insertMeta("fluid_fields", Int32Metadata(descr.fluid_fields));
	sim_data.insertMeta("active_fields", Int32Metadata(descr.active_fields));
	sim_data.insertMeta("resolution", Vec3IMetadata(Vec3I(fluid->_res[0], fluid->_res[1], fluid->_res[2])));
	sim_data.insertMeta("max_resolution", Int32Metadata(fluid->_maxRes)); // also res_min, res_base?
	sim_data.insertMeta("delta_x", FloatMetadata(fluid->_dx));
	sim_data.insertMeta("delta_t", FloatMetadata(fluid->_dt));
	sim_data.insertMeta("shift", Vec3IMetadata(Vec3I(descr.shift)));
	sim_data.insertMeta("obj_shift_f", Vec3SMetadata(Vec3s(descr.obj_shift_f)));

	Mat4s obj_mat = Mat4s(
			descr.obmat[0][0], descr.obmat[0][1], descr.obmat[0][2], descr.obmat[0][3],
	        descr.obmat[1][0], descr.obmat[1][1], descr.obmat[1][2], descr.obmat[1][3],
	        descr.obmat[2][0], descr.obmat[2][1], descr.obmat[2][2], descr.obmat[2][3],
	        descr.obmat[3][0], descr.obmat[3][1], descr.obmat[3][2], descr.obmat[3][3]);

	sim_data.insertMeta("obmat", Mat4SMetadata(obj_mat));
	sim_data.insertMeta("active_color", Vec3SMetadata(Vec3s(descr.active_color)));

	return sim_data;
}

void OpenVDB_export_fluid(FLUID_3D *fluid, WTURBULENCE *wt, FluidDomainDescr descr, const char *filename, float *shadow)
{
	Mat4R fluid_mat = Mat4R(
		  descr.fluidmat[0][0], descr.fluidmat[0][1], descr.fluidmat[0][2], descr.fluidmat[0][3],
          descr.fluidmat[1][0], descr.fluidmat[1][1], descr.fluidmat[1][2], descr.fluidmat[1][3],
          descr.fluidmat[2][0], descr.fluidmat[2][1], descr.fluidmat[2][2], descr.fluidmat[2][3],
          descr.fluidmat[3][0], descr.fluidmat[3][1], descr.fluidmat[3][2], descr.fluidmat[3][3]);

	GridPtrVec gridVec;
	math::CoordBBox bbox(Coord(0), Coord(fluid->_xRes - 1, fluid->_yRes - 1, fluid->_zRes - 1));

	math::Transform::Ptr transform = math::Transform::createLinearTransform(fluid_mat);

	OpenVDB_export_grid<FloatGrid>(gridVec, "Shadow", shadow, 0.0f, bbox, transform);
	OpenVDB_export_grid<FloatGrid>(gridVec, "Density", fluid->_density, 0.0f, bbox, transform);

	if (fluid->_heat) {
		OpenVDB_export_grid<FloatGrid>(gridVec, "Heat", fluid->_heat, 0.0f, bbox, transform);
		OpenVDB_export_grid<FloatGrid>(gridVec, "Heat Old", fluid->_heatOld, 0.0f, bbox, transform);
	}

	if (fluid->_flame) {
		OpenVDB_export_grid<FloatGrid>(gridVec, "Flame", fluid->_flame, 0.0f, bbox, transform);
		OpenVDB_export_grid<FloatGrid>(gridVec, "Fuel", fluid->_fuel, 0.0f, bbox, transform);
		OpenVDB_export_grid<FloatGrid>(gridVec, "React", fluid->_react, 0.0f, bbox, transform);
	}

	if (fluid->_color_r) {
		OpenVDB_export_vector_grid(gridVec, "Color", fluid->_color_r, fluid->_color_g, fluid->_color_b, bbox, transform);
	}

	OpenVDB_export_vector_grid(gridVec, "Velocity", fluid->_xVelocity, fluid->_yVelocity, fluid->_zVelocity, bbox, transform);
	OpenVDB_export_grid<Int32Grid>(gridVec, "Obstacles", fluid->_obstacles, (unsigned char)0, bbox, transform);

	if (wt) {
		Mat4R fluid_matBig = Mat4R(
			  descr.fluidmathigh[0][0], descr.fluidmathigh[0][1], descr.fluidmathigh[0][2], descr.fluidmathigh[0][3],
	          descr.fluidmathigh[1][0], descr.fluidmathigh[1][1], descr.fluidmathigh[1][2], descr.fluidmathigh[1][3],
	          descr.fluidmathigh[2][0], descr.fluidmathigh[2][1], descr.fluidmathigh[2][2], descr.fluidmathigh[2][3],
	          descr.fluidmathigh[3][0], descr.fluidmathigh[3][1], descr.fluidmathigh[3][2], descr.fluidmathigh[3][3]);

		math::Transform::Ptr transformBig = math::Transform::createLinearTransform(fluid_matBig);
		math::CoordBBox bboxBig(Coord(0), Coord(wt->_xResBig - 1, wt->_yResBig - 1, wt->_zResBig - 1));

		OpenVDB_export_grid<FloatGrid>(gridVec, "Density High", wt->_densityBig, 0.0f, bboxBig, transformBig);

		if (wt->_flameBig) {
			OpenVDB_export_grid<FloatGrid>(gridVec, "Flame High", wt->_flameBig, 0.0f, bboxBig, transformBig);
			OpenVDB_export_grid<FloatGrid>(gridVec, "Fuel High", wt->_fuelBig, 0.0f, bboxBig, transformBig);
			OpenVDB_export_grid<FloatGrid>(gridVec, "React High", wt->_reactBig, 0.0f, bboxBig, transformBig);
		}

		if (wt->_color_rBig) {
			OpenVDB_export_vector_grid(gridVec, "Color High", wt->_color_rBig, wt->_color_gBig, wt->_color_bBig, bboxBig, transformBig);
		}

		OpenVDB_export_vector_grid(gridVec, "Texture Coordinates", wt->_tcU, wt->_tcV, wt->_tcW, bbox, transform);
	}

	MetaMap simData = getSimMetaMap(fluid, descr);

	io::File file(filename);

	file.setCompression(io::COMPRESS_ACTIVE_MASK | io::COMPRESS_ZIP);

	file.write(gridVec, simData);
	file.close();

	gridVec.clear();
}

static void readSimMetaMap(MetaMap::Ptr sim_data, FLUID_3D *fluid, FluidDomainDescr *descr)
{
	descr->fluid_fields = sim_data->metaValue<Int32>("fluid_fields");
	descr->active_fields = sim_data->metaValue<Int32>("active_fields");
//	Vec3I res = sim_data->metaValue<Vec3I>("resolution");
	//fluid->_res = Vec3Int(res[0], res[1], res[2]);
	fluid->_maxRes = sim_data->metaValue<Int32>("max_resolution");
	fluid->_dx = sim_data->metaValue<float>("delta_x");
	fluid->_dt = sim_data->metaValue<float>("delta_t");

	/* TODO (kevin) */
	//	Vec3I shift = grid->metaValue<Vec3I>("shift");
	//	Vec3s obj_shift_f = grid->metaValue<Vec3s>("obj_shift_f");
	//	Mat4s obj_mat = grid->metaValue<Mat4s>("descr.obmat");
	//	Vec3s active_color = grid->metaValue<Vec3s>("active_color");
}

void OpenVDB_import_fluid(FLUID_3D *fluid, WTURBULENCE *wt, FluidDomainDescr *descr, const char *filename, float *shadow)
{
	/* TODO(kevin): deduplicate this call */
	initialize();
	io::File file(filename);

	file.open();

	readSimMetaMap(file.getMetadata(), fluid, descr);

	math::CoordBBox bbox(Coord(0), Coord(fluid->_xRes - 1, fluid->_yRes - 1, fluid->_zRes - 1));
	printf("Import resolution: %d, %d, %d\n", fluid->_xRes, fluid->_yRes, fluid->_zRes);

	GridBase::Ptr grid = file.readGrid("Shadow");
	OpenVDB_import_grid<FloatGrid>(grid, shadow, bbox);

	grid = file.readGrid("Density");
	OpenVDB_import_grid<FloatGrid>(grid, fluid->_density, bbox);

	if (fluid->_heat) {
		grid = file.readGrid("Heat");
		OpenVDB_import_grid<FloatGrid>(grid, fluid->_heat, bbox);
		grid = file.readGrid("Heat Old");
		OpenVDB_import_grid<FloatGrid>(grid, fluid->_heatOld, bbox);
	}

	if (fluid->_flame) {
		grid = file.readGrid("Flame");
		OpenVDB_import_grid<FloatGrid>(grid, fluid->_flame, bbox);
		grid = file.readGrid("Fuel");
		OpenVDB_import_grid<FloatGrid>(grid, fluid->_fuel, bbox);
		grid = file.readGrid("React");
		OpenVDB_import_grid<FloatGrid>(grid, fluid->_react, bbox);
	}

	if (fluid->_color_r) {
		grid = file.readGrid("Color");
		OpenVDB_import_grid_vector(grid, fluid->_color_r, fluid->_color_g, fluid->_color_b, bbox);
	}

	grid = file.readGrid("Velocity");
	OpenVDB_import_grid_vector(grid, fluid->_xVelocity, fluid->_zVelocity, fluid->_zVelocity, bbox);

	grid = file.readGrid("Obstacles");
	OpenVDB_import_grid<Int32Grid>(grid, fluid->_obstacles, bbox);


	if (wt) {
		math::CoordBBox bboxBig(Coord(0), Coord(wt->_xResBig - 1, wt->_yResBig - 1, wt->_zResBig - 1));

		grid = file.readGrid("Density High");
		OpenVDB_import_grid<FloatGrid>(grid, wt->_densityBig, bboxBig);

		if (wt->_flameBig) {
			grid = file.readGrid("Flame High");
			OpenVDB_import_grid<FloatGrid>(grid, wt->_flameBig, bboxBig);
			grid = file.readGrid("Fuel High");
			OpenVDB_import_grid<FloatGrid>(grid, wt->_fuelBig, bboxBig);
			grid = file.readGrid("React High");
			OpenVDB_import_grid<FloatGrid>(grid, wt->_reactBig, bboxBig);
		}

		if (wt->_color_rBig) {
			grid = file.readGrid("Color High");
			OpenVDB_import_grid_vector(grid, wt->_color_rBig, wt->_color_gBig, wt->_color_bBig, bboxBig);
		}

		grid = file.readGrid("Texture Coordinates");
		OpenVDB_import_grid_vector(grid, wt->_tcU, wt->_tcV, wt->_tcW, bbox);
	}

	file.close();
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

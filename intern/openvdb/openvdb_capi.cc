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

#include "MEM_guardedalloc.h"

#include "openvdb_capi.h"
#include "openvdb_dense_convert.h"
#include "openvdb_smoke.h"
#include "openvdb_util.h"

struct OpenVDBFloatGrid { int unused; };
struct OpenVDBIntGrid { int unused; };
struct OpenVDBVectorGrid { int unused; };

int OpenVDB_getVersionHex()
{
    return openvdb::OPENVDB_LIBRARY_VERSION;
}

void OpenVDB_get_grid_info(const char *filename, OpenVDBGridInfoCallback cb, void *userdata)
{
	Timer(__func__);

	using namespace openvdb;

	initialize();

	io::File file(filename);
	file.open();

	GridPtrVecPtr grids = file.getGrids();
	int grid_num = grids->size();

	for (size_t i = 0; i < grid_num; ++i) {
		GridBase::ConstPtr grid = (*grids)[i];

		Name name = grid->getName();
		Name value_type = grid->valueType();
		bool is_color = false;
		if (grid->getMetadata< TypedMetadata<bool> >("is_color"))
			is_color = grid->metaValue<bool>("is_color");

		cb(userdata, name.c_str(), value_type.c_str(), is_color);
	}
}

OpenVDBFloatGrid *OpenVDB_export_grid_fl(OpenVDBWriter *writer,
                                         const char *name, float *data,
                                         const int res[3], float matrix[4][4],
                                         OpenVDBFloatGrid *mask)
{
	Timer(__func__);

	using namespace openvdb;

	OpenVDBFloatGrid *grid =
	        (OpenVDBFloatGrid *)internal::OpenVDB_export_grid<FloatGrid>(writer, name, data, res, matrix, (FloatGrid *)mask);
	return grid;
}

OpenVDBIntGrid *OpenVDB_export_grid_ch(OpenVDBWriter *writer,
                                       const char *name, unsigned char *data,
                                       const int res[3], float matrix[4][4],
                                       OpenVDBFloatGrid *mask)
{
	Timer(__func__);

	using namespace openvdb;

	OpenVDBIntGrid *grid =
	        (OpenVDBIntGrid *)internal::OpenVDB_export_grid<Int32Grid>(writer, name, data, res, matrix, (FloatGrid *)mask);
	return grid;
}

OpenVDBVectorGrid *OpenVDB_export_grid_vec(struct OpenVDBWriter *writer,
                                           const char *name,
                                           const float *data_x, const float *data_y, const float *data_z,
                                           const int res[3], float matrix[4][4], short vec_type,
                                           const bool is_color, OpenVDBFloatGrid *mask)
{
	Timer(__func__);

	using namespace openvdb;

	OpenVDBVectorGrid *grid =
	(OpenVDBVectorGrid *)internal::OpenVDB_export_vector_grid(writer, name,
	                                     data_x, data_y, data_z, res, matrix,
	                                     static_cast<VecType>(vec_type),
	                                     is_color, (FloatGrid *)mask);
	return grid;
}

void OpenVDB_import_grid_fl(OpenVDBReader *reader,
                            const char *name, float **data,
                            const int res[3])
{
	Timer(__func__);

	internal::OpenVDB_import_grid<openvdb::FloatGrid>(reader, name, data, res);
}

void OpenVDB_import_grid_ch(OpenVDBReader *reader,
                            const char *name, unsigned char **data,
                            const int res[3])
{
	internal::OpenVDB_import_grid<openvdb::Int32Grid>(reader, name, data, res);
}

void OpenVDB_import_grid_vec(struct OpenVDBReader *reader,
                             const char *name,
                             float **data_x, float **data_y, float **data_z,
                             const int res[3])
{
	Timer(__func__);

	internal::OpenVDB_import_grid_vector(reader, name, data_x, data_y, data_z, res);
}

OpenVDBWriter *OpenVDBWriter_create()
{
	return new OpenVDBWriter();
}

void OpenVDBWriter_free(OpenVDBWriter *writer)
{
	delete writer;
	writer = NULL;
}

void OpenVDBWriter_set_flags(OpenVDBWriter *writer, const int flag, const bool half)
{
	using namespace openvdb;

	int compression_flags = io::COMPRESS_ACTIVE_MASK;

	if (flag == 0) {
		compression_flags |= io::COMPRESS_ZIP;
	}
	else if (flag == 1) {
		compression_flags |= io::COMPRESS_BLOSC;
	}
	else {
		compression_flags = io::COMPRESS_NONE;
	}

	writer->setFlags(compression_flags, half);
}

void OpenVDBWriter_add_meta_fl(OpenVDBWriter *writer, const char *name, const float value)
{
	writer->insertFloatMeta(name, value);
}

void OpenVDBWriter_add_meta_int(OpenVDBWriter *writer, const char *name, const int value)
{
	writer->insertIntMeta(name, value);
}

void OpenVDBWriter_add_meta_v3(OpenVDBWriter *writer, const char *name, const float value[3])
{
	writer->insertVec3sMeta(name, value);
}

void OpenVDBWriter_add_meta_v3_int(OpenVDBWriter *writer, const char *name, const int value[3])
{
	writer->insertVec3IMeta(name, value);
}

void OpenVDBWriter_add_meta_mat4(OpenVDBWriter *writer, const char *name, float value[4][4])
{
	writer->insertMat4sMeta(name, value);
}

void OpenVDBWriter_write(OpenVDBWriter *writer, const char *filename)
{
	writer->write(filename);
}

OpenVDBReader *OpenVDBReader_create()
{
	return new OpenVDBReader();
}

void OpenVDBReader_free(OpenVDBReader *reader)
{
	delete reader;
	reader = NULL;
}

void OpenVDBReader_open(OpenVDBReader *reader, const char *filename)
{
	reader->open(filename);
}

void OpenVDBReader_get_meta_fl(OpenVDBReader *reader, const char *name, float *value)
{
	reader->floatMeta(name, *value);
}

void OpenVDBReader_get_meta_int(OpenVDBReader *reader, const char *name, int *value)
{
	reader->intMeta(name, *value);
}

void OpenVDBReader_get_meta_v3(OpenVDBReader *reader, const char *name, float value[3])
{
	reader->vec3sMeta(name, value);
}

void OpenVDBReader_get_meta_v3_int(OpenVDBReader *reader, const char *name, int value[3])
{
	reader->vec3IMeta(name, value);
}

void OpenVDBReader_get_meta_mat4(OpenVDBReader *reader, const char *name, float value[4][4])
{
	reader->mat4sMeta(name, value);
}

/* ------------------------------------------------------------------------- */
/* Simulation */

struct OpenVDBSmokeData *OpenVDB_create_smoke_data(float cell_mat[4][4])
{
	return (OpenVDBSmokeData *)(new internal::SmokeData(internal::convertMatrix(cell_mat)));
}

void OpenVDB_free_smoke_data(struct OpenVDBSmokeData *data)
{
	delete ((internal::SmokeData *)data);
}

void OpenVDB_smoke_set_points(struct OpenVDBSmokeData *pdata, struct OpenVDBPointInputStream *points)
{
	internal::SmokeData *data = (internal::SmokeData *)pdata;
	data->set_points(points);
}

void OpenVDB_smoke_get_points(struct OpenVDBSmokeData *pdata, struct OpenVDBPointOutputStream *points)
{
	internal::SmokeData *data = (internal::SmokeData *)pdata;
	data->get_points(points);
}

static void get_mesh_geometry(float mat[4][4], OpenVDBMeshIterator *it,
                              std::vector<openvdb::math::Vec3s> &vertices, std::vector<openvdb::Vec3I> &triangles)
{
	using openvdb::math::Vec3s;
	using openvdb::Vec3I;
	using openvdb::Vec4I;
	using openvdb::Mat4R;
	
	Mat4R M = internal::convertMatrix(mat);
	
	for (; it->has_vertices(it); it->next_vertex(it)) {
		float co[3];
		it->get_vertex(it, co);
		Vec3s v(co[0], co[1], co[2]);
		vertices.push_back(M.transform(v));
	}
	
	for (; it->has_triangles(it); it->next_triangle(it)) {
		int a, b, c;
		it->get_triangle(it, &a, &b, &c);
		assert(a < vertices.size() && b < vertices.size() && c < vertices.size());
		triangles.push_back(Vec3I(a, b, c));
	}
}

#if 0
void OpenVDB_smoke_add_inflow(struct OpenVDBSmokeData *data, float mat[4][4], struct OpenVDBMeshIterator *it,
                              float flow_density, bool incremental)
{
	using openvdb::math::Vec3s;
	using openvdb::Vec3I;
	
	std::vector<Vec3s> vertices;
	std::vector<Vec3I> triangles;
	get_mesh_geometry(mat, it, vertices, triangles);
	
	((internal::OpenVDBSmokeData *)data)->add_inflow(vertices, triangles, flow_density, incremental);
}
#endif

void OpenVDB_smoke_add_obstacle(OpenVDBSmokeData *data, float mat[4][4], OpenVDBMeshIterator *it)
{
	using openvdb::math::Vec3s;
	using openvdb::Vec3I;
	
	std::vector<Vec3s> vertices;
	std::vector<Vec3I> triangles;
	get_mesh_geometry(mat, it, vertices, triangles);
	
	((internal::SmokeData *)data)->add_obstacle(vertices, triangles);
}

void OpenVDB_smoke_clear_obstacles(OpenVDBSmokeData *data)
{
	((internal::SmokeData *)data)->clear_obstacles();
}

void OpenVDB_smoke_set_gravity(struct OpenVDBSmokeData *data, const float g[3])
{
	((internal::SmokeData *)data)->set_gravity(openvdb::Vec3f(g));
}

bool OpenVDB_smoke_step(struct OpenVDBSmokeData *data, float dt, int num_substeps)
{
	return ((internal::SmokeData *)data)->step(dt, num_substeps);
}

bool OpenVDB_smoke_get_pressure_result(struct OpenVDBSmokeData *pdata, double *err_abs, double *err_rel, int *iterations)
{
	internal::SmokeData *data = (internal::SmokeData *)pdata;
	if (err_abs) *err_abs = data->pressure_result.absoluteError;
	if (err_rel) *err_rel = data->pressure_result.relativeError;
	if (iterations) *iterations = data->pressure_result.iterations;
	return data->pressure_result.success;
}

/* ------------------------------------------------------------------------- */
/* Drawing */

/* Helper macro for compact grid type selection code.
 * This allows conversion of a grid id enum to templated function calls.
 * DO_GRID must be defined locally.
 */
#define SELECT_SMOKE_GRID(data, type) \
	switch ((type)) { \
		case OpenVDBSmokeGrid_Density: { DO_GRID(data->density.get()) } break; \
		case OpenVDBSmokeGrid_Velocity: { DO_GRID(data->velocity.get()) } break; \
		case OpenVDBSmokeGrid_Pressure: { DO_GRID(data->pressure.get()) } break; \
		case OpenVDBSmokeGrid_Divergence: { DO_GRID(data->tmp_divergence.get()) } break; \
	} (void)0

void OpenVDB_smoke_get_draw_buffers_cells(OpenVDBSmokeData *pdata, OpenVDBSmokeGridType grid,
                                          float (**r_verts)[3], float (**r_colors)[3], int *r_numverts)
{
	const int min_level = 0;
	const int max_level = 3;
	
	internal::SmokeData *data = (internal::SmokeData *)pdata;
	
#define DO_GRID(grid) \
	internal::OpenVDB_get_draw_buffer_size_cells(grid, min_level, max_level, true, r_numverts); \
	*r_verts = (float (*)[3])MEM_mallocN((*r_numverts) * sizeof(float) * 3, "OpenVDB vertex buffer"); \
	*r_colors = (float (*)[3])MEM_mallocN((*r_numverts) * sizeof(float) * 3, "OpenVDB color buffer"); \
	internal::OpenVDB_get_draw_buffers_cells(grid, min_level, max_level, true, *r_verts, *r_colors);
	
	SELECT_SMOKE_GRID(data, grid);
	
#undef DO_GRID
}

void OpenVDB_smoke_get_draw_buffers_boxes(OpenVDBSmokeData *pdata, OpenVDBSmokeGridType grid, float value_scale,
                                          float (**r_verts)[3], float (**r_colors)[3], float (**r_normals)[3], int *r_numverts)
{
	internal::SmokeData *data = (internal::SmokeData *)pdata;
	
#define DO_GRID(grid) \
	internal::OpenVDB_get_draw_buffer_size_boxes(grid, r_numverts); \
	const size_t bufsize_v3 = (*r_numverts) * sizeof(float) * 3; \
	*r_verts = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB vertex buffer"); \
	*r_colors = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB color buffer"); \
	*r_normals = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB normal buffer"); \
	internal::OpenVDB_get_draw_buffers_boxes(grid, value_scale, *r_verts, *r_colors, *r_normals);
	
	SELECT_SMOKE_GRID(data, grid);
	
#undef DO_GRID
}

void OpenVDB_smoke_get_draw_buffers_needles(OpenVDBSmokeData *pdata, OpenVDBSmokeGridType grid, float value_scale,
                                            float (**r_verts)[3], float (**r_colors)[3], float (**r_normals)[3], int *r_numverts)
{
	internal::SmokeData *data = (internal::SmokeData *)pdata;
	
#define DO_GRID(grid) \
	internal::OpenVDB_get_draw_buffer_size_needles(grid, r_numverts); \
	const size_t bufsize_v3 = (*r_numverts) * sizeof(float) * 3; \
	*r_verts = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB vertex buffer"); \
	*r_colors = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB color buffer"); \
	*r_normals = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB normal buffer"); \
	internal::OpenVDB_get_draw_buffers_needles(grid, value_scale, *r_verts, *r_colors, *r_normals);
	
	SELECT_SMOKE_GRID(data, grid);
	
#undef DO_GRID
}

void OpenVDB_smoke_get_bounds(struct OpenVDBSmokeData *pdata, OpenVDBSmokeGridType grid,
                              float bbmin[3], float bbmax[3])
{
	internal::SmokeData *data = (internal::SmokeData *)pdata;

#define DO_GRID(grid) \
	internal::OpenVDB_get_grid_bounds(grid, bbmin, bbmax);
	
	SELECT_SMOKE_GRID(data, grid);
	
#undef DO_GRID
}

float *OpenVDB_smoke_get_texture_buffer(struct OpenVDBSmokeData *pdata, OpenVDBSmokeGridType grid,
                                        int res[3], float bbmin[3], float bbmax[3])
{
	internal::SmokeData *data = (internal::SmokeData *)pdata;
	
#define DO_GRID(grid) \
	if (!internal::OpenVDB_get_dense_texture_res(grid, res, bbmin, bbmax)) \
		return NULL; \
	int numcells = res[0] * res[1] * res[2]; \
	float *buffer = (float *)MEM_mallocN(numcells * sizeof(float), "smoke VDB domain texture buffer"); \
	internal::OpenVDB_create_dense_texture(grid, buffer); \
	return buffer;
	
	SELECT_SMOKE_GRID(data, grid);
	
#undef DO_GRID
	
	return NULL;
}

void OpenVDB_smoke_get_value_range(struct OpenVDBSmokeData *pdata, OpenVDBSmokeGridType grid, float *bg, float *min, float *max)
{
	internal::SmokeData *data = (internal::SmokeData *)pdata;
	
#define DO_GRID(grid) \
	internal::OpenVDB_get_grid_value_range(grid, bg, min, max);
	
	SELECT_SMOKE_GRID(data, grid);
	
#undef DO_GRID
}

#undef OPENVDB_SELECT_GRID

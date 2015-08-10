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

#ifndef __OPENVDB_CAPI_H__
#define __OPENVDB_CAPI_H__

#ifdef __cplusplus
extern "C" {
#endif

struct OpenVDBReader;
struct OpenVDBWriter;
struct OpenVDBFloatGrid;
struct OpenVDBIntGrid;
struct OpenVDBVectorGrid;
struct OpenVDBSmokeData;

int OpenVDB_getVersionHex(void);

typedef void (*OpenVDBGridInfoCallback)(void *userdata, const char *name, const char *value_type, bool is_color);
void OpenVDB_get_grid_info(const char *filename, OpenVDBGridInfoCallback cb, void *userdata);

enum {
	VEC_INVARIANT = 0,
	VEC_COVARIANT = 1,
	VEC_COVARIANT_NORMALIZE = 2,
	VEC_CONTRAVARIANT_RELATIVE = 3,
	VEC_CONTRAVARIANT_ABSOLUTE = 4,
};

struct OpenVDBFloatGrid *OpenVDB_export_grid_fl(struct OpenVDBWriter *writer,
                                                const char *name, float *data,
                                                const int res[3], float matrix[4][4],
                                                struct OpenVDBFloatGrid *mask);

struct OpenVDBIntGrid *OpenVDB_export_grid_ch(struct OpenVDBWriter *writer,
                                              const char *name, unsigned char *data,
                                              const int res[3], float matrix[4][4],
                                              struct OpenVDBFloatGrid *mask);

struct OpenVDBVectorGrid *OpenVDB_export_grid_vec(struct OpenVDBWriter *writer,
                                                  const char *name,
                                                  const float *data_x, const float *data_y, const float *data_z,
                                                  const int res[3], float matrix[4][4], short vec_type,
                                                  const bool is_color,
                                                  struct OpenVDBFloatGrid *mask);

void OpenVDB_import_grid_fl(struct OpenVDBReader *reader,
                            const char *name, float **data,
                            const int res[3]);

void OpenVDB_import_grid_ch(struct OpenVDBReader *reader,
                            const char *name, unsigned char **data,
                            const int res[3]);

void OpenVDB_import_grid_vec(struct OpenVDBReader *reader,
                             const char *name,
                             float **data_x, float **data_y, float **data_z,
                             const int res[3]);

struct OpenVDBWriter *OpenVDBWriter_create(void);
void OpenVDBWriter_free(struct OpenVDBWriter *writer);
void OpenVDBWriter_set_flags(struct OpenVDBWriter *writer, const int flag, const bool half);
void OpenVDBWriter_add_meta_fl(struct OpenVDBWriter *writer, const char *name, const float value);
void OpenVDBWriter_add_meta_int(struct OpenVDBWriter *writer, const char *name, const int value);
void OpenVDBWriter_add_meta_v3(struct OpenVDBWriter *writer, const char *name, const float value[3]);
void OpenVDBWriter_add_meta_v3_int(struct OpenVDBWriter *writer, const char *name, const int value[3]);
void OpenVDBWriter_add_meta_mat4(struct OpenVDBWriter *writer, const char *name, float value[4][4]);
void OpenVDBWriter_write(struct OpenVDBWriter *writer, const char *filename);

struct OpenVDBReader *OpenVDBReader_create(void);
void OpenVDBReader_free(struct OpenVDBReader *reader);
void OpenVDBReader_open(struct OpenVDBReader *reader, const char *filename);
void OpenVDBReader_get_meta_fl(struct OpenVDBReader *reader, const char *name, float *value);
void OpenVDBReader_get_meta_int(struct OpenVDBReader *reader, const char *name, int *value);
void OpenVDBReader_get_meta_v3(struct OpenVDBReader *reader, const char *name, float value[3]);
void OpenVDBReader_get_meta_v3_int(struct OpenVDBReader *reader, const char *name, int value[3]);
void OpenVDBReader_get_meta_mat4(struct OpenVDBReader *reader, const char *name, float value[4][4]);

/* Simulation */

struct OpenVDBSmokeData *OpenVDB_create_smoke_data(float cell_mat[4][4]);
void OpenVDB_free_smoke_data(struct OpenVDBSmokeData *data);

/* generic iterator for reading mesh data from unknown sources */
struct OpenVDBMeshIterator;
typedef bool (*OpenVDBMeshHasVerticesFn)(struct OpenVDBMeshIterator *it);
typedef bool (*OpenVDBMeshHasTrianglesFn)(struct OpenVDBMeshIterator *it);
typedef void (*OpenVDBMeshNextVertexFn)(struct OpenVDBMeshIterator *it);
typedef void (*OpenVDBMeshNextTriangleFn)(struct OpenVDBMeshIterator *it);
typedef void (*OpenVDBMeshGetVertexFn)(struct OpenVDBMeshIterator *it, float co[3]);
typedef void (*OpenVDBMeshGetTriangleFn)(struct OpenVDBMeshIterator *it, int *a, int *b, int *c);
typedef struct OpenVDBMeshIterator {
	OpenVDBMeshHasVerticesFn has_vertices;
	OpenVDBMeshHasTrianglesFn has_triangles;
	
	OpenVDBMeshNextVertexFn next_vertex;
	OpenVDBMeshNextTriangleFn next_triangle;
	
	OpenVDBMeshGetVertexFn get_vertex;
	OpenVDBMeshGetTriangleFn get_triangle;
} OpenVDBMeshIterator;

void OpenVDB_smoke_add_inflow(struct OpenVDBSmokeData *data, float mat[4][4], struct OpenVDBMeshIterator *it,
                              float flow_density, bool incremental);
void OpenVDB_smoke_add_obstacle(struct OpenVDBSmokeData *data, float mat[4][4], struct OpenVDBMeshIterator *it);
void OpenVDB_smoke_clear_obstacles(struct OpenVDBSmokeData *data);

bool OpenVDB_smoke_step(struct OpenVDBSmokeData *data, float dt, int substeps);
bool OpenVDB_smoke_get_pressure_result(struct OpenVDBSmokeData *data, double *err_abs, double *err_rel, int *iterations);

/* Drawing */

typedef enum OpenVDBSmokeGridType {
	OpenVDBSmokeGrid_Density,
	OpenVDBSmokeGrid_Velocity,
	OpenVDBSmokeGrid_Pressure,
} OpenVDBSmokeGridType;

void OpenVDB_smoke_get_bounds(struct OpenVDBSmokeData *pdata, OpenVDBSmokeGridType grid,
                              float bbmin[3], float bbmax[3]);

void OpenVDB_smoke_get_draw_buffers_cells(struct OpenVDBSmokeData *data, OpenVDBSmokeGridType grid,
                                          float (**r_verts)[3], float (**r_colors)[3], int *r_numverts);
void OpenVDB_smoke_get_draw_buffers_boxes(struct OpenVDBSmokeData *data, OpenVDBSmokeGridType grid, float value_min, float value_max,
                                          float (**r_verts)[3], float (**r_colors)[3], float (**r_normals)[3], int *r_numverts);
void OpenVDB_smoke_get_draw_buffers_needles(struct OpenVDBSmokeData *data, OpenVDBSmokeGridType grid, float value_min, float value_max,
                                            float (**r_verts)[3], float (**r_colors)[3], float (**r_normals)[3], int *r_numverts);

float *OpenVDB_smoke_get_texture_buffer(struct OpenVDBSmokeData *data, OpenVDBSmokeGridType grid,
                                        int res[3], float bbmin[3], float bbmax[3]);

void OpenVDB_smoke_get_value_range(struct OpenVDBSmokeData *data, OpenVDBSmokeGridType grid, float *min, float *max);

#ifdef __cplusplus
}
#endif

#endif /* __OPENVDB_CAPI_H__ */

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

struct bNode;
struct bNodeTree;
struct OpenVDBReader;
struct OpenVDBWriter;

int OpenVDB_getVersionHex(void);

void OpenVDB_getNodeSockets(const char *filename, struct bNodeTree *ntree, struct bNode *node);


/* This duplicates a few properties from SmokeDomainSettings,
 * but it's more convenient/readable to pass a struct than having a huge set of
 * parameters to a function
 */
typedef struct FluidDomainDescr {
	float obmat[4][4];
	float fluidmat[4][4];
	float fluidmathigh[4][4];
	int shift[3];
	float obj_shift_f[3];
	int fluid_fields;
	float active_color[3];
	int active_fields;
} FluidDomainDescr;

enum {
	OPENVDB_NO_ERROR      = 0,
	OPENVDB_ARITHM_ERROR  = 1,
	OPENVDB_ILLEGAL_ERROR = 2,
	OPENVDB_INDEX_ERROR   = 3,
	OPENVDB_IO_ERROR      = 4,
	OPENVDB_KEY_ERROR     = 5,
	OPENVDB_LOOKUP_ERROR  = 6,
	OPENVDB_IMPL_ERROR    = 7,
	OPENVDB_REF_ERROR     = 8,
	OPENVDB_TYPE_ERROR    = 9,
	OPENVDB_VALUE_ERROR   = 10,
	OPENVDB_UNKNOWN_ERROR = 11,
};

void OpenVDB_update_fluid_transform(const char *filename, FluidDomainDescr descr);

void OpenVDB_export_grid_fl(struct OpenVDBWriter *writer,
                            const char *name, float *data,
                            const int res[3], float matrix[4][4]);

void OpenVDB_export_grid_ch(struct OpenVDBWriter *writer,
                            const char *name, unsigned char *data,
                            const int res[3], float matrix[4][4]);

void OpenVDB_export_grid_vec(struct OpenVDBWriter *writer,
                             const char *name,
                             const float *data_x, const float *data_y, const float *data_z,
                             const int res[3], float matrix[4][4]);

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
void OpenVDBWriter_set_compression(struct OpenVDBWriter *writer, const int flag);
void OpenVDBWriter_add_meta_fl(struct OpenVDBWriter *writer, const char *name, const float value);
void OpenVDBWriter_add_meta_int(struct OpenVDBWriter *writer, const char *name, const int value);
void OpenVDBWriter_add_meta_v3(struct OpenVDBWriter *writer, const char *name, const float value[3]);
void OpenVDBWriter_add_meta_v3_int(struct OpenVDBWriter *writer, const char *name, const int value[3]);
void OpenVDBWriter_add_meta_mat4(struct OpenVDBWriter *writer, const char *name, float value[4][4]);
void OpenVDBWriter_write(struct OpenVDBWriter *writer, const char *filename);

struct OpenVDBReader *OpenVDBReader_create(const char *filename);
void OpenVDBReader_free(struct OpenVDBReader *reader);
void OpenVDBReader_get_meta_fl(struct OpenVDBReader *reader, const char *name, float *value);
void OpenVDBReader_get_meta_int(struct OpenVDBReader *reader, const char *name, int *value);
void OpenVDBReader_get_meta_v3(struct OpenVDBReader *reader, const char *name, float value[3]);
void OpenVDBReader_get_meta_v3_int(struct OpenVDBReader *reader, const char *name, int value[3]);
void OpenVDBReader_get_meta_mat4(struct OpenVDBReader *reader, const char *name, float value[4][4]);

#ifdef __cplusplus
}
#endif

#endif /* __OPENVDB_CAPI_H__ */

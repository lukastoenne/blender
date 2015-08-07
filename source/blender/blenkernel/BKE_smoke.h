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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Daniel Genrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_SMOKE_H__
#define __BKE_SMOKE_H__

/** \file BKE_smoke.h
 *  \ingroup bke
 *  \author Daniel Genrich
 */

struct SmokeModifierData;
struct SmokeDomainSettings;
struct SmokeDomainVDBSettings;

struct OpenVDBCache;

typedef float (*bresenham_callback)(float *result, float *input, int res[3], int *pixel, float *tRay, float correct);

struct DerivedMesh *smokeModifier_do(struct SmokeModifierData *smd, struct Scene *scene, struct Object *ob, struct DerivedMesh *dm, bool for_render);

void smoke_reallocate_fluid(struct SmokeDomainSettings *sds, float dx, int res[3], int free_old);
void smoke_reallocate_highres_fluid(struct SmokeDomainSettings *sds, float dx, int res[3], int free_old);
void smokeModifier_free(struct SmokeModifierData *smd);
void smokeModifier_reset(struct SmokeModifierData *smd);
void smokeModifier_reset_turbulence(struct SmokeModifierData *smd);
void smokeModifier_createType(struct SmokeModifierData *smd);
void smokeModifier_copy(struct SmokeModifierData *smd, struct SmokeModifierData *tsmd);

float smoke_get_velocity_at(struct Object *ob, float position[3], float velocity[3]);
int smoke_get_data_flags(struct SmokeDomainSettings *sds);

/* OpenVDB domain data */
void smoke_vdb_init_data(struct Object *ob, struct SmokeDomainVDBSettings *sds);
void smoke_vdb_free_data(struct SmokeDomainVDBSettings *sds);

void smoke_vdb_get_bounds(struct SmokeDomainVDBSettings *sds, float bbmin[3], float bbmax[3]);
void smoke_vdb_get_draw_buffers(struct SmokeDomainVDBSettings *sds,
                                float (**r_verts)[3], float (**r_colors)[3],
                                float (**r_normals)[3], int *r_numverts, bool *r_use_quads);
float *smoke_vdb_create_dense_texture(struct SmokeDomainVDBSettings *sds, int res[3], float bbmin[3], float bbmax[3]);

void smoke_vdb_display_range_adjust(struct SmokeDomainVDBSettings *sds);

/* OpenVDB smoke export/import */

typedef void (*update_cb)(void *, float progress, int *cancel);

void smokeModifier_OpenVDB_export(struct SmokeModifierData *smd, struct Scene *scene,
                                  struct Object *ob, struct DerivedMesh *dm,
                                  update_cb update,
                                  void *update_cb_data);

bool smokeModifier_OpenVDB_import(struct SmokeModifierData *smd, struct Scene *scene, struct Object *ob, struct OpenVDBCache *cache);

struct OpenVDBCache *BKE_openvdb_get_current_cache(struct SmokeDomainSettings *sds);
void BKE_openvdb_cache_filename(char *r_filename, const char *path, const char *fname, const char *relbase, int frame);

#endif /* __BKE_SMOKE_H__ */

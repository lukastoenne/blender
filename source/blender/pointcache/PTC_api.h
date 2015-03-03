/*
 * Copyright 2013, Blender Foundation.
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
 */

#ifndef PTC_API_H
#define PTC_API_H

#include "util/util_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Main;
struct Scene;
struct EvaluationContext;
struct ListBase;
struct PointerRNA;
struct ReportList;

struct ClothModifierData;
struct DerivedMesh;
struct DynamicPaintSurface;
struct ModifierData;
struct Object;
struct ParticleSystem;
struct CacheModifierData;
struct RigidBodyWorld;
struct SmokeDomainSettings;
struct SoftBody;

struct PTCWriterArchive;
struct PTCReaderArchive;
struct PTCWriter;
struct PTCReader;

void PTC_alembic_init(void);

/*** Error Handling ***/
void PTC_error_handler_std(void);
void PTC_error_handler_callback(PTCErrorCallback cb, void *userdata);
void PTC_error_handler_reports(struct ReportList *reports);
void PTC_error_handler_modifier(struct ModifierData *md);

void PTC_bake(struct Main *bmain, struct Scene *scene, struct EvaluationContext *evalctx,
              struct ListBase *writers, struct DerivedMesh **render_dm_ptr, int start_frame, int end_frame,
              short *stop, short *do_update, float *progress);

/*** Archive ***/

const char *PTC_get_default_archive_extension(void);

struct PTCWriterArchive *PTC_open_writer_archive(struct Scene *scene, const char *path);
void PTC_close_writer_archive(struct PTCWriterArchive *archive);

struct PTCReaderArchive *PTC_open_reader_archive(struct Scene *scene, const char *path);
void PTC_close_reader_archive(struct PTCReaderArchive *archive);

void PTC_writer_set_archive(struct PTCWriter *writer, struct PTCWriterArchive *archive);
void PTC_reader_set_archive(struct PTCReader *reader, struct PTCReaderArchive *archive);

/*** Reader/Writer Interface ***/

void PTC_writer_free(struct PTCWriter *writer);
void PTC_write_sample(struct PTCWriter *writer);

void PTC_reader_free(struct PTCReader *reader);
bool PTC_reader_get_frame_range(struct PTCReader *reader, int *start_frame, int *end_frame);
PTCReadSampleResult PTC_read_sample(struct PTCReader *reader, float frame);
PTCReadSampleResult PTC_test_sample(struct PTCReader *reader, float frame);

/* get writer/reader from RNA type */
struct PTCWriter *PTC_writer_from_rna(struct Scene *scene, struct PointerRNA *ptr);
struct PTCReader *PTC_reader_from_rna(struct Scene *scene, struct PointerRNA *ptr);

/* Particles */
struct PTCWriter *PTC_writer_particles(const char *name, struct Object *ob, struct ParticleSystem *psys);
struct PTCReader *PTC_reader_particles(const char *name, struct Object *ob, struct ParticleSystem *psys);
int PTC_reader_particles_totpoint(struct PTCReader *reader);

struct PTCWriter *PTC_writer_particles_pathcache_parents(const char *name, struct Object *ob, struct ParticleSystem *psys);
struct PTCReader *PTC_reader_particles_pathcache_parents(const char *name, struct Object *ob, struct ParticleSystem *psys);
struct PTCWriter *PTC_writer_particles_pathcache_children(const char *name, struct Object *ob, struct ParticleSystem *psys);
struct PTCReader *PTC_reader_particles_pathcache_children(const char *name, struct Object *ob, struct ParticleSystem *psys);

/* Cloth */
struct PTCWriter *PTC_writer_cloth(const char *name, struct Object *ob, struct ClothModifierData *clmd);
struct PTCReader *PTC_reader_cloth(const char *name, struct Object *ob, struct ClothModifierData *clmd);
struct PTCWriter *PTC_writer_hair_dynamics(const char *name, struct Object *ob, struct ClothModifierData *clmd);
struct PTCReader *PTC_reader_hair_dynamics(const char *name, struct Object *ob, struct ClothModifierData *clmd);

struct PTCWriter *PTC_writer_derived_mesh(const char *name, struct Object *ob, struct DerivedMesh **dm_ptr);
struct PTCReader *PTC_reader_derived_mesh(const char *name, struct Object *ob);
struct DerivedMesh *PTC_reader_derived_mesh_acquire_result(struct PTCReader *reader);
void PTC_reader_derived_mesh_discard_result(struct PTCReader *reader);

struct PTCWriter *PTC_writer_derived_final_realtime(const char *name, struct Object *ob);
struct PTCWriter *PTC_writer_cache_modifier_realtime(const char *name, struct Object *ob, struct CacheModifierData *cmd);
struct PTCWriter *PTC_writer_derived_final_render(const char *name, struct Scene *scene, struct Object *ob, struct DerivedMesh **render_dm_ptr);
struct PTCWriter *PTC_writer_cache_modifier_render(const char *name, struct Scene *scene, struct Object *ob, struct CacheModifierData *cmd);

#ifdef __cplusplus
} /* extern C */
#endif

#endif  /* PTC_API_H */

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
struct CacheLibrary;

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

void PTC_bake(struct Main *bmain, struct Scene *scene, struct EvaluationContext *evalctx, struct ListBase *writers, int start_frame, int end_frame,
              short *stop, short *do_update, float *progress);

/*** Archive ***/

struct PTCWriterArchive *PTC_open_writer_archive(struct Scene *scene, const char *path);
void PTC_close_writer_archive(struct PTCWriterArchive *archive);

struct PTCReaderArchive *PTC_open_reader_archive(struct Scene *scene, const char *path);
void PTC_close_reader_archive(struct PTCReaderArchive *archive);

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

struct PTCReader *PTC_cachelib_reader_derived_mesh(struct CacheLibrary *cachelib, struct PTCReaderArchive *archive, struct Object *ob);
struct PTCReader *PTC_cachelib_reader_hair_dynamics(struct CacheLibrary *cachelib, struct PTCReaderArchive *archive, struct Object *ob, struct ParticleSystem *psys);
struct PTCReader *PTC_cachelib_reader_particle_pathcache_parents(struct CacheLibrary *cachelib, struct PTCReaderArchive *archive, struct Object *ob, struct ParticleSystem *psys);
struct PTCReader *PTC_cachelib_reader_particle_pathcache_children(struct CacheLibrary *cachelib, struct PTCReaderArchive *archive, struct Object *ob, struct ParticleSystem *psys);

PTCReadSampleResult PTC_cachelib_read_sample_derived_mesh(struct Scene *scene, float frame, struct CacheLibrary *cachelib, struct Object *ob, struct DerivedMesh **r_dm);
PTCReadSampleResult PTC_cachelib_read_sample_hair_dynamics(struct Scene *scene, float frame, struct CacheLibrary *cachelib, struct Object *ob, struct ParticleSystem *psys);
PTCReadSampleResult PTC_cachelib_read_sample_particle_pathcache_parents(struct Scene *scene, float frame, struct CacheLibrary *cachelib, struct Object *ob, struct ParticleSystem *psys);
PTCReadSampleResult PTC_cachelib_read_sample_particle_pathcache_children(struct Scene *scene, float frame, struct CacheLibrary *cachelib, struct Object *ob, struct ParticleSystem *psys);

struct PTCWriterArchive *PTC_cachelib_writers(struct Scene *scene, int required_mode, struct CacheLibrary *cachelib, struct ListBase *writers);
void PTC_cachelib_writers_free(struct PTCWriterArchive *archive, struct ListBase *writers);

/* Particles */
struct PTCWriter *PTC_writer_particles(struct PTCWriterArchive *archive, const char *name, struct Object *ob, struct ParticleSystem *psys);
struct PTCReader *PTC_reader_particles(struct PTCReaderArchive *archive, const char *name, struct Object *ob, struct ParticleSystem *psys);
int PTC_reader_particles_totpoint(struct PTCReader *reader);

struct PTCWriter *PTC_writer_particle_pathcache_parents(struct PTCWriterArchive *archive, const char *name, struct Object *ob, struct ParticleSystem *psys);
struct PTCReader *PTC_reader_particle_pathcache_parents(struct PTCReaderArchive *archive, const char *name, struct Object *ob, struct ParticleSystem *psys);
struct PTCWriter *PTC_writer_particle_pathcache_children(struct PTCWriterArchive *archive, const char *name, struct Object *ob, struct ParticleSystem *psys);
struct PTCReader *PTC_reader_particle_pathcache_children(struct PTCReaderArchive *archive, const char *name, struct Object *ob, struct ParticleSystem *psys);

/* Cloth */
struct PTCWriter *PTC_writer_cloth(struct PTCWriterArchive *archive, const char *name, struct Object *ob, struct ClothModifierData *clmd);
struct PTCReader *PTC_reader_cloth(struct PTCReaderArchive *archive, const char *name, struct Object *ob, struct ClothModifierData *clmd);
struct PTCWriter *PTC_writer_hair_dynamics(struct PTCWriterArchive *archive, const char *name, struct Object *ob, struct ClothModifierData *clmd);
struct PTCReader *PTC_reader_hair_dynamics(struct PTCReaderArchive *archive, const char *name, struct Object *ob, struct ClothModifierData *clmd);

struct PTCWriter *PTC_writer_derived_mesh(struct PTCWriterArchive *archive, const char *name, struct Object *ob, struct DerivedMesh **dm_ptr);
struct PTCReader *PTC_reader_derived_mesh(struct PTCReaderArchive *archive, const char *name, struct Object *ob);
struct DerivedMesh *PTC_reader_derived_mesh_acquire_result(struct PTCReader *reader);
void PTC_reader_derived_mesh_discard_result(struct PTCReader *reader);
struct PTCWriter *PTC_writer_derived_final(struct PTCWriterArchive *_archive, const char *name, struct Object *ob);
struct PTCWriter *PTC_writer_cache_modifier(struct PTCWriterArchive *_archive, const char *name, struct Object *ob, struct CacheModifierData *cmd);

#ifdef __cplusplus
} /* extern C */
#endif

#endif  /* PTC_API_H */

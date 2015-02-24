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
struct PointerRNA;
struct ReportList;

struct ClothModifierData;
struct DerivedMesh;
struct DynamicPaintSurface;
struct ModifierData;
struct Object;
struct ParticleSystem;
struct PointCacheModifierData;
struct RigidBodyWorld;
struct SmokeDomainSettings;
struct SoftBody;

struct PTCWriterArchive;
struct PTCReaderArchive;
struct PTCWriter;
struct PTCReader;

/*** Error Handling ***/
void PTC_error_handler_std(void);
void PTC_error_handler_callback(PTCErrorCallback cb, void *userdata);
void PTC_error_handler_reports(struct ReportList *reports);
void PTC_error_handler_modifier(struct ModifierData *md);

void PTC_bake(struct Main *bmain, struct Scene *scene, struct EvaluationContext *evalctx, struct PTCWriter *writer, int start_frame, int end_frame,
              short *stop, short *do_update, float *progress);

/*** Archive ***/

struct PTCWriterArchive *PTC_open_writer_archive(Scene *scene, const char *path);
void PTC_close_writer_archive(struct PTCWriterArchive *archive);

struct PTCReaderArchive *PTC_open_reader_archive(Scene *scene, const char *path);
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

/* Particles */
struct PTCWriter *PTC_writer_particles(struct PTCWriterArchive *archive, struct Object *ob, struct ParticleSystem *psys);
struct PTCReader *PTC_reader_particles(struct PTCReaderArchive *archive, struct Object *ob, struct ParticleSystem *psys);
int PTC_reader_particles_totpoint(struct PTCReader *reader);

typedef enum eParticlePathsMode {
	PTC_PARTICLE_PATHS_PARENTS = 0,
	PTC_PARTICLE_PATHS_CHILDREN = 1,
} eParticlePathsMode;

struct PTCWriter *PTC_writer_particle_paths(struct PTCWriterArchive *archive, struct Object *ob, struct ParticleSystem *psys);
struct PTCReader *PTC_reader_particle_paths(struct PTCReaderArchive *archive, struct Object *ob, struct ParticleSystem *psys, eParticlePathsMode mode);

/* Cloth */
struct PTCWriter *PTC_writer_cloth(struct PTCWriterArchive *archive, struct Object *ob, struct ClothModifierData *clmd);
struct PTCReader *PTC_reader_cloth(struct PTCReaderArchive *archive, struct Object *ob, struct ClothModifierData *clmd);

/* SoftBody */
struct PTCWriter *PTC_writer_softbody(struct PTCWriterArchive *archive, struct Object *ob, struct SoftBody *softbody);
struct PTCReader *PTC_reader_softbody(struct PTCReaderArchive *archive, struct Object *ob, struct SoftBody *softbody);

/* Rigid Bodies */
struct PTCWriter *PTC_writer_rigidbody(struct PTCWriterArchive *archive, struct Scene *scene, struct RigidBodyWorld *rbw);
struct PTCReader *PTC_reader_rigidbody(struct PTCReaderArchive *archive, struct Scene *scene, struct RigidBodyWorld *rbw);

/* Smoke */
struct PTCWriter *PTC_writer_smoke(struct PTCWriterArchive *archive, struct Object *ob, struct SmokeDomainSettings *domain);
struct PTCReader *PTC_reader_smoke(struct PTCReaderArchive *archive, struct Object *ob, struct SmokeDomainSettings *domain);

/* Dynamic Paint */
struct PTCWriter *PTC_writer_dynamicpaint(struct PTCWriterArchive *archive, struct Object *ob, struct DynamicPaintSurface *surface);
struct PTCReader *PTC_reader_dynamicpaint(struct PTCReaderArchive *archive, struct Object *ob, struct DynamicPaintSurface *surface);

/* Modifier Stack */
typedef enum ePointCacheModifierMode {
	MOD_POINTCACHE_MODE_NONE,
	MOD_POINTCACHE_MODE_READ,
	MOD_POINTCACHE_MODE_WRITE,
} ePointCacheModifierMode;

struct PTCWriter *PTC_writer_point_cache(struct PTCWriterArchive *archive, struct Object *ob, struct PointCacheModifierData *pcmd);
struct PTCReader *PTC_reader_point_cache(struct PTCReaderArchive *archive, struct Object *ob, struct PointCacheModifierData *pcmd);
struct DerivedMesh *PTC_reader_point_cache_acquire_result(struct PTCReader *reader);
void PTC_reader_point_cache_discard_result(struct PTCReader *reader);
ePointCacheModifierMode PTC_mod_point_cache_get_mode(struct PointCacheModifierData *pcmd);
/* returns the actual new mode, in case a change didn't succeed */
ePointCacheModifierMode PTC_mod_point_cache_set_mode(struct Scene *scene, struct Object *ob, struct PointCacheModifierData *pcmd, ePointCacheModifierMode mode);

#ifdef __cplusplus
} /* extern C */
#endif

#endif  /* PTC_API_H */

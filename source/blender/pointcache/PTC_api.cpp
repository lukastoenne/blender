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

#include "MEM_guardedalloc.h"

#include "PTC_api.h"

#include "util/util_error_handler.h"

#include "reader.h"
#include "writer.h"
#include "export.h"

#include "ptc_types.h"

extern "C" {
#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DNA_listBase.h"
#include "DNA_modifier_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_modifier.h"
#include "BKE_report.h"

#include "RNA_access.h"
}

using namespace PTC;

void PTC_error_handler_std(void)
{
	ErrorHandler::clear_default_handler();
}

void PTC_error_handler_callback(PTCErrorCallback cb, void *userdata)
{
	ErrorHandler::set_default_handler(new CallbackErrorHandler(cb, userdata));
}

static ReportType report_type_from_error_level(PTCErrorLevel level)
{
	switch (level) {
		case PTC_ERROR_NONE:        return RPT_DEBUG;
		case PTC_ERROR_INFO:        return RPT_INFO;
		case PTC_ERROR_WARNING:     return RPT_WARNING;
		case PTC_ERROR_CRITICAL:    return RPT_ERROR;
	}
	return RPT_ERROR;
}

static void error_handler_reports_cb(void *vreports, PTCErrorLevel level, const char *message)
{
	ReportList *reports = (ReportList *)vreports;
	
	BKE_report(reports, report_type_from_error_level(level), message);
}

void PTC_error_handler_reports(struct ReportList *reports)
{
	ErrorHandler::set_default_handler(new CallbackErrorHandler(error_handler_reports_cb, reports));
}

static void error_handler_modifier_cb(void *vmd, PTCErrorLevel UNUSED(level), const char *message)
{
	ModifierData *md = (ModifierData *)vmd;
	
	modifier_setError(md, "%s", message);
}

void PTC_error_handler_modifier(struct ModifierData *md)
{
	ErrorHandler::set_default_handler(new CallbackErrorHandler(error_handler_modifier_cb, md));
}


const char *PTC_get_default_archive_extension(void)
{
	return PTC::Factory::alembic->get_default_extension().c_str();
}

PTCWriterArchive *PTC_open_writer_archive(Scene *scene, const char *path)
{
	return (PTCWriterArchive *)PTC::Factory::alembic->open_writer_archive(scene, path, NULL);
}

void PTC_close_writer_archive(PTCWriterArchive *_archive)
{
	PTC::WriterArchive *archive = (PTC::WriterArchive *)_archive;
	delete archive;
}

PTCReaderArchive *PTC_open_reader_archive(Scene *scene, const char *path)
{
	return (PTCReaderArchive *)PTC::Factory::alembic->open_reader_archive(scene, path, NULL);
}

void PTC_close_reader_archive(PTCReaderArchive *_archive)
{
	PTC::ReaderArchive *archive = (PTC::ReaderArchive *)_archive;
	delete archive;
}

void PTC_writer_init(PTCWriter *_writer, PTCWriterArchive *_archive)
{
	PTC::Writer *writer = (PTC::Writer *)_writer;
	PTC::WriterArchive *archive = (PTC::WriterArchive *)_archive;
	writer->init(archive);
}

void PTC_writer_create_refs(PTCWriter *_writer)
{
	PTC::Writer *writer = (PTC::Writer *)_writer;
	writer->create_refs();
}

void PTC_reader_init(PTCReader *_reader, PTCReaderArchive *_archive)
{
	PTC::Reader *reader = (PTC::Reader *)_reader;
	PTC::ReaderArchive *archive = (PTC::ReaderArchive *)_archive;
	reader->init(archive);
}

/* ========================================================================= */

void PTC_writer_free(PTCWriter *_writer)
{
	PTC::Writer *writer = (PTC::Writer *)_writer;
	delete writer;
}

void PTC_write_sample(struct PTCWriter *_writer)
{
	PTC::Writer *writer = (PTC::Writer *)_writer;
	writer->write_sample();
}

void PTC_bake(struct Main *bmain, struct Scene *scene, struct EvaluationContext *evalctx,
              PTCWriter *writer, int start_frame, int end_frame,
              short *stop, short *do_update, float *progress)
{
	PTC::Exporter exporter(bmain, scene, evalctx, stop, do_update, progress);
	exporter.bake(writer, start_frame, end_frame);
}


void PTC_reader_free(PTCReader *_reader)
{
	PTC::Reader *reader = (PTC::Reader *)_reader;
	delete reader;
}

bool PTC_reader_get_frame_range(PTCReader *_reader, int *start_frame, int *end_frame)
{
	PTC::Reader *reader = (PTC::Reader *)_reader;
	int sfra, efra;
	if (reader->get_frame_range(sfra, efra)) {
		if (start_frame) *start_frame = sfra;
		if (end_frame) *end_frame = efra;
		return true;
	}
	else {
		return false;
	}
}

PTCReadSampleResult PTC_read_sample(PTCReader *_reader, float frame)
{
	PTC::Reader *reader = (PTC::Reader *)_reader;
	return reader->read_sample(frame);
}

PTCReadSampleResult PTC_test_sample(PTCReader *_reader, float frame)
{
	PTC::Reader *reader = (PTC::Reader *)_reader;
	return reader->test_sample(frame);
}

char *PTC_get_archive_info(PTCReaderArchive *_archive)
{
	PTC::ReaderArchive *archive = (PTC::ReaderArchive *)_archive;
	
	std::string info = archive->get_info();
	return BLI_sprintfN("%s", info.c_str());
}


PTCWriter *PTC_writer_dupligroup(const char *name, struct EvaluationContext *eval_ctx, struct Scene *scene, struct Group *group)
{
	return (PTCWriter *)PTC::Factory::alembic->create_writer_dupligroup(name, eval_ctx, scene, group);
}

PTCReader *PTC_reader_dupligroup(const char *name, struct Group *group, struct DupliCache *dupcache)
{
	return (PTCReader *)PTC::Factory::alembic->create_reader_dupligroup(name, group, dupcache);
}


/* get writer/reader from RNA type */
PTCWriter *PTC_writer_from_rna(Scene *scene, PointerRNA *ptr)
{
#if 0
#if 0
	if (RNA_struct_is_a(ptr->type, &RNA_ParticleSystem)) {
		Object *ob = (Object *)ptr->id.data;
		ParticleSystem *psys = (ParticleSystem *)ptr->data;
		return PTC_writer_particles_combined(scene, ob, psys);
	}
#endif
	if (RNA_struct_is_a(ptr->type, &RNA_ClothModifier)) {
		Object *ob = (Object *)ptr->id.data;
		ClothModifierData *clmd = (ClothModifierData *)ptr->data;
		return PTC_writer_cloth(scene, ob, clmd);
	}
#endif
	return NULL;
}

PTCReader *PTC_reader_from_rna(Scene *scene, PointerRNA *ptr)
{
#if 0
	if (RNA_struct_is_a(ptr->type, &RNA_ParticleSystem)) {
		Object *ob = (Object *)ptr->id.data;
		ParticleSystem *psys = (ParticleSystem *)ptr->data;
		/* XXX particles are bad ...
		 * this can be either the actual particle cache or the hair dynamics cache,
		 * which is actually the cache of the internal cloth modifier
		 */
		bool use_cloth_cache = psys->part->type == PART_HAIR && (psys->flag & PSYS_HAIR_DYNAMICS);
		if (use_cloth_cache && psys->clmd)
			return PTC_reader_cloth(scene, ob, psys->clmd);
		else
			return PTC_reader_particles(scene, ob, psys);
	}
	if (RNA_struct_is_a(ptr->type, &RNA_ClothModifier)) {
		Object *ob = (Object *)ptr->id.data;
		ClothModifierData *clmd = (ClothModifierData *)ptr->data;
		return PTC_reader_cloth(scene, ob, clmd);
	}
#endif
	return NULL;
}


/* ==== CLOTH ==== */

PTCWriter *PTC_writer_cloth(const char *name, Object *ob, ClothModifierData *clmd)
{
	return (PTCWriter *)PTC::Factory::alembic->create_writer_cloth(name, ob, clmd);
}

PTCReader *PTC_reader_cloth(const char *name, Object *ob, ClothModifierData *clmd)
{
	return (PTCReader *)PTC::Factory::alembic->create_reader_cloth(name, ob, clmd);
}


/* ==== MESH ==== */

PTCWriter *PTC_writer_derived_mesh(const char *name, Object *ob, DerivedMesh **dm_ptr)
{
	return (PTCWriter *)PTC::Factory::alembic->create_writer_derived_mesh(name, ob, dm_ptr);
}

PTCReader *PTC_reader_derived_mesh(const char *name, Object *ob)
{
	return (PTCReader *)PTC::Factory::alembic->create_reader_derived_mesh(name, ob);
}

struct DerivedMesh *PTC_reader_derived_mesh_acquire_result(PTCReader *_reader)
{
	DerivedMeshReader *reader = (DerivedMeshReader *)_reader;
	return reader->acquire_result();
}

void PTC_reader_derived_mesh_discard_result(PTCReader *_reader)
{
	DerivedMeshReader *reader = (DerivedMeshReader *)_reader;
	reader->discard_result();
}


PTCWriter *PTC_writer_derived_final_realtime(const char *name, Object *ob)
{
	return (PTCWriter *)PTC::Factory::alembic->create_writer_derived_final_realtime(name, ob);
}

PTCWriter *PTC_writer_cache_modifier_realtime(const char *name, Object *ob, CacheModifierData *cmd)
{
	return (PTCWriter *)PTC::Factory::alembic->create_writer_cache_modifier_realtime(name, ob, cmd);
}

PTCWriter *PTC_writer_derived_final_render(const char *name, Scene *scene, Object *ob, DerivedMesh **render_dm_ptr)
{
	return (PTCWriter *)PTC::Factory::alembic->create_writer_derived_final_render(name, scene, ob, render_dm_ptr);
}

PTCWriter *PTC_writer_cache_modifier_render(const char *name, Scene *scene, Object *ob, CacheModifierData *cmd)
{
	return (PTCWriter *)PTC::Factory::alembic->create_writer_cache_modifier_render(name, scene, ob, cmd);
}


/* ==== OBJECT ==== */

PTCWriter *PTC_writer_object(const char *name, Object *ob)
{
	return (PTCWriter *)PTC::Factory::alembic->create_writer_object(name, ob);
}

PTCReader *PTC_reader_object(const char *name, Object *ob)
{
	return (PTCReader *)PTC::Factory::alembic->create_reader_object(name, ob);
}


/* ==== GROUP ==== */

PTCWriter *PTC_writer_group(const char *name, Group *group)
{
	return (PTCWriter *)PTC::Factory::alembic->create_writer_group(name, group);
}

PTCReader *PTC_reader_group(const char *name, Group *group)
{
	return (PTCReader *)PTC::Factory::alembic->create_writer_group(name, group);
}


/* ==== PARTICLES ==== */

PTCWriter *PTC_writer_particles(const char *name, Object *ob, ParticleSystem *psys)
{
	return (PTCWriter *)PTC::Factory::alembic->create_writer_particles(name, ob, psys);
}

PTCReader *PTC_reader_particles(const char *name, Object *ob, ParticleSystem *psys)
{
	return (PTCReader *)PTC::Factory::alembic->create_reader_particles(name, ob, psys);
}

int PTC_reader_particles_totpoint(PTCReader *_reader)
{
	return ((PTC::ParticlesReader *)_reader)->totpoint();
}

PTCWriter *PTC_writer_hair_dynamics(const char *name, Object *ob, ParticleSystem *psys)
{
	return (PTCWriter *)PTC::Factory::alembic->create_writer_hair_dynamics(name, ob, psys);
}

PTCReader *PTC_reader_hair_dynamics(const char *name, Object *ob, ParticleSystem *psys)
{
	return (PTCReader *)PTC::Factory::alembic->create_reader_hair_dynamics(name, ob, psys);
}

PTCWriter *PTC_writer_particles_pathcache_parents(const char *name, Object *ob, ParticleSystem *psys)
{
	return (PTCWriter *)PTC::Factory::alembic->create_writer_particles_pathcache_parents(name, ob, psys);
}

PTCReader *PTC_reader_particles_pathcache_parents(const char *name, Object *ob, ParticleSystem *psys)
{
	return (PTCReader *)PTC::Factory::alembic->create_reader_particles_pathcache_parents(name, ob, psys);
}

PTCWriter *PTC_writer_particles_pathcache_children(const char *name, Object *ob, ParticleSystem *psys)
{
	return (PTCWriter *)PTC::Factory::alembic->create_writer_particles_pathcache_children(name, ob, psys);
}

PTCReader *PTC_reader_particles_pathcache_children(const char *name, Object *ob, ParticleSystem *psys)
{
	return (PTCReader *)PTC::Factory::alembic->create_reader_particles_pathcache_children(name, ob, psys);
}

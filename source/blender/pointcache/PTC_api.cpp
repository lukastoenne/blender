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
#include "util_path.h"

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
	return (PTCWriterArchive *)PTC::Factory::alembic->create_writer_archive(scene, path, NULL);
}

void PTC_close_writer_archive(PTCWriterArchive *_archive)
{
	PTC::WriterArchive *archive = (PTC::WriterArchive *)_archive;
	delete archive;
}

PTCReaderArchive *PTC_open_reader_archive(Scene *scene, const char *path)
{
	return (PTCReaderArchive *)PTC::Factory::alembic->create_reader_archive(scene, path, NULL);
}

void PTC_close_reader_archive(PTCReaderArchive *_archive)
{
	PTC::ReaderArchive *archive = (PTC::ReaderArchive *)_archive;
	delete archive;
}


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

void PTC_bake(struct Main *bmain, struct Scene *scene, struct EvaluationContext *evalctx, struct ListBase *writers, int start_frame, int end_frame,
              short *stop, short *do_update, float *progress)
{
	PTC::Exporter exporter(bmain, scene, evalctx, stop, do_update, progress);
	exporter.bake(writers, start_frame, end_frame);
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

PTCWriter *PTC_writer_cloth(PTCWriterArchive *_archive, const char *name, Object *ob, ClothModifierData *clmd)
{
	PTC::WriterArchive *archive = (PTC::WriterArchive *)_archive;
	return (PTCWriter *)PTC::Factory::alembic->create_writer_cloth(archive, name, ob, clmd);
}

PTCReader *PTC_reader_cloth(PTCReaderArchive *_archive, const char *name, Object *ob, ClothModifierData *clmd)
{
	PTC::ReaderArchive *archive = (PTC::ReaderArchive *)_archive;
	return (PTCReader *)PTC::Factory::alembic->create_reader_cloth(archive, name, ob, clmd);
}

PTCWriter *PTC_writer_hair_dynamics(PTCWriterArchive *_archive, const char *name, Object *ob, ClothModifierData *clmd)
{
	PTC::WriterArchive *archive = (PTC::WriterArchive *)_archive;
	return (PTCWriter *)PTC::Factory::alembic->create_writer_hair_dynamics(archive, name, ob, clmd);
}

PTCReader *PTC_reader_hair_dynamics(PTCReaderArchive *_archive, const char *name, Object *ob, ClothModifierData *clmd)
{
	PTC::ReaderArchive *archive = (PTC::ReaderArchive *)_archive;
	return (PTCReader *)PTC::Factory::alembic->create_reader_hair_dynamics(archive, name, ob, clmd);
}


/* ==== MESH ==== */

PTCWriter *PTC_writer_derived_mesh(PTCWriterArchive *_archive, const char *name, Object *ob, DerivedMesh **dm_ptr)
{
	PTC::WriterArchive *archive = (PTC::WriterArchive *)_archive;
	return (PTCWriter *)PTC::Factory::alembic->create_writer_derived_mesh(archive, name, ob, dm_ptr);
}

PTCReader *PTC_reader_derived_mesh(PTCReaderArchive *_archive, const char *name, Object *ob)
{
	PTC::ReaderArchive *archive = (PTC::ReaderArchive *)_archive;
	return (PTCReader *)PTC::Factory::alembic->create_reader_derived_mesh(archive, name, ob);
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


PTCWriter *PTC_writer_derived_final(PTCWriterArchive *_archive, const char *name, Object *ob)
{
	PTC::WriterArchive *archive = (PTC::WriterArchive *)_archive;
	return (PTCWriter *)PTC::Factory::alembic->create_writer_derived_final(archive, name, ob);
}


PTCWriter *PTC_writer_cache_modifier(PTCWriterArchive *_archive, const char *name, Object *ob, CacheModifierData *cmd)
{
	PTC::WriterArchive *archive = (PTC::WriterArchive *)_archive;
	return (PTCWriter *)PTC::Factory::alembic->create_writer_cache_modifier(archive, name, ob, cmd);
}


/* ==== PARTICLES ==== */

PTCWriter *PTC_writer_particles(PTCWriterArchive *_archive, const char *name, Object *ob, ParticleSystem *psys)
{
	PTC::WriterArchive *archive = (PTC::WriterArchive *)_archive;
	return (PTCWriter *)PTC::Factory::alembic->create_writer_particles(archive, name, ob, psys);
}

PTCReader *PTC_reader_particles(PTCReaderArchive *_archive, const char *name, Object *ob, ParticleSystem *psys)
{
	PTC::ReaderArchive *archive = (PTC::ReaderArchive *)_archive;
	return (PTCReader *)PTC::Factory::alembic->create_reader_particles(archive, name, ob, psys);
}

int PTC_reader_particles_totpoint(PTCReader *_reader)
{
	return ((PTC::ParticlesReader *)_reader)->totpoint();
}

PTCWriter *PTC_writer_particles_pathcache_parents(PTCWriterArchive *_archive, const char *name, Object *ob, ParticleSystem *psys)
{
	PTC::WriterArchive *archive = (PTC::WriterArchive *)_archive;
	return (PTCWriter *)PTC::Factory::alembic->create_writer_particles_pathcache_parents(archive, name, ob, psys);
}

PTCReader *PTC_reader_particles_pathcache_parents(PTCReaderArchive *_archive, const char *name, Object *ob, ParticleSystem *psys)
{
	PTC::ReaderArchive *archive = (PTC::ReaderArchive *)_archive;
	return (PTCReader *)PTC::Factory::alembic->create_reader_particles_pathcache_parents(archive, name, ob, psys);
}

PTCWriter *PTC_writer_particles_pathcache_children(PTCWriterArchive *_archive, const char *name, Object *ob, ParticleSystem *psys)
{
	PTC::WriterArchive *archive = (PTC::WriterArchive *)_archive;
	return (PTCWriter *)PTC::Factory::alembic->create_writer_particles_pathcache_children(archive, name, ob, psys);
}

PTCReader *PTC_reader_particles_pathcache_children(PTCReaderArchive *_archive, const char *name, Object *ob, ParticleSystem *psys)
{
	PTC::ReaderArchive *archive = (PTC::ReaderArchive *)_archive;
	return (PTCReader *)PTC::Factory::alembic->create_reader_particles_pathcache_children(archive, name, ob, psys);
}

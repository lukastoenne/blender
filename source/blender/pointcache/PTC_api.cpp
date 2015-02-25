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

#include "alembic.h"
#include "ptc_types.h"
#include "util_path.h"

extern "C" {
#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DNA_cache_library_types.h"
#include "DNA_listBase.h"
#include "DNA_modifier_types.h"

#include "BKE_cache_library.h"
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


PTCWriterArchive *PTC_open_writer_archive(Scene *scene, const char *path)
{
	return (PTCWriterArchive *)abc_writer_archive(scene, path, NULL);
}

void PTC_close_writer_archive(PTCWriterArchive *_archive)
{
	PTC::WriterArchive *archive = (PTC::WriterArchive *)_archive;
	delete archive;
}

PTCReaderArchive *PTC_open_reader_archive(Scene *scene, const char *path)
{
	return (PTCReaderArchive *)abc_reader_archive(scene, path, NULL);
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
#if 0 /* modifier uses internal writer during scene update */
	if (RNA_struct_is_a(ptr->type, &RNA_PointCacheModifier)) {
		Object *ob = (Object *)ptr->id.data;
		PointCacheModifierData *pcmd = (PointCacheModifierData *)ptr->data;
		return PTC_writer_point_cache(scene, ob, pcmd);
	}
#endif
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
	if (RNA_struct_is_a(ptr->type, &RNA_PointCacheModifier)) {
		Object *ob = (Object *)ptr->id.data;
		PointCacheModifierData *pcmd = (PointCacheModifierData *)ptr->data;
		return PTC_reader_point_cache(scene, ob, pcmd);
	}
#endif
	return NULL;
}


PTCReaderArchive *PTC_cachlib_readers(Scene *scene, CacheLibrary *cachelib, ListBase *readers)
{
	std::string filename = ptc_archive_path(cachelib->filepath, (ID *)cachelib, cachelib->id.lib);
	PTCReaderArchive *archive = PTC_open_reader_archive(scene, filename.c_str());
	
	BLI_listbase_clear(readers);
	
	for (CacheItem *item = (CacheItem *)cachelib->items.first; item; item = item->next) {
		if (!(item->flag & CACHE_ITEM_ENABLED))
			continue;
		
		PTCReader *reader = NULL;
		switch (item->type) {
			case CACHE_TYPE_DERIVED_MESH:
//				reader = PTC_reader_point_cache(archive, )
				break;
			default:
				break;
		}
		
		if (reader) {
			LinkData *link = (LinkData *)MEM_callocN(sizeof(LinkData), "cachelib readers link");
			link->data = reader;
			BLI_addtail(readers, link);
		}
	}
	
	return archive;
}

void PTC_cachlib_readers_free(PTCReaderArchive *archive, ListBase *readers)
{
	for (LinkData *link = (LinkData *)readers->first; link; link = link->next) {
		PTCReader *reader = (PTCReader *)link->data;
		PTC_reader_free(reader);
	}
	BLI_freelistN(readers);
	
	PTC_close_reader_archive(archive);
}

PTCWriterArchive *PTC_cachlib_writers(Scene *scene, CacheLibrary *cachelib, ListBase *writers)
{
	std::string filename = ptc_archive_path(cachelib->filepath, (ID *)cachelib, cachelib->id.lib);
	PTCWriterArchive *archive = PTC_open_writer_archive(scene, filename.c_str());
	
	BLI_listbase_clear(writers);
	
	for (CacheItem *item = (CacheItem *)cachelib->items.first; item; item = item->next) {
		char name[2*MAX_NAME];
		
		if (!(item->flag & CACHE_ITEM_ENABLED))
			continue;
		
		BKE_cache_item_name(item->ob, item->type, item->index, name);
		
		PTCWriter *writer = NULL;
		switch (item->type) {
			case CACHE_TYPE_DERIVED_MESH:
//				writer = PTC_writer_point_cache(archive, )
				break;
			case CACHE_TYPE_HAIR: {
				ParticleSystem *psys = (ParticleSystem *)BLI_findlink(&item->ob->particlesystem, item->index);
				if (psys && psys->part && psys->part->type == PART_HAIR && psys->clmd) {
					writer = PTC_writer_cloth(archive, name, item->ob, psys->clmd);
				}
				break;
			};
			default:
				break;
		}
		
		if (writer) {
			LinkData *link = (LinkData *)MEM_callocN(sizeof(LinkData), "cachelib writers link");
			link->data = writer;
			BLI_addtail(writers, link);
		}
	}
	
	return archive;
}

void PTC_cachlib_writers_free(PTCWriterArchive *archive, ListBase *writers)
{
	for (LinkData *link = (LinkData *)writers->first; link; link = link->next) {
		PTCWriter *writer = (PTCWriter *)link->data;
		PTC_writer_free(writer);
	}
	BLI_freelistN(writers);
	
	PTC_close_writer_archive(archive);
}


/* ==== CLOTH ==== */

PTCWriter *PTC_writer_cloth(PTCWriterArchive *_archive, const char *name, Object *ob, ClothModifierData *clmd)
{
	PTC::WriterArchive *archive = (PTC::WriterArchive *)_archive;
	return (PTCWriter *)abc_writer_cloth(archive, name, ob, clmd);
}

PTCReader *PTC_reader_cloth(PTCReaderArchive *_archive, const char *name, Object *ob, ClothModifierData *clmd)
{
	PTC::ReaderArchive *archive = (PTC::ReaderArchive *)_archive;
	return (PTCReader *)abc_reader_cloth(archive, name, ob, clmd);
}


/* ==== MESH ==== */

PTCWriter *PTC_writer_derived_mesh(PTCWriterArchive *_archive, const char *name, Object *ob, DerivedMesh **dm_ptr)
{
	PTC::WriterArchive *archive = (PTC::WriterArchive *)_archive;
	return (PTCWriter *)abc_writer_derived_mesh(archive, name, ob, dm_ptr);
}

PTCReader *PTC_reader_derived_mesh(PTCReaderArchive *_archive, const char *name, Object *ob)
{
	PTC::ReaderArchive *archive = (PTC::ReaderArchive *)_archive;
	return (PTCReader *)abc_reader_derived_mesh(archive, name, ob);
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


PTCWriter *PTC_writer_point_cache(PTCWriterArchive *_archive, const char *name, Object *ob, PointCacheModifierData *pcmd)
{
	PTC::WriterArchive *archive = (PTC::WriterArchive *)_archive;
	return (PTCWriter *)abc_writer_point_cache(archive, name, ob, pcmd);
}

PTCReader *PTC_reader_point_cache(PTCReaderArchive *_archive, const char *name, Object *ob, PointCacheModifierData *pcmd)
{
	PTC::ReaderArchive *archive = (PTC::ReaderArchive *)_archive;
	return (PTCReader *)abc_reader_point_cache(archive, name, ob, pcmd);
}

ePointCacheModifierMode PTC_mod_point_cache_get_mode(PointCacheModifierData *pcmd)
{
	/* can't have simultaneous read and write */
	if (pcmd->writer) {
		BLI_assert(!pcmd->reader);
		return MOD_POINTCACHE_MODE_WRITE;
	}
	else if (pcmd->reader) {
		BLI_assert(!pcmd->writer);
		return MOD_POINTCACHE_MODE_READ;
	}
	else
		return MOD_POINTCACHE_MODE_NONE;
}

#if 0
ePointCacheModifierMode PTC_mod_point_cache_set_mode(Scene *scene, Object *ob, PointCacheModifierData *pcmd, ePointCacheModifierMode mode)
{
	switch (mode) {
		case MOD_POINTCACHE_MODE_READ:
			if (pcmd->writer) {
				PTC_writer_free(pcmd->writer);
				pcmd->writer = NULL;
			}
			if (!pcmd->reader) {
				pcmd->reader = PTC_reader_point_cache(scene, ob, pcmd);
			}
			return pcmd->reader ? MOD_POINTCACHE_MODE_READ : MOD_POINTCACHE_MODE_NONE;
		
		case MOD_POINTCACHE_MODE_WRITE:
			if (pcmd->reader) {
				PTC_reader_free(pcmd->reader);
				pcmd->reader = NULL;
			}
			if (!pcmd->writer) {
				pcmd->writer = PTC_writer_point_cache(scene, ob, pcmd);
			}
			return pcmd->writer ? MOD_POINTCACHE_MODE_WRITE : MOD_POINTCACHE_MODE_NONE;
		
		default:
			if (pcmd->writer) {
				PTC_writer_free(pcmd->writer);
				pcmd->writer = NULL;
			}
			if (pcmd->reader) {
				PTC_reader_free(pcmd->reader);
				pcmd->reader = NULL;
			}
			return MOD_POINTCACHE_MODE_NONE;
	}
}
#endif


/* ==== PARTICLES ==== */

PTCWriter *PTC_writer_particles(PTCWriterArchive *_archive, const char *name, Object *ob, ParticleSystem *psys)
{
	PTC::WriterArchive *archive = (PTC::WriterArchive *)_archive;
	return (PTCWriter *)abc_writer_particles(archive, name, ob, psys);
}

PTCReader *PTC_reader_particles(PTCReaderArchive *_archive, const char *name, Object *ob, ParticleSystem *psys)
{
	PTC::ReaderArchive *archive = (PTC::ReaderArchive *)_archive;
	return (PTCReader *)abc_reader_particles(archive, name, ob, psys);
}

int PTC_reader_particles_totpoint(PTCReader *_reader)
{
	return ((PTC::ParticlesReader *)_reader)->totpoint();
}

PTCWriter *PTC_writer_particle_paths(PTCWriterArchive *_archive, const char *name, Object *ob, ParticleSystem *psys)
{
	PTC::WriterArchive *archive = (PTC::WriterArchive *)_archive;
	return (PTCWriter *)abc_writer_particle_paths(archive, name, ob, psys);
}

PTCReader *PTC_reader_particle_paths(PTCReaderArchive *_archive, const char *name, Object *ob, ParticleSystem *psys, eParticlePathsMode mode)
{
	PTC::ReaderArchive *archive = (PTC::ReaderArchive *)_archive;
	return (PTCReader *)abc_reader_particle_paths(archive, name, ob, psys, mode);
}

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
 * Copyright 2013, Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "PTC_api.h"

#include "util/util_error_handler.h"

#include "reader.h"
#include "writer.h"

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

class StubFactory : public Factory {
	const std::string &get_default_extension() { static std::string ext = ""; return ext; }
	WriterArchive *open_writer_archive(Scene */*scene*/, const std::string &/*name*/, ErrorHandler */*error_handler*/) { return NULL; }
	ReaderArchive *open_reader_archive(Scene */*scene*/, const std::string &/*name*/, ErrorHandler */*error_handler*/) { return NULL; }
	Writer *create_writer_duplicache(const std::string &/*name*/, Group */*group*/, DupliCache */*dupcache*/, int /*datatypes*/, bool /*do_sim_debug*/) { return NULL; }
	Reader *create_reader_duplicache(const std::string &/*name*/, Group */*group*/, DupliCache */*dupcache*/, bool /*read_strands_motion*/, bool /*read_strands_children*/, bool /*do_sim_debug*/) { return NULL; }
	Reader *create_reader_duplicache_object(const std::string &/*name*/, Object */*ob*/, DupliObjectData */*data*/, bool /*read_strands_motion*/, bool /*read_strands_children*/) { return NULL; }
};

#ifndef WITH_PTC_ALEMBIC
void PTC_alembic_init()
{
	static StubFactory stub_factory;
	PTC::Factory::alembic = &stub_factory;
}
#endif

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

void PTC_writer_archive_set_pass(PTCWriterArchive *_archive, PTCPass pass)
{
	PTC::WriterArchive *archive = (PTC::WriterArchive *)_archive;
	archive->set_pass(pass);
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

void PTC_reader_archive_set_pass(PTCReaderArchive *_archive, PTCPass pass)
{
	PTC::ReaderArchive *archive = (PTC::ReaderArchive *)_archive;
	archive->set_pass(pass);
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

void PTC_get_archive_info(PTCReaderArchive *_archive, void (*stream)(void *, const char *), void *userdata)
{
	PTC::ReaderArchive *archive = (PTC::ReaderArchive *)_archive;
	archive->get_info(stream, userdata);
}


PTCWriter *PTC_writer_duplicache(const char *name, struct Group *group, struct DupliCache *dupcache, int datatypes, bool do_sim_debug)
{
	return (PTCWriter *)PTC::Factory::alembic->create_writer_duplicache(name, group, dupcache, datatypes, do_sim_debug);
}

PTCReader *PTC_reader_duplicache(const char *name, struct Group *group, struct DupliCache *dupcache,
                                 bool read_strands_motion, bool read_strands_children, bool read_sim_debug)
{
	return (PTCReader *)PTC::Factory::alembic->create_reader_duplicache(name, group, dupcache,
	                                                                    read_strands_motion, read_strands_children, read_sim_debug);
}

PTCReader *PTC_reader_duplicache_object(const char *name, struct Object *ob, struct DupliObjectData *data,
                                        bool read_strands_motion, bool read_strands_children)
{
	return (PTCReader *)PTC::Factory::alembic->create_reader_duplicache_object(name, ob, data, read_strands_motion, read_strands_children);
}

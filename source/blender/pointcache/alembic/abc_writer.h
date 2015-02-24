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

#ifndef PTC_ABC_WRITER_H
#define PTC_ABC_WRITER_H

#include <string>

#include <Alembic/Abc/OArchive.h>

#include "writer.h"

#include "abc_frame_mapper.h"

#include "util_error_handler.h"

struct Scene;

namespace PTC {

using namespace Alembic;

class AbcWriterArchive : public WriterArchive, public FrameMapper {
public:
	AbcWriterArchive(Scene *scene, const std::string &filename, ErrorHandler *error_handler);
	virtual ~AbcWriterArchive();
	
	uint32_t frame_sampling_index() const { return m_frame_sampling; }
	Abc::TimeSamplingPtr frame_sampling();
	
	Abc::OArchive archive;
	
protected:
	ErrorHandler *m_error_handler;
	uint32_t m_frame_sampling;
};

class AbcWriter {
public:
	AbcWriter(AbcWriterArchive *archive) :
	    m_archive(archive)
	{}
	
	AbcWriterArchive *archive() const { return m_archive; }
	
private:
	AbcWriterArchive *m_archive;
};

} /* namespace PTC */

#endif  /* PTC_WRITER_H */

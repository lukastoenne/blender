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

//#include <Alembic/AbcCoreHDF5/ReadWrite.h>
#include <Alembic/AbcCoreHDF5/ReadWrite.h>
#include <Alembic/AbcCoreOgawa/ReadWrite.h>

#include "abc_writer.h"

#include "util_error_handler.h"

extern "C" {
#include "BLI_fileops.h"
#include "BLI_path_util.h"

#include "DNA_scene_types.h"
}

namespace PTC {

using namespace Abc;

/* make sure the file's directory exists */
static void ensure_directory(const char *filename)
{
	char dir[FILE_MAXDIR];
	BLI_split_dir_part(filename, dir, sizeof(dir));
	BLI_dir_create_recursive(dir);
}

AbcWriterArchive::AbcWriterArchive(Scene *scene, const std::string &filename, ErrorHandler *error_handler) :
    FrameMapper(scene),
    m_error_handler(error_handler)
{
	ensure_directory(filename.c_str());
	
	PTC_SAFE_CALL_BEGIN
//	archive = OArchive(AbcCoreHDF5::WriteArchive(), filename, Abc::ErrorHandler::kThrowPolicy);
	archive = OArchive(AbcCoreOgawa::WriteArchive(), filename, Abc::ErrorHandler::kThrowPolicy);
	
	chrono_t cycle_time = this->seconds_per_frame();
	chrono_t start_time = this->start_time();
	m_frame_sampling = archive.addTimeSampling(TimeSampling(cycle_time, start_time));
	
	PTC_SAFE_CALL_END_HANDLER(m_error_handler)
}

AbcWriterArchive::~AbcWriterArchive()
{
}

TimeSamplingPtr AbcWriterArchive::frame_sampling()
{
	return archive.getTimeSampling(m_frame_sampling);
}

} /* namespace PTC */

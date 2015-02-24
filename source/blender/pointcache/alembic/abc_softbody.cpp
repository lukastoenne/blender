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

#include "alembic.h"
#include "abc_softbody.h"
#include "util_path.h"

extern "C" {
#include "DNA_object_types.h"
#include "DNA_object_force.h"
}

#include "PTC_api.h"

namespace PTC {

using namespace Abc;
using namespace AbcGeom;

AbcSoftBodyWriter::AbcSoftBodyWriter(AbcWriterArchive *archive, Object *ob, SoftBody *softbody) :
    SoftBodyWriter(ob, softbody, archive),
    AbcWriter(archive)
{
	if (archive->archive) {
//		OObject root = archive->archive.getTop();
//		m_points = OPoints(root, m_psys->name, archive->frame_sampling_index());
	}
}

AbcSoftBodyWriter::~AbcSoftBodyWriter()
{
}

void AbcSoftBodyWriter::write_sample()
{
	if (!archive()->archive)
		return;
}


AbcSoftBodyReader::AbcSoftBodyReader(AbcReaderArchive *archive, Object *ob, SoftBody *softbody) :
    SoftBodyReader(ob, softbody, archive),
    AbcReader(archive)
{
	if (archive->archive.valid()) {
		IObject root = archive->archive.getTop();
//		m_points = IPoints(root, m_psys->name);
	}
}

AbcSoftBodyReader::~AbcSoftBodyReader()
{
}

PTCReadSampleResult AbcSoftBodyReader::read_sample(float frame)
{
	return PTC_READ_SAMPLE_INVALID;
}

/* ==== API ==== */

Writer *abc_writer_softbody(WriterArchive *archive, Object *ob, SoftBody *softbody)
{
	BLI_assert(dynamic_cast<AbcWriterArchive *>(archive));
	return new AbcSoftBodyWriter((AbcWriterArchive *)archive, ob, softbody);
}

Reader *abc_reader_softbody(ReaderArchive *archive, Object *ob, SoftBody *softbody)
{
	BLI_assert(dynamic_cast<AbcReaderArchive *>(archive));
	return new AbcSoftBodyReader((AbcReaderArchive *)archive, ob, softbody);
}

} /* namespace PTC */

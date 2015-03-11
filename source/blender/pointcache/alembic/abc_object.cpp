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

#include "abc_object.h"

extern "C" {
#include "BLI_math.h"

#include "DNA_object_types.h"

#include "BKE_object.h"
}

namespace PTC {

using namespace Abc;
using namespace AbcGeom;

AbcObjectWriter::AbcObjectWriter(const std::string &name, Object *ob) :
    ObjectWriter(ob, name)
{
}

void AbcObjectWriter::open_archive(WriterArchive *archive)
{
	BLI_assert(dynamic_cast<AbcWriterArchive*>(archive));
	AbcWriter::abc_archive(static_cast<AbcWriterArchive*>(archive));
	
	if (abc_archive()->archive) {
		m_abc_object = abc_archive()->add_id_object<OObject>((ID *)m_ob);
	}
}

void AbcObjectWriter::create_refs()
{
	if ((m_ob->transflag & OB_DUPLIGROUP) && m_ob->dup_group) {
		OObject abc_group = abc_archive()->get_id_object((ID *)m_ob->dup_group);
		if (abc_group)
			m_abc_object.addChildInstance(abc_group, "dup_group");
	}
}

void AbcObjectWriter::write_sample()
{
	if (!abc_archive()->archive)
		return;
}


AbcObjectReader::AbcObjectReader(const std::string &name, Object *ob) :
    ObjectReader(ob, name)
{
}

void AbcObjectReader::open_archive(ReaderArchive *archive)
{
	BLI_assert(dynamic_cast<AbcReaderArchive*>(archive));
	AbcReader::abc_archive(static_cast<AbcReaderArchive*>(archive));
	
	if (abc_archive()->archive) {
		m_abc_object = abc_archive()->get_id_object((ID *)m_ob);
	}
}

PTCReadSampleResult AbcObjectReader::read_sample(float frame)
{
	if (!m_abc_object)
		return PTC_READ_SAMPLE_INVALID;
	
	return PTC_READ_SAMPLE_EXACT;
}

} /* namespace PTC */

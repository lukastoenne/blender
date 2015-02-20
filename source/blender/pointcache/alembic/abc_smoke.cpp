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
#include "abc_smoke.h"
#include "util_path.h"

extern "C" {
#include "DNA_object_types.h"
#include "DNA_smoke_types.h"
}

#include "PTC_api.h"

namespace PTC {

using namespace Abc;
using namespace AbcGeom;

AbcSmokeWriter::AbcSmokeWriter(Scene *scene, Object *ob, SmokeDomainSettings *domain) :
    SmokeWriter(scene, ob, domain, &m_archive),
    m_archive(scene, ptc_archive_path("//blendcache/", &ob->id, ob->id.lib), m_error_handler)
{
	if (m_archive.archive) {
//		OObject root = m_archive.archive.getTop();
//		m_points = OPoints(root, m_psys->name, m_archive.frame_sampling_index());
	}
}

AbcSmokeWriter::~AbcSmokeWriter()
{
}

void AbcSmokeWriter::write_sample()
{
	if (!m_archive.archive)
		return;
}


AbcSmokeReader::AbcSmokeReader(Scene *scene, Object *ob, SmokeDomainSettings *domain) :
    SmokeReader(scene, ob, domain, &m_archive),
    m_archive(scene, ptc_archive_path("//blendcache/", &ob->id, ob->id.lib), m_error_handler)
{
	if (m_archive.archive.valid()) {
		IObject root = m_archive.archive.getTop();
//		m_points = IPoints(root, m_psys->name);
	}
}

AbcSmokeReader::~AbcSmokeReader()
{
}

PTCReadSampleResult AbcSmokeReader::read_sample(float frame)
{
	return PTC_READ_SAMPLE_INVALID;
}

/* ==== API ==== */

Writer *abc_writer_smoke(Scene *scene, Object *ob, SmokeDomainSettings *domain)
{
	return new AbcSmokeWriter(scene, ob, domain);
}

Reader *abc_reader_smoke(Scene *scene, Object *ob, SmokeDomainSettings *domain)
{
	return new AbcSmokeReader(scene, ob, domain);
}

} /* namespace PTC */

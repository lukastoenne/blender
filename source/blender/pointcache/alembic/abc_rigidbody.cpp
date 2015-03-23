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

#include "abc_rigidbody.h"

extern "C" {
#include "DNA_scene_types.h"
#include "DNA_rigidbody_types.h"
}

#include "PTC_api.h"

namespace PTC {

using namespace Abc;
using namespace AbcGeom;

AbcRigidBodyWriter::AbcRigidBodyWriter(Scene *scene, RigidBodyWorld *rbw) :
    RigidBodyWriter(scene, rbw, &m_archive),
    m_archive(scene, &scene->id, rbw->pointcache, m_error_handler)
{
	if (m_archive.archive) {
	}
}

AbcRigidBodyWriter::~AbcRigidBodyWriter()
{
}

void AbcRigidBodyWriter::write_sample()
{
	if (!m_archive.archive)
		return;
}


AbcRigidBodyReader::AbcRigidBodyReader(Scene *scene, RigidBodyWorld *rbw) :
    RigidBodyReader(scene, rbw, &m_archive),
    m_archive(scene, &scene->id, rbw->pointcache, m_error_handler)
{
	if (m_archive.archive.valid()) {
	}
}

AbcRigidBodyReader::~AbcRigidBodyReader()
{
}

PTCReadSampleResult AbcRigidBodyReader::read_sample(float frame)
{
	return PTC_READ_SAMPLE_INVALID;
}

/* ==== API ==== */

Writer *abc_writer_rigidbody(Scene *scene, RigidBodyWorld *rbw)
{
	return new AbcRigidBodyWriter(scene, rbw);
}

Reader *abc_reader_rigidbody(Scene *scene, RigidBodyWorld *rbw)
{
	return new AbcRigidBodyReader(scene, rbw);
}

} /* namespace PTC */

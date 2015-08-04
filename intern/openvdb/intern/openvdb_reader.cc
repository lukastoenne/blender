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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "openvdb_reader.h"

#define COPY_MAX_BYTES 10485760 /* 10 Mb */

OpenVDBReader::OpenVDBReader()
    : m_meta_map(new openvdb::MetaMap)
{
	/* Although it is safe, it may not be good to have this here... */
	openvdb::initialize();
	m_file = NULL;
}

OpenVDBReader::~OpenVDBReader()
{
	cleanup_file();
}

void OpenVDBReader::open(const openvdb::Name &filename)
{
	cleanup_file();

	m_file = new openvdb::io::File(filename);
	m_file->setCopyMaxBytes(COPY_MAX_BYTES);
	m_file->open();

	m_meta_map = m_file->getMetadata();
}

void OpenVDBReader::floatMeta(const openvdb::Name &name, float &value)
{
	value = m_meta_map->metaValue<float>(name);
}

void OpenVDBReader::intMeta(const openvdb::Name &name, int &value)
{
	value = m_meta_map->metaValue<int>(name);
}

void OpenVDBReader::vec3sMeta(const openvdb::Name &name, float value[3])
{
	openvdb::Vec3s meta_val = m_meta_map->metaValue<openvdb::Vec3s>(name);

	value[0] = meta_val.x();
	value[1] = meta_val.y();
	value[2] = meta_val.z();
}

void OpenVDBReader::vec3IMeta(const openvdb::Name &name, int value[3])
{
	openvdb::Vec3i meta_val = m_meta_map->metaValue<openvdb::Vec3i>(name);

	value[0] = meta_val.x();
	value[1] = meta_val.y();
	value[2] = meta_val.z();
}

void OpenVDBReader::mat4sMeta(const openvdb::Name &name, float value[4][4])
{
	openvdb::Mat4s meta_val = m_meta_map->metaValue<openvdb::Mat4s>(name);

	for (int i =  0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			value[i][j] = meta_val[i][j];
		}
	}
}

openvdb::GridBase::Ptr OpenVDBReader::getGrid(const openvdb::Name &name)
{
	return m_file->readGrid(name);
}

size_t OpenVDBReader::numGrids() const
{
	return m_file->getGrids()->size();
}

void OpenVDBReader::cleanup_file()
{
	if (m_file) {
		m_file->close();
		delete m_file;
	}
}

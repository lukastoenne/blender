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

#ifndef PTC_ABC_OBJECT_H
#define PTC_ABC_OBJECT_H

#include <Alembic/Abc/IObject.h>
#include <Alembic/Abc/OObject.h>
#include <Alembic/AbcGeom/IXform.h>
#include <Alembic/AbcGeom/OXform.h>

#include "ptc_types.h"

#include "abc_reader.h"
#include "abc_schema.h"
#include "abc_writer.h"

struct Object;

namespace PTC {

class AbcObjectWriter : public ObjectWriter, public AbcWriter {
public:
	AbcObjectWriter(const std::string &name, Object *ob);
	
	void open_archive(WriterArchive *archive);
	void create_refs();
	
	void write_sample();
	
private:
	Abc::OObject m_abc_object;
};

class AbcObjectReader : public ObjectReader, public AbcReader {
public:
	AbcObjectReader(const std::string &name, Object *ob);
	
	void open_archive(ReaderArchive *archive);
	
	PTCReadSampleResult read_sample(float frame);
	
private:
	Abc::IObject m_abc_object;
};

} /* namespace PTC */

#endif  /* PTC_OBJECT_H */

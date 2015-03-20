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
#include "abc_mesh.h"

struct DerivedMesh;
struct Object;
struct Scene;

namespace PTC {

class AbcObjectWriter : public ObjectWriter, public AbcWriter {
public:
	AbcObjectWriter(const std::string &name, Scene *scene, Object *ob);
	
	void init_abc();
#if 0
	void create_refs();
#endif
	
	void write_sample();
	
private:
	Scene *m_scene;
	DerivedMesh *m_final_dm;
	
	Abc::OObject m_abc_object;
	AbcDerivedMeshWriter m_dm_writer;
};

class AbcObjectReader : public ObjectReader, public AbcReader {
public:
	AbcObjectReader(const std::string &name, Object *ob);
	
	void init_abc();
	
	PTCReadSampleResult read_sample(float frame);
	
private:
	Abc::IObject m_abc_object;
};

} /* namespace PTC */

#endif  /* PTC_OBJECT_H */

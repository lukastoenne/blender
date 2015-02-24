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

#ifndef PTC_ABC_RIGIDBODY_H
#define PTC_ABC_RIGIDBODY_H

//#include <Alembic/AbcGeom/IPoints.h>
//#include <Alembic/AbcGeom/OPoints.h>

#include "ptc_types.h"

#include "abc_reader.h"
#include "abc_schema.h"
#include "abc_writer.h"

struct RigidBodyWorld;

namespace PTC {

class AbcRigidBodyWriter : public RigidBodyWriter, public AbcWriter {
public:
	AbcRigidBodyWriter(AbcWriterArchive *archive, Scene *scene, RigidBodyWorld *rbw);
	~AbcRigidBodyWriter();
	
	void write_sample();
	
private:
//	AbcGeom::OPoints m_points;
};

class AbcRigidBodyReader : public RigidBodyReader, public AbcReader {
public:
	AbcRigidBodyReader(AbcReaderArchive *archive, Scene *scene, RigidBodyWorld *rbw);
	~AbcRigidBodyReader();
	
	PTCReadSampleResult read_sample(float frame);
	
private:
//	AbcGeom::IPoints m_points;
};

} /* namespace PTC */

#endif  /* PTC_RIGIDBODY_H */

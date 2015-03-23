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

#ifndef PTC_ABC_SOFTBODY_H
#define PTC_ABC_SOFTBODY_H

//#include <Alembic/AbcGeom/IPoints.h>
//#include <Alembic/AbcGeom/OPoints.h>

#include "ptc_types.h"

#include "abc_reader.h"
#include "abc_schema.h"
#include "abc_writer.h"

struct Object;
struct SoftBody;

namespace PTC {

class AbcSoftBodyWriter : public SoftBodyWriter {
public:
	AbcSoftBodyWriter(Scene *scene, Object *ob, SoftBody *softbody);
	~AbcSoftBodyWriter();
	
	void write_sample();
	
private:
	AbcWriterArchive m_archive;
//	AbcGeom::OPoints m_points;
};

class AbcSoftBodyReader : public SoftBodyReader {
public:
	AbcSoftBodyReader(Scene *scene, Object *ob, SoftBody *softbody);
	~AbcSoftBodyReader();
	
	PTCReadSampleResult read_sample(float frame);
	
private:
	AbcReaderArchive m_archive;
//	AbcGeom::IPoints m_points;
};

} /* namespace PTC */

#endif  /* PTC_SOFTBODY_H */

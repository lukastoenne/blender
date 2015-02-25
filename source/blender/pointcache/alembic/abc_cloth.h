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

#ifndef PTC_ABC_CLOTH_H
#define PTC_ABC_CLOTH_H

#include <Alembic/AbcGeom/IPoints.h>
#include <Alembic/AbcGeom/OPoints.h>

#include "ptc_types.h"

#include "abc_reader.h"
#include "abc_schema.h"
#include "abc_writer.h"

struct Object;
struct ClothModifierData;

namespace PTC {

class AbcClothWriter : public ClothWriter, public AbcWriter {
public:
	AbcClothWriter(AbcWriterArchive *archive, const std::string &name, Object *ob, ClothModifierData *clmd);
	~AbcClothWriter();
	
	void write_sample();
	
private:
	AbcGeom::OPoints m_points;
	AbcGeom::OV3fGeomParam m_param_velocities;
	AbcGeom::OP3fGeomParam m_param_goal_positions;
};

class AbcClothReader : public ClothReader, public AbcReader {
public:
	AbcClothReader(AbcReaderArchive *archive, const std::string &name, Object *ob, ClothModifierData *clmd);
	~AbcClothReader();
	
	PTCReadSampleResult read_sample(float frame);
	
private:
	AbcGeom::IPoints m_points;
	AbcGeom::IV3fGeomParam m_param_velocities;
	AbcGeom::IP3fGeomParam m_param_goal_positions;
};


/* Hair is just a cloth sim in disguise ... */

class AbcHairDynamicsWriter : public AbcClothWriter {
public:
	AbcHairDynamicsWriter(AbcWriterArchive *archive, const std::string &name, Object *ob, ClothModifierData *clmd) :
	    AbcClothWriter(archive, name, ob, clmd)
	{}
};

class AbcHairDynamicsReader : public AbcClothReader {
public:
	AbcHairDynamicsReader(AbcReaderArchive *archive, const std::string &name, Object *ob, ClothModifierData *clmd) :
	    AbcClothReader(archive, name, ob, clmd)
	{}
	
	PTCReadSampleResult read_sample(float frame) { return AbcClothReader::read_sample(frame); }
};

} /* namespace PTC */

#endif  /* PTC_CLOTH_H */

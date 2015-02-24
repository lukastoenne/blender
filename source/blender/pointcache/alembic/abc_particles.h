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

#ifndef PTC_ABC_PARTICLES_H
#define PTC_ABC_PARTICLES_H

#include <Alembic/AbcGeom/IPoints.h>
#include <Alembic/AbcGeom/OPoints.h>
#include <Alembic/AbcGeom/ICurves.h>
#include <Alembic/AbcGeom/OCurves.h>

#include "ptc_types.h"

#include "PTC_api.h"

#include "abc_reader.h"
#include "abc_schema.h"
#include "abc_writer.h"

struct Object;
struct ParticleSystem;
struct ParticleCacheKey;

namespace PTC {

class AbcClothWriter;

class AbcParticlesWriter : public ParticlesWriter, public AbcWriter {
public:
	AbcParticlesWriter(AbcWriterArchive *archive, Object *ob, ParticleSystem *psys);
	~AbcParticlesWriter();
	
	void write_sample();
	
private:
	AbcGeom::OPoints m_points;
};

class AbcParticlesReader : public ParticlesReader, public AbcReader {
public:
	AbcParticlesReader(AbcReaderArchive *archive, Object *ob, ParticleSystem *psys);
	~AbcParticlesReader();
	
	PTCReadSampleResult read_sample(float frame);
	
private:
	AbcGeom::IPoints m_points;
};

class AbcParticlePathsWriter : public ParticlesWriter, public AbcWriter {
public:
	AbcParticlePathsWriter(AbcWriterArchive *archive, Object *ob, ParticleSystem *psys, ParticleCacheKey ***pathcache, int *totpath, const std::string &suffix);
	~AbcParticlePathsWriter();
	
	void write_sample();
	
private:
	ParticleCacheKey ***m_pathcache;
	int *m_totpath;
	std::string m_suffix;
	
	AbcGeom::OCurves m_curves;
	
	AbcGeom::OV3fGeomParam m_param_velocities;
	AbcGeom::OQuatfGeomParam m_param_rotations;
	AbcGeom::OV3fGeomParam m_param_colors;
	AbcGeom::OFloatGeomParam m_param_times;
};

class AbcParticlePathsReader : public ParticlesReader, public AbcReader {
public:
	AbcParticlePathsReader(AbcReaderArchive *archive, Object *ob, ParticleSystem *psys, ParticleCacheKey ***pathcache, int *totpath, const std::string &suffix);
	~AbcParticlePathsReader();
	
	PTCReadSampleResult read_sample(float frame);
	
private:
	ParticleCacheKey ***m_pathcache;
	int *m_totpath;
	std::string m_suffix;
	
	AbcGeom::ICurves m_curves;
	
	AbcGeom::IV3fGeomParam m_param_velocities;
	AbcGeom::IQuatfGeomParam m_param_rotations;
	AbcGeom::IV3fGeomParam m_param_colors;
	AbcGeom::IFloatGeomParam m_param_times;
};

} /* namespace PTC */

#endif  /* PTC_PARTICLES_H */

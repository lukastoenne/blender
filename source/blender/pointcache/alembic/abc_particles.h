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
	AbcParticlesWriter(AbcWriterArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys);
	~AbcParticlesWriter();
	
	void write_sample();
	
private:
	AbcGeom::OPoints m_points;
};

class AbcParticlesReader : public ParticlesReader, public AbcReader {
public:
	AbcParticlesReader(AbcReaderArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys);
	~AbcParticlesReader();
	
	PTCReadSampleResult read_sample(float frame);
	
private:
	AbcGeom::IPoints m_points;
};

class AbcParticlePathcacheWriter : public ParticlesWriter, public AbcWriter {
protected:
	AbcParticlePathcacheWriter(AbcWriterArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys, ParticleCacheKey ***pathcache, int *totpath, const std::string &suffix);
	~AbcParticlePathcacheWriter();
	
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

class AbcParticlePathcacheReader : public ParticlesReader, public AbcReader {
protected:
	AbcParticlePathcacheReader(AbcReaderArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys, ParticleCacheKey ***pathcache, int *totpath, const std::string &suffix);
	
	PTCReadSampleResult read_sample(float frame);
	
protected:
	ParticleCacheKey ***m_pathcache;
	int *m_totpath;
	std::string m_suffix;
	
	AbcGeom::ICurves m_curves;
	
	AbcGeom::IV3fGeomParam m_param_velocities;
	AbcGeom::IQuatfGeomParam m_param_rotations;
	AbcGeom::IV3fGeomParam m_param_colors;
	AbcGeom::IFloatGeomParam m_param_times;
};

class AbcParticlePathcacheParentsWriter : public AbcParticlePathcacheWriter {
public:
	AbcParticlePathcacheParentsWriter(AbcWriterArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys) :
	    AbcParticlePathcacheWriter(archive, name, ob, psys, &psys->pathcache, &psys->totpart, "__parents")
	{}
};

class AbcParticlePathcacheParentsReader : public AbcParticlePathcacheReader {
public:
	AbcParticlePathcacheParentsReader(AbcReaderArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys) :
	    AbcParticlePathcacheReader(archive, name, ob, psys, &psys->pathcache, &psys->totpart, "__parents")
	{}
};

class AbcParticlePathcacheChildrenWriter : public AbcParticlePathcacheWriter {
public:
	AbcParticlePathcacheChildrenWriter(AbcWriterArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys) :
	    AbcParticlePathcacheWriter(archive, name, ob, psys, &psys->childcache, &psys->totchild, "__children")
	{}
};

class AbcParticlePathcacheChildrenReader : public AbcParticlePathcacheReader {
public:
	AbcParticlePathcacheChildrenReader(AbcReaderArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys) :
	    AbcParticlePathcacheReader(archive, name, ob, psys, &psys->childcache, &psys->totchild, "__children")
	{}
};

} /* namespace PTC */

#endif  /* PTC_PARTICLES_H */

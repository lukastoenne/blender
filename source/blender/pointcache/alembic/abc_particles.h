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
#include "abc_cloth.h"

struct ListBase;
struct Object;
struct ParticleSystem;
struct ParticleCacheKey;
struct Strands;
struct StrandsChildren;

namespace PTC {

class AbcDerivedMeshWriter;
class AbcDerivedMeshReader;

class AbcParticlesWriter : public ParticlesWriter, public AbcWriter {
public:
	AbcParticlesWriter(const std::string &name, Object *ob, ParticleSystem *psys);
	~AbcParticlesWriter();
	
	void init_abc(Abc::OObject parent);
	
	void write_sample();
	
private:
	AbcGeom::OPoints m_points;
};

class AbcParticlesReader : public ParticlesReader, public AbcReader {
public:
	AbcParticlesReader(const std::string &name, Object *ob, ParticleSystem *psys);
	~AbcParticlesReader();
	
	void init_abc(Abc::IObject object);
	
	PTCReadSampleResult read_sample(float frame);
	
private:
	AbcGeom::IPoints m_points;
};


class AbcHairChildrenWriter : public ParticlesWriter, public AbcWriter {
public:
	AbcHairChildrenWriter(const std::string &name, Object *ob, ParticleSystem *psys);
	~AbcHairChildrenWriter();
	
	void init_abc(Abc::OObject parent);
	
	void write_sample();
	
private:
	AbcGeom::OCurves m_curves;
	AbcGeom::OFloatGeomParam m_param_times;
};


class AbcHairWriter : public ParticlesWriter, public AbcWriter {
public:
	AbcHairWriter(const std::string &name, Object *ob, ParticleSystem *psys);
	~AbcHairWriter();
	
	void init(WriterArchive *archive);
	void init_abc(Abc::OObject parent);
	
	void write_sample();
	
private:
	ParticleSystemModifierData *m_psmd;
	
	AbcGeom::OCurves m_curves;
	AbcGeom::OM33fGeomParam m_param_root_matrix;
	AbcGeom::OFloatGeomParam m_param_times;
	AbcGeom::OFloatGeomParam m_param_weights;
	
	AbcHairChildrenWriter m_child_writer;
};


class AbcStrandsChildrenWriter : public AbcWriter {
public:
	AbcStrandsChildrenWriter(const std::string &name, const std::string &abc_name, DupliObjectData *dobdata);
	
	StrandsChildren *get_strands() const;
	
	void init_abc(Abc::OObject parent);
	
	void write_sample();
	
private:
	std::string m_name;
	std::string m_abc_name;
	DupliObjectData *m_dobdata;
	
	AbcGeom::OCurves m_curves;
	AbcGeom::OFloatGeomParam m_param_times;
};


class AbcStrandsWriter : public AbcWriter {
public:
	AbcStrandsWriter(const std::string &name, DupliObjectData *dobdata);
	
	Strands *get_strands() const;
	
	void init(WriterArchive *archive);
	void init_abc(Abc::OObject parent);
	
	void write_sample();
	
private:
	std::string m_name;
	DupliObjectData *m_dobdata;
	
	AbcGeom::OCurves m_curves;
	AbcGeom::OM33fGeomParam m_param_root_matrix;
	AbcGeom::OFloatGeomParam m_param_times;
	AbcGeom::OFloatGeomParam m_param_weights;
	AbcGeom::OCompoundProperty m_param_motion_state;
	AbcGeom::OP3fGeomParam m_param_motion_co;
	AbcGeom::OV3fGeomParam m_param_motion_vel;
	
	AbcStrandsChildrenWriter m_child_writer;
};


class AbcStrandsChildrenReader : public AbcReader {
public:
	AbcStrandsChildrenReader(StrandsChildren *strands);
	~AbcStrandsChildrenReader();
	
	void init_abc(Abc::IObject object);
	
	PTCReadSampleResult read_sample(float frame);
	
	StrandsChildren *acquire_result();
	void discard_result();
	
private:
	StrandsChildren *m_strands;
	
	AbcGeom::ICurves m_curves;
	AbcGeom::IFloatGeomParam m_param_times;
};


class AbcStrandsReader : public AbcReader {
public:
	AbcStrandsReader(Strands *strands, StrandsChildren *children);
	~AbcStrandsReader();
	
	void init(ReaderArchive *archive);
	void init_abc(Abc::IObject object);
	
	PTCReadSampleResult read_sample(float frame);
	
	Strands *acquire_result();
	void discard_result();
	
	AbcStrandsChildrenReader &child_reader() { return m_child_reader; }
	
private:
	Strands *m_strands;
	
	AbcGeom::ICurves m_curves;
	AbcGeom::IM33fGeomParam m_param_root_matrix;
	AbcGeom::IFloatGeomParam m_param_times;
	AbcGeom::IFloatGeomParam m_param_weights;
	AbcGeom::ICompoundProperty m_param_motion_state;
	AbcGeom::IP3fGeomParam m_param_motion_co;
	AbcGeom::IV3fGeomParam m_param_motion_vel;
	
	AbcStrandsChildrenReader m_child_reader;
};


/* Hair is just a cloth sim in disguise ... */

class AbcHairDynamicsWriter : public ParticlesWriter, public AbcWriter {
public:
	AbcHairDynamicsWriter(const std::string &name, Object *ob, ParticleSystem *psys);
	
	void init_abc(Abc::OObject parent);
	
	void write_sample();
	
private:
	AbcClothWriter m_cloth_writer;
};

class AbcHairDynamicsReader : public ParticlesReader, public AbcReader {
public:
	AbcHairDynamicsReader(const std::string &name, Object *ob, ParticleSystem *psys);
	
	void init_abc(Abc::IObject object);
	
	PTCReadSampleResult read_sample(float frame);
	
private:
	AbcClothReader m_cloth_reader;
};


class AbcParticlePathcacheWriter : public ParticlesWriter, public AbcWriter {
protected:
	AbcParticlePathcacheWriter(const std::string &name, Object *ob, ParticleSystem *psys, ParticleCacheKey ***pathcache, int *totpath, const std::string &suffix);
	~AbcParticlePathcacheWriter();
	
	void init_abc(Abc::OObject parent);
	
	void write_sample();
	
private:
	ParticleCacheKey ***m_pathcache;
	int *m_totpath;
	std::string m_suffix;
	
	AbcGeom::OCurves m_curves;
	
	AbcGeom::OV3fGeomParam m_param_velocities;
	AbcGeom::OQuatfGeomParam m_param_rotations;
	AbcGeom::OC3fGeomParam m_param_colors;
	AbcGeom::OFloatGeomParam m_param_times;
};

class AbcParticlePathcacheReader : public ParticlesReader, public AbcReader {
protected:
	AbcParticlePathcacheReader(const std::string &name, Object *ob, ParticleSystem *psys, ParticleCacheKey ***pathcache, int *totpath, const std::string &suffix);
	
	void init_abc(Abc::IObject object);
	
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
	AbcParticlePathcacheParentsWriter(const std::string &name, Object *ob, ParticleSystem *psys) :
	    AbcParticlePathcacheWriter(name, ob, psys, &psys->pathcache, &psys->totpart, "__parents")
	{}
};

class AbcParticlePathcacheParentsReader : public AbcParticlePathcacheReader {
public:
	AbcParticlePathcacheParentsReader(const std::string &name, Object *ob, ParticleSystem *psys) :
	    AbcParticlePathcacheReader(name, ob, psys, &psys->pathcache, &psys->totpart, "__parents")
	{}
};

class AbcParticlePathcacheChildrenWriter : public AbcParticlePathcacheWriter {
public:
	AbcParticlePathcacheChildrenWriter(const std::string &name, Object *ob, ParticleSystem *psys) :
	    AbcParticlePathcacheWriter(name, ob, psys, &psys->childcache, &psys->totchild, "__children")
	{}
};

class AbcParticlePathcacheChildrenReader : public AbcParticlePathcacheReader {
public:
	AbcParticlePathcacheChildrenReader(const std::string &name, Object *ob, ParticleSystem *psys) :
	    AbcParticlePathcacheReader(name, ob, psys, &psys->childcache, &psys->totchild, "__children")
	{}
};

} /* namespace PTC */

#endif  /* PTC_PARTICLES_H */

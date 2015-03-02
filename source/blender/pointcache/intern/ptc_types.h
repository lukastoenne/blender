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

#ifndef PTC_TYPES_H
#define PTC_TYPES_H

#include "reader.h"
#include "writer.h"

extern "C" {
#include "DNA_dynamicpaint_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_particle_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_smoke_types.h"
}

namespace PTC {

class ClothWriter : public Writer {
public:
	ClothWriter(Object *ob, ClothModifierData *clmd, WriterArchive *archive) :
	    Writer((ID *)ob, archive),
	    m_ob(ob),
	    m_clmd(clmd)
	{}
	
	~ClothWriter()
	{}
	
protected:
	Object *m_ob;
	ClothModifierData *m_clmd;
};

class ClothReader : public Reader {
public:
	ClothReader(Object *ob, ClothModifierData *clmd, ReaderArchive *archive) :
	    Reader((ID *)ob, archive),
	    m_ob(ob),
	    m_clmd(clmd)
	{}
	
	~ClothReader()
	{}
	
protected:
	Object *m_ob;
	ClothModifierData *m_clmd;
};


class DerivedMeshWriter : public Writer {
public:
	/** \note Targeted DerivedMesh at \a dm_ptr must be available only on \fn write_sample calls */
	DerivedMeshWriter(Object *ob, DerivedMesh **dm_ptr, WriterArchive *archive) :
	    Writer((ID *)ob, archive),
	    m_ob(ob),
	    m_dm_ptr(dm_ptr)
	{}
	
	~DerivedMeshWriter()
	{}
	
protected:
	Object *m_ob;
	DerivedMesh **m_dm_ptr;
};

class DerivedMeshReader : public Reader {
public:
	DerivedMeshReader(Object *ob, ReaderArchive *archive) :
	    Reader((ID *)ob, archive),
	    m_ob(ob),
	    m_result(0)
	{}
	
	~DerivedMeshReader()
	{}
	
	virtual DerivedMesh *acquire_result();
	virtual void discard_result();
	
protected:
	Object *m_ob;
	DerivedMesh *m_result;
};

class ParticlesWriter : public Writer {
public:
	ParticlesWriter(Object *ob, ParticleSystem *psys, WriterArchive *archive) :
	    Writer((ID *)ob, archive),
	    m_ob(ob),
	    m_psys(psys)
	{}
	
	~ParticlesWriter()
	{}
	
protected:
	Object *m_ob;
	ParticleSystem *m_psys;
};

class ParticlesReader : public Reader {
public:
	ParticlesReader(Object *ob, ParticleSystem *psys, ReaderArchive *archive) :
	    Reader((ID *)ob, archive),
	    m_ob(ob),
	    m_psys(psys),
	    m_totpoint(0)
	{}
	
	~ParticlesReader()
	{}
	
	int totpoint() const { return m_totpoint; }
	
protected:
	Object *m_ob;
	ParticleSystem *m_psys;
	
	int m_totpoint;
};

struct Factory {
	virtual const std::string &get_default_extension() = 0;
	virtual WriterArchive *create_writer_archive(Scene *scene, const std::string &name, ErrorHandler *error_handler) = 0;
	virtual ReaderArchive *create_reader_archive(Scene *scene, const std::string &name, ErrorHandler *error_handler) = 0;
	
	/* Particles */
	virtual Writer *create_writer_particles(WriterArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys) = 0;
	virtual Reader *create_reader_particles(ReaderArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys) = 0;
	virtual Writer *create_writer_particles_pathcache_parents(WriterArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys) = 0;
	virtual Reader *create_reader_particles_pathcache_parents(ReaderArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys) = 0;
	virtual Writer *create_writer_particles_pathcache_children(WriterArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys) = 0;
	virtual Reader *create_reader_particles_pathcache_children(ReaderArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys) = 0;
	
	/* Cloth */
	virtual Writer *create_writer_cloth(WriterArchive *archive, const std::string &name, Object *ob, ClothModifierData *clmd) = 0;
	virtual Reader *create_reader_cloth(ReaderArchive *archive, const std::string &name, Object *ob, ClothModifierData *clmd) = 0;
	virtual Writer *create_writer_hair_dynamics(WriterArchive *archive, const std::string &name, Object *ob, ClothModifierData *clmd) = 0;
	virtual Reader *create_reader_hair_dynamics(ReaderArchive *archive, const std::string &name, Object *ob, ClothModifierData *clmd) = 0;
	
	/* Modifier Stack */
	virtual Writer *create_writer_derived_mesh(WriterArchive *archive, const std::string &name, Object *ob, DerivedMesh **dm_ptr) = 0;
	virtual Reader *create_reader_derived_mesh(ReaderArchive *archive, const std::string &name, Object *ob) = 0;
	
	virtual Writer *create_writer_derived_final(WriterArchive *archive, const std::string &name, Object *ob) = 0;
	virtual Writer *create_writer_cache_modifier(WriterArchive *archive, const std::string &name, Object *ob, CacheModifierData *cmd) = 0;
	
	static Factory *alembic;
};

} /* namespace PTC */

#endif  /* PTC_EXPORT_H */

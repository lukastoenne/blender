/*
 * Copyright 2015, Blender Foundation.
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

#include "PTC_api.h"

#include "ptc_types.h"

#include "abc_reader.h"
#include "abc_writer.h"
#include "abc_cloth.h"
#include "abc_mesh.h"
#include "abc_particles.h"

namespace PTC {

class AbcFactory : public Factory {
	const std::string &get_default_extension()
	{
		static std::string ext = "abc";
		return ext;
	}
	
	WriterArchive *create_writer_archive(Scene *scene, const std::string &name, ErrorHandler *error_handler)
	{
		return new AbcWriterArchive(scene, name, error_handler);
	}
	
	ReaderArchive *create_reader_archive(Scene *scene, const std::string &name, ErrorHandler *error_handler)
	{
		return new AbcReaderArchive(scene, name, error_handler);
	}
	
	/* Particles */
	Writer *create_writer_particles(WriterArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys)
	{
		return new AbcParticlesWriter(static_cast<AbcWriterArchive*>(archive), name, ob, psys);
	}
	
	Reader *create_reader_particles(ReaderArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys)
	{
		return new AbcParticlesReader(static_cast<AbcReaderArchive*>(archive), name, ob, psys);
	}
	
	Writer *create_writer_particles_pathcache_parents(WriterArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys)
	{
		return new AbcParticlePathcacheParentsWriter(static_cast<AbcWriterArchive*>(archive), name, ob, psys);
	}
	
	Reader *create_reader_particles_pathcache_parents(ReaderArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys)
	{
		return new AbcParticlePathcacheParentsReader(static_cast<AbcReaderArchive*>(archive), name, ob, psys);
	}
	
	Writer *create_writer_particles_pathcache_children(WriterArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys)
	{
		return new AbcParticlePathcacheChildrenWriter(static_cast<AbcWriterArchive*>(archive), name, ob, psys);
	}
	
	Reader *create_reader_particles_pathcache_children(ReaderArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys)
	{
		return new AbcParticlePathcacheChildrenReader(static_cast<AbcReaderArchive*>(archive), name, ob, psys);
	}
	
	
	/* Cloth */
	Writer *create_writer_cloth(WriterArchive *archive, const std::string &name, Object *ob, ClothModifierData *clmd)
	{
		return new AbcClothWriter(static_cast<AbcWriterArchive*>(archive), name, ob, clmd);
	}
	
	Reader *create_reader_cloth(ReaderArchive *archive, const std::string &name, Object *ob, ClothModifierData *clmd)
	{
		return new AbcClothReader(static_cast<AbcReaderArchive*>(archive), name, ob, clmd);
	}
	
	Writer *create_writer_hair_dynamics(WriterArchive *archive, const std::string &name, Object *ob, ClothModifierData *clmd)
	{
		return new AbcHairDynamicsWriter(static_cast<AbcWriterArchive*>(archive), name, ob, clmd);
	}
	
	Reader *create_reader_hair_dynamics(ReaderArchive *archive, const std::string &name, Object *ob, ClothModifierData *clmd)
	{
		return new AbcHairDynamicsReader(static_cast<AbcReaderArchive*>(archive), name, ob, clmd);
	}
	
	/* Modifier Stack */
	Writer *create_writer_derived_mesh(WriterArchive *archive, const std::string &name, Object *ob, DerivedMesh **dm_ptr)
	{
		return new AbcDerivedMeshWriter(static_cast<AbcWriterArchive*>(archive), name, ob, dm_ptr);
	}
	
	Reader *create_reader_derived_mesh(ReaderArchive *archive, const std::string &name, Object *ob)
	{
		return new AbcDerivedMeshReader(static_cast<AbcReaderArchive*>(archive), name, ob);
	}
	
	Writer *create_writer_derived_final(WriterArchive *archive, const std::string &name, Object *ob)
	{
		return new AbcDerivedFinalWriter(static_cast<AbcWriterArchive*>(archive), name, ob);
	}
	
	Writer *create_writer_cache_modifier(WriterArchive *archive, const std::string &name, Object *ob, CacheModifierData *cmd)
	{
		return new AbcCacheModifierWriter(static_cast<AbcWriterArchive*>(archive), name, ob, cmd);
	}
};

}

void PTC_alembic_init()
{
	static PTC::AbcFactory abc_factory;
	
	PTC::Factory::alembic = &abc_factory;
}

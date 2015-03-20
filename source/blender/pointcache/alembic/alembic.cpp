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
#include "abc_group.h"
#include "abc_mesh.h"
#include "abc_object.h"
#include "abc_particles.h"

namespace PTC {

class AbcFactory : public Factory {
	const std::string &get_default_extension()
	{
		static std::string ext = "abc";
		return ext;
	}
	
	WriterArchive *open_writer_archive(Scene *scene, const std::string &name, ErrorHandler *error_handler)
	{
		return AbcWriterArchive::open(scene, name, error_handler);
	}
	
	ReaderArchive *open_reader_archive(Scene *scene, const std::string &name, ErrorHandler *error_handler)
	{
		return AbcReaderArchive::open(scene, name, error_handler);
	}
	
	Writer *create_writer_object(const std::string &name, Scene *scene, Object *ob)
	{
		return new AbcObjectWriter(name, scene, ob);
	}

	Reader *create_reader_object(const std::string &name, Object *ob)
	{
		return new AbcObjectReader(name, ob);
	}
	
	Writer *create_writer_group(const std::string &name, Group *group)
	{
		return new AbcGroupWriter(name, group);
	}
	
	Reader *create_reader_group(const std::string &name, Group *group)
	{
		return new AbcGroupReader(name, group);
	}
	
	/* Particles */
	Writer *create_writer_particles(const std::string &name, Object *ob, ParticleSystem *psys)
	{
		return new AbcParticlesWriter(name, ob, psys);
	}
	
	Reader *create_reader_particles(const std::string &name, Object *ob, ParticleSystem *psys)
	{
		return new AbcParticlesReader(name, ob, psys);
	}
	
	Writer *create_writer_hair_dynamics(const std::string &name, Object *ob, ParticleSystem *psys)
	{
		return new AbcHairDynamicsWriter(name, ob, psys);
	}
	
	Reader *create_reader_hair_dynamics(const std::string &name, Object *ob, ParticleSystem *psys)
	{
		return new AbcHairDynamicsReader(name, ob, psys);
	}
	
	Writer *create_writer_particles_pathcache_parents(const std::string &name, Object *ob, ParticleSystem *psys)
	{
		return new AbcParticlePathcacheParentsWriter(name, ob, psys);
	}
	
	Reader *create_reader_particles_pathcache_parents(const std::string &name, Object *ob, ParticleSystem *psys)
	{
		return new AbcParticlePathcacheParentsReader(name, ob, psys);
	}
	
	Writer *create_writer_particles_pathcache_children(const std::string &name, Object *ob, ParticleSystem *psys)
	{
		return new AbcParticlePathcacheChildrenWriter(name, ob, psys);
	}
	
	Reader *create_reader_particles_pathcache_children(const std::string &name, Object *ob, ParticleSystem *psys)
	{
		return new AbcParticlePathcacheChildrenReader(name, ob, psys);
	}
	
	
	/* Cloth */
	Writer *create_writer_cloth(const std::string &name, Object *ob, ClothModifierData *clmd)
	{
		return new AbcClothWriter(name, ob, clmd);
	}
	
	Reader *create_reader_cloth(const std::string &name, Object *ob, ClothModifierData *clmd)
	{
		return new AbcClothReader(name, ob, clmd);
	}
	
	/* Modifier Stack */
	Writer *create_writer_derived_mesh(const std::string &name, Object *ob, DerivedMesh **dm_ptr)
	{
		return new AbcDerivedMeshWriter(name, ob, dm_ptr);
	}
	
	Reader *create_reader_derived_mesh(const std::string &name, Object *ob)
	{
		return new AbcDerivedMeshReader(name, ob);
	}
	
	Writer *create_writer_derived_final_realtime(const std::string &name, Object *ob)
	{
		return new AbcDerivedFinalRealtimeWriter(name, ob);
	}
	
	Writer *create_writer_derived_final_render(const std::string &name, Scene *scene, Object *ob, DerivedMesh **render_dm_ptr)
	{
		return new AbcDerivedFinalRenderWriter(name, scene, ob, render_dm_ptr);
	}
	
	Writer *create_writer_cache_modifier_realtime(const std::string &name, Object *ob, CacheModifierData *cmd)
	{
		return new AbcCacheModifierRealtimeWriter(name, ob, cmd);
	}
	
	Writer *create_writer_cache_modifier_render(const std::string &name, Scene *scene, Object *ob, CacheModifierData *cmd)
	{
		return new AbcCacheModifierRenderWriter(name, scene, ob, cmd);
	}
	
	
	Writer *create_writer_dupligroup(const std::string &name, EvaluationContext *eval_ctx, Scene *scene, Group *group)
	{
		return new AbcDupligroupWriter(name, eval_ctx, scene, group);
	}
	
	Reader *create_reader_dupligroup(const std::string &name, Group *group, DupliCache *dupcache)
	{
		return new AbcDupligroupReader(name, group, dupcache);
	}
};

}

void PTC_alembic_init()
{
	static PTC::AbcFactory abc_factory;
	
	PTC::Factory::alembic = &abc_factory;
}

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

#ifndef PTC_ALEMBIC_H
#define PTC_ALEMBIC_H

#include <string>

#include "reader.h"
#include "writer.h"

struct Object;
struct ClothModifierData;
struct DynamicPaintSurface;
struct CacheModifierData;
struct DerivedMesh;
struct ParticleSystem;
struct RigidBodyWorld;
struct SmokeDomainSettings;
struct SoftBody;

namespace PTC {

WriterArchive *abc_writer_archive(Scene *scene, const std::string &name, ErrorHandler *error_handler);
ReaderArchive *abc_reader_archive(Scene *scene, const std::string &name, ErrorHandler *error_handler);

/* Particles */
Writer *abc_writer_particles(WriterArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys);
Reader *abc_reader_particles(ReaderArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys);
Writer *abc_writer_particle_pathcache_parents(WriterArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys);
Reader *abc_reader_particle_pathcache_parents(ReaderArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys);
Writer *abc_writer_particle_pathcache_children(WriterArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys);
Reader *abc_reader_particle_pathcache_children(ReaderArchive *archive, const std::string &name, Object *ob, ParticleSystem *psys);

/* Cloth */
Writer *abc_writer_cloth(WriterArchive *archive, const std::string &name, Object *ob, ClothModifierData *clmd);
Reader *abc_reader_cloth(ReaderArchive *archive, const std::string &name, Object *ob, ClothModifierData *clmd);
Writer *abc_writer_hair_dynamics(WriterArchive *archive, const std::string &name, Object *ob, ClothModifierData *clmd);
Reader *abc_reader_hair_dynamics(ReaderArchive *archive, const std::string &name, Object *ob, ClothModifierData *clmd);

/* Modifier Stack */
Writer *abc_writer_derived_mesh(WriterArchive *archive, const std::string &name, Object *ob, DerivedMesh **dm_ptr);
Reader *abc_reader_derived_mesh(ReaderArchive *archive, const std::string &name, Object *ob);

Writer *abc_writer_derived_final(WriterArchive *archive, const std::string &name, Object *ob);
Writer *abc_writer_cache_modifier(WriterArchive *archive, const std::string &name, Object *ob, CacheModifierData *cmd);

} /* namespace PTC */

#endif  /* PTC_EXPORT_H */

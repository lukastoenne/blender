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

#include "reader.h"
#include "writer.h"

#include "PTC_api.h"

struct Object;
struct ClothModifierData;
struct DynamicPaintSurface;
struct PointCacheModifierData;
struct DerivedMesh;
struct ParticleSystem;
struct RigidBodyWorld;
struct SmokeDomainSettings;
struct SoftBody;

namespace PTC {

/* Particles */
Writer *abc_writer_particles(Scene *scene, Object *ob, ParticleSystem *psys);
Reader *abc_reader_particles(Scene *scene, Object *ob, ParticleSystem *psys);
//Writer *abc_writer_particle_paths(Scene *scene, Object *ob, ParticleSystem *psys);
Reader *abc_reader_particle_paths(Scene *scene, Object *ob, ParticleSystem *psys, eParticlePathsMode mode);
Writer *abc_writer_particle_combined(Scene *scene, Object *ob, ParticleSystem *psys);

/* Cloth */
Writer *abc_writer_cloth(Scene *scene, Object *ob, ClothModifierData *clmd);
Reader *abc_reader_cloth(Scene *scene, Object *ob, ClothModifierData *clmd);

/* SoftBody */
Writer *abc_writer_softbody(Scene *scene, Object *ob, SoftBody *softbody);
Reader *abc_reader_softbody(Scene *scene, Object *ob, SoftBody *softbody);

/* Rigid Bodies */
Writer *abc_writer_rigidbody(Scene *scene, RigidBodyWorld *rbw);
Reader *abc_reader_rigidbody(Scene *scene, RigidBodyWorld *rbw);

/* Smoke */
Writer *abc_writer_smoke(Scene *scene, Object *ob, SmokeDomainSettings *domain);
Reader *abc_reader_smoke(Scene *scene, Object *ob, SmokeDomainSettings *domain);

/* Dynamic Paint */
Writer *abc_writer_dynamicpaint(Scene *scene, Object *ob, DynamicPaintSurface *surface);
Reader *abc_reader_dynamicpaint(Scene *scene, Object *ob, DynamicPaintSurface *surface);

/* Modifier Stack */
Writer *abc_writer_point_cache(Scene *scene, Object *ob, PointCacheModifierData *pcmd);
Reader *abc_reader_point_cache(Scene *scene, Object *ob, PointCacheModifierData *pcmd);

} /* namespace PTC */

#endif  /* PTC_EXPORT_H */

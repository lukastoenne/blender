/*
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 *
 * Copyright 2011, Blender Foundation.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_NPARTICLE_H
#define BKE_NPARTICLE_H

/** \file BKE_nparticle.h
 *  \ingroup bke
 */

#include "BLI_sys_types.h"

struct NParticleSystem;
struct NParticleState;
struct NParticleAttribute;

/* XXX where to put this? */
typedef uint32_t NParticleID;

struct NParticleSystem *BKE_nparticle_system_new(void);
void BKE_nparticle_system_free(struct NParticleSystem *psys);
struct NParticleSystem *BKE_nparticle_system_copy(struct NParticleSystem *psys);

struct NParticleAttribute *BKE_nparticle_attribute_find(struct NParticleSystem *psys, const char *name);
struct NParticleAttribute *BKE_nparticle_attribute_new(struct NParticleSystem *psys, const char *name, int datatype);
void BKE_nparticle_attribute_remove(struct NParticleSystem *psys, struct NParticleAttribute *attr);
void BKE_nparticle_attribute_remove_all(struct NParticleSystem *psys);
void BKE_nparticle_attribute_move(struct NParticleSystem *psys, int from_index, int to_index);
struct NParticleAttribute *BKE_nparticle_attribute_copy(struct NParticleSystem *to_psys,
                                                        struct NParticleSystem *from_psys, struct NParticleAttribute *from_attr);


int BKE_nparticle_find_index(struct NParticleSystem *psys, NParticleID id);
bool BKE_nparticle_exists(struct NParticleSystem *psys, NParticleID id);

typedef struct NParticleIterator {
	int index;
} NParticleIterator;

void BKE_nparticle_iter_init(struct NParticleSystem *psys, struct NParticleIterator *it);
void BKE_nparticle_iter_next(struct NParticleSystem *psys, struct NParticleIterator *it);
bool BKE_nparticle_iter_valid(struct NParticleSystem *psys, struct NParticleIterator *it);

float BKE_nparticle_state_get_float(struct NParticleState *state, NParticleID pid, const char *attr);
void BKE_nparticle_state_set_float(struct NParticleState *state, NParticleID pid, const char *attr, float value);

#if 0 /* old code */
#include "BLI_math.h"
#include "BLI_pagedbuffer.h"

#include "DNA_nparticle_types.h"

#include "BKE_node.h"

struct Object;
struct Scene;
struct DupliObject;
struct NParticleSystem;
struct NParticlesModifierData;
struct NParticleDupliObject;


BLI_INLINE int pit_get_particle_flag(struct NParticleSystem *psys, struct bPagedBufferIterator *it, NParticleFlagLayerType flag)
{
	return (((*PBUF_GET_DATA_POINTER(it, psys->standard_attribute[PAR_ATTR_TYPE_FLAG]->layer, NParticleFlagLayerType)) & flag) != 0);
}
BLI_INLINE void pit_set_particle_flag(struct NParticleSystem *psys, struct bPagedBufferIterator *it, NParticleFlagLayerType flag)
{
	(*PBUF_GET_DATA_POINTER(it, psys->standard_attribute[PAR_ATTR_TYPE_FLAG]->layer_write, NParticleFlagLayerType)) |= flag;
}
BLI_INLINE void pit_clear_particle_flag(struct NParticleSystem *psys, struct bPagedBufferIterator *it, NParticleFlagLayerType flag)
{
	(*PBUF_GET_DATA_POINTER(it, psys->standard_attribute[PAR_ATTR_TYPE_FLAG]->layer_write, NParticleFlagLayerType)) &= ~flag;
}

BLI_INLINE NParticleRNG *pit_get_particle_rng(struct NParticleSystem *psys, struct bPagedBufferIterator *it)
{
	return PBUF_GET_DATA_POINTER(it, psys->standard_attribute[PAR_ATTR_TYPE_RANDOM_SEED]->layer, NParticleRNG);
}

BLI_INLINE const NParticleVector *pit_get_particle_position(struct NParticleSystem *psys, struct bPagedBufferIterator *it)
{
	return PBUF_GET_DATA_POINTER(it, psys->standard_attribute[PAR_ATTR_TYPE_POSITION]->layer, NParticleVector);
}

BLI_INLINE void pit_set_particle_position(struct NParticleSystem *psys, struct bPagedBufferIterator *it, const NParticleVector *value)
{
	copy_v3_v3((float *)PBUF_GET_DATA_POINTER(it, psys->standard_attribute[PAR_ATTR_TYPE_POSITION]->layer_write, NParticleVector), (const float *)value);
}

BLI_INLINE const NParticleQuaternion *pit_get_particle_rotation(struct NParticleSystem *psys, struct bPagedBufferIterator *it)
{
	return PBUF_GET_DATA_POINTER(it, psys->standard_attribute[PAR_ATTR_TYPE_ROTATION]->layer, NParticleQuaternion);
}

BLI_INLINE void pit_set_particle_rotation(struct NParticleSystem *psys, struct bPagedBufferIterator *it, const NParticleQuaternion *value)
{
	copy_qt_qt((float *)PBUF_GET_DATA_POINTER(it, psys->standard_attribute[PAR_ATTR_TYPE_ROTATION]->layer_write, NParticleQuaternion), (const float *)value);
}

struct bPagedBufferIterator pit_init_particles(struct NParticleSystem *psys);
struct bPagedBufferIterator pit_init_particles_at(struct NParticleSystem *psys, int index);
void pit_next_particle(struct NParticleSystem *psys, struct bPagedBufferIterator *it);

void npar_init(struct NParticleSystem *psys);
void npar_free(struct NParticleSystem *psys);
void npar_copy(struct NParticleSystem *to, struct NParticleSystem *from);

struct bPagedBufferIterator npar_create_particles(struct NParticleSystem *psys, struct NParticleSource *source, float time, float num);
void npar_free_dead_particles(struct NParticleSystem *psys);

void npar_reset(struct NParticleSystem *psys);

struct NParticleAttribute *npar_find_custom_attribute(struct NParticleSystem *psys, const char *name);
void npar_custom_attribute_unique_name(struct ListBase *attributes, struct NParticleAttribute *attr);
void npar_init_attribute(struct NParticleAttribute *attr, int type, struct NParticleSystem *psys);
void npar_init_custom_attribute(struct NParticleAttribute *attr, int datatype, const char *name, struct NParticleSystem *psys);
struct NParticleAttribute *npar_add_attribute(struct NParticleSystem *psys, int type);
struct NParticleAttribute *npar_add_custom_attribute(struct NParticleSystem *psys, int datatype, const char *name);
void npar_remove_attribute(struct NParticleSystem *psys, struct NParticleAttribute *attr);
int npar_socket_type_from_attribute(struct NParticleAttribute *attr);

void npar_update_state(struct NParticleAttribute *attr, struct bPagedBufferIterator *pit);
void npar_update_state_all(struct NParticleSystem *psys, struct bPagedBufferIterator *pit);

struct NParticleSource *npar_source_new(struct NParticleSystem *psys);

struct NParticleDupliObject *npar_dupli_object_new(void);
void npar_dupli_object_free(struct NParticleDupliObject *dupli);
struct NParticleDupliObject *npar_dupli_object_copy(struct NParticleDupliObject *dupli);
void npar_create_dupli_object(struct NParticlesModifierData *pmd, struct bPagedBufferIterator *pit, struct Object **ob, float mat[][4]);

void npar_filter_objects(struct Scene *scene, int type, void *userdata, void (*walk)(void *userdata, Scene *scene, struct Object *ob, struct bNodeTree *ntree));
#endif

#endif

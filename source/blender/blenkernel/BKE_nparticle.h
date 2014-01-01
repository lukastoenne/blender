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
#include "BLI_utildefines.h"

struct NParticleSystem;
struct NParticleAttribute;
struct NParticleState;
struct NParticleAttributeState;
struct NParticleDisplay;

/* XXX where to put this? */
typedef uint32_t NParticleID;

const char *BKE_nparticle_datatype_name(int datatype);

struct NParticleSystem *BKE_nparticle_system_new(void);
void BKE_nparticle_system_free(struct NParticleSystem *psys);
struct NParticleSystem *BKE_nparticle_system_copy(struct NParticleSystem *psys);

void BKE_nparticle_system_set_state(struct NParticleSystem *psys, struct NParticleState *state);

struct NParticleState *BKE_nparticle_state_new(struct NParticleSystem *psys);
struct NParticleState *BKE_nparticle_state_copy(struct NParticleState *state);
void BKE_nparticle_state_free(struct NParticleState *state);

struct NParticleAttribute *BKE_nparticle_attribute_find(struct NParticleSystem *psys, const char *name);
struct NParticleAttribute *BKE_nparticle_attribute_new(struct NParticleSystem *psys, const char *name, int datatype, int flag);
void BKE_nparticle_attribute_remove(struct NParticleSystem *psys, struct NParticleAttribute *attr);
void BKE_nparticle_attribute_remove_all(struct NParticleSystem *psys);
void BKE_nparticle_attribute_move(struct NParticleSystem *psys, int from_index, int to_index);
struct NParticleAttribute *BKE_nparticle_attribute_copy(struct NParticleSystem *to_psys,
                                                        struct NParticleSystem *from_psys, struct NParticleAttribute *from_attr);

int BKE_nparticle_state_num_attributes(struct NParticleState *state);
struct NParticleAttributeState *BKE_nparticle_state_find_attribute(struct NParticleState *state, const char *name);
struct NParticleAttributeState *BKE_nparticle_state_get_attribute_by_index(struct NParticleState *state, int index);

int BKE_nparticle_state_num_particles(struct NParticleState *state);

void *BKE_nparticle_attribute_state_data(struct NParticleAttributeState *attrstate, int index);

typedef struct NParticleAttributeStateIterator {
	/* XXX for now is simply a pointer, using ListBase next/prev.
	 * Eventually this will become a hash table iterator.
	 */
	struct NParticleAttributeState *attrstate;
} NParticleAttributeStateIterator;

void BKE_nparticle_state_attributes_begin(struct NParticleState *state, struct NParticleAttributeStateIterator *iter);
bool BKE_nparticle_state_attribute_iter_valid(struct NParticleAttributeStateIterator *iter);
void BKE_nparticle_state_attribute_iter_next(struct NParticleAttributeStateIterator *iter);
void BKE_nparticle_state_attribute_iter_end(struct NParticleAttributeStateIterator *iter);


int BKE_nparticle_find_index(struct NParticleState *state, NParticleID id);
bool BKE_nparticle_exists(struct NParticleState *state, NParticleID id);
int BKE_nparticle_add(struct NParticleState *state, NParticleID id);
void BKE_nparticle_remove(struct NParticleState *state, NParticleID id);

typedef struct NParticleIterator {
	struct NParticleState *state;
	int index;
} NParticleIterator;

void BKE_nparticle_iter_init(struct NParticleState *state, struct NParticleIterator *it);
void BKE_nparticle_iter_from_id(struct NParticleState *state, struct NParticleIterator *it, NParticleID id);
void BKE_nparticle_iter_from_index(struct NParticleState *state, struct NParticleIterator *it, int index);
void BKE_nparticle_iter_next(struct NParticleIterator *it);
bool BKE_nparticle_iter_valid(struct NParticleIterator *it);

/*void *BKE_nparticle_iter_get_data(struct NParticleIterator *it, const char *attr);*/
int BKE_nparticle_iter_get_int(struct NParticleIterator *it, const char *attr);
void BKE_nparticle_iter_set_int(struct NParticleIterator *it, const char *attr, int value);
float BKE_nparticle_iter_get_float(struct NParticleIterator *it, const char *attr);
void BKE_nparticle_iter_set_float(struct NParticleIterator *it, const char *attr, float value);
void BKE_nparticle_iter_get_vector(struct NParticleIterator *it, const char *attr, float *result);
void BKE_nparticle_iter_set_vector(struct NParticleIterator *it, const char *attr, const float *value);
void *BKE_nparticle_iter_get_pointer(struct NParticleIterator *it, const char *attr);
void BKE_nparticle_iter_set_pointer(struct NParticleIterator *it, const char *attr, void *value);

BLI_INLINE NParticleID BKE_nparticle_iter_get_id(struct NParticleIterator *it)
{
	return (NParticleID)BKE_nparticle_iter_get_int(it, "id");
}


struct NParticleDisplay *BKE_nparticle_display_particle(void);
struct NParticleDisplay *BKE_nparticle_display_copy(struct NParticleDisplay *display);
void BKE_nparticle_display_free(struct NParticleDisplay *display);

#endif /* BKE_NPARTICLE_H */

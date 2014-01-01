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

/** \file blender/blenkernel/intern/nparticle.c
 *  \ingroup bke
 */

#include <string.h>
#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"	/* XXX stupid, needed by ghash, should be included there ... */
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_pagedbuffer.h"
#include "BLI_string.h"

#include "DNA_nparticle_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"

#include "BKE_nparticle.h"
#include "BKE_rigidbody.h"

/* XXX TODO make this configurable */
#define PAGE_BYTES 65536

const char *BKE_nparticle_datatype_name(int datatype)
{
	switch (datatype) {
		case PAR_ATTR_DATATYPE_INTERNAL: return "internal";
		case PAR_ATTR_DATATYPE_FLOAT: return "float";
		case PAR_ATTR_DATATYPE_INT: return "int";
		case PAR_ATTR_DATATYPE_BOOL: return "bool";
		case PAR_ATTR_DATATYPE_VECTOR: return "vector";
		case PAR_ATTR_DATATYPE_POINT: return "point";
		case PAR_ATTR_DATATYPE_NORMAL: return "normal";
		case PAR_ATTR_DATATYPE_COLOR: return "color";
		case PAR_ATTR_DATATYPE_MATRIX: return "matrix";
		case PAR_ATTR_DATATYPE_POINTER: return "pointer";
		default: return "";
	}
}

static size_t nparticle_elem_bytes(int datatype)
{
	switch (datatype) {
		case PAR_ATTR_DATATYPE_FLOAT: return sizeof(float);
		case PAR_ATTR_DATATYPE_INT: return sizeof(int);
		case PAR_ATTR_DATATYPE_BOOL: return sizeof(bool);
		case PAR_ATTR_DATATYPE_VECTOR:
		case PAR_ATTR_DATATYPE_POINT:
		case PAR_ATTR_DATATYPE_NORMAL:
			return sizeof(float)*3;
		case PAR_ATTR_DATATYPE_COLOR: return sizeof(float)*4;
		case PAR_ATTR_DATATYPE_MATRIX: return sizeof(float)*16;
		case PAR_ATTR_DATATYPE_POINTER: return sizeof(void*);

		default:
			BLI_assert(false);	/* unknown datatype, should never happen */
			return 0;
	}
}

static void nparticle_attribute_state_init(NParticleAttribute *attr, NParticleAttributeState *attrstate)
{
	attrstate->desc = attr->desc;
	attrstate->hashkey = BLI_ghashutil_strhash(attr->desc.name);
	BLI_pbuf_init(&attrstate->data, PAGE_BYTES, nparticle_elem_bytes(attr->desc.datatype));
}

static void nparticle_attribute_state_free(NParticleAttributeState *state)
{
	BLI_pbuf_free(&state->data);
}

static void nparticle_attribute_state_copy(NParticleAttributeState *to, NParticleAttributeState *from)
{
	to->desc = from->desc;
	to->hashkey = from->hashkey;
	BLI_pbuf_copy(&to->data, &from->data);
}


static NParticleAttributeState *nparticle_state_add_attribute(NParticleState *state, NParticleAttribute *attr)
{
	NParticleAttributeState *attrstate = MEM_callocN(sizeof(NParticleAttributeState), "particle attribute state");
	nparticle_attribute_state_init(attr, attrstate);
	BLI_addtail(&state->attributes, attrstate);
	return attrstate;
}

static void nparticle_state_remove_attribute(NParticleState *state, const char *name)
{
	NParticleAttributeState *attrstate = BKE_nparticle_state_find_attribute(state, name);
	if (attrstate) {
		BLI_remlink(&state->attributes, attrstate);
		nparticle_attribute_state_free(attrstate);
		MEM_freeN(attrstate);
	}
}

static void nparticle_state_clear(NParticleState *state)
{
	NParticleAttributeState *attrstate;
	for (attrstate = state->attributes.first; attrstate; attrstate = attrstate->next)
		nparticle_attribute_state_free(attrstate);
	BLI_freelistN(&state->attributes);
}

static void nparticle_system_sync_state_attributes(NParticleSystem *psys, NParticleState *state)
{
	NParticleAttribute *attr;
	NParticleAttributeState *attrstate, *attrstate_next;
	
	for (attrstate = state->attributes.first; attrstate; attrstate = attrstate->next)
		attrstate->flag &= ~PAR_ATTR_STATE_TEST;
	for (attr = psys->attributes.first; attr; attr = attr->next) {
		attrstate = BKE_nparticle_state_find_attribute(state, attr->desc.name);
		
		/* add missing attributes */
		if (!attrstate) {
			attrstate = nparticle_state_add_attribute(state, attr);
		}
		
		attrstate->flag |= PAR_ATTR_STATE_TEST;
	}
	
	/* remove unused attribute states */
	for (attrstate = state->attributes.first; attrstate; attrstate = attrstate_next) {
		attrstate_next = attrstate->next;
		
		if (!(attrstate->flag & PAR_ATTR_STATE_TEST)) {
			BLI_remlink(&state->attributes, attrstate);
			nparticle_attribute_state_free(attrstate);
			MEM_freeN(attrstate);
		}
	}
}


static void nparticle_system_default_attributes(NParticleSystem *psys)
{
	/* required attributes */
	BKE_nparticle_attribute_new(psys, "id", PAR_ATTR_DATATYPE_INT, PAR_ATTR_REQUIRED | PAR_ATTR_READONLY);
	
	/* common attributes */
	BKE_nparticle_attribute_new(psys, "position", PAR_ATTR_DATATYPE_POINT, PAR_ATTR_PROTECTED);
	
	/* XXX bullet RB pointers, this should be based on actual simulation settings and requirements */
	BKE_nparticle_attribute_new(psys, "rigid_body", PAR_ATTR_DATATYPE_POINTER, PAR_ATTR_PROTECTED | PAR_ATTR_TEMPORARY);
	BKE_nparticle_attribute_new(psys, "collision_shape", PAR_ATTR_DATATYPE_POINTER, PAR_ATTR_PROTECTED | PAR_ATTR_TEMPORARY);
}

NParticleSystem *BKE_nparticle_system_new(void)
{
	NParticleSystem *psys = MEM_callocN(sizeof(NParticleSystem), "nparticle system");
	
	nparticle_system_default_attributes(psys);
	
	psys->state = BKE_nparticle_state_new(psys);
	
	return psys;
}

void BKE_nparticle_system_free(NParticleSystem *psys)
{
	BKE_nparticle_attribute_remove_all(psys);
	
	BKE_nparticle_state_free(psys->state);
	
	MEM_freeN(psys);
}

NParticleSystem *BKE_nparticle_system_copy(NParticleSystem *psys)
{
	NParticleSystem *npsys = MEM_dupallocN(psys);
	NParticleAttribute *attr, *nattr;
	
	npsys->attributes.first = npsys->attributes.last = NULL;
	for (attr = psys->attributes.first; attr; attr = attr->next) {
		nattr = BKE_nparticle_attribute_copy(npsys, psys, attr);
	}
	
	if (psys->state)
		npsys->state = BKE_nparticle_state_copy(psys->state);
	
	return npsys;
}

void BKE_nparticle_system_set_state(NParticleSystem *psys, NParticleState *state)
{
	if (state) {
		if (psys->state)
			BKE_nparticle_state_free(psys->state);
		
		psys->state = BKE_nparticle_state_copy(state);
	}
}


NParticleState *BKE_nparticle_state_new(NParticleSystem *psys)
{
	NParticleState *state = MEM_callocN(sizeof(NParticleState), "particle state");
	
	nparticle_system_sync_state_attributes(psys, state);
	
	return state;
}

NParticleState *BKE_nparticle_state_copy(NParticleState *state)
{
	NParticleState *nstate = MEM_dupallocN(state);
	NParticleAttributeState *attrstate, *nattrstate;
	
	BLI_duplicatelist(&state->attributes, &nstate->attributes);
	
	for (attrstate = state->attributes.first, nattrstate = nstate->attributes.first;
	     attrstate;
	     attrstate = attrstate->next, nattrstate = nattrstate->next)
		nparticle_attribute_state_copy(nattrstate, attrstate);
	
	return nstate;
}

void BKE_nparticle_state_free(NParticleState *state)
{
	NParticleAttributeState *attrstate;
	for (attrstate = state->attributes.first; attrstate; attrstate = attrstate->next)
		nparticle_attribute_state_free(attrstate);
	BLI_freelistN(&state->attributes);
	MEM_freeN(state);
}


void BKE_nparticle_state_attributes_begin(NParticleState *state, NParticleAttributeStateIterator *iter)
{
	iter->attrstate = state->attributes.first;
}

bool BKE_nparticle_state_attribute_iter_valid(NParticleAttributeStateIterator *iter)
{
	return (iter->attrstate != NULL);
}

void BKE_nparticle_state_attribute_iter_next(NParticleAttributeStateIterator *iter)
{
	iter->attrstate = iter->attrstate->next;
}

void BKE_nparticle_state_attribute_iter_end(NParticleAttributeStateIterator *iter)
{
	iter->attrstate = NULL;
}


NParticleAttribute *BKE_nparticle_attribute_find(NParticleSystem *psys, const char *name)
{
	NParticleAttribute *attr;
	for (attr = psys->attributes.first; attr; attr = attr->next)
		if (STREQ(attr->desc.name, name))
			return attr;
	return NULL;
}

NParticleAttribute *BKE_nparticle_attribute_new(NParticleSystem *psys, const char *name, int datatype, int flag)
{
	NParticleAttribute *attr;
	
	attr = BKE_nparticle_attribute_find(psys, name);
	if (attr) {
		/* if attribute with the same name exists, remove it first */
		BKE_nparticle_attribute_remove(psys, attr);
	}
	
	attr = MEM_callocN(sizeof(NParticleAttribute), "particle system attribute");
	BLI_strncpy(attr->desc.name, name, sizeof(attr->desc.name));
	attr->desc.datatype = datatype;
	attr->desc.flag = flag;
	
	BLI_addtail(&psys->attributes, attr);
	
	if (psys->state)
		nparticle_state_add_attribute(psys->state, attr);
	
	return attr;
}

void BKE_nparticle_attribute_remove(NParticleSystem *psys, NParticleAttribute *attr)
{
	if (psys->state)
		nparticle_state_remove_attribute(psys->state, attr->desc.name);
	
	BLI_remlink(&psys->attributes, attr);
	MEM_freeN(attr);
}

void BKE_nparticle_attribute_remove_all(NParticleSystem *psys)
{
	NParticleAttribute *attr, *attr_next;
	
	if (psys->state)
		nparticle_state_clear(psys->state);
	
	for (attr = psys->attributes.first; attr; attr = attr_next) {
		attr_next = attr->next;
		MEM_freeN(attr);
	}
	psys->attributes.first = psys->attributes.last = NULL;
}

NParticleAttribute *BKE_nparticle_attribute_copy(NParticleSystem *to_psys, NParticleSystem *UNUSED(from_psys), NParticleAttribute *from_attr)
{
	NParticleAttribute *to_attr = MEM_dupallocN(from_attr);
	BLI_addtail(&to_psys->attributes, to_attr);
	
	if (to_psys->state)
		nparticle_state_add_attribute(to_psys->state, to_attr);
	
	return to_attr;
}

void BKE_nparticle_attribute_move(NParticleSystem *psys, int from_index, int to_index)
{
	NParticleAttribute *attr;
	
	if (from_index == to_index)
		return;
	if (from_index < 0 || to_index < 0)
		return;
	
	attr = BLI_findlink(&psys->attributes, from_index);
	if (to_index < from_index) {
		NParticleAttribute *nextattr = BLI_findlink(&psys->attributes, to_index);
		if (nextattr) {
			BLI_remlink(&psys->attributes, attr);
			BLI_insertlinkbefore(&psys->attributes, nextattr, attr);
		}
	}
	else {
		NParticleAttribute *prevattr = BLI_findlink(&psys->attributes, to_index);
		if (prevattr) {
			BLI_remlink(&psys->attributes, attr);
			BLI_insertlinkafter(&psys->attributes, prevattr, attr);
		}
	}
}


int BKE_nparticle_state_num_attributes(NParticleState *state)
{
	return BLI_countlist(&state->attributes);
}

NParticleAttributeState *BKE_nparticle_state_find_attribute(NParticleState *state, const char *name)
{
	int hashkey = BLI_ghashutil_strhash(name);
	NParticleAttributeState *attrstate;
	for (attrstate = state->attributes.first; attrstate; attrstate = attrstate->next)
		if (attrstate->hashkey == hashkey)
			return attrstate;
	return NULL;
}

BLI_INLINE NParticleAttributeState *nparticle_state_find_attribute_id(NParticleState *state)
{
	return BKE_nparticle_state_find_attribute(state, "id");
}

NParticleAttributeState *BKE_nparticle_state_get_attribute_by_index(NParticleState *state, int index)
{
	return BLI_findlink(&state->attributes, index);
}

int BKE_nparticle_state_num_particles(NParticleState *state)
{
	NParticleAttributeState *attrstate = nparticle_state_find_attribute_id(state);
	return attrstate ? attrstate->data.totelem : 0;
}

void *BKE_nparticle_attribute_state_data(NParticleAttributeState *attrstate, int index)
{
	return BLI_pbuf_get(&attrstate->data, index);
}


int BKE_nparticle_find_index(NParticleState *state, NParticleID id)
{
	NParticleAttributeState *attrstate = nparticle_state_find_attribute_id(state);
	if (attrstate) {
		bPagedBuffer *pbuf = &attrstate->data;
		bPagedBufferIterator it;
		
		for (BLI_pbuf_iter_init(pbuf, &it); BLI_pbuf_iter_valid(pbuf, &it); BLI_pbuf_iter_next(pbuf, &it)) {
			if (*(int*)it.data == id)
				return it.index;
		}
	}
	return -1;
}

bool BKE_nparticle_exists(NParticleState *state, NParticleID id)
{
	return BKE_nparticle_find_index(state, id) >= 0;
}

static bool nparticle_attribute_is_id(NParticleAttributeDescription *desc)
{
	return STREQ(desc->name, "id");
}

int BKE_nparticle_add(NParticleState *state, NParticleID id)
{
	int index = BKE_nparticle_find_index(state, id);
	if (index < 0) {
		NParticleAttributeState *attrstate;
		for (attrstate = state->attributes.first; attrstate; attrstate = attrstate->next) {
			BLI_pbuf_add_elements(&attrstate->data, 1);
			index = attrstate->data.totelem - 1;
			
			if (nparticle_attribute_is_id(&attrstate->desc)) {
				*(int*)BLI_pbuf_get(&attrstate->data, index) = (int)id;
			}
			else {
				/* XXX default value? */
				memset(BLI_pbuf_get(&attrstate->data, index), 0, attrstate->data.elem_bytes);
			}
		}
	}
	
	return index;
}

void BKE_nparticle_remove(NParticleState *state, NParticleID id)
{
	int index = BKE_nparticle_find_index(state, id);
	if (index >= 0) {
		NParticleAttributeState *attrstate;
		for (attrstate = state->attributes.first; attrstate; attrstate = attrstate->next) {
			/* XXX TODO paged buffer doesn't support removing yet */
//			BLI_pbuf_remove_elements(&attrstate->data, index);
		}
	}
}


void BKE_nparticle_iter_init(NParticleState *state, NParticleIterator *it)
{
	it->state = state;
	it->index = 0;
}

void BKE_nparticle_iter_from_id(NParticleState *state, NParticleIterator *it, NParticleID id)
{
	it->state = state;
	it->index = BKE_nparticle_find_index(state, id);
}

void BKE_nparticle_iter_from_index(NParticleState *state, NParticleIterator *it, int index)
{
	NParticleAttributeState *attrstate = nparticle_state_find_attribute_id(state);
	it->state = state;
	if (index >= 0 && attrstate && index < attrstate->data.totelem)
		it->index = index;
	else
		it->index = -1;
}

void BKE_nparticle_iter_next(NParticleIterator *it)
{
	++it->index;
}

bool BKE_nparticle_iter_valid(NParticleIterator *it)
{
	NParticleAttributeState *attrstate = nparticle_state_find_attribute_id(it->state);
	if (attrstate)
		return it->index >= 0 && it->index < attrstate->data.totelem;
	else
		return false;
}


BLI_INLINE void *nparticle_data_ptr(NParticleState *state, const char *name, int index)
{
	NParticleAttributeState *attrstate = BKE_nparticle_state_find_attribute(state, name);
	if (attrstate)
		return BLI_pbuf_get(&attrstate->data, index);
	else
		return NULL;
}

BLI_INLINE bool nparticle_check_attribute_type(NParticleState *state, const char *name, eParticleAttributeDataType datatype)
{
	/* sanity check to ensure the retrieved data attribute has the correct type.
	 * only happens in debug, no overhead created for release builds.
	 */
	NParticleAttributeState *attrstate = BKE_nparticle_state_find_attribute(state, name);
	return !attrstate || attrstate->desc.datatype == datatype;
}

#if 0 /* unused */
void *BKE_nparticle_iter_get_data(struct NParticleIterator *it, const char *attr)
{
	return nparticle_data_ptr(it->state, attr, it->index);
}
#endif

int BKE_nparticle_iter_get_int(NParticleIterator *it, const char *attr)
{
	int *data = nparticle_data_ptr(it->state, attr, it->index);
	BLI_assert(nparticle_check_attribute_type(it->state, attr, PAR_ATTR_DATATYPE_INT));
	return data ? *data : 0;
}

void BKE_nparticle_iter_set_int(NParticleIterator *it, const char *attr, int value)
{
	int *data = nparticle_data_ptr(it->state, attr, it->index);
	BLI_assert(nparticle_check_attribute_type(it->state, attr, PAR_ATTR_DATATYPE_INT));
	if (data)
		*data = value;
}

float BKE_nparticle_iter_get_float(NParticleIterator *it, const char *attr)
{
	float *data = nparticle_data_ptr(it->state, attr, it->index);
	BLI_assert(nparticle_check_attribute_type(it->state, attr, PAR_ATTR_DATATYPE_FLOAT));
	return data ? *data : 0.0f;
}

void BKE_nparticle_iter_set_float(NParticleIterator *it, const char *attr, float value)
{
	float *data = nparticle_data_ptr(it->state, attr, it->index);
	BLI_assert(nparticle_check_attribute_type(it->state, attr, PAR_ATTR_DATATYPE_FLOAT));
	if (data)
		*data = value;
}

void BKE_nparticle_iter_get_vector(NParticleIterator *it, const char *attr, float *result)
{
	float *data = nparticle_data_ptr(it->state, attr, it->index);
	BLI_assert(nparticle_check_attribute_type(it->state, attr, PAR_ATTR_DATATYPE_VECTOR)
	           || nparticle_check_attribute_type(it->state, attr, PAR_ATTR_DATATYPE_POINT)
	           || nparticle_check_attribute_type(it->state, attr, PAR_ATTR_DATATYPE_NORMAL));
	if (data)
		copy_v3_v3(result, data);
	else
		zero_v3(result);
}

void BKE_nparticle_iter_set_vector(NParticleIterator *it, const char *attr, const float *value)
{
	float *data = nparticle_data_ptr(it->state, attr, it->index);
	BLI_assert(nparticle_check_attribute_type(it->state, attr, PAR_ATTR_DATATYPE_VECTOR)
	           || nparticle_check_attribute_type(it->state, attr, PAR_ATTR_DATATYPE_POINT)
	           || nparticle_check_attribute_type(it->state, attr, PAR_ATTR_DATATYPE_NORMAL));
	if (data)
		copy_v3_v3(data, value);
}

void *BKE_nparticle_iter_get_pointer(NParticleIterator *it, const char *attr)
{
	void **data = nparticle_data_ptr(it->state, attr, it->index);
	BLI_assert(nparticle_check_attribute_type(it->state, attr, PAR_ATTR_DATATYPE_POINTER));
	return data ? *data : NULL;
}

void BKE_nparticle_iter_set_pointer(NParticleIterator *it, const char *attr, void *value)
{
	void **data = nparticle_data_ptr(it->state, attr, it->index);
	BLI_assert(nparticle_check_attribute_type(it->state, attr, PAR_ATTR_DATATYPE_POINTER));
	if (data)
		*data = value;
}


NParticleDisplay *BKE_nparticle_display_particle(void)
{
	NParticleDisplay *display = MEM_callocN(sizeof(NParticleDisplay), "particle display");
	display->type = PAR_DISPLAY_PARTICLE;
	BLI_strncpy(display->attribute, "position", sizeof(display->attribute));
	return display;
}

NParticleDisplay *BKE_nparticle_display_copy(NParticleDisplay *display)
{
	NParticleDisplay *ndisplay = MEM_dupallocN(display);
	return ndisplay;
}

void BKE_nparticle_display_free(NParticleDisplay *display)
{
	MEM_freeN(display);
}


void BKE_nparticle_system_update_rigid_body(RigidBodyWorld *rbw, Object *ob, NParticleSystem *psys)
{
	
}

void BKE_nparticle_system_apply_rigid_body(RigidBodyWorld *rbw, Object *ob, NParticleSystem *psys)
{
	
}

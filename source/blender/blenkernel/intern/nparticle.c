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

#include "BLI_listbase.h"
#include "BLI_pagedbuffer.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_nparticle_types.h"

#include "BKE_nparticle.h"

/* XXX TODO make this configurable */
#define PAGE_BYTES 65536

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

		default:
			BLI_assert(false);	/* unknown datatype, should never happen */
			return 0;
	}
}

static void nparticle_attribute_state_init(NParticleAttribute *attr, NParticleAttributeState *state)
{
	BLI_pbuf_init(&state->data, PAGE_BYTES, nparticle_elem_bytes(attr->desc.datatype));
}

static void nparticle_attribute_state_free(NParticleAttributeState *state)
{
	BLI_pbuf_free(&state->data);
}

static void nparticle_attribute_state_copy(NParticleAttributeState *to, NParticleAttributeState *from)
{
	BLI_pbuf_copy(&to->data, &from->data);
}


NParticleSystem *BKE_nparticle_system_new(void)
{
	NParticleSystem *psys = MEM_callocN(sizeof(NParticleSystem), "nparticle system");
	return psys;
}

void BKE_nparticle_system_free(NParticleSystem *psys)
{
	BKE_nparticle_attribute_remove_all(psys);
	MEM_freeN(psys);
}

NParticleSystem *BKE_nparticle_system_copy(NParticleSystem *psys)
{
	NParticleSystem *npsys = MEM_dupallocN(psys);
	NParticleAttribute *attr;
	
	npsys->attributes.first = npsys->attributes.last = NULL;
	for (attr = psys->attributes.first; attr; attr = attr->next) {
		BKE_nparticle_attribute_copy(npsys, psys, attr);
	}
	
	return npsys;
}


NParticleAttribute *BKE_nparticle_attribute_find(NParticleSystem *psys, const char *name)
{
	NParticleAttribute *attr;
	for (attr = psys->attributes.first; attr; attr = attr->next)
		if (STREQ(attr->desc.name, name))
			return attr;
	return NULL;
}

NParticleAttribute *BKE_nparticle_attribute_new(NParticleSystem *psys, const char *name, int datatype)
{
	NParticleAttribute *attr;
	
	attr = BKE_nparticle_attribute_find(psys, name);
	if (attr) {
		/* if attribute with the same name exists, remove it first */
		BKE_nparticle_attribute_remove(psys, attr);
	}
	
	if (!attr) {
		attr = MEM_callocN(sizeof(NParticleAttribute), "particle system attribute");
		BLI_strncpy(attr->desc.name, name, sizeof(attr->desc.name));
		attr->desc.datatype = datatype;
		
		BLI_addtail(&psys->attributes, attr);
		
		attr->state = MEM_callocN(sizeof(NParticleAttributeState), "particle attribute state");
		nparticle_attribute_state_init(attr, attr->state);
	}
	
	return attr;
}

void BKE_nparticle_attribute_remove(NParticleSystem *psys, NParticleAttribute *attr)
{
	if (attr->state) {
		nparticle_attribute_state_free(attr->state);
		MEM_freeN(attr->state);
	}
	
	BLI_remlink(&psys->attributes, attr);
	MEM_freeN(attr);
}

void BKE_nparticle_attribute_remove_all(NParticleSystem *psys)
{
	NParticleAttribute *attr, *attr_next;
	
	for (attr = psys->attributes.first; attr; attr = attr_next) {
		attr_next = attr->next;
		
		if (attr->state) {
			nparticle_attribute_state_free(attr->state);
			MEM_freeN(attr->state);
		}
		
		MEM_freeN(attr);
	}
	psys->attributes.first = psys->attributes.last = NULL;
}

NParticleAttribute *BKE_nparticle_attribute_copy(NParticleSystem *to_psys, NParticleSystem *UNUSED(from_psys), NParticleAttribute *from_attr)
{
	NParticleAttribute *to_attr = MEM_dupallocN(from_attr);
	
	to_attr->state = MEM_callocN(sizeof(NParticleAttributeState), "particle attribute state");
	nparticle_attribute_state_init(to_attr, to_attr->state);
	
	BLI_addtail(&to_psys->attributes, to_attr);
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


#if 0 /* old code */
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_nparticle_types.h"
#include "DNA_object_types.h"
#include "DNA_pagedbuffer_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_pagedbuffer.h"
#include "BLI_path_util.h"
#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_node.h"

#include "BKE_nparticle.h"
#include "BKE_object.h"
#include "BKE_library.h"
#include "BKE_depsgraph.h"

#include "RNA_access.h"


struct bPagedBufferIterator pit_init_particles(struct NParticleSystem *psys)
{
	return pit_init(&psys->particles);
}

struct bPagedBufferIterator pit_init_particles_at(struct NParticleSystem *psys, int index)
{
	return pit_init_at(&psys->particles, index);
}

/* skips dead particles */
void pit_next_particle(struct NParticleSystem *psys, bPagedBufferIterator *it)
{
	do {
		pit_next(it);
	} while (it->valid && pit_get_particle_flag(psys, it, NPAR_DEAD));
}

static void pbuf_print(bPagedBuffer *pbuf)
{
	bPagedBufferLayerInfo *layer = pbuf->layers.first;
//	char indent[64] = {'\0'};
	
	while (layer) {
		printf("Layer %s: ", layer->name);
		printf(" index=%d, stride=%d\n", layer->layer, layer->stride);
		
		layer = layer->next;
	}
}

void npar_init(NParticleSystem *psys)
{
	/* XXX NPARTICLES page size default? */
	pbufInit(&psys->particles, 1024);
	
	npar_add_attribute(psys, PAR_ATTR_TYPE_FLAG);
	npar_add_attribute(psys, PAR_ATTR_TYPE_ID);
	npar_add_attribute(psys, PAR_ATTR_TYPE_SOURCE_ID);
	npar_add_attribute(psys, PAR_ATTR_TYPE_RANDOM_SEED);
	npar_add_attribute(psys, PAR_ATTR_TYPE_REM_INDEX);
	npar_add_attribute(psys, PAR_ATTR_TYPE_BIRTH_TIME);
	npar_add_attribute(psys, PAR_ATTR_TYPE_POSITION);
	npar_add_attribute(psys, PAR_ATTR_TYPE_VELOCITY);
	npar_add_attribute(psys, PAR_ATTR_TYPE_FORCE);
	npar_add_attribute(psys, PAR_ATTR_TYPE_MASS);
	npar_add_attribute(psys, PAR_ATTR_TYPE_ROTATION);
	npar_add_attribute(psys, PAR_ATTR_TYPE_ANGULAR_VELOCITY);
	npar_add_attribute(psys, PAR_ATTR_TYPE_TORQUE);
	npar_add_attribute(psys, PAR_ATTR_TYPE_ANGULAR_MASS);
	
	nodePathMapInit(&psys->node_path_map);
	
	psys->next_source_id = 0;
	
	/* pbuf_print(&psys->particles); */
}

void npar_free(NParticleSystem *psys)
{
	NParticleAttribute *attr, *attr_next;
	
	/* free attributes */
	for (attr=psys->attributes.first; attr; attr=attr_next) {
		attr_next = attr->next;
		npar_remove_attribute(psys, attr);
	}
	
	nodePathMapFree(&psys->node_path_map);
	
	pbufFree(&psys->particles);
}

static NParticleAttribute *npar_copy_attribute(NParticleAttribute *attr)
{
	NParticleAttribute *nattr = MEM_dupallocN(attr);
	nattr->prev = nattr->next = NULL;
	
	nattr->layer = attr->layer->new_layer;
	nattr->layer_write = attr->layer_write->new_layer;
	
	/* temporary copy pointer for easy mapping */
	attr->new_attribute = nattr;
	
	return nattr;
}

void npar_copy(NParticleSystem *to, NParticleSystem *from)
{
	NParticleAttribute *attr;
	int i;
	
	pbufCopy(&to->particles, &from->particles);
	
	/* copy attributes */
	to->attributes.first = to->attributes.last = NULL;
	for (attr=from->attributes.first; attr; attr=attr->next) {
		BLI_addtail(&to->attributes, npar_copy_attribute(attr));
	}
	for (i=0; i < PAR_ATTR_TYPE_MAX; ++i) {
		if (from->standard_attribute[i])
			to->standard_attribute[i] = from->standard_attribute[i]->new_attribute;
	}
	if (from->active_attribute)
		to->active_attribute = from->active_attribute->new_attribute;
	
	nodePathMapCopy(&to->node_path_map, &from->node_path_map);
}

bPagedBufferIterator npar_create_particles(NParticleSystem *psys, NParticleSource *source, float time, float num)
{
	int delta;
	bPagedBufferIterator pit, res;
	
	source->emit_carry += num;
	delta = (int)(source->emit_carry);
	source->emit_carry -= floorf(source->emit_carry);
	
	res = pbufAppendElements(&psys->particles, delta);
	for (pit=res; pit.valid; pit_next(&pit)) {
		int id;
		
		id = source->next_element_id++;
		pit_set_int(&pit, psys->standard_attribute[PAR_ATTR_TYPE_ID]->layer, id);
		pit_set_int(&pit, psys->standard_attribute[PAR_ATTR_TYPE_SOURCE_ID]->layer, source->id);
		pit_set_float(&pit, psys->standard_attribute[PAR_ATTR_TYPE_BIRTH_TIME]->layer, time);
	}
	
	return res;
}

static int npar_test_dead(bPagedBufferIterator *pit, void *userdata)
{
	return pit_get_particle_flag((NParticleSystem*)userdata, pit, NPAR_DEAD);
}

void npar_free_dead_particles(NParticleSystem *psys)
{
	pbufCompress(&psys->particles, npar_test_dead, psys, psys->standard_attribute[PAR_ATTR_TYPE_REM_INDEX]->layer);
}

void npar_reset(NParticleSystem *psys)
{
	pbufReset(&psys->particles);
}


void npar_init_attribute(NParticleAttribute *attr, int type, NParticleSystem *psys)
{
	attr->type = type;
	attr->flag = 0;
	/* only gets created if psys != NULL */
	attr->layer = NULL;
	attr->layer_write = NULL;
	
	switch (type) {
	case PAR_ATTR_TYPE_FLAG: {
		static NParticleFlagLayerType default_value = 0;
		attr->datatype = PAR_ATTR_DATATYPE_INTERNAL;
		strcpy(attr->name, "__flag__");
		if (psys) {
			attr->layer = pbufLayerAdd(&psys->particles, "__flag__", sizeof(NParticleFlagLayerType), &default_value);
		}
		break;
	}
	case PAR_ATTR_TYPE_ID: {
		static int default_value = 0;
		attr->datatype = PAR_ATTR_DATATYPE_INT;
		strcpy(attr->name, "ID");
		if (psys) {
			attr->layer = pbufLayerAdd(&psys->particles, "__id__", sizeof(int), &default_value);
		}
		break;
	}
	case PAR_ATTR_TYPE_SOURCE_ID: {
		static int default_value = 0;
		attr->datatype = PAR_ATTR_DATATYPE_INT;
		strcpy(attr->name, "Source ID");
		if (psys) {
			attr->layer = pbufLayerAdd(&psys->particles, "__source_id__", sizeof(int), &default_value);
		}
		break;
	}
	case PAR_ATTR_TYPE_RANDOM_SEED: {
		static NParticleRNG default_value;
		attr->datatype = PAR_ATTR_DATATYPE_INTERNAL;
		strcpy(attr->name, "__random_seed__");
		if (psys) {
			attr->layer = pbufLayerAdd(&psys->particles, "__random_seed__", sizeof(NParticleRNG), &default_value);
		}
		break;
	}
	case PAR_ATTR_TYPE_REM_INDEX: {
		static int default_value = 0;
		attr->datatype = PAR_ATTR_DATATYPE_INTERNAL;
		strcpy(attr->name, "__rem_index__");
		if (psys) {
			attr->layer = pbufLayerAdd(&psys->particles, "__rem_index__", sizeof(int), &default_value);
		}
		break;
	}
	case PAR_ATTR_TYPE_BIRTH_TIME: {
		static float default_value = 0.0f;
		attr->datatype = PAR_ATTR_DATATYPE_FLOAT;
		strcpy(attr->name, "Birth Time");
		if (psys) {
			attr->layer = pbufLayerAdd(&psys->particles, "__birth_time__", sizeof(float), &default_value);
		}
		break;
	}
	case PAR_ATTR_TYPE_POSITION: {
		static NParticleVector default_value = { 0.0f, 0.0f, 0.0f };
		attr->datatype = PAR_ATTR_DATATYPE_POINT;
		attr->flag |= PAR_ATTR_STATE;
		strcpy(attr->name, "Position");
		if (psys) {
			attr->layer = pbufLayerAdd(&psys->particles, "__position__", sizeof(NParticleVector), &default_value);
			attr->layer_write = pbufLayerAdd(&psys->particles, "__position_write__", sizeof(NParticleVector), &default_value);
		}
		break;
	}
	case PAR_ATTR_TYPE_VELOCITY: {
		static NParticleVector default_value = { 0.0f, 0.0f, 0.0f };
		attr->datatype = PAR_ATTR_DATATYPE_NORMAL;
		attr->flag |= PAR_ATTR_STATE;
		strcpy(attr->name, "Velocity");
		if (psys) {
			attr->layer = pbufLayerAdd(&psys->particles, "__velocity__", sizeof(NParticleVector), &default_value);
			attr->layer_write = pbufLayerAdd(&psys->particles, "__velocity_write__", sizeof(NParticleVector), &default_value);
		}
		break;
	}
	case PAR_ATTR_TYPE_FORCE: {
		static NParticleVector default_value = { 0.0f, 0.0f, 0.0f };
		attr->datatype = PAR_ATTR_DATATYPE_NORMAL;
		strcpy(attr->name, "Force");
		if (psys) {
			attr->layer = pbufLayerAdd(&psys->particles, "__force__", sizeof(NParticleVector), &default_value);
		}
		break;
	}
	case PAR_ATTR_TYPE_MASS: {
		static float default_value = 1.0f;
		attr->datatype = PAR_ATTR_DATATYPE_FLOAT;
		attr->flag |= PAR_ATTR_STATE;
		strcpy(attr->name, "Mass");
		if (psys) {
			attr->layer = pbufLayerAdd(&psys->particles, "__mass__", sizeof(float), &default_value);
			attr->layer_write = pbufLayerAdd(&psys->particles, "__mass_write__", sizeof(float), &default_value);
		}
		break;
	}
	case PAR_ATTR_TYPE_ROTATION: {
		static NParticleQuaternion default_value = { 1.0f, 0.0f, 0.0f, 0.0f };
		attr->datatype = PAR_ATTR_DATATYPE_QUATERNION;
		attr->flag |= PAR_ATTR_STATE;
		strcpy(attr->name, "Rotation");
		if (psys) {
			attr->layer = pbufLayerAdd(&psys->particles, "__rotation__", sizeof(NParticleQuaternion), &default_value);
			attr->layer_write = pbufLayerAdd(&psys->particles, "__rotation_write__", sizeof(NParticleQuaternion), &default_value);
		}
		break;
	}
	case PAR_ATTR_TYPE_ANGULAR_VELOCITY: {
		static NParticleVector default_value = { 0.0f, 0.0f, 0.0f };
		attr->datatype = PAR_ATTR_DATATYPE_VECTOR;
		attr->flag |= PAR_ATTR_STATE;
		strcpy(attr->name, "Angular Velocity");
		if (psys) {
			attr->layer = pbufLayerAdd(&psys->particles, "__angular_velocity__", sizeof(NParticleVector), &default_value);
			attr->layer_write = pbufLayerAdd(&psys->particles, "__angular_velocity_write__", sizeof(NParticleVector), &default_value);
		}
		break;
	}
	case PAR_ATTR_TYPE_TORQUE: {
		static NParticleVector default_value = { 0.0f, 0.0f, 0.0f };
		attr->datatype = PAR_ATTR_DATATYPE_VECTOR;
		strcpy(attr->name, "Torque");
		if (psys) {
			attr->layer = pbufLayerAdd(&psys->particles, "__torque__", sizeof(NParticleVector), &default_value);
		}
		break;
	}
	case PAR_ATTR_TYPE_ANGULAR_MASS: {
		static float default_value = 1.0f;
		attr->datatype = PAR_ATTR_DATATYPE_FLOAT;
		attr->flag |= PAR_ATTR_STATE;
		strcpy(attr->name, "Angular Mass");
		if (psys) {
			attr->layer = pbufLayerAdd(&psys->particles, "__angular_mass__", sizeof(float), &default_value);
			attr->layer_write = pbufLayerAdd(&psys->particles, "__angular_mass_write__", sizeof(float), &default_value);
		}
		break;
	}
	
	case PAR_ATTR_TYPE_UNDEFINED:
		attr->datatype = PAR_ATTR_DATATYPE_INTERNAL;
		strcpy(attr->name, "");
		break;
	default: {
		/* XXX this should never happen, assert makes it easier to detect missing definition
		 * when new attributes are added.
		 */
		assert(0);
	}
	}
	
	/* little trick: this allows writing to non-state attributes without extra check */
	if (!attr->layer_write)
		attr->layer_write = attr->layer;
}

void npar_init_custom_attribute(NParticleAttribute *attr, int datatype, const char *name, NParticleSystem *psys)
{
	char name_write[256];
	
	attr->type = PAR_ATTR_TYPE_CUSTOM;
	attr->datatype = datatype;
	strcpy(attr->name, name);
	attr->flag |= PAR_ATTR_STATE;
	attr->layer = NULL;		/* only gets created if psys != NULL */
	attr->layer_write = NULL;		/* only gets created if psys != NULL */
	
	sprintf(name_write, "%s_write", name);
	
	if (psys) {
		switch (datatype) {
			case PAR_ATTR_DATATYPE_FLOAT: {
				static float default_value = 0.0f;
				attr->layer = pbufLayerAdd(&psys->particles, name, sizeof(float), &default_value);
				attr->layer_write = pbufLayerAdd(&psys->particles, name_write, sizeof(float), &default_value);
				break;
			}
			case PAR_ATTR_DATATYPE_INT: {
				static int default_value = 0;
				attr->layer = pbufLayerAdd(&psys->particles, name, sizeof(int), &default_value);
				attr->layer_write = pbufLayerAdd(&psys->particles, name_write, sizeof(int), &default_value);
				break;
			}
			case PAR_ATTR_DATATYPE_VECTOR:
			case PAR_ATTR_DATATYPE_POINT:
			case PAR_ATTR_DATATYPE_NORMAL:
			{
				static NParticleVector default_value = { 0.0f, 0.0f, 0.0f };
				attr->layer = pbufLayerAdd(&psys->particles, name, sizeof(NParticleVector), &default_value);
				attr->layer_write = pbufLayerAdd(&psys->particles, name_write, sizeof(NParticleVector), &default_value);
				break;
			}
			case PAR_ATTR_DATATYPE_COLOR: {
				static NParticleColor default_value = { 0.0f, 0.0f, 0.0f, 0.0f };
				attr->layer = pbufLayerAdd(&psys->particles, name, sizeof(NParticleColor), &default_value);
				attr->layer_write = pbufLayerAdd(&psys->particles, name_write, sizeof(NParticleColor), &default_value);
				break;
			}
			case PAR_ATTR_DATATYPE_QUATERNION: {
				static NParticleQuaternion default_value = { 1.0f, 0.0f, 0.0f, 0.0f };
				attr->layer = pbufLayerAdd(&psys->particles, name, sizeof(NParticleQuaternion), &default_value);
				attr->layer_write = pbufLayerAdd(&psys->particles, name_write, sizeof(NParticleQuaternion), &default_value);
				break;
			}
				
			default: {
				/* XXX this should never happen, assert makes it easier to detect missing definition. */
				assert(0);
			}
		}
	}
}

NParticleAttribute *npar_find_custom_attribute(NParticleSystem *psys, const char *name)
{
	NParticleAttribute *attr = psys->attributes.first;
	for (; attr; attr=attr->next)
		if (attr->type == PAR_ATTR_TYPE_CUSTOM && strcmp(attr->name, name)==0)
			break;
	return attr;
}

void npar_custom_attribute_unique_name(ListBase *attributes, NParticleAttribute *attr)
{
	const char *default_name = "Custom";
	BLI_uniquename(attributes, attr, default_name, '.', offsetof(NParticleAttribute, name), sizeof(attr->name));
}

NParticleAttribute *npar_add_attribute(NParticleSystem *psys, int type) {
	NParticleAttribute *attr = psys->standard_attribute[type];
	
	assert(!ELEM(type, PAR_ATTR_TYPE_UNDEFINED, PAR_ATTR_TYPE_CUSTOM));
	
	if (!attr) {
		attr = MEM_callocN(sizeof(NParticleAttribute), "particle attribute");
		npar_init_attribute(attr, type, psys);
		BLI_addtail(&psys->attributes, attr);
		
		/* add to standard attribute map */
		psys->standard_attribute[type] = attr;
	}
	
	return attr;
}

NParticleAttribute *npar_add_custom_attribute(NParticleSystem *psys, int datatype, const char *name)
{
	NParticleAttribute *attr;
	
	attr = MEM_callocN(sizeof(NParticleAttribute), "particle attribute");
	npar_init_custom_attribute(attr, datatype, name, psys);
	BLI_addtail(&psys->attributes, attr);
	
	/* make sure the attribute name is unique */
	npar_custom_attribute_unique_name(&psys->attributes, attr);
	
	/* select new attribute if none is active so far */
	if (!psys->active_attribute)
		psys->active_attribute = attr;
	
	return attr;
}

void npar_remove_attribute(NParticleSystem *psys, NParticleAttribute *attr) {
	/* make sure active attribute pointer stays valid */
	if (psys->active_attribute==attr)
		psys->active_attribute = NULL;
	
	/* remove from standard attribute map */
	if (!ELEM(attr->type, PAR_ATTR_TYPE_UNDEFINED, PAR_ATTR_TYPE_CUSTOM))
		psys->standard_attribute[attr->type] = NULL;
	
	if (attr->layer) {
		pbufLayerRemove(&psys->particles, attr->layer);
		BLI_remlink(&psys->attributes, attr);
		MEM_freeN(attr);
	}
}

int npar_socket_type_from_attribute(NParticleAttribute *attr)
{
	switch (attr->datatype) {
		case PAR_ATTR_DATATYPE_FLOAT:
			return SOCK_FLOAT;
		case PAR_ATTR_DATATYPE_INT:
			return SOCK_INT;
		case PAR_ATTR_DATATYPE_VECTOR:
		case PAR_ATTR_DATATYPE_POINT:
		case PAR_ATTR_DATATYPE_NORMAL:
			return SOCK_VECTOR;
		case PAR_ATTR_DATATYPE_QUATERNION:
			return SOCK_ROTATION;
		case PAR_ATTR_DATATYPE_COLOR:
			return SOCK_RGBA;
	
		default:
			assert(0);	/* should never happen */
			return -1;
	}
}

void npar_update_state(NParticleAttribute *attr, bPagedBufferIterator *pit)
{
	assert(attr->layer != NULL);
	assert(attr->layer_write != NULL);
	memcpy(PBUF_GET_GENERIC_DATA_POINTER(pit, attr->layer), PBUF_GET_GENERIC_DATA_POINTER(pit, attr->layer_write), attr->layer->stride);
}

void npar_update_state_all(NParticleSystem *psys, bPagedBufferIterator *pit)
{
	NParticleAttribute *attr;
	for (attr = psys->attributes.first; attr; attr=attr->next) {
		if (!(attr->flag & PAR_ATTR_STATE))
			continue;
		
		assert(attr->layer != NULL);
		assert(attr->layer_write != NULL);
		memcpy(PBUF_GET_GENERIC_DATA_POINTER(pit, attr->layer), PBUF_GET_GENERIC_DATA_POINTER(pit, attr->layer_write), attr->layer->stride);
	}
}


NParticleSource *npar_source_new(NParticleSystem *psys)
{
	NParticleSource *source = MEM_callocN(sizeof(NParticleSource), "particle source");
	nodePathInstanceInit(&source->base, NODE_INSTANCE_PARTICLE_SOURCE);
	source->id = psys->next_source_id++;
	source->next_element_id = 0;
	return source;
}


NParticleDupliObject *npar_dupli_object_new(void)
{
	NParticleDupliObject *dupli = MEM_callocN(sizeof(NParticleDupliObject), "nparticles dupli object");
	return dupli;
}

void npar_dupli_object_free(NParticleDupliObject *dupli)
{
	MEM_freeN(dupli);
}

NParticleDupliObject *npar_dupli_object_copy(NParticleDupliObject *dupli)
{
	NParticleDupliObject *ndupli = MEM_dupallocN(dupli);
	return ndupli;
}

void npar_create_dupli_object(NParticlesModifierData *pmd, bPagedBufferIterator *pit, Object **ob, float mat[][4])
{
	NParticleSystem *psys = pmd->psys;
	NParticleDupliObject *dupli;
	const NParticleVector *pos;
	const NParticleQuaternion *rot;
	float size[3];
	
	if (!pmd->dupli_objects.first) {
		*ob = NULL;
		return;
	}
	
	/* XXX TODO */
	dupli = pmd->dupli_objects.first;
	*ob = dupli->ob;
	
	pos = pit_get_particle_position(psys, pit);
	rot = pit_get_particle_rotation(psys, pit);
	size[0] = size[1] = size[2] = 1.0f;	/* XXX TODO */
	
	loc_quat_size_to_mat4(mat, (const float *)pos, (const float *)rot, size);
}

static int npar_check_particle_type(bNodeTree *ntree, int type)
{
	PointerRNA ptr;
	RNA_id_pointer_create((ID *)ntree, &ptr);
	return (RNA_enum_get(&ptr, "particle_type") & type) != 0;
}

void npar_filter_objects(Scene *scene, int type, void *userdata, void (*walk)(void *userdata, Scene *scene, Object *ob, bNodeTree *ntree))
{
	Base *base;
	for (base = scene->base.first; base; base = base->next) {
		if (type & PAR_NODETREE_EMITTER) {
			NParticlesModifierExtData *pmd = (NParticlesModifierExtData *)modifiers_findByType(base->object, eModifierType_NParticleEmitter);
			if (pmd) {
				BLI_assert(npar_check_particle_type(pmd->nodetree, type));
				walk(userdata, scene, base->object, pmd->nodetree);
			}
		}
		
		if (type & PAR_NODETREE_MODIFIER) {
			NParticlesModifierExtData *pmd = (NParticlesModifierExtData *)modifiers_findByType(base->object, eModifierType_NParticleModifier);
			if (pmd) {
				BLI_assert(npar_check_particle_type(pmd->nodetree, type));
				walk(userdata, scene, base->object, pmd->nodetree);
			}
		}
		
		if (type & PAR_NODETREE_SYSTEM) {
			NParticlesModifierData *pmd = (NParticlesModifierData *)modifiers_findByType(base->object, eModifierType_NParticleSystem);
			if (pmd) {
				BLI_assert(npar_check_particle_type(pmd->nodetree, type));
				walk(userdata, scene, base->object, pmd->nodetree);
			}
		}
	}
}
#endif

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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/particle_modifier.c
 *  \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "DNA_particle_types.h"

#include "BKE_particle.h"

#include "BLF_translation.h"

static ParticleModifierTypeInfo *particle_modifier_types[NUM_PARTICLE_MODIFIER_TYPES];

static ParticleModifierTypeInfo modifierType_None = {
	"None",
	"ParticleModifierData",
	sizeof(ParticleModifierData)
};

static ParticleModifierTypeInfo modifierType_MeshDeform = {
	"MeshDeform",
	"MeshDeformParticleModifierData",
	sizeof(MeshDeformParticleModifierData)
};

void particle_modifier_types_init(void)
{
#define INIT_TYPE(name) (particle_modifier_types[eParticleModifierType_##name] = &modifierType_##name)
	INIT_TYPE(None);
	INIT_TYPE(MeshDeform);
#undef INIT_TYPE
}

ParticleModifierTypeInfo *particle_modifier_type_info_get(ParticleModifierType type)
{
	/* type unsigned, no need to check < 0 */
	if (type < NUM_PARTICLE_MODIFIER_TYPES && particle_modifier_types[type]->name[0] != '\0') {
		return particle_modifier_types[type];
	}
	else {
		return NULL;
	}
}

void particle_modifier_foreachIDLink(Object *ob, ParticleSystem *psys, IDWalkParticleFunc walk, void *userData)
{
	ParticleModifierData *md = psys->modifiers.first;

	for (; md; md = md->next) {
		ParticleModifierTypeInfo *mti = particle_modifier_type_info_get(md->type);

		if (mti->foreachIDLink)
			mti->foreachIDLink(md, ob, psys, walk, userData);
	}
}

ParticleModifierData  *particle_modifier_new(int type)
{
	ParticleModifierTypeInfo *mti = particle_modifier_type_info_get(type);
	ParticleModifierData *md = MEM_callocN(mti->structSize, mti->structName);
	
	/* note, this name must be made unique later */
	BLI_strncpy(md->name, DATA_(mti->name), sizeof(md->name));

	md->type = type;

	if (mti->initData) mti->initData(md);

	return md;
}

void particle_modifier_free(ParticleModifierData *md)
{
	ParticleModifierTypeInfo *mti = particle_modifier_type_info_get(md->type);

	if (mti->freeData) mti->freeData(md);
	if (md->error) MEM_freeN(md->error);

	MEM_freeN(md);
}

void particle_modifier_unique_name(ListBase *modifiers, ParticleModifierData *md)
{
	if (modifiers && md) {
		ParticleModifierTypeInfo *mti = particle_modifier_type_info_get(md->type);
		
		BLI_uniquename(modifiers, md, DATA_(mti->name), '.', offsetof(ModifierData, name), sizeof(md->name));
	}
}

ParticleModifierData *particle_modifier_findByName(Object *UNUSED(ob), ParticleSystem *psys, const char *name)
{
	return BLI_findstring(&(psys->modifiers), name, offsetof(ParticleModifierData, name));
}

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

/** \file blender/modifiers/intern/MOD_wrinkle.c
 *  \ingroup modifiers
 */

#include <string.h>
#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_modifier.h"
#include "BKE_wrinkle.h"

#include "MOD_util.h"

#include "depsgraph_private.h"
#include "DEG_depsgraph_build.h"

static void initData(ModifierData *md) 
{
	WrinkleModifierData *wmd = (WrinkleModifierData *)md;
	
	UNUSED_VARS(wmd);
	
	/* XXX linker hack: only using BKE_wrinkle functions from RNA
	 * does not prevent them from being stripped!
	 */
	static void *__force_link_wrinkle__ = BKE_wrinkle_map_add;
	UNUSED_VARS(__force_link_wrinkle__);
}

static void copyData(ModifierData *md, ModifierData *target)
{
	WrinkleModifierData *wmd  = (WrinkleModifierData *)md;
	WrinkleModifierData *twmd = (WrinkleModifierData *)target;
	
	modifier_copyData_generic(md, target);
	
	for (WrinkleMapSettings *map = twmd->wrinkle_maps.first; map; map = map->next) {
		if (map->texture) {
			id_us_plus(&map->texture->id);
		}
	}
	
	UNUSED_VARS(wmd);
}

static void freeData(ModifierData *md)
{
	WrinkleModifierData *wmd = (WrinkleModifierData *)md;
	
	for (WrinkleMapSettings *map = wmd->wrinkle_maps.first; map; map = map->next) {
		if (map->texture) {
			id_us_min(&map->texture->id);
		}
	}
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	WrinkleModifierData *wmd = (WrinkleModifierData *)md;
	CustomDataMask dataMask = 0;
	
	dataMask |= CD_MASK_ORCO | CD_MASK_MLOOPUV;
	
	for (WrinkleMapSettings *map = wmd->wrinkle_maps.first; map; map = map->next) {
		/* ask for vertexgroups if we need them */
		if (map->defgrp_name[0])
			dataMask |= CD_MASK_MDEFORMVERT;
		
		/* ask for UV coordinates if we need them */
		if (map->texmapping == MOD_DISP_MAP_UV)
			dataMask |= CD_MASK_MTFACE;
	}

	return dataMask;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob, DerivedMesh *derivedData,
                                  ModifierApplyFlag UNUSED(flag))
{
	Mesh *mesh = ob->data;
	WrinkleModifierData *wmd = (WrinkleModifierData *)md;
	DerivedMesh *dm = CDDM_copy(derivedData);
	
	const float (*orco)[3] = dm->getVertDataArray(dm, CD_ORCO);
	/* XXX this is terrible, but deform-only input DM doesn't seem to provide a proper CD_ORCO layer */
	float (*orco_buf)[3] = NULL;
	if (orco == NULL) {
		orco_buf = MEM_mallocN(sizeof(float) * 3 * mesh->totvert, "orco buffer");
		for (int i = 0; i < mesh->totvert; ++i) {
			copy_v3_v3(orco_buf[i], mesh->mvert[i].co);
		}
		orco = orco_buf;
	}
	
	BKE_wrinkle_apply(wmd, dm, orco);
	
	if (orco_buf)
		MEM_freeN(orco_buf);
	
	return dm;
}

static void updateDepgraph(ModifierData *md, DagForest *UNUSED(forest),
                           struct Main *UNUSED(bmain),
                           struct Scene *UNUSED(scene),
                           Object *UNUSED(ob),
                           DagNode *UNUSED(obNode))
{
	WrinkleModifierData *wmd = (WrinkleModifierData *)md;
	
	UNUSED_VARS(wmd);
}

static void updateDepsgraph(ModifierData *md,
                            struct Main *UNUSED(bmain),
                            struct Scene *UNUSED(scene),
                            Object *UNUSED(ob),
                            struct DepsNodeHandle *UNUSED(node))
{
	WrinkleModifierData *wmd = (WrinkleModifierData *)md;
	
	UNUSED_VARS(wmd);
}

static void foreachObjectLink(ModifierData *md, Object *ob,
                              ObjectWalkFunc walk, void *userData)
{
	WrinkleModifierData *wmd = (WrinkleModifierData *)md;
	
	for (WrinkleMapSettings *map = wmd->wrinkle_maps.first; map; map = map->next) {
		walk(userData, ob, &map->map_object, IDWALK_NOP);
	}
}

static void foreachIDLink(ModifierData *md, Object *ob,
                          IDWalkFunc walk, void *userData)
{
	WrinkleModifierData *wmd = (WrinkleModifierData *)md;
	
	for (WrinkleMapSettings *map = wmd->wrinkle_maps.first; map; map = map->next) {
		walk(userData, ob, (ID **)&map->texture, IDWALK_USER);
	}
	
	foreachObjectLink(md, ob, (ObjectWalkFunc)walk, userData);
}

/* XXX this callback is too limiting: the texture pointers are not direct
 * members of the ModifierData, so we can give back a good property name!
 */
#if 0
static void foreachTexLink(ModifierData *md, Object *ob,
                           TexWalkFunc walk, void *userData)
{
	WrinkleModifierData *wmd = (WrinkleModifierData *)md;
	
	for (WrinkleMapSettings *map = wmd->map_settings.first; map; map = map->next) {
		walk(userData, ob, map, "texture");
	}
}
#endif

ModifierTypeInfo modifierType_Wrinkle = {
	/* name */              "Wrinkle",
	/* structName */        "WrinkleModifierData",
	/* structSize */        sizeof(WrinkleModifierData),
	/* type */              eModifierTypeType_NonGeometrical,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsMapping |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_UsesPreview,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    updateDepgraph,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */  NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    NULL/*foreachTexLink*/,
};

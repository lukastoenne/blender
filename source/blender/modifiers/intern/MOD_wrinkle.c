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

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_modifier.h"
#include "BKE_texture.h"

#include "depsgraph_private.h"
#include "DEG_depsgraph_build.h"

static void initData(ModifierData *md) 
{
	WrinkleModifierData *wmd = (WrinkleModifierData *)md;
	
	wmd->flag = 0;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	WrinkleModifierData *wmd  = (WrinkleModifierData *)md;
	WrinkleModifierData *twmd = (WrinkleModifierData *)target;
	
	UNUSED_VARS(wmd, twmd);
}

static void freeData(ModifierData *md)
{
	WrinkleModifierData *wmd = (WrinkleModifierData *)md;
	
	UNUSED_VARS(wmd);
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	WrinkleModifierData *wmd = (WrinkleModifierData *)md;
	CustomDataMask dataMask = 0;

	dataMask |= CD_MASK_MLOOPUV;

	UNUSED_VARS(wmd);
	return dataMask;
}

static void deformVerts(ModifierData *md, Object *ob,
                        DerivedMesh *derivedData,
                        float (*vertexCos)[3],
                        int numVerts,
                        ModifierApplyFlag UNUSED(flag))
{
	WrinkleModifierData *wmd = (WrinkleModifierData *)md;

	UNUSED_VARS(wmd, ob, derivedData, vertexCos, numVerts);
}

static void deformVertsEM(ModifierData *md, Object *ob,
                          struct BMEditMesh *em,
                          DerivedMesh *derivedData,
                          float (*vertexCos)[3],
                          int numVerts)
{
	WrinkleModifierData *wmd = (WrinkleModifierData *)md;
	
	UNUSED_VARS(wmd, ob, em, derivedData, vertexCos, numVerts);
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

static void foreachIDLink(ModifierData *md, Object *ob,
                          IDWalkFunc walk, void *userData)
{
	WrinkleModifierData *wmd = (WrinkleModifierData *)md;
	
	UNUSED_VARS(wmd, ob, walk, userData);
}

ModifierTypeInfo modifierType_Wrinkle = {
	/* name */              "Wrinkle",
	/* structName */        "WrinkleModifierData",
	/* structSize */        sizeof(WrinkleModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh,

	/* copyData */          copyData,
	/* deformVerts */       deformVerts,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     deformVertsEM,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    updateDepgraph,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */  NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    NULL,
};

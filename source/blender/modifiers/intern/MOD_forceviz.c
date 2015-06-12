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

/** \file blender/modifiers/intern/MOD_forceviz.c
 *  \ingroup modifiers
 */

#include <string.h>
#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_effect.h"
#include "BKE_image.h"
#include "BKE_modifier.h"
#include "BKE_texture.h"

#include "depsgraph_private.h"
#include "DEG_depsgraph_build.h"

static void initData(ModifierData *md) 
{
	ForceVizModifierData *fmd = (ForceVizModifierData *)md;
	
	BKE_texture_mapping_default(&fmd->tex_mapping, TEXMAP_TYPE_POINT);
	fmd->iuser.frames = 1;
	fmd->iuser.sfra = 1;
	fmd->iuser.fie_ima = 2;
	fmd->iuser.ok = 1;
	
	fmd->flag = MOD_FORCEVIZ_USE_IMG_VEC;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	ForceVizModifierData *fmd  = (ForceVizModifierData *)md;
	ForceVizModifierData *tfmd = (ForceVizModifierData *)target;
	
	memcpy(&tfmd->tex_mapping, &fmd->tex_mapping, sizeof(tfmd->tex_mapping));
	memcpy(&tfmd->iuser, &fmd->iuser, sizeof(tfmd->iuser));
}

static void freeData(ModifierData *UNUSED(md))
{
//	ForceVizModifierData *fmd = (ForceVizModifierData *)md;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	ForceVizModifierData *fmd = (ForceVizModifierData *)md;
	CustomDataMask dataMask = 0;

	dataMask |= CD_MASK_MLOOPUV;

	return dataMask;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob, 
                                  DerivedMesh *dm,
                                  ModifierApplyFlag UNUSED(flag))
{
	ForceVizModifierData *fmd = (ForceVizModifierData *)md;

	BKE_forceviz_do(fmd, md->scene, ob, dm);

	return dm;
}

static void updateDepgraph(ModifierData *UNUSED(md), DagForest *UNUSED(forest),
                           struct Main *UNUSED(bmain),
                           struct Scene *UNUSED(scene),
                           Object *UNUSED(ob),
                           DagNode *UNUSED(obNode))
{
//	ForceVizModifierData *fmd = (ForceVizModifierData *)md;
}

static void updateDepsgraph(ModifierData *UNUSED(md),
                            struct Main *UNUSED(bmain),
                            struct Scene *UNUSED(scene),
                            Object *UNUSED(ob),
                            struct DepsNodeHandle *UNUSED(node))
{
//	ForceVizModifierData *fmd = (ForceVizModifierData *)md;
}

static bool dependsOnTime(ModifierData *UNUSED(md))
{
	return true;
}

static void foreachIDLink(ModifierData *md, Object *ob,
                          IDWalkFunc walk, void *userData)
{
	ForceVizModifierData *fmd = (ForceVizModifierData *)md;

	walk(userData, ob, (ID **)&fmd->image_vec);
	walk(userData, ob, (ID **)&fmd->image_div);
	walk(userData, ob, (ID **)&fmd->image_curl);
}

ModifierTypeInfo modifierType_ForceViz = {
	/* name */              "Force Visualization",
	/* structName */        "ForceVizModifierData",
	/* structSize */        sizeof(ForceVizModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh,

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
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */  NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    NULL,
};

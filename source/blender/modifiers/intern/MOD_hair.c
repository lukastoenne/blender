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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2014 by the Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_hair.c
 *  \ingroup modifiers
 */

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_hair_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_hair.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"

#include "depsgraph_private.h"

#include "HAIR_capi.h"

#include "MOD_util.h"
#include "MEM_guardedalloc.h"

static void initData(ModifierData *md)
{
	HairModifierData *hmd = (HairModifierData *) md;
	
	hmd->hairsys = BKE_hairsys_new();
}

static void freeData(ModifierData *md)
{
	HairModifierData *hmd = (HairModifierData *) md;
	
	if (hmd->solver)
		HAIR_solver_free(hmd->solver);
	
	BKE_hairsys_free(hmd->hairsys);
}

static void copyData(ModifierData *md, ModifierData *target)
{
	HairModifierData *hmd = (HairModifierData *) md;
	HairModifierData *thmd = (HairModifierData *) target;
	
	if (thmd->hairsys)
		BKE_hairsys_free(thmd->hairsys);
	
	thmd->hairsys = BKE_hairsys_copy(hmd->hairsys);
	
	thmd->solver = NULL;
}

static DerivedMesh *applyModifier(ModifierData *UNUSED(md), Object *UNUSED(ob),
                                  DerivedMesh *dm,
                                  ModifierApplyFlag UNUSED(flag))
{
	return dm;
}

static void updateDepgraph(
        ModifierData *UNUSED(md), DagForest *UNUSED(forest), Scene *UNUSED(scene),
        Object *UNUSED(ob), DagNode *UNUSED(obNode))
{
	/*HairModifierData *hmd = (HairModifierData *) md;*/
}

static bool dependsOnTime(ModifierData *UNUSED(md))
{
	return true;
}


ModifierTypeInfo modifierType_Hair = {
	/* name */              "Hair",
	/* structName */        "HairModifierData",
	/* structSize */        sizeof(HairModifierData),
	/* type */              eModifierTypeType_Nonconstructive,

	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_Single,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};

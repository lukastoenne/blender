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

/** \file blender/modifiers/intern/MOD_meshsampletest.c
 *  \ingroup modifiers
 */


#include <limits.h>

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"

#include "MOD_modifiertypes.h"
#include "MEM_guardedalloc.h"

#include "BLI_strict_flags.h"

static void initData(ModifierData *md)
{
	MeshSampleTestModifierData *smd = (MeshSampleTestModifierData *) md;
}

static void copyData(ModifierData *md, ModifierData *target)
{
#if 0
	MeshSampleTestModifierData *smd = (MeshSampleTestModifierData *) md;
	MeshSampleTestModifierData *tsmd = (MeshSampleTestModifierData *) target;
#endif
	modifier_copyData_generic(md, target);
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
                                  DerivedMesh *derivedData,
                                  ModifierApplyFlag flag)
{
	DerivedMesh *dm = derivedData;
	DerivedMesh *result;
	MeshSampleTestModifierData *smd = (MeshSampleTestModifierData *) md;
	
	result = derivedData;
	
	return result;
}

ModifierTypeInfo modifierType_MeshSampleTest = {
	/* name */              "MeshSampleTest",
	/* structName */        "MeshSampleTestModifierData",
	/* structSize */        sizeof(MeshSampleTestModifierData),
	/* type */              eModifierTypeType_Constructive,

	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_EnableInEditmode,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};

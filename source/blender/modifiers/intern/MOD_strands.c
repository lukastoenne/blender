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
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Daniel Dunbar
 *                 Ton Roosendaal,
 *                 Ben Batt,
 *                 Brecht Van Lommel,
 *                 Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_displace.c
 *  \ingroup modifiers
 */

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "DNA_object_types.h"
#include "DNA_strand_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_editstrands.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_modifier.h"
#include "BKE_strands.h"

#include "depsgraph_private.h"
#include "MEM_guardedalloc.h"

#include "MOD_util.h"


static void initData(ModifierData *md)
{
	StrandsModifierData *smd = (StrandsModifierData *) md;
	
	smd->strands = BKE_strands_new();
	
	smd->num_fibers = 0;
	smd->fibers = NULL;
	
	smd->flag |= MOD_STRANDS_SHOW_STRANDS |
	             MOD_STRANDS_SHOW_FIBERS;
	
	smd->edit = NULL;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	StrandsModifierData *smd = (StrandsModifierData *) md;
	StrandsModifierData *tsmd = (StrandsModifierData *) target;

	if (tsmd->strands) {
		BKE_strands_free(tsmd->strands);
	}
	if (tsmd->fibers) {
		MEM_freeN(tsmd->fibers);
	}

	modifier_copyData_generic(md, target);
	
	if (smd->strands) {
		tsmd->strands = BKE_strands_copy(smd->strands);
	}
	if (smd->fibers) {
		tsmd->fibers = MEM_dupallocN(smd->fibers);
	}
	
	tsmd->edit = NULL;
}

static void freeData(ModifierData *md)
{
	StrandsModifierData *smd = (StrandsModifierData *) md;
	
	if (smd->strands) {
		BKE_strands_free(smd->strands);
	}
	if (smd->fibers) {
		MEM_freeN(smd->fibers);
	}
	
	if (smd->edit) {
		BKE_editstrands_free(smd->edit);
		MEM_freeN(smd->edit);
	}
}

static DerivedMesh *applyModifier(ModifierData *md, Object *UNUSED(ob),
                                  DerivedMesh *dm,
                                  ModifierApplyFlag UNUSED(flag))
{
	StrandsModifierData *smd = (StrandsModifierData *) md;
	
	if (smd->strands) {
		
		/* regenerate roots if necessary */
		if (smd->fibers == NULL && smd->num_fibers > 0) {
			smd->fibers = BKE_strands_scatter(smd->strands, dm, smd->num_fibers, smd->seed);
		}
		
		if (smd->strands->data_final)
			BKE_strand_data_free(smd->strands->data_final);
		smd->strands->data_final = BKE_strand_data_calc(smd->strands, dm,
		                                                smd->fibers, smd->fibers ? smd->num_fibers : 0);
		
		if (smd->edit) {
			/* clear draw data from edit when updating */
			BKE_editstrands_clear_drawdata(smd->edit);
		}
	}
	
	return dm;
}

ModifierTypeInfo modifierType_Strands = {
	/* name */              "Strands",
	/* structName */        "StrandsModifierData",
	/* structSize */        sizeof(StrandsModifierData),
	/* type */              eModifierTypeType_NonGeometrical,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsEditmode,

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
	/* updateDepgraph */    NULL,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};

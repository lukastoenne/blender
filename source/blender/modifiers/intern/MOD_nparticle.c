/*
* $Id$
*
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
* Contributor(s): none yet.
*
* ***** END GPL LICENSE BLOCK *****
*
*/

/** \file blender/modifiers/intern/MOD_nparticles.c
 *  \ingroup modifiers
 */

#include <stddef.h>

#include "BLI_listbase.h"

#include "DNA_modifier_types.h"
#include "DNA_nparticle_types.h"

#include "BKE_modifier.h"
#include "BKE_nparticle.h"

struct DerivedMesh;
struct Object;

static void nparticle_system_initData(ModifierData *md) 
{
	NParticleSystemModifierData *pmd= (NParticleSystemModifierData *)md;
	
	pmd->psys = BKE_nparticle_system_new();
	
	/* add default particle display */
	BLI_addtail(&pmd->display, BKE_nparticle_display_particle());
}

static void nparticle_system_freeData(ModifierData *md)
{
	NParticleSystemModifierData *pmd= (NParticleSystemModifierData *)md;
	NParticleDisplay *display, *display_next;
	
	for (display = pmd->display.first; display; display = display_next) {
		display_next = display->next;
		BKE_nparticle_display_free(display);
	}
	pmd->display.first = pmd->display.last = NULL;
	
	BKE_nparticle_system_free(pmd->psys);
	pmd->psys = NULL;
}

static void nparticle_system_copyData(ModifierData *md, ModifierData *target)
{
	NParticleSystemModifierData *pmd= (NParticleSystemModifierData *)md;
	NParticleSystemModifierData *tpmd= (NParticleSystemModifierData *)target;
	NParticleDisplay *display, *ndisplay;
	
	tpmd->psys = BKE_nparticle_system_copy(pmd->psys);
	
	tpmd->display.first = tpmd->display.last = NULL;
	for (display = pmd->display.first; display; display = display->next) {
		ndisplay = BKE_nparticle_display_copy(display);
		BLI_addtail(&tpmd->display, ndisplay);
	}
}

static struct DerivedMesh *nparticle_system_applyModifier(ModifierData *UNUSED(md), struct Object *UNUSED(ob),
                                                          struct DerivedMesh *derivedData,
                                                          ModifierApplyFlag UNUSED(flag))
{
	/*NParticleSystemModifierData *pmd= (NParticleSystemModifierData *)md;*/
	return derivedData;
}

ModifierTypeInfo modifierType_NParticleSystem = {
	/* name */              "Particles",
	/* structName */        "NParticleSystemModifierData",
	/* structSize */        sizeof(NParticleSystemModifierData),
	/* type */              eModifierTypeType_NonGeometrical,
	/* flags */             eModifierTypeFlag_AcceptsMesh	/* for now only allow single particle buffer for unambiguous access */
	                        | eModifierTypeFlag_Single
	                        | eModifierTypeFlag_UsesPointCache,

	/* copyData */          nparticle_system_copyData,
	/* deformVerts */       NULL,
	/* deformVertsEM */     NULL,
	/* deformMatrices */    NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     nparticle_system_applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          nparticle_system_initData,
	/* requiredDataMask */  NULL,
	/* freeData */          nparticle_system_freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
};

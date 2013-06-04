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

#include "MEM_guardedalloc.h"

#include "DNA_nparticle_types.h"
#include "DNA_pagedbuffer_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_pagedbuffer.h"
#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_global.h"
#include "BKE_modifier.h"
#include "BKE_nparticle.h"
#include "BKE_scene.h"
#include "depsgraph_private.h"

#include "MOD_util.h"

#include "GNO_blender.h"


static void npar_system_initData(ModifierData *md) 
{
	NParticlesModifierData *pmd= (NParticlesModifierData*) md;
	
	pmd->psys = MEM_callocN(sizeof(NParticleSystem), "particle system");
	npar_init(pmd->psys);
	
	pmd->substeps = 10;
}

static void npar_system_freeData(ModifierData *md)
{
	NParticlesModifierData *pmd= (NParticlesModifierData*) md;
	NParticleDupliObject *dupli, *dupli_next;
	
	for (dupli = pmd->dupli_objects.first; dupli; dupli = dupli_next) {
		dupli_next = dupli->next;
		npar_dupli_object_free(dupli);
	}
	
	if (pmd->session) {
		GNO_session_end(pmd->session);
		pmd->session = NULL;
	}
	
	npar_free(pmd->psys);
	MEM_freeN(pmd->psys);
	pmd->psys = NULL;
}

static void npar_system_copyData(ModifierData *md, ModifierData *target)
{
	NParticlesModifierData *pmd= (NParticlesModifierData*) md;
	NParticlesModifierData *tpmd= (NParticlesModifierData*) target;
	NParticleDupliObject *dupli, *tdupli;
	
	tpmd->nodetree = pmd->nodetree;
	
	tpmd->psys = MEM_mallocN(sizeof(NParticleSystem), "particle system");
	npar_copy(tpmd->psys, pmd->psys);
	
	for (dupli = pmd->dupli_objects.first; dupli; dupli = dupli->next) {
		tdupli = npar_dupli_object_copy(dupli);
		BLI_addtail(&tpmd->dupli_objects,  tdupli);
	}
}

static int npar_system_dependsOnTime(ModifierData *UNUSED(md))
{
	return 1;
}

static void npar_system_foreachIDLink(ModifierData *md, Object *ob,
						  IDWalkFunc walk, void *userData)
{
	NParticlesModifierData *pmd = (NParticlesModifierData*) md;
	NParticleDupliObject *dupli;
	
	walk(userData, ob, (ID**)&pmd->nodetree);
	
	for (dupli = pmd->dupli_objects.first; dupli; dupli = dupli->next)
		walk(userData, ob, (ID**)&dupli->ob);
	
//	foreachObjectLink(md, ob, (ObjectWalkFunc)walk, userData);
}

static void npar_system_updateDepgraph(ModifierData *md, DagForest *forest, struct Scene *scene, Object *ob, DagNode *obNode)
{
	NParticlesModifierData *pmd = (NParticlesModifierData *)md;
	
	if (pmd->nodetree)
		GNO_update_depgraph_from_nodes(pmd->nodetree, forest, scene, ob, obNode);
}

/* calculates particle timestep */
static DerivedMesh *npar_system_applyModifier(ModifierData *md, Object *ob,
                                  DerivedMesh *derivedData,
                                  ModifierApplyFlag UNUSED(flag))
{
	NParticlesModifierData *pmd= (NParticlesModifierData*) md;
	float cfra = BKE_scene_frame_get(md->scene);
	DerivedMesh *result = derivedData;
	
//	npar_update_frame(pmd, cfra);
	
	if (!pmd->nodetree)
		return derivedData;
	
	if (!pmd->session)
		pmd->session = GNO_session_begin(G.main, md->scene);
	
	GNO_session_sync(pmd->session);
	
	/* ******** time step ******** */
	if (cfra == 1.0f) {
		npar_reset(pmd->psys);
	}
	if (pmd->cfra < cfra) {
		float sec_per_frame = 1.0f / md->scene->r.frs_sec;
		float dfra_base = 1.0f / (float)pmd->substeps;
		
		float step_cfra = pmd->cfra;
		float step_time = step_cfra * sec_per_frame;
		while (step_cfra < cfra) {
			/* limit timestep up to current time */
			float dfra = MIN2(dfra_base, cfra - step_cfra);
			float dtime = dfra * sec_per_frame;
			
			/* avoid very small timesteps from float rounding errors
			 * XXX necessary?
			 */
			if (dfra < 0.000001f)
				break;
			
			GNO_timestep(pmd->session, ob, pmd->psys, pmd->nodetree,
			             step_cfra, dfra, step_time, dtime);
			
			step_cfra += dfra;
			step_time += dtime;
		}
	}
	
	/* clean up the buffer */
	npar_free_dead_particles(pmd->psys);
	
	/* update to new frame value */
	pmd->cfra = cfra;
	/* **************** */
	
	/* update dupli flag */
	if (pmd->render_mode == MOD_NPAR_RENDER_DUPLI)
		ob->transflag |= OB_DUPLINPARTS;
	else
		ob->transflag &= ~OB_DUPLINPARTS;
	
	return result;
}

/* disabled particles in editmode for now, until support for proper derivedmesh
 * updates is coded */
#if 0
static void npar_system_applyModifierEM(
				ModifierData *md, Object *ob, EditMesh *editData,
				DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
}
#endif


ModifierTypeInfo modifierType_NParticleSystem = {
	/* name */              "Particle System",
	/* structName */        "NParticlesModifierData",
	/* structSize */        sizeof(NParticlesModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_Single		/* single modifier needed only for unambiguous access (physics panel) */
							| eModifierTypeFlag_UsesPointCache /*
							| eModifierTypeFlag_SupportsEditmode
							| eModifierTypeFlag_EnableInEditmode */,

	/* copyData */          npar_system_copyData,
	/* deformVerts */       NULL,
	/* deformVertsEM */     NULL /* deformVertsEM */ ,
	/* deformMatrices */    NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     npar_system_applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          npar_system_initData,
	/* requiredDataMask */  NULL,
	/* freeData */          npar_system_freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    npar_system_updateDepgraph,
	/* dependsOnTime */     npar_system_dependsOnTime,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     npar_system_foreachIDLink,
};


static void npar_modifier_initData(ModifierData *UNUSED(md))
{
}

static void npar_modifier_freeData(ModifierData *UNUSED(md))
{
}

static void npar_modifier_copyData(ModifierData *md, ModifierData *target)
{
	NParticlesModifierData *pmd = (NParticlesModifierData *)md;
	NParticlesModifierData *tpmd = (NParticlesModifierData *)target;
	
	tpmd->nodetree = pmd->nodetree;
}

static void npar_modifier_foreachIDLink(ModifierData *md, Object *ob,
						  IDWalkFunc walk, void *userData)
{
	NParticlesModifierExtData *pmd = (NParticlesModifierExtData*) md;
	
	walk(userData, ob, (ID**)&pmd->nodetree);
}

static DerivedMesh *npar_modifier_applyModifier(ModifierData *UNUSED(md), Object *UNUSED(ob),
                                     DerivedMesh *derivedData,
                                     ModifierApplyFlag UNUSED(flag))
{
	return derivedData;
}

static DerivedMesh *npar_modifier_applyModifierEM(ModifierData *UNUSED(md), Object *UNUSED(ob),
                                    struct BMEditMesh *UNUSED(editData),
                                    DerivedMesh *derivedData)
{
	return derivedData;
}

ModifierTypeInfo modifierType_NParticleModifier = {
	/* name */              "Particle Modifier",
	/* structName */        "NParticlesModifierExtData",
	/* structSize */        sizeof(NParticlesModifierExtData),
	/* type */              eModifierTypeType_NonGeometrical,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_AcceptsCVs
							| eModifierTypeFlag_SupportsMapping
							| eModifierTypeFlag_SupportsEditmode
							| eModifierTypeFlag_EnableInEditmode,

	/* copyData */          npar_modifier_copyData,
	/* deformVerts */       NULL,
	/* deformVertsEM */     NULL,
	/* deformMatrices */    NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     npar_modifier_applyModifier,
	/* applyModifierEM */   npar_modifier_applyModifierEM,
	/* initData */          npar_modifier_initData,
	/* requiredDataMask */  NULL,
	/* freeData */          npar_modifier_freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     npar_modifier_foreachIDLink,
};

ModifierTypeInfo modifierType_NParticleEmitter = {
	/* name */              "Particle Emitter",
	/* structName */        "NParticlesModifierExtData",
	/* structSize */        sizeof(NParticlesModifierExtData),
	/* type */              eModifierTypeType_NonGeometrical,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_AcceptsCVs
							| eModifierTypeFlag_SupportsMapping
							| eModifierTypeFlag_SupportsEditmode
							| eModifierTypeFlag_EnableInEditmode,

	/* copyData */          npar_modifier_copyData,
	/* deformVerts */       NULL,
	/* deformVertsEM */     NULL,
	/* deformMatrices */    NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     npar_modifier_applyModifier,
	/* applyModifierEM */   npar_modifier_applyModifierEM,
	/* initData */          npar_modifier_initData,
	/* requiredDataMask */  NULL,
	/* freeData */          npar_modifier_freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     npar_modifier_foreachIDLink,
};

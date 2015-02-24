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
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/modifiers/intern/MOD_pointcache.c
 *  \ingroup modifiers
 */

#include <stdio.h>
#include <stdlib.h>

#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_DerivedMesh.h"
#include "BKE_scene.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_main.h"
#include "BKE_pointcache.h"

#include "PTC_api.h"

#include "MEM_guardedalloc.h"

#include "MOD_modifiertypes.h"

#include "MOD_util.h"

struct BMEditMesh;

static void initData(ModifierData *md)
{
	PointCacheModifierData *pcmd = (PointCacheModifierData *)md;

	pcmd->flag = 0;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	/*PointCacheModifierData *pcmd = (PointCacheModifierData *)md;
	PointCacheModifierData *tpcmd = (PointCacheModifierData *)target;*/

	modifier_copyData_generic(md, target);
}

static void freeData(ModifierData *md)
{
	PointCacheModifierData *pcmd = (PointCacheModifierData *)md;
	
	if (pcmd->reader)
		PTC_reader_free(pcmd->reader);
	if (pcmd->writer)
		PTC_writer_free(pcmd->writer);
}

static bool dependsOnTime(ModifierData *md)
{
	PointCacheModifierData *pcmd = (PointCacheModifierData *)md;
	
	/* considered time-dependent when reading from cache file */
	/* TODO check cache frame range here to optimize */
	return PTC_mod_point_cache_get_mode(pcmd) == MOD_POINTCACHE_MODE_READ;
}

static DerivedMesh *pointcache_do(PointCacheModifierData *pcmd, Object *ob, DerivedMesh *dm)
{
	Scene *scene = pcmd->modifier.scene;
	const float cfra = BKE_scene_frame_get(scene);
	ePointCacheModifierMode mode = PTC_mod_point_cache_get_mode(pcmd);
	
	DerivedMesh *finaldm = dm;
	
#if 0
	if (mode == MOD_POINTCACHE_MODE_NONE) {
		mode = PTC_mod_point_cache_set_mode(scene, ob, pcmd, MOD_POINTCACHE_MODE_READ);
	}
	
	if (mode == MOD_POINTCACHE_MODE_WRITE) {
		pcmd->output_dm = dm;
		PTC_write_sample(pcmd->writer);
		pcmd->output_dm = NULL;
	}
	else if (mode == MOD_POINTCACHE_MODE_READ) {
		if (PTC_read_sample(pcmd->reader, cfra) == PTC_READ_SAMPLE_INVALID) {
			modifier_setError(&pcmd->modifier, "%s", "Cannot read cache file");
		}
		else {
			DerivedMesh *result = PTC_reader_point_cache_acquire_result(pcmd->reader);
			if (result)
				finaldm = result;
		}
	}
#endif
	
	return finaldm;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
                                  DerivedMesh *dm,
                                  ModifierApplyFlag UNUSED(flag))
{
	PointCacheModifierData *pcmd = (PointCacheModifierData *)md;

	return pointcache_do(pcmd, ob, dm);
}

static DerivedMesh *applyModifierEM(ModifierData *md, Object *ob,
                                    struct BMEditMesh *UNUSED(editData),
                                    DerivedMesh *dm,
                                    ModifierApplyFlag UNUSED(flag))
{
	PointCacheModifierData *pcmd = (PointCacheModifierData *)md;

	return pointcache_do(pcmd, ob, dm);
}

ModifierTypeInfo modifierType_PointCache = {
	/* name */              "Point Cache",
	/* structName */        "PointCacheModifierData",
	/* structSize */        sizeof(PointCacheModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_AcceptsCVs |
	                        eModifierTypeFlag_SupportsEditmode,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   applyModifierEM,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */  NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};

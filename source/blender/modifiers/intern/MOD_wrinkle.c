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
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_modifier.h"
#include "BKE_texture.h"
#include "BKE_wrinkle.h"

#include "MOD_util.h"

#include "depsgraph_private.h"
#include "DEG_depsgraph_build.h"

static void cache_triangles(MVertTri **r_tri_verts, int **r_vert_numtri, const MLoop *mloop, const MLoopTri *looptri, int numverts, int numlooptri)
{
	MVertTri *tri_verts = MEM_mallocN(sizeof(MVertTri) * numlooptri, "triangle vertices");
	int *vert_numtri = MEM_callocN(sizeof(int) * numverts, "vertex triangle number");
	
	int i;
	for (i = 0; i < numlooptri; i++) {
		int v1 = mloop[looptri[i].tri[0]].v;
		int v2 = mloop[looptri[i].tri[1]].v;
		int v3 = mloop[looptri[i].tri[2]].v;
		
		tri_verts[i].tri[0] = v1;
		tri_verts[i].tri[1] = v2;
		tri_verts[i].tri[2] = v3;
		
		++vert_numtri[v1];
		++vert_numtri[v2];
		++vert_numtri[v3];
	}
	
	*r_tri_verts = tri_verts;
	*r_vert_numtri = vert_numtri;
}

typedef struct TriDeform {
	float a; /* x axis scale */
	float d; /* y axis scale */
	float b; /* shear */
} TriDeform;

/* 2D shape parameters of a triangle.
 * L is the base length
 * H is the height
 * x is the distance of the opposing point from the y axis
 * 
 *  H |     o                 
 *    |    /.\                
 *    |   / .  \              
 *    |  /  .    \            
 *    | /   .      \      
 *    |/    .        \    
 *    o----------------o--
 *          x          L
 */
static void get_triangle_shape(const float co1[3], const float co2[3], const float co3[3],
                               float *L, float *H, float *x)
{
	float v1[3], v2[3];
	sub_v3_v3v3(v1, co2, co1);
	sub_v3_v3v3(v2, co3, co1);
	
	float s[3], t[3];
	*L = normalize_v3_v3(s, v1);
	*x = dot_v3v3(v2, s);
	madd_v3_v3v3fl(t, v2, s, -(*x));
	*H = len_v3(t);
}

/* Get a 2D transform from the original triangle to the deformed,
 * as well as the inverse.
 * 
 * We chose v1 as the X axis and the Y axis orthogonal in the triangle plane.
 * The transform then has 3 degrees of freedom:
 * a scaling factor for both x and y and a shear factor.
 */
static void get_triangle_deform(TriDeform *def, TriDeform *idef,
                                const MVertTri *tri,
                                const MVert *mverts, const float (*orco)[3])
{
	float oL, oH, ox;
	get_triangle_shape(orco[tri->tri[0]], orco[tri->tri[1]], orco[tri->tri[2]], &oL, &oH, &ox);
	if (oL == 0.0f || oH == 0.0f) {
		def->a = idef->a = 1.0f;
		def->b = idef->b = 0.0f;
		def->d = idef->d = 1.0f;
		return;
	}
	
	float L, H, x;
	get_triangle_shape(mverts[tri->tri[0]].co, mverts[tri->tri[1]].co, mverts[tri->tri[2]].co, &L, &H, &x);
	if (L == 0.0f || H == 0.0f) {
		def->a = idef->a = 1.0f;
		def->b = idef->b = 0.0f;
		def->d = idef->d = 1.0f;
		return;
	}
	
	def->a = L / oL;
	def->d = H / oH;
	def->b = (x * oL - ox * L) / (oL * oH);
	idef->a = oL / L;
	idef->d = oH / H;
	idef->b = (ox * L - x * oL) / (L * H);
}

static void wrinkle_vgroup(WrinkleModifierData *wmd, DerivedMesh *dm, const float (*orco)[3])
{
	BLI_assert(orco != NULL);
	DM_ensure_looptri(dm);
	
	int numverts = dm->getNumVerts(dm);
	int numtris = dm->getNumLoopTri(dm);
	const MLoop *mloop = dm->getLoopArray(dm);
	const MLoopTri *looptri = dm->getLoopTriArray(dm);
	const MVert *mverts = dm->getVertArray(dm);
	
	MVertTri *tri_verts;
	int *vert_numtri;
	cache_triangles(&tri_verts, &vert_numtri, mloop, looptri, numverts, numtris);
	
	printf("WRINKLE:\n");
	float *influence = MEM_callocN(sizeof(float) * numverts, "wrinkle influence");
	for (int i = 0; i < numtris; ++i) {
		TriDeform def, idef;
		get_triangle_deform(&def, &idef, &tri_verts[i], mverts, orco);
		
		float C1 = 1.0f;
		float C2 = 0.0f;
		float C3 = 0.0f;
		float C4 = 1.0f;
		float h = 1.0f - (C1*(idef.a - 1.0f) + C2*idef.b + C3*(idef.d - 1.0f)) / C4;
		
//		printf("  %d: [%.3f, %.3f, 0.0, %.3f, %.4f]\n", i, def.a, def.b, def.d, h);
		
		for (int k = 0; k < 3; ++k) {
			int v = tri_verts[i].tri[k];
			
			influence[v] += h;
		}
	}
	
	for (int i = 0; i < numverts; ++i) {
		if (vert_numtri[i] > 0) {
			influence[i] /= (float)vert_numtri[i];
		}
	}
#if 0
	for (int i = 0; i < numverts; ++i) {
		if (vert_numtri[i] > 0) {
			madd_v3_v3fl(coords[i], offset[i], 1.0f / (float)vert_numtri[i]);
		}
	}
#endif
	
	MEM_freeN(tri_verts);
	MEM_freeN(vert_numtri);
	MEM_freeN(influence);
}

static void wrinkle_do(WrinkleModifierData *wmd, DerivedMesh *dm, const float (*orco)[3])
{
	wrinkle_vgroup(wmd, dm, orco);
}

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
	
	wrinkle_do(wmd, dm, orco);
	
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

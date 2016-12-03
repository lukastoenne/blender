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

/** \file wrinkle.c
 *  \ingroup blenkernel
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_library.h"
#include "BKE_texture.h"
#include "BKE_wrinkle.h"

#include "RE_shader_ext.h"

static WrinkleMapSettings *wrinkle_map_create(Tex *texture)
{
	WrinkleMapSettings *map = MEM_callocN(sizeof(WrinkleMapSettings), "WrinkleMapSettings");
	
	if (texture) {
		map->texture = texture;
		id_us_plus(&texture->id);
	}
	
	map->direction = MOD_WRINKLE_DIR_NOR;
	
	return map;
}

static void wrinkle_map_free(WrinkleMapSettings *map)
{
	if (map->texture) {
		id_us_min(&map->texture->id);
	}
	
	MEM_freeN(map);
}

WrinkleMapSettings *BKE_wrinkle_map_add(WrinkleModifierData *wmd)
{
	WrinkleMapSettings *map = wrinkle_map_create(NULL);
	
	BLI_addtail(&wmd->wrinkle_maps, map);
	return map;
}

void BKE_wrinkle_map_remove(WrinkleModifierData *wmd, WrinkleMapSettings *map)
{
	BLI_assert(BLI_findindex(&wmd->wrinkle_maps, map) != -1);
	
	BLI_remlink(&wmd->wrinkle_maps, map);
	
	wrinkle_map_free(map);
}

void BKE_wrinkle_maps_clear(WrinkleModifierData *wmd)
{
	WrinkleMapSettings *map_next;
	for (WrinkleMapSettings *map = wmd->wrinkle_maps.first; map; map = map_next) {
		map_next = map->next;
		
		wrinkle_map_free(map);
	}
	BLI_listbase_clear(&wmd->wrinkle_maps);
}

void BKE_wrinkle_map_move(WrinkleModifierData *wmd, int from_index, int to_index)
{
	BLI_assert(from_index >= 0 && from_index < BLI_listbase_count(&wmd->wrinkle_maps));
	BLI_assert(to_index >= 0 && to_index < BLI_listbase_count(&wmd->wrinkle_maps));
	
	WrinkleMapSettings *map = BLI_findlink(&wmd->wrinkle_maps, from_index);
	WrinkleMapSettings *map_next = BLI_findlink(&wmd->wrinkle_maps, to_index);
	if (to_index >= from_index)
		map_next = map_next->next;
	
	BLI_remlink(&wmd->wrinkle_maps, map);
	BLI_insertlinkbefore(&wmd->wrinkle_maps, map_next, map);
}

/* ========================================================================= */

static void get_texture_coords(WrinkleMapSettings *map, Object *ob, DerivedMesh *dm,
                               int numverts, const float (*co)[3], float (*texco)[3])
{
	int i;
	int texmapping = map->texmapping;
	float mapob_imat[4][4];

	if (texmapping == MOD_DISP_MAP_OBJECT) {
		if (map->map_object)
			invert_m4_m4(mapob_imat, map->map_object->obmat);
		else /* if there is no map object, default to local */
			texmapping = MOD_DISP_MAP_LOCAL;
	}

	/* UVs need special handling, since they come from faces */
	if (texmapping == MOD_DISP_MAP_UV) {
		if (CustomData_has_layer(&dm->loopData, CD_MLOOPUV)) {
			MPoly *mpoly = dm->getPolyArray(dm);
			MPoly *mp;
			MLoop *mloop = dm->getLoopArray(dm);
			char *done = MEM_callocN(sizeof(*done) * numverts,
			                         "get_texture_coords done");
			int numPolys = dm->getNumPolys(dm);
			char uvname[MAX_CUSTOMDATA_LAYER_NAME];
			MLoopUV *mloop_uv;

			CustomData_validate_layer_name(&dm->loopData, CD_MLOOPUV, map->uvlayer_name, uvname);
			mloop_uv = CustomData_get_layer_named(&dm->loopData, CD_MLOOPUV, uvname);

			/* verts are given the UV from the first face that uses them */
			for (i = 0, mp = mpoly; i < numPolys; ++i, ++mp) {
				unsigned int fidx = mp->totloop - 1;

				do {
					unsigned int lidx = mp->loopstart + fidx;
					unsigned int vidx = mloop[lidx].v;

					if (done[vidx] == 0) {
						/* remap UVs from [0, 1] to [-1, 1] */
						texco[vidx][0] = (mloop_uv[lidx].uv[0] * 2.0f) - 1.0f;
						texco[vidx][1] = (mloop_uv[lidx].uv[1] * 2.0f) - 1.0f;
						done[vidx] = 1;
					}

				} while (fidx--);
			}

			MEM_freeN(done);
			return;
		}
		else /* if there are no UVs, default to local */
			texmapping = MOD_DISP_MAP_LOCAL;
	}

	for (i = 0; i < numverts; ++i, ++co, ++texco) {
		switch (texmapping) {
			case MOD_DISP_MAP_LOCAL:
				copy_v3_v3(*texco, *co);
				break;
			case MOD_DISP_MAP_GLOBAL:
				mul_v3_m4v3(*texco, ob->obmat, *co);
				break;
			case MOD_DISP_MAP_OBJECT:
				mul_v3_m4v3(*texco, ob->obmat, *co);
				mul_m4_v3(mapob_imat, *texco);
				break;
		}
	}
}

static void wrinkle_texture_displace(const float *influence, DerivedMesh *dm, Scene *scene, Tex *texture, const float (*texco)[3])
{
	/* XXX any nicer way to ensure we only get a CDDM? */
	BLI_assert(dm->type == DM_TYPE_CDDM);
	
	DM_ensure_normals(dm);
	
	int numverts = dm->getNumVerts(dm);
	MVert *mverts = dm->getVertArray(dm);
	float (*coords)[3] = MEM_mallocN(sizeof(float) * 3 * numverts, "vertex coords");
	for (int i = 0; i < numverts; i++) {
		MVert *mv = &mverts[i];
		float w = influence[i];
		
		float nor[3];
		normal_short_to_float_v3(nor, mv->no);
		
		TexResult texres;
		BKE_texture_get_value(scene, texture, (float *)texco[i], &texres, false);
		
		/* TODO use texres for displacement */
		
		madd_v3_v3v3fl(coords[i], mv->co, nor, w);
	}
	
	CDDM_apply_vert_coords(dm, coords);
}

static void wrinkle_set_vgroup_weights(const float *influence, int numverts, int defgrp_index, MDeformVert *dvert)
{
	BLI_assert(dvert != NULL);
	BLI_assert(defgrp_index >= 0);
	
	for (int i = 0; i < numverts; i++) {
		float w = influence[i];
		MDeformVert *dv = &dvert[i];
		MDeformWeight *dw = defvert_find_index(dv, defgrp_index);

		/* If the vertex is in this vgroup, remove it if needed, or just update it. */
		if (dw) {
			if (w == 0.0f) {
				defvert_remove_group(dv, dw);
			}
			else {
				dw->weight = w;
			}
		}
		/* Else, add it if needed! */
		else if (w > 0.0f) {
			defvert_add_index_notest(dv, defgrp_index, w);
		}
	}
}

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

/* create an array of per-vertex influence for a given wrinkle map */
static float* get_wrinkle_map_influence(WrinkleModifierData *wmd, DerivedMesh *dm, const float (*orco)[3],
                                        WrinkleMapSettings *map)
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
	
//	printf("WRINKLE:\n");
	float *influence = MEM_callocN(sizeof(float) * numverts, "wrinkle influence");
	for (int i = 0; i < numtris; ++i) {
		TriDeform def, idef;
		get_triangle_deform(&def, &idef, &tri_verts[i], mverts, orco);
		
		float C1 = 1.0f;
		float C2 = 1.0f;
		float C3 = 0.0f;
		float C4 = 1.0f;
//		float h = 1.0f - (C1*(idef.a - 1.0f) + C2*idef.b + C3*(idef.d - 1.0f)) / C4;
		float h = (C1*(idef.a - 1.0f) + C2*idef.b + C3*(idef.d - 1.0f)) / C4;
		
//		printf("  %d: [%.3f, %.3f, 0.0, %.3f, %.4f]\n", i, def.a, def.b, def.d, h);
		
		for (int k = 0; k < 3; ++k) {
			int v = tri_verts[i].tri[k];
			
			influence[v] += h;
		}
	}
	
	for (int i = 0; i < numverts; ++i) {
		if (vert_numtri[i] > 0) {
			float w = influence[i] / (float)vert_numtri[i];
			CLAMP_MIN(w, 0.0f);
			
			influence[i] = w;
		}
	}
	
	MEM_freeN(tri_verts);
	MEM_freeN(vert_numtri);
	
	return influence;
}

void BKE_wrinkle_apply(Object *ob, WrinkleModifierData *wmd, DerivedMesh *dm, const float (*orco)[3])
{
	const bool apply_displace = wmd->flag & MOD_WRINKLE_APPLY_DISPLACEMENT;
	const bool apply_vgroups = wmd->flag & MOD_WRINKLE_APPLY_VERTEX_GROUPS;
	int numverts = dm->getNumVerts(dm);
	
	for (WrinkleMapSettings *map = wmd->wrinkle_maps.first; map; map = map->next) {
		int defgrp_index = -1;
		MDeformVert *dvert = NULL;
		bool apply_map_vgroup = false;
		
		if (apply_vgroups) {
			/* Get vgroup idx from its name. */
			defgrp_index = defgroup_name_index(ob, map->defgrp_name);
			
			if (defgrp_index != -1) {
				dvert = CustomData_duplicate_referenced_layer(&dm->vertData, CD_MDEFORMVERT, numverts);
				/* If no vertices were ever added to an object's vgroup, dvert might be NULL. */
				if (!dvert) {
					/* add a valid data layer */
					dvert = CustomData_add_layer_named(&dm->vertData, CD_MDEFORMVERT, CD_CALLOC,
					                                   NULL, numverts, map->defgrp_name);
				}
				if (dvert)
					apply_map_vgroup = true;
			}
		}
		
		if (apply_displace || apply_map_vgroup) {
			float *influence = get_wrinkle_map_influence(wmd, dm, orco, map);
			if (influence) {
				if (apply_displace) {
					if (map->texture) {
						float (*texco)[3] = MEM_mallocN(sizeof(float) * 3 * numverts, "texco");
						get_texture_coords(map, ob, dm, numverts, orco, texco);
						
						wrinkle_texture_displace(influence, dm, wmd->modifier.scene, map->texture, texco);
						
						MEM_freeN(texco);
					}
				}
				if (apply_map_vgroup)
					wrinkle_set_vgroup_weights(influence, numverts, defgrp_index, dvert);
				
				MEM_freeN(influence);
			}
		}
	}
}

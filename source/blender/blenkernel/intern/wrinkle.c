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
#include "BLI_string.h"

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_modifier.h"
#include "BKE_object_deform.h"
#include "BKE_texture.h"
#include "BKE_wrinkle.h"

#include "RE_shader_ext.h"

//#define WRINKLE_DEBUG

#ifdef WRINKLE_DEBUG
#include "BKE_effect.h"
#include "BLI_ghash.h"
#endif

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

static void cache_triangles(MVertTri **r_tri_verts, int **r_vert_numtri, DerivedMesh *dm)
{
	int numverts = dm->getNumVerts(dm);
	int numlooptri = dm->getNumLoopTri(dm);
	const MLoop *mloop = dm->getLoopArray(dm);
	const MLoopTri *looptri = dm->getLoopTriArray(dm);
	
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
BLI_INLINE void get_triangle_shape_ex(const float co1[3], const float co2[3], const float co3[3],
                                      float *L, float *H, float *x, float trimat[3][3])
{
	float v1[3], v2[3];
	sub_v3_v3v3(v1, co2, co1);
	sub_v3_v3v3(v2, co3, co1);
	
	float s[3], t[3];
	*L = normalize_v3_v3(s, v1);
	*x = dot_v3v3(v2, s);
	madd_v3_v3v3fl(t, v2, s, -(*x));
	*H = len_v3(t);
	
	if (trimat) {
		copy_v3_v3(trimat[0], s);
		normalize_v3_v3(trimat[1], t);
		cross_v3_v3v3(trimat[2], trimat[0], trimat[1]);
	}
}

BLI_INLINE void get_triangle_shape(const float co1[3], const float co2[3], const float co3[3],
                                   float *L, float *H, float *x)
{
	return get_triangle_shape_ex(co1, co2, co3, L, H, x, NULL);
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
static void get_wrinkle_map_influence(DerivedMesh *dm, const float (*orco)[3], const MVertTri *tri_verts,
                                      const WrinkleMapCoefficients *coeff,
                                      float *influence)
{
	BLI_assert(orco != NULL);
	
	int numtris = dm->getNumLoopTri(dm);
	BLI_assert(coeff->numtris == numtris);
	const MVert *mverts = dm->getVertArray(dm);
	
	for (int i = 0; i < numtris; ++i) {
		TriDeform def, idef;
		get_triangle_deform(&def, &idef, &tri_verts[i], mverts, orco);
		
		const float *C = coeff->C[i];
		float h;
		if (fabs(C[3]) < 1.0e-6f)
			h = 1.0f;
		else
			h = 1.0f - (C[0]*(idef.a - 1.0f) + C[1]*idef.b + C[2]*(idef.d - 1.0f)) / C[3];
		
		influence[i] = h;
	}
}

/* ========================================================================= */

typedef struct WrinkleMapCache {
	struct WrinkleMapCache *next, *prev;
	
	Key *key;
	KeyBlock *keyblock, *refblock;
	int defgrp_index;
	MDeformVert *dvert, *dvert_orig;
	
	float *influence;
	float *vertex_weight;
} WrinkleMapCache;

static void build_wrinkle_map_cache(Object *ob, ListBase *map_cache)
{
	Mesh *mesh = ob->data;
	int numverts = mesh->totvert;
	
	BLI_listbase_clear(map_cache);
	
	Key *key = BKE_key_from_object(ob);
	if (!key)
		return;
	
	for (KeyBlock *kb = key->block.first; kb; kb = kb->next) {
		if (kb == key->refkey
		    || (kb->flag & KEYBLOCK_MUTE)
		    || kb->curval == 0.0f
		    || !(kb->flag & KEYBLOCK_WRINKLE_MAP))
			continue;
		
		KeyBlock *refb = BLI_findlink(&key->block, kb->relative);
		if (!refb)
			continue;
		
		int defgrp_index = defgroup_name_index(ob, kb->vgroup);
		if (defgrp_index == -1)
			continue;
		
		MDeformVert *dvert = mesh->dvert;
		if (!dvert) {
			dvert = BKE_object_defgroup_data_create(&ob->id);
			if (!dvert)
				continue;
		}
		
		WrinkleMapCache *map = MEM_callocN(sizeof(WrinkleMapCache), "WrinkleMapCache");
		map->key = key;
		map->keyblock = kb;
		map->refblock = refb;
		map->defgrp_index = defgrp_index;
		map->dvert = dvert;
		map->dvert_orig = MEM_dupallocN(dvert);
		for (int i = 0; i < numverts; ++i) {
			if (dvert[i].dw)
				dvert[i].dw = MEM_dupallocN(dvert[i].dw);
		}
		
		map->influence = NULL;
		map->vertex_weight = NULL;
		
		BLI_addtail(map_cache, map);
	}
}

static void free_wrinkle_map_cache(Object *ob, ListBase *map_cache)
{
	Mesh *mesh = ob->data;
	int numverts = mesh->totvert;
	
	for (WrinkleMapCache *map = map_cache->first; map; map = map->next) {
		if (map->influence)
			MEM_freeN(map->influence);
		if (map->vertex_weight)
			MEM_freeN(map->vertex_weight);
		
		for (int i = 0; i < numverts; ++i) {
			if (map->dvert[i].dw)
				MEM_freeN(map->dvert[i].dw);
		}
		memcpy(map->dvert, map->dvert_orig, sizeof(MDeformVert) * numverts);
		MEM_freeN(map->dvert_orig);
	}
	BLI_freelistN(map_cache);
}

/* ========================================================================= */

BLI_INLINE float smooth_blend(float weight, float sum, float variance, float smoothness)
{
	if (UNLIKELY(sum == 0.0f))
		return 0.0f;
	
	float factor;
	if (weight < 0.5f*(sum - variance))
		factor = 0.0f;
	else if (weight >= 0.5f*(sum + variance))
		factor = 1.0f;
	else {
		float t = (2.0f*weight - sum) / variance;
		factor = powf(t, smoothness + 1.0f);
	}
	
	return factor * weight;
}

static void blend_wrinkle_influence(const ListBase *map_cache, int numtris, float var, float smooth)
{
	float *sum = MEM_callocN(sizeof(float) * numtris, "map influence sum");
	for (WrinkleMapCache *map = map_cache->first; map; map = map->next) {
		for (int i = 0; i < numtris; ++i) {
			sum[i] += map->influence[i];
		}
	}
	
	for (WrinkleMapCache *map = map_cache->first; map; map = map->next) {
		for (int i = 0; i < numtris; ++i) {
			map->influence[i] = smooth_blend(map->influence[i], sum[i], var, smooth);
		}
	}
	
	MEM_freeN(sum);
}

static void bake_vertex_influence(const ListBase *map_cache, int numtris, int numverts,
                                  const MVertTri *tri_verts, const int *vert_numtri)
{
	for (WrinkleMapCache *map = map_cache->first; map; map = map->next) {
		BLI_assert(map->influence != NULL);
		
		BLI_assert(map->vertex_weight == NULL);
		map->vertex_weight = MEM_callocN(sizeof(float) * numverts, "wrinkle vertex weight");
		
		for (int i = 0; i < numtris; ++i) {
			float weight = 1.0f - map->influence[i];
			for (int k = 0; k < 3; ++k) {
				int v = tri_verts[i].tri[k];
				
				map->vertex_weight[v] += weight;
			}
		}
		
		for (int i = 0; i < numverts; ++i) {
			if (vert_numtri[i] > 0) {
				float avg_weight = map->vertex_weight[i] / (float)vert_numtri[i];
				CLAMP_MIN(avg_weight, 0.0f);
				
				map->vertex_weight[i] = avg_weight;
			}
		}
	}
}

static WrinkleMapCoefficients *find_wrinkle_coefficients(const ListBase *wrinkle_coeff, const char *name)
{
	return BLI_findstring(wrinkle_coeff, name, offsetof(WrinkleMapCoefficients, name));
}

static void cache_wrinkle_map_influence(const WrinkleModifierData *wmd, const ListBase *map_cache,
                                        DerivedMesh *dm, const float (*orco)[3])
{
	int numtris = dm->getNumLoopTri(dm);
	int numverts = dm->getNumVerts(dm);
	
	MVertTri *tri_verts;
	int *vert_numtri;
	cache_triangles(&tri_verts, &vert_numtri, dm);
	
	for (WrinkleMapCache *map = map_cache->first; map; map = map->next) {
		BLI_assert(map->influence == NULL);
		map->influence = MEM_mallocN(sizeof(float) * numtris, "wrinkle map influence");
		
		const WrinkleMapCoefficients *coeff = find_wrinkle_coefficients(&wmd->wrinkle_coeff, map->keyblock->name);
		if (coeff)
			get_wrinkle_map_influence(dm, orco, tri_verts, coeff, map->influence);
		else
			memset(map->influence, 0, sizeof(float) * numtris);
	}
	
	blend_wrinkle_influence(map_cache, numtris, wmd->blend_variance, wmd->blend_smoothness);
	bake_vertex_influence(map_cache, numtris, numverts, tri_verts, vert_numtri);
	
	MEM_freeN(tri_verts);
	MEM_freeN(vert_numtri);
}

/* ========================================================================= */

#if 0
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
		
		if (w > 0.0f) {
			float nor[3];
			normal_short_to_float_v3(nor, mv->no);
			
			TexResult texres = {0};
			BKE_texture_get_value(scene, texture, (float *)texco[i], &texres, false);
			
			/* TODO use texres for displacement */
			
			madd_v3_v3v3fl(coords[i], mv->co, nor, w);
		}
	}
	
	CDDM_apply_vert_coords(dm, coords);
	MEM_freeN(coords);
}
#endif

static void wrinkle_set_vgroup_weights(const float *influence, int numverts, int defgrp_index, MDeformVert *dvert,
                                       bool use_clamp)
{
	BLI_assert(dvert != NULL);
	BLI_assert(defgrp_index >= 0);
	
	for (int i = 0; i < numverts; i++) {
		float w = influence[i];
		if (use_clamp) {
			CLAMP(w, 0.0f, 1.0f);
		}
		
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

static void copy_dm_coords(DerivedMesh *dm, float (*coords)[3])
{
	int numverts = dm->getNumVerts(dm);
	MVert *mverts = dm->getVertArray(dm);
	for (int i = 0; i < numverts; ++i) {
		copy_v3_v3(coords[i], mverts[i].co);
	}
}

static float* wrinkle_shapekey_eval(Object *ob, ModifierData *wrinkle_md, int numverts, ListBase *map_cache)
{
	Scene *scene = wrinkle_md->scene;
	float (*coords)[3] = NULL;
	
	for (WrinkleMapCache *map = map_cache->first; map; map = map->next) {
		/* Use clamping, so shape key influence does not exceed 1.
		 * Note that this can violate the area conservation feature!
		 */
		const bool use_clamp = true;
		
		wrinkle_set_vgroup_weights(map->vertex_weight, numverts, map->defgrp_index, map->dvert, use_clamp);
	}
	
	/* temporarily disable modifiers behind (and including) the wrinkle modifier */
	for (ModifierData *md = wrinkle_md; md; md = md->next)
		md->mode |= eModifierMode_DisableTemporary;
	
	uint64_t data_mask = CD_MASK_BAREMESH;
	DerivedMesh *dm = mesh_get_derived_final(scene, ob, data_mask);
	
	/* restore settings */
	for (ModifierData *md = wrinkle_md; md; md = md->next)
		md->mode &= ~eModifierMode_DisableTemporary;
	
	/* XXX this is necessary because some modifiers may change topology even if
	 * just the shapekey influence changes!
	 */
	if (dm->getNumVerts(dm) == numverts) {
		coords = MEM_mallocN(sizeof(float) * 3 * numverts, "wrinkle vertex coordinates");
		copy_dm_coords(dm, coords);
	}
	
	dm->needsFree = true;
	dm->release(dm);
	
	return (float *)coords;
}

void BKE_wrinkle_apply(Object *ob, WrinkleModifierData *wmd, DerivedMesh *dm, const float (*orco)[3])
{
	const bool apply_displace = wmd->flag & MOD_WRINKLE_APPLY_DISPLACEMENT;
	const bool apply_vgroups = wmd->flag & MOD_WRINKLE_APPLY_VERTEX_GROUPS;
	if (!(apply_displace || apply_vgroups))
		return;
	
	DM_ensure_looptri(dm);
	
	{
		WrinkleMapCoefficients *coeff = wmd->wrinkle_coeff.first;
		if (!coeff) {
			modifier_setError(&wmd->modifier, "Wrinkle coefficients missing");
			return;
		}
		
		int numtris = dm->getNumLoopTri(dm);
		if (coeff->numtris != numtris) {
			modifier_setError(&wmd->modifier, "Triangles changed from %d to %d", coeff->numtris, numtris);
			return;
		}
	}
	
	int numverts = dm->getNumVerts(dm);
	
	ListBase map_cache;
	build_wrinkle_map_cache(ob, &map_cache);
	
	cache_wrinkle_map_influence(wmd, &map_cache, dm, orco);
	
	if (apply_displace) {
		float (*coords)[3] = (float (*)[3])wrinkle_shapekey_eval(ob, &wmd->modifier, numverts, &map_cache);
		if (coords) {
			CDDM_apply_vert_coords(dm, coords);
		}
		MEM_freeN(coords);
	}
	
	if (apply_vgroups) {
		for (WrinkleMapCache *map = map_cache.first; map; map = map->next) {
			MDeformVert *dvert = CustomData_duplicate_referenced_layer(&dm->vertData, CD_MDEFORMVERT, numverts);
			/* If no vertices were ever added to an object's vgroup, dvert might be NULL. */
			if (!dvert) {
				/* add a valid data layer */
				dvert = CustomData_add_layer_named(&dm->vertData, CD_MDEFORMVERT, CD_CALLOC,
				                                   NULL, numverts, map->keyblock->vgroup);
			}
			
			if (dvert)
				wrinkle_set_vgroup_weights(map->vertex_weight, numverts, map->defgrp_index, dvert, false);
		}
	}
	
	free_wrinkle_map_cache(ob, &map_cache);
}

/* ========================================================================= */

static void calc_shapekey_triangle_coefficients(float (*C_data)[4], int numtris, MVertTri *tri_verts,
                                                const float (*data)[3], const float (*refdata)[3])
{
#ifdef WRINKLE_DEBUG
	int debug_nth = 1;
#endif
	
	for (int i = 0; i < numtris; ++i) {
		float *C = C_data[i];
		int k0 = tri_verts[i].tri[0];
		int k1 = tri_verts[i].tri[1];
		int k2 = tri_verts[i].tri[2];
		
		/* triangle space coordinates */
		float u[3], v[3];
		float trimat[3][3], itrimat[3][3];
		get_triangle_shape_ex(refdata[k0], refdata[k1], refdata[k2], &u[0], &v[1], &v[0], trimat);
		u[1] = 0.0f;
		u[2] = 0.0f;
		v[2] = 0.0f;
		transpose_m3_m3(itrimat, trimat); /* cheap invert for orthogonal matrix */
		
		/* shape coordinates in triangle space */
		float us[3], vs[3];
		sub_v3_v3v3(us, data[k1], data[k0]);
		sub_v3_v3v3(vs, data[k2], data[k0]);
		mul_m3_v3(itrimat, us);
		mul_m3_v3(itrimat, vs);
		
#if 0
		float u[2], v[2]; /* base triangle shape */
		float us[2], vs[2]; /* wrinkled triangle shape */
		get_triangle_shape(refdata[k0], refdata[k1], refdata[k2], &u[0], &v[1], &v[0]);
		u[1] = 0.0f; /* aligned with triangle x axis */
		get_triangle_shape(data[k0], data[k1], data[k2], &us[0], &vs[1], &vs[0]);
		us[1] = 0.0f; /* aligned with triangle x axis */
		
		float p[2], q[2];
		sub_v2_v2v2(p, us, u);
		sub_v2_v2v2(q, vs, v);
		
		float N = cross_v2v2(u, v);
		float Ns = cross_v2v2(us, vs);
#endif
		
#if 0
		sub_v3_v3v3(u, refdata[k1], refdata[k0]);
		sub_v3_v3v3(v, refdata[k2], refdata[k0]);
		sub_v3_v3v3(us, data[k1], data[k0]);
		sub_v3_v3v3(vs, data[k2], data[k0]);
#endif
		float p[3], q[3];
		sub_v3_v3v3(p, us, u);
		sub_v3_v3v3(q, vs, v);
		
		float N[3], Ns[3];
		cross_v3_v3v3(N, u, v);
		cross_v3_v3v3(Ns, us, vs);
		
		float lN = len_v3(N);
		float lNs = len_v3(Ns);
		if (lN == 0.0f || lNs == 0.0f) {
			/* uses modulation factor 1 */
			C[0] = 0.0f;
			C[1] = 0.0f;
			C[2] = 0.0f;
			C[3] = 0.0f;
		}
		
#if 0
		float duda[3] = { -u[0], 0.0f, 0.0f };
		float dudb[3] = { u[1], 0.0f, 0.0f };
		float dudd[3] = { 0.0f, -u[1], 0.0f };
		float dvda[3] = { -v[0], 0.0f, 0.0f };
		float dvdb[3] = { v[1], 0.0f, 0.0f };
		float dvdd[3] = { 0.0f, -v[1], 0.0f };
		
		float t[3];
		normalize_v3_v3(t, Ns);
		
		float du[3], dv[3];
		cross_v3_v3v3(du, duda, Ns);
		cross_v3_v3v3(dv, dvda, Ns);
		C[0] = dot_v3v3(t, du) + dot_v3v3(t, dv);
		cross_v3_v3v3(du, dudb, Ns);
		cross_v3_v3v3(dv, dvdb, Ns);
		C[1] = dot_v3v3(t, du) + dot_v3v3(t, dv);
		cross_v3_v3v3(du, dudd, Ns);
		cross_v3_v3v3(dv, dvdd, Ns);
		C[2] = dot_v3v3(t, du) + dot_v3v3(t, dv);
		cross_v3_v3v3(du, p, Ns);
		cross_v3_v3v3(dv, q, Ns);
		C[3] = dot_v3v3(t, du) + dot_v3v3(t, dv);
#endif
		
#if 1
		float order = signf(Ns[2]);
		C[0] = order * (v[0]*us[1] - u[0]*vs[1]);
		C[1] = order * (v[1]*p[1] - u[1]*q[1]);
		C[2] = order * (u[1]*vs[0] - v[1]*us[0]);
		C[3] = order * (Ns[2] - N[2]);
#endif
		
#if 0
		/* partial derivative of area difference wrt. (a-1), b, (d-1) */
		C[0] = 0.5f * (N[1]*N[1] + N[2]*N[2]) / lN;
		C[1] = -0.5f * N[0] * N[1] / lN;
		C[2] = 0.5f * (N[0]*N[0] + N[2]*N[2]) / lN;
		
		/* partial derivative of area difference wrt. (h-1) */
		float t[3], dNs[3];
		cross_v3_v3v3(dNs, p, vs);
		cross_v3_v3v3(t, us, q);
		add_v3_v3(dNs, t);
		C[3] = 0.5f * dot_v3v3(Ns, dNs) / lNs;
#endif
		
#ifdef WRINKLE_DEBUG
		float X0[3];
		X0[0] = 0.01; X0[1] = 0.01f; X0[2] = 0.0f;
		mul_m3_v3(trimat, X0);
		add_v3_v3(X0, refdata[k0]);
		
//		BKE_sim_debug_data_add_vector(X0, trimat[0], 1,0,0, "wrinkle", i, 991);
//		BKE_sim_debug_data_add_vector(X0, trimat[1], 0,1,0, "wrinkle", i, 992);
//		BKE_sim_debug_data_add_vector(X0, trimat[2], 0,0,1, "wrinkle", i, 993);
		
		if (BLI_ghashutil_inthash(i) % debug_nth == 0) {
			BKE_sim_debug_data_add_line(refdata[k0], refdata[k1], 0.8,0.8,0.8, "wrinkle", i, 111);
			BKE_sim_debug_data_add_line(refdata[k1], refdata[k2], 0.8,0.8,0.8, "wrinkle", i, 112);
			BKE_sim_debug_data_add_line(refdata[k2], refdata[k0], 0.8,0.8,0.8, "wrinkle", i, 113);
			
//			BKE_sim_debug_data_add_vector(X0, u, 1,0,0, "wrinkle", i, 8374);
//			BKE_sim_debug_data_add_vector(X0, v, 0,1,0, "wrinkle", i, 8375);
			
//			BKE_sim_debug_data_add_vector(X0, us, 1,0,0.7, "wrinkle", i, 8384);
//			BKE_sim_debug_data_add_vector(X0, vs, 0,1,0.7, "wrinkle", i, 8385);
			
//			BKE_sim_debug_data_add_vector(X0, N, 1,0,0, "wrinkle", i, 8374);
//			BKE_sim_debug_data_add_vector(X0, Ns, 1,0,1, "wrinkle", i, 8375);
//			BKE_sim_debug_data_add_vector(X0, dNs, 1,1,0, "wrinkle", i, 8376);
			
//			float tmp[3];
//			mul_v3_m3v3(tmp, trimat, us);
//			mul_v3_fl(tmp, C[0]);
//			BKE_sim_debug_data_add_vector(X0, tmp, 1,0,0, "wrinkle", i, 823);
//			mul_v3_m3v3(tmp, trimat, vs);
//			mul_v3_fl(tmp, C[2]);
//			BKE_sim_debug_data_add_vector(X0, tmp, 0,1,0, "wrinkle", i, 824);
		}
#endif
	}
}

void BKE_wrinkle_coeff_calc(Object *ob, WrinkleModifierData *wmd)
{
	Mesh *mesh = ob->data;
	int numverts = mesh->totvert;
	
	BKE_wrinkle_coeff_free(wmd);
	
	DerivedMesh *dm = CDDM_from_mesh(mesh);
	DM_ensure_looptri(dm);
	
	MVertTri *tri_verts;
	int *vert_numtri;
	cache_triangles(&tri_verts, &vert_numtri, dm);
	
	float (*shape)[3] = MEM_mallocN(sizeof(float) * 3 * numverts, "relative shapekey");
	int numtris = dm->getNumLoopTri(dm);
	
	ListBase map_cache;
	build_wrinkle_map_cache(ob, &map_cache);
	for (WrinkleMapCache *map = map_cache.first; map; map = map->next) {
		WrinkleMapCoefficients *coeff = MEM_callocN(sizeof(WrinkleMapCoefficients), "WrinkleMapCoefficients");
		BLI_strncpy(coeff->name, map->keyblock->name, sizeof(coeff->name));
		
		coeff->numtris = numtris;
		coeff->C = MEM_mallocN(sizeof(float) * 4 * numtris, "wrinkle map triangle coefficients");
		
		calc_shapekey_triangle_coefficients(coeff->C, numtris, tri_verts, map->keyblock->data, map->refblock->data);
		
		BLI_addtail(&wmd->wrinkle_coeff, coeff);
	}
	free_wrinkle_map_cache(ob, &map_cache);
	
	MEM_freeN(shape);
	MEM_freeN(tri_verts);
	MEM_freeN(vert_numtri);
	
	dm->release(dm);
}

void BKE_wrinkle_coeff_free(WrinkleModifierData *wmd)
{
	WrinkleMapCoefficients *coeff, *coeff_next;
	for (coeff = wmd->wrinkle_coeff.first; coeff; coeff = coeff_next) {
		coeff_next = coeff->next;
		
		if (coeff->C)
			MEM_freeN(coeff->C);
		MEM_freeN(coeff);
	}
	BLI_listbase_clear(&wmd->wrinkle_coeff);
}

bool BKE_wrinkle_has_coeff(WrinkleModifierData *wmd)
{
	return !BLI_listbase_is_empty(&wmd->wrinkle_coeff);
}

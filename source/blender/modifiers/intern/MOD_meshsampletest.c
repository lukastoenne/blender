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

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_mesh_sample.h"

#include "MOD_modifiertypes.h"
#include "MEM_guardedalloc.h"

#include "BLI_rand.h"
#include "BLI_strict_flags.h"

static void initData(ModifierData *md)
{
#if 0
	MeshSampleTestModifierData *smd = (MeshSampleTestModifierData *) md;
#endif
}

static void copyData(ModifierData *md, ModifierData *target)
{
#if 0
	MeshSampleTestModifierData *smd = (MeshSampleTestModifierData *) md;
#endif
	MeshSampleTestModifierData *tsmd = (MeshSampleTestModifierData *) target;
	
	modifier_copyData_generic(md, target);
	
	if (tsmd->samples) {
		tsmd->samples = MEM_dupallocN(tsmd->samples);
	}
}

static void freeData(ModifierData *md)
{
	MeshSampleTestModifierData *smd = (MeshSampleTestModifierData *) md;
	if (smd->samples) {
		MEM_freeN(smd->samples);
	}
}

static void notch_size(int *num_verts, int *num_edges, int *num_loops, int *num_polys)
{
#if 0
	*num_verts = 4;
	*num_edges = 4;
	*num_loops = 4;
	*num_polys = 1;
#else
	*num_verts = 4;
	*num_edges = 6;
	*num_loops = 9;
	*num_polys = 3;
#endif
}

static void add_notch(const float loc[3], const float nor[3],
                      MVert *mverts, MEdge *medges, MLoop *mloops, MPoly *mpolys,
                      unsigned int mvert_offset, unsigned int medge_offset, unsigned int mloop_offset)
{
	static const float size = 0.05f;
	static const float basescale = 0.3f;
	const float up[3] = {0,0,1};
	float quat[4];
	
	rotation_between_vecs_to_quat(quat, up, nor);
	
#if 0
	zero_v3(mverts[0].co);
	mverts[0].co[0] += 0.5f*size;
	mverts[0].co[1] += 0.5f*size;
	
	zero_v3(mverts[1].co);
	mverts[1].co[0] -= 0.5f*size;
	mverts[1].co[1] += 0.5f*size;
	
	zero_v3(mverts[2].co);
	mverts[2].co[0] -= 0.5f*size;
	mverts[2].co[1] -= 0.5f*size;
	
	zero_v3(mverts[3].co);
	mverts[3].co[0] += 0.5f*size;
	mverts[3].co[1] -= 0.5f*size;
	
	mul_qt_v3(quat, mverts[0].co);
	mul_qt_v3(quat, mverts[1].co);
	mul_qt_v3(quat, mverts[2].co);
	mul_qt_v3(quat, mverts[3].co);
	add_v3_v3(mverts[0].co, loc);
	add_v3_v3(mverts[1].co, loc);
	add_v3_v3(mverts[2].co, loc);
	add_v3_v3(mverts[3].co, loc);
	
	medges[0].v1 = mvert_offset + 0;
	medges[0].v2 = mvert_offset + 1;
	
	medges[1].v1 = mvert_offset + 1;
	medges[1].v2 = mvert_offset + 2;
	
	medges[2].v1 = mvert_offset + 2;
	medges[2].v2 = mvert_offset + 3;
	
	medges[3].v1 = mvert_offset + 3;
	medges[3].v2 = mvert_offset + 0;
	
	mloops[0].v = mvert_offset + 0;
	mloops[1].v = mvert_offset + 1;
	mloops[2].v = mvert_offset + 2;
	mloops[3].v = mvert_offset + 3;
	mloops[0].e = medge_offset + 0;
	mloops[1].e = medge_offset + 1;
	mloops[2].e = medge_offset + 2;
	mloops[3].e = medge_offset + 3;
	
	mpolys[0].loopstart = (int)(mloop_offset + 0);
	mpolys[0].totloop = 4;
#else
	zero_v3(mverts[0].co);
	mverts[0].co[1] += size * basescale;
	
	zero_v3(mverts[1].co);
	mverts[1].co[0] -= 0.866f*size * basescale;
	mverts[1].co[1] -= 0.5f*size * basescale;
	
	zero_v3(mverts[2].co);
	mverts[2].co[0] += 0.866f*size * basescale;
	mverts[2].co[1] -= 0.5f*size * basescale;
	
	zero_v3(mverts[3].co);
	mverts[3].co[2] += size;
	
	mul_qt_v3(quat, mverts[0].co);
	mul_qt_v3(quat, mverts[1].co);
	mul_qt_v3(quat, mverts[2].co);
	mul_qt_v3(quat, mverts[3].co);
	add_v3_v3(mverts[0].co, loc);
	add_v3_v3(mverts[1].co, loc);
	add_v3_v3(mverts[2].co, loc);
	add_v3_v3(mverts[3].co, loc);
	
	medges[0].v1 = mvert_offset + 0;
	medges[0].v2 = mvert_offset + 1;
	
	medges[1].v1 = mvert_offset + 1;
	medges[1].v2 = mvert_offset + 2;
	
	medges[2].v1 = mvert_offset + 2;
	medges[2].v2 = mvert_offset + 0;
	
	medges[3].v1 = mvert_offset + 0;
	medges[3].v2 = mvert_offset + 3;
	
	medges[4].v1 = mvert_offset + 1;
	medges[4].v2 = mvert_offset + 3;
	
	medges[5].v1 = mvert_offset + 2;
	medges[5].v2 = mvert_offset + 3;
	
	mloops[0].v = mvert_offset + 0;
	mloops[1].v = mvert_offset + 1;
	mloops[2].v = mvert_offset + 3;
	mloops[0].e = medge_offset + 0;
	mloops[1].e = medge_offset + 4;
	mloops[2].e = medge_offset + 3;
	
	mloops[3].v = mvert_offset + 1;
	mloops[4].v = mvert_offset + 2;
	mloops[5].v = mvert_offset + 3;
	mloops[3].e = medge_offset + 1;
	mloops[4].e = medge_offset + 5;
	mloops[5].e = medge_offset + 4;
	
	mloops[6].v = mvert_offset + 2;
	mloops[7].v = mvert_offset + 0;
	mloops[8].v = mvert_offset + 3;
	mloops[6].e = medge_offset + 2;
	mloops[7].e = medge_offset + 3;
	mloops[8].e = medge_offset + 5;

	mpolys[0].loopstart = (int)(mloop_offset + 0);
	mpolys[0].totloop = 3;
	
	mpolys[1].loopstart = (int)(mloop_offset + 3);
	mpolys[1].totloop = 3;
	
	mpolys[2].loopstart = (int)(mloop_offset + 6);
	mpolys[2].totloop = 3;
#endif
}

typedef struct IsectTest {
	IsectTriAABBData isect[6];
	int num_isect;
	float tri[3][3];
} IsectTest;

static void make_isect_test(IsectTest *test, RNG *rng, const float bbmin[3], const float bbmax[3])
{
#if 1
	BLI_rng_get_float_unit_v3(rng, test->tri[0]);
	mul_v3_fl(test->tri[0], BLI_rng_get_float(rng) * 5.0f);
	BLI_rng_get_float_unit_v3(rng, test->tri[1]);
	mul_v3_fl(test->tri[1], BLI_rng_get_float(rng) * 5.0f);
	BLI_rng_get_float_unit_v3(rng, test->tri[2]);
	mul_v3_fl(test->tri[2], BLI_rng_get_float(rng) * 5.0f);
#else
	float u[2];
	BLI_rng_get_float_unit_v2(rng, u);
	mul_v2_fl(u, 3.0f);
	test->tri[0][0] = u[0]; test->tri[0][1] = u[1]; test->tri[0][2] = 0.5f;
	BLI_rng_get_float_unit_v2(rng, u);
	mul_v2_fl(u, 3.0f);
	test->tri[1][0] = u[0]; test->tri[1][1] = u[1]; test->tri[1][2] = 0.5f;
	BLI_rng_get_float_unit_v2(rng, u);
	mul_v2_fl(u, 3.0f);
	test->tri[2][0] = u[0]; test->tri[2][1] = u[1]; test->tri[2][2] = 0.5f;
#endif
	
	isect_tri_aabb(test->tri, bbmin, bbmax, test->isect, &test->num_isect);
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
                                  DerivedMesh *derivedData,
                                  ModifierApplyFlag flag)
{
	DerivedMesh *dm = derivedData;
	DerivedMesh *result;
	MeshSampleTestModifierData *smd = (MeshSampleTestModifierData *) md;
	
	result = dm;
	
#if 0
	if (smd->totsamples > 0) {
		int num_verts = dm->getNumVerts(dm);
		int num_edges = dm->getNumEdges(dm);
		int num_loops = dm->getNumLoops(dm);
		int num_polys = dm->getNumPolys(dm);
		int notch_verts, notch_edges, notch_loops, notch_polys;
		MVert *mv;
		MEdge *me;
		MLoop *ml;
		MPoly *mp;
		MSurfaceSample *sample;
		int i;
		
		if (!smd->samples) {
			MSurfaceSampleStorage storage;
			
			smd->samples = MEM_callocN(sizeof(MSurfaceSample) * (unsigned int)smd->totsamples, "test samples");
			
			BKE_mesh_sample_storage_array(&storage, smd->samples, smd->totsamples);
			BKE_mesh_sample_generate_poisson_disk_by_amount(&storage, dm, smd->seed, smd->totsamples);
			BKE_mesh_sample_storage_release(&storage);
		}
		
		notch_size(&notch_verts, &notch_edges, &notch_loops, &notch_polys);
		result = CDDM_from_template(dm,
		                            num_verts + notch_verts * smd->totsamples,
		                            num_edges + notch_edges * smd->totsamples,
		                            0,
		                            num_loops + notch_loops * smd->totsamples,
		                            num_polys + notch_polys * smd->totsamples);
		
		/*copy customdata to original geometry*/
		DM_copy_vert_data(dm, result, 0, 0, num_verts);
		DM_copy_edge_data(dm, result, 0, 0, num_edges);
		DM_copy_loop_data(dm, result, 0, 0, num_loops);
		DM_copy_poly_data(dm, result, 0, 0, num_polys);
		
		if (!CustomData_has_layer(&dm->vertData, CD_MVERT)) {
			dm->copyVertArray(dm, CDDM_get_verts(result));
		}
		if (!CustomData_has_layer(&dm->edgeData, CD_MEDGE)) {
			dm->copyEdgeArray(dm, CDDM_get_edges(result));
		}
		if (!CustomData_has_layer(&dm->polyData, CD_MPOLY)) {
			dm->copyLoopArray(dm, CDDM_get_loops(result));
			dm->copyPolyArray(dm, CDDM_get_polys(result));
		}
		
		mv = result->getVertArray(result);
		me = result->getEdgeArray(result);
		ml = result->getLoopArray(result);
		mp = result->getPolyArray(result);
		for (i = 0, sample = smd->samples; i < smd->totsamples; ++i, ++sample) {
			float loc[3], nor[3];
			
			BKE_mesh_sample_eval(dm, sample, loc, nor);
			
			add_notch(loc, nor,
			          mv + num_verts + i * notch_verts, me + num_edges + i * notch_edges,
			          ml + num_loops + i * notch_loops, mp + num_polys + i * notch_polys,
			          (unsigned int)(num_verts + i * notch_verts),
			          (unsigned int)(num_edges + i * notch_edges),
			          (unsigned int)(num_loops + i * notch_loops));
		}
		
		result->dirty |= DM_DIRTY_NORMALS;
	}
#else
	{
#if 0
		IsectTest test;
		RNG *rng = BLI_rng_new(smd->seed);
		float bbmin[3], bbmax[3];
		
		bbmin[0] = -1.3f;
		bbmin[1] = 0.16f;
		bbmin[2] = -0.5f;
		bbmax[0] = 3.0f;
		bbmax[1] = 1.0f;
		bbmax[2] = 1.1f;
		make_isect_test(&test, rng, bbmin, bbmax);
		
		BLI_rng_free(rng);
#else
		const int numtest = smd->totsamples;
		IsectTest *test = MEM_callocN(sizeof(IsectTest) * (size_t)numtest, "test");
		RNG *rng = BLI_rng_new(smd->seed);
		float bbmin[3], bbmax[3];
		int i, totvert, totpoly;
		
		bbmin[0] = -1.3f;
		bbmin[1] = 0.16f;
		bbmin[2] = -0.5f;
		bbmax[0] = 3.0f;
		bbmax[1] = 1.0f;
		bbmax[2] = 1.1f;
		
		totvert = 0;
		totpoly = 0;
		for (i = 0; i < numtest; ++i) {
			make_isect_test(&test[i], rng, bbmin, bbmax);
			totvert += test[i].num_isect;
			totpoly += test[i].num_isect > 0 ? 1 : 0;
		}
		
		{
#if 0
			MVert *mv;
			MLoop *ml;
			MPoly *mp;
			int k, kstart;
			
			result = CDDM_new(totvert, 0, 0, totvert, totpoly);
			mv = result->getVertArray(result);
			ml = result->getLoopArray(result);
			mp = result->getPolyArray(result);
			
			kstart = 0;
			for (i = 0; i < numtest; ++i) {
				if (test[i].num_isect > 0) {
					for (k = 0; k < test[i].num_isect; ++k, ++mv, ++ml) {
						copy_v3_v3(mv->co, test[i].isect[k].co);
						ml->v = (unsigned int)(kstart + k);
					}
					mp->loopstart = kstart;
					mp->totloop = test[i].num_isect;
					++mp;
					
					kstart += test[i].num_isect;
				}
			}
#elif 1
			MVert *mv;
			MEdge *me;
			int k, kstart;
			
			result = CDDM_new(totvert, totvert, 0, 0, 0);
			mv = result->getVertArray(result);
			me = result->getEdgeArray(result);
			
			kstart = 0;
			for (i = 0; i < numtest; ++i) {
				if (test[i].num_isect > 0) {
					for (k = 0; k < test[i].num_isect; ++k, ++mv, ++me) {
						copy_v3_v3(mv->co, test[i].isect[k].co);
						me->v1 = (unsigned int)(kstart + k);
						me->v2 = (unsigned int)(kstart + (k + 1) % test[i].num_isect);
						if (me->v2 < me->v1)
							SWAP(unsigned int, me->v1, me->v2);
					}
					
					kstart += test[i].num_isect;
				}
			}
#endif
		}
		
		result->dirty |= DM_DIRTY_NORMALS;
		CDDM_calc_normals(result);
		
		MEM_freeN(test);
		BLI_rng_free(rng);
#endif
	}
#endif
	
//	if (result != dm)
//		dm->release(dm);
	
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
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};

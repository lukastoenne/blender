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

/** \file blender/blenkernel/intern/strands.c
 *  \ingroup bke
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_DerivedMesh.h"
#include "BKE_mesh_sample.h"
#include "BKE_strands.h"

#include "GPU_buffers.h"
#include "GPU_strands.h"

Strands *BKE_strands_new(void)
{
	Strands *strands = MEM_callocN(sizeof(Strands), "strands");
	return strands;
}

Strands *BKE_strands_copy(Strands *strands)
{
	Strands *nstrands = MEM_dupallocN(strands);
	
	if (strands->curves) {
		nstrands->curves = MEM_dupallocN(strands->curves);
	}
	if (strands->verts) {
		nstrands->verts = MEM_dupallocN(strands->verts);
	}
	
	/* lazy initialized */
	nstrands->gpu_shader = NULL;
	nstrands->data_final = NULL;
	
	return nstrands;
}

void BKE_strands_free(Strands *strands)
{
	if (strands->gpu_shader)
		GPU_strand_shader_free(strands->gpu_shader);
	GPU_strands_buffer_free(strands->data_final);
	
	if (strands->data_final)
		BKE_strand_data_free(strands->data_final);
	
	if (strands->curves)
		MEM_freeN(strands->curves);
	if (strands->verts)
		MEM_freeN(strands->verts);
	MEM_freeN(strands);
}

/* ------------------------------------------------------------------------- */

StrandData *BKE_strand_data_calc(Strands *strands, DerivedMesh *scalp,
                                 StrandRoot *roots, int num_roots)
{
	StrandData *data = MEM_callocN(sizeof(StrandData), "StrandData");
	
	data->totverts = strands->totverts;
	data->totcurves = strands->totcurves;
	data->totroots = num_roots;
	data->verts = MEM_mallocN(sizeof(StrandVertexData) * data->totverts, "StrandVertexData");
	data->curves = MEM_mallocN(sizeof(StrandCurveData) * data->totcurves, "StrandCurveData");
	data->roots = MEM_mallocN(sizeof(StrandRootData) * data->totroots, "StrandRootData");
	
	int c;
	StrandCurve *scurve = strands->curves;
	StrandCurveData *curve = data->curves;
	for (c = 0; c < data->totcurves; ++c, ++scurve, ++curve) {
		curve->verts_begin = scurve->verts_begin;
		curve->num_verts = scurve->num_verts;
		
		BKE_mesh_sample_eval(scalp, &scurve->root, curve->mat[3], curve->mat[2], curve->mat[0]);
		cross_v3_v3v3(curve->mat[1], curve->mat[2], curve->mat[0]);
		
		int v;
		StrandVertex *svert = strands->verts + scurve->verts_begin;
		StrandVertexData *vert = data->verts + curve->verts_begin;
		for (v = 0; v < curve->num_verts; ++v, ++svert, ++vert) {
			mul_v3_m4v3(vert->co, curve->mat, svert->co);
		}
	}
	
	int i;
	StrandRoot *sroot = roots;
	StrandRootData *root = data->roots;
	for (i = 0; i < data->totroots; ++i, ++sroot, ++root) {
		float nor[3], tang[3];
		BKE_mesh_sample_eval(scalp, &sroot->root, root->co, nor, tang);
		
		int k;
		for (k = 0; k < 4; ++k) {
			root->control_index[k] = sroot->control_index[k];
			root->control_weight[k] = sroot->control_weights[k];
		}
	}
	
	return data;
}

void BKE_strand_data_free(StrandData *data)
{
	if (data) {
		GPU_strands_buffer_free(data);
		
		if (data->verts)
			MEM_freeN(data->verts);
		if (data->curves)
			MEM_freeN(data->curves);
		if (data->roots)
			MEM_freeN(data->roots);
		MEM_freeN(data);
	}
}

/* ------------------------------------------------------------------------- */

void BKE_strands_test_init(struct Strands *strands, struct DerivedMesh *scalp,
                           int totcurves, int maxverts,
                           unsigned int seed)
{
	const unsigned int totverts = totcurves * maxverts;
	
	MeshSampleGenerator *gen = BKE_mesh_sample_gen_surface_random(scalp, seed);
	unsigned int i, k;
	
	StrandCurve *curves = MEM_mallocN(sizeof(StrandCurve) * totcurves, "StrandCurve buffer");
	StrandVertex *verts = MEM_mallocN(sizeof(StrandVertex) * totverts, "StrandVertex buffer");
	
	StrandCurve *c = curves;
	StrandVertex *v = verts;
	unsigned int verts_begin = 0;
	for (i = 0; i < totcurves; ++i, ++c) {
		if (BKE_mesh_sample_generate(gen, &c->root)) {
			c->verts_begin = verts_begin;
			c->num_verts = maxverts;
			
			for (k = 0; k < c->num_verts; ++k, ++v) {
				v->co[0] = 0.0f;
				v->co[1] = 0.0f;
				v->co[2] = (c->num_verts > 1) ? (float)k / (c->num_verts - 1) : 0.0f;
			}
			
			verts_begin += c->num_verts;
		}
		else {
			/* clear remaining samples */
			memset(c, 0, sizeof(StrandCurve) * totcurves - i);
			break;
		}
	}
	
	BKE_mesh_sample_free_generator(gen);
	
	if (strands->curves)
		MEM_freeN(strands->curves);
	if (strands->verts)
		MEM_freeN(strands->verts);
	strands->curves = curves;
	strands->verts = verts;
	strands->totcurves = totcurves;
	strands->totverts = totverts;
}

StrandRoot *BKE_strands_scatter(Strands *strands,
                                struct DerivedMesh *scalp, unsigned int amount,
                                unsigned int seed)
{
	MeshSampleGenerator *gen = BKE_mesh_sample_gen_surface_random(scalp, seed);
	unsigned int i;
	
	StrandRoot *roots = MEM_mallocN(sizeof(StrandRoot) * amount, "StrandRoot");
	StrandRoot *root;
	
	UNUSED_VARS(strands);
	for (i = 0, root = roots; i < amount; ++i, ++root) {
		if (BKE_mesh_sample_generate(gen, &root->root)) {
			int k;
			/* TODO find weights to "nearest" control strands */
			for (k = 0; k < 4; ++k) {
				root->control_index[k] = STRAND_INDEX_NONE;
				root->control_weights[k] = 0.0f;
			}
		}
		else {
			/* clear remaining samples */
			memset(root, 0, sizeof(StrandRoot) * amount - i);
			break;
		}
	}
	
	BKE_mesh_sample_free_generator(gen);
	
	return roots;
}

#if 0
StrandData *BKE_strand_data_interpolate(StrandInfo *strands, unsigned int num_strands,
                                        const StrandCurve *controls, struct StrandCurveParams *params)
{
	StrandData *data = MEM_callocN(sizeof(StrandData), "strand interpolation data");
	StrandInfo *s;
	StrandCurve *c;
	StrandVertex *v;
	unsigned int verts_begin;
	unsigned int i;
	
	data->totcurves = num_strands;
	data->totverts = num_strands * params->max_verts;
	data->curves = MEM_mallocN(sizeof(StrandCurve) * data->totcurves, "strand curves");
	data->verts = MEM_mallocN(sizeof(StrandVertex) * data->totverts, "strand vertices");
	
	UNUSED_VARS(controls);
	verts_begin = 0;
	v = data->verts;
	for (i = 0, s = strands, c = data->curves; i < num_strands; ++i) {
		unsigned int k;
		
		c->num_verts = params->max_verts;
		c->verts_begin = verts_begin;
		
		if (params->scalp) {
			BKE_mesh_sample_eval(params->scalp, &s->root, c->rootmat[3], c->rootmat[2], c->rootmat[0]);
			cross_v3_v3v3(c->rootmat[1], c->rootmat[2], c->rootmat[0]);
		}
		else {
			unit_m4(c->rootmat);
		}
		
		for (k = 0; k < c->num_verts; ++k, ++v) {
			v->co[0] = 0.0f;
			v->co[1] = 0.0f;
			if (c->num_verts > 1)
				v->co[2] = k / (c->num_verts - 1);
		}
		
		verts_begin += c->num_verts;
	}
	
	return data;
}

void BKE_strand_data_free(StrandData *data)
{
	if (data) {
		if (data->curves)
			MEM_freeN(data->curves);
		if (data->verts)
			MEM_freeN(data->verts);
		MEM_freeN(data);
	}
}
#endif

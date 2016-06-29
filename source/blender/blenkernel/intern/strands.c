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

#include "BKE_mesh_sample.h"
#include "BKE_strands.h"

#include "GPU_strands.h"

Strands *BKE_strands_new(void)
{
	Strands *strands = MEM_callocN(sizeof(Strands), "strands");
	return strands;
}

Strands *BKE_strands_copy(Strands *strands)
{
	Strands *nstrands = MEM_dupallocN(strands);
	
	if (strands->controls) {
		nstrands->controls = MEM_dupallocN(strands->controls);
	}
	
	/* lazy initialized */
	nstrands->gpu_strands = NULL;
	
	return nstrands;
}

void BKE_strands_free(Strands *strands)
{
	if (strands->gpu_strands)
		GPU_strands_free(strands->gpu_strands);
	
	MEM_freeN(strands);
}

StrandInfo *BKE_strands_scatter(struct DerivedMesh *scalp, unsigned int amount,
                                const ControlStrand *controls, unsigned int num_controls,
                                unsigned int seed)
{
	MeshSampleGenerator *gen = BKE_mesh_sample_gen_surface_random(scalp, seed);
	unsigned int i;
	
	StrandInfo *strands = MEM_mallocN(sizeof(StrandInfo) * amount, "strands");
	StrandInfo *s;
	
	UNUSED_VARS(controls, num_controls);
	for (i = 0, s = strands; i < amount; ++i, ++s) {
		if (BKE_mesh_sample_generate(gen, &s->root)) {
			int k;
			/* TODO find weights to "nearest" control strands */
			for (k = 0; k < 4; ++k) {
				s->control_index[k] = STRAND_INDEX_NONE;
				s->control_weights[k] = 0.0f;
			}
		}
		else {
			/* clear remaining samples */
			memset(s, 0, sizeof(StrandInfo) * amount - i);
			break;
		}
	}
	
	BKE_mesh_sample_free_generator(gen);
	
	return strands;
}

StrandData *BKE_strand_data_interpolate(StrandInfo *strands, unsigned int num_strands,
                                        const ControlStrand *controls, struct StrandCurveParams *params)
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

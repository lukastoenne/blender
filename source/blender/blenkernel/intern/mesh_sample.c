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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/mesh_sample.c
 *  \ingroup bke
 *
 * Sample a mesh surface or volume and evaluate samples on deformed meshes.
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_rand.h"

#include "BKE_mesh_sample.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"

#include "BLI_strict_flags.h"

/* Evaluate */

void BKE_mesh_sample_eval(DerivedMesh *dm, const MSurfaceSample *sample, float loc[3], float nor[3])
{
	MVert *mverts = dm->getVertArray(dm);
	MVert *v1, *v2, *v3, *v4;
	MFace *mfaces = dm->getTessFaceArray(dm);
	int totfaces = dm->getNumTessFaces(dm);
	MFace *mface = &mfaces[sample->orig_face];
	float vnor[3];
	
	zero_v3(loc);
	zero_v3(nor);
	
	if (sample->orig_face >= totfaces)
		return;
	
	v1 = &mverts[mface->v1];
	v2 = &mverts[mface->v2];
	v3 = &mverts[mface->v3];
	
	madd_v3_v3fl(loc, v1->co, sample->orig_weights[0]);
	madd_v3_v3fl(loc, v2->co, sample->orig_weights[1]);
	madd_v3_v3fl(loc, v3->co, sample->orig_weights[2]);
	
	normal_short_to_float_v3(vnor, v1->no);
	madd_v3_v3fl(nor, vnor, sample->orig_weights[0]);
	normal_short_to_float_v3(vnor, v2->no);
	madd_v3_v3fl(nor, vnor, sample->orig_weights[1]);
	normal_short_to_float_v3(vnor, v3->no);
	madd_v3_v3fl(nor, vnor, sample->orig_weights[2]);
	
	if (mface->v4) {
		v4 = &mverts[mface->v4];
		
		madd_v3_v3fl(loc, v4->co, sample->orig_weights[3]);
		
		normal_short_to_float_v3(vnor, v4->no);
		madd_v3_v3fl(nor, vnor, sample->orig_weights[3]);
	}
}


/* Iterators */

#if 0
static void mesh_sample_surface_array_iterator_next(MSurfaceSampleArrayIterator *iter)
{
	++iter->cur;
	--iter->remaining;
}

static bool mesh_sample_surface_array_iterator_valid(MSurfaceSampleArrayIterator *iter)
{
	return (iter->remaining > 0);
}

static void mesh_sample_surface_array_iterator_free(MSurfaceSampleArrayIterator *iter)
{
}

void BKE_mesh_sample_surface_array_begin(MSurfaceSampleArrayIterator *iter, MSurfaceSample *array, int totarray)
{
	iter->cur = array;
	iter->remaining = totarray;
	
	iter->base.next = 
}
#endif


/* Sampling */

void BKE_mesh_sample_info_random(MSurfaceSampleInfo *info, DerivedMesh *dm, unsigned int seed)
{
	info->algorithm = MSS_RANDOM;
	info->dm = dm;
	
	info->rng = BLI_rng_new(seed);
}

void BKE_mesh_sample_info_release(MSurfaceSampleInfo *info)
{
	if (info->rng) {
		BLI_rng_free(info->rng);
		info->rng = NULL;
	}
}


static void mesh_sample_surface_random(const MSurfaceSampleInfo *info, MSurfaceSample *sample)
{
	MFace *mfaces = info->dm->getTessFaceArray(info->dm);
	int totfaces = info->dm->getNumTessFaces(info->dm);
	MFace *mface;
	float sum, inv_sum;
	
	sample->orig_face = BLI_rng_get_int(info->rng) % totfaces;
	
	mface = &mfaces[sample->orig_face];
	sample->orig_weights[0] = BLI_rng_get_float(info->rng);
	sample->orig_weights[1] = BLI_rng_get_float(info->rng);
	sample->orig_weights[2] = BLI_rng_get_float(info->rng);
	sample->orig_weights[3] = mface->v4 ? BLI_rng_get_float(info->rng) : 0.0f;
	
	sum = sample->orig_weights[0] +
	      sample->orig_weights[1] +
	      sample->orig_weights[2] +
	      sample->orig_weights[3];
	inv_sum = sum > 0.0f ? 1.0f/sum : 0.0f;
	sample->orig_weights[0] *= inv_sum;
	sample->orig_weights[1] *= inv_sum;
	sample->orig_weights[2] *= inv_sum;
	sample->orig_weights[3] *= inv_sum;
}

void BKE_mesh_sample_surface_array(const MSurfaceSampleInfo *info, MSurfaceSample *samples, int totsample)
{
	MSurfaceSample *sample;
	int i;
	
	switch (info->algorithm) {
		case MSS_RANDOM: {
			DM_ensure_tessface(info->dm);
			for (sample = samples, i = 0; i < totsample; ++sample, ++i)
				mesh_sample_surface_random(info, sample);
			break;
		}
	}
}

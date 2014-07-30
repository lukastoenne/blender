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

bool BKE_mesh_sample_eval(DerivedMesh *dm, const MSurfaceSample *sample, float loc[3], float nor[3])
{
	MVert *mverts = dm->getVertArray(dm);
	unsigned int totverts = (unsigned int)dm->getNumVerts(dm);
	MVert *v1, *v2, *v3;
	float vnor[3];
	
	zero_v3(loc);
	zero_v3(nor);
	
	if (sample->orig_verts[0] >= totverts ||
	    sample->orig_verts[1] >= totverts ||
	    sample->orig_verts[2] >= totverts)
		return false;
	
	v1 = &mverts[sample->orig_verts[0]];
	v2 = &mverts[sample->orig_verts[1]];
	v3 = &mverts[sample->orig_verts[2]];
	
	madd_v3_v3fl(loc, v1->co, sample->orig_weights[0]);
	madd_v3_v3fl(loc, v2->co, sample->orig_weights[1]);
	madd_v3_v3fl(loc, v3->co, sample->orig_weights[2]);
	
	normal_short_to_float_v3(vnor, v1->no);
	madd_v3_v3fl(nor, vnor, sample->orig_weights[0]);
	normal_short_to_float_v3(vnor, v2->no);
	madd_v3_v3fl(nor, vnor, sample->orig_weights[1]);
	normal_short_to_float_v3(vnor, v3->no);
	madd_v3_v3fl(nor, vnor, sample->orig_weights[2]);
	
	normalize_v3(nor);
	
	return true;
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
	float a, b;
	
	mface = &mfaces[BLI_rng_get_int(info->rng) % totfaces];
	
	if (mface->v4 && BLI_rng_get_int(info->rng) % 2 == 0) {
		sample->orig_verts[0] = mface->v3;
		sample->orig_verts[1] = mface->v4;
		sample->orig_verts[2] = mface->v1;
	}
	else {
		sample->orig_verts[0] = mface->v1;
		sample->orig_verts[1] = mface->v2;
		sample->orig_verts[2] = mface->v3;
	}
	
	a = BLI_rng_get_float(info->rng);
	b = BLI_rng_get_float(info->rng);
	if (a + b > 1.0f) {
		a = 1.0f - a;
		b = 1.0f - b;
	}
	sample->orig_weights[0] = 1.0f - (a + b);
	sample->orig_weights[1] = a;
	sample->orig_weights[2] = b;
}

void BKE_mesh_sample_surface_array(const MSurfaceSampleInfo *info, MSurfaceSample *samples, int totsample)
{
	BKE_mesh_sample_surface_array_stride(info, samples, (int)sizeof(MSurfaceSample), totsample);
}

void BKE_mesh_sample_surface_array_stride(const struct MSurfaceSampleInfo *info, struct MSurfaceSample *first, int stride, int totsample)
{
	MSurfaceSample *sample;
	int i;
	
	switch (info->algorithm) {
		case MSS_RANDOM: {
			DM_ensure_tessface(info->dm);
			for (sample = first, i = 0; i < totsample; sample = (MSurfaceSample *)((char *)sample + stride), ++i)
				mesh_sample_surface_random(info, sample);
			break;
		}
	}
}

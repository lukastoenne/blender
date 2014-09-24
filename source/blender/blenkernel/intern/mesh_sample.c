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

/* ==== Evaluate ==== */

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


/* ==== Sampling ==== */

static bool mesh_sample_store_array_sample(void *vdata, int capacity, int index, const MSurfaceSample *sample)
{
	MSurfaceSample *data = vdata;
	if (index >= capacity)
		return false;
	
	data[index] = *sample;
	return true;
}

void BKE_mesh_sample_storage_array(MSurfaceSampleStorage *storage, MSurfaceSample *samples, int capacity)
{
	storage->store_sample = mesh_sample_store_array_sample;
	storage->capacity = capacity;
	storage->data = samples;
	storage->free_data = false;
}

void BKE_mesh_sample_storage_release(MSurfaceSampleStorage *storage)
{
	if (storage->free_data)
		MEM_freeN(storage->data);
}


void BKE_mesh_sample_generate_random(MSurfaceSampleStorage *dst, DerivedMesh *dm, unsigned int seed, int totsample)
{
	MFace *mfaces;
	int totfaces;
	RNG *rng;
	MFace *mface;
	float a, b;
	int i;
	
	rng = BLI_rng_new(seed);
	
	DM_ensure_tessface(dm);
	mfaces = dm->getTessFaceArray(dm);
	totfaces = dm->getNumTessFaces(dm);
	
	for (i = 0; i < totsample; ++i) {
		MSurfaceSample sample = {0};
		
		mface = &mfaces[BLI_rng_get_int(rng) % totfaces];
		
		if (mface->v4 && BLI_rng_get_int(rng) % 2 == 0) {
			sample.orig_verts[0] = mface->v3;
			sample.orig_verts[1] = mface->v4;
			sample.orig_verts[2] = mface->v1;
		}
		else {
			sample.orig_verts[0] = mface->v1;
			sample.orig_verts[1] = mface->v2;
			sample.orig_verts[2] = mface->v3;
		}
		
		a = BLI_rng_get_float(rng);
		b = BLI_rng_get_float(rng);
		if (a + b > 1.0f) {
			a = 1.0f - a;
			b = 1.0f - b;
		}
		sample.orig_weights[0] = 1.0f - (a + b);
		sample.orig_weights[1] = a;
		sample.orig_weights[2] = b;
		
		if (!dst->store_sample(dst->data, dst->capacity, i, &sample))
			break;
	}
	
	BLI_rng_free(rng);
}

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

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_rand.h"

#include "BKE_bvhutils.h"
#include "BKE_mesh_sample.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"

#include "BLI_strict_flags.h"

/* ==== Evaluate ==== */

bool BKE_mesh_sample_eval(DerivedMesh *dm, const MSurfaceSample *sample, float loc[3], float nor[3], float tang[3])
{
	MVert *mverts = dm->getVertArray(dm);
	unsigned int totverts = (unsigned int)dm->getNumVerts(dm);
	MVert *v1, *v2, *v3;
	
	zero_v3(loc);
	zero_v3(nor);
	zero_v3(tang);
	
	if (sample->orig_verts[0] >= totverts ||
	    sample->orig_verts[1] >= totverts ||
	    sample->orig_verts[2] >= totverts)
		return false;
	
	v1 = &mverts[sample->orig_verts[0]];
	v2 = &mverts[sample->orig_verts[1]];
	v3 = &mverts[sample->orig_verts[2]];
	
	{ /* location */
		madd_v3_v3fl(loc, v1->co, sample->orig_weights[0]);
		madd_v3_v3fl(loc, v2->co, sample->orig_weights[1]);
		madd_v3_v3fl(loc, v3->co, sample->orig_weights[2]);
	}
	
	{ /* normal */
		float vnor[3];
		
		normal_short_to_float_v3(vnor, v1->no);
		madd_v3_v3fl(nor, vnor, sample->orig_weights[0]);
		normal_short_to_float_v3(vnor, v2->no);
		madd_v3_v3fl(nor, vnor, sample->orig_weights[1]);
		normal_short_to_float_v3(vnor, v3->no);
		madd_v3_v3fl(nor, vnor, sample->orig_weights[2]);
		
		normalize_v3(nor);
	}
	
	{ /* tangent */
		float edge[3];
		
		/* XXX simply using the v1-v2 edge as a tangent vector for now ...
		 * Eventually mikktspace generated tangents (CD_TANGENT tessface layer)
		 * should be used for consistency, but requires well-defined tessface
		 * indices for the mesh surface samples.
		 */
		
		sub_v3_v3v3(edge, v2->co, v1->co);
		/* make edge orthogonal to nor */
		madd_v3_v3fl(edge, nor, -dot_v3v3(edge, nor));
		normalize_v3_v3(tang, edge);
	}
	
	return true;
}

bool BKE_mesh_sample_shapekey(Key *key, KeyBlock *kb, const MSurfaceSample *sample, float loc[3])
{
	float *v1, *v2, *v3;

	(void)key;  /* Unused in release builds. */

	BLI_assert(key->elemsize == 3 * sizeof(float));
	BLI_assert(sample->orig_verts[0] < (unsigned int)kb->totelem);
	BLI_assert(sample->orig_verts[1] < (unsigned int)kb->totelem);
	BLI_assert(sample->orig_verts[2] < (unsigned int)kb->totelem);
	
	v1 = (float *)kb->data + sample->orig_verts[0] * 3;
	v2 = (float *)kb->data + sample->orig_verts[1] * 3;
	v3 = (float *)kb->data + sample->orig_verts[2] * 3;
	
	zero_v3(loc);
	madd_v3_v3fl(loc, v1, sample->orig_weights[0]);
	madd_v3_v3fl(loc, v2, sample->orig_weights[1]);
	madd_v3_v3fl(loc, v3, sample->orig_weights[2]);
	
	/* TODO use optional vgroup weights to determine if a shapeky actually affects the sample */
	return true;
}


/* ==== Sampling Utilities ==== */

BLI_INLINE void mesh_sample_weights_from_loc(MSurfaceSample *sample, DerivedMesh *dm, int face_index, const float loc[3])
{
	MFace *face = &dm->getTessFaceArray(dm)[face_index];
	unsigned int index[4] = { face->v1, face->v2, face->v3, face->v4 };
	MVert *mverts = dm->getVertArray(dm);
	
	float *v1 = mverts[face->v1].co;
	float *v2 = mverts[face->v2].co;
	float *v3 = mverts[face->v3].co;
	float *v4 = face->v4 ? mverts[face->v4].co : NULL;
	float w[4];
	int tri[3];
	
	interp_weights_face_v3_index(tri, w, v1, v2, v3, v4, loc);
	
	sample->orig_verts[0] = index[tri[0]];
	sample->orig_verts[1] = index[tri[1]];
	sample->orig_verts[2] = index[tri[2]];
	sample->orig_weights[0] = w[tri[0]];
	sample->orig_weights[1] = w[tri[1]];
	sample->orig_weights[2] = w[tri[2]];
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

void BKE_mesh_sample_storage_single(MSurfaceSampleStorage *storage, MSurfaceSample *sample)
{
	/* handled as just a special array case with capacity = 1 */
	storage->store_sample = mesh_sample_store_array_sample;
	storage->capacity = 1;
	storage->data = sample;
	storage->free_data = false;
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


int BKE_mesh_sample_generate_random(MSurfaceSampleStorage *dst, DerivedMesh *dm, unsigned int seed, int totsample)
{
	MFace *mfaces;
	int totfaces;
	RNG *rng;
	MFace *mface;
	float a, b;
	int i, stored = 0;
	
	rng = BLI_rng_new(seed);
	
	DM_ensure_tessface(dm);
	mfaces = dm->getTessFaceArray(dm);
	totfaces = dm->getNumTessFaces(dm);
	
	for (i = 0; i < totsample; ++i) {
		MSurfaceSample sample = {{0}};
		
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
		
		if (dst->store_sample(dst->data, dst->capacity, i, &sample))
			++stored;
		else
			break;
	}
	
	BLI_rng_free(rng);
	
	return stored;
}


static bool sample_bvh_raycast(MSurfaceSample *sample, DerivedMesh *dm, BVHTreeFromMesh *bvhdata, const float ray_start[3], const float ray_end[3])
{
	BVHTreeRayHit hit;
	float ray_normal[3], dist;

	sub_v3_v3v3(ray_normal, ray_end, ray_start);
	dist = normalize_v3(ray_normal);
	
	hit.index = -1;
	hit.dist = dist;

	if (BLI_bvhtree_ray_cast(bvhdata->tree, ray_start, ray_normal, 0.0f,
	                         &hit, bvhdata->raycast_callback, bvhdata) >= 0) {
		
		mesh_sample_weights_from_loc(sample, dm, hit.index, hit.co);
		
		return true;
	}
	else
		return false;
}

int BKE_mesh_sample_generate_raycast(MSurfaceSampleStorage *dst, DerivedMesh *dm, MeshSampleRayCallback ray_cb, void *userdata, int totsample)
{
	BVHTreeFromMesh bvhdata;
	float ray_start[3], ray_end[3];
	int i, stored = 0;
	
	DM_ensure_tessface(dm);
	
	memset(&bvhdata, 0, sizeof(BVHTreeFromMesh));
	bvhtree_from_mesh_faces(&bvhdata, dm, 0.0f, 4, 6);
	
	if (bvhdata.tree) {
		for (i = 0; i < totsample; ++i) {
			if (ray_cb(userdata, ray_start, ray_end)) {
				MSurfaceSample sample;
				if (sample_bvh_raycast(&sample, dm, &bvhdata, ray_start, ray_end)) {
					if (dst->store_sample(dst->data, dst->capacity, i, &sample))
						++stored;
					else
						break;
				}
			}
		}
	}
	
	free_bvhtree_from_mesh(&bvhdata);
	
	return stored;
}

/* ==== Utilities ==== */

#include "DNA_particle_types.h"

#include "BKE_bvhutils.h"
#include "BKE_particle.h"

bool BKE_mesh_sample_from_particle(MSurfaceSample *sample, ParticleSystem *psys, DerivedMesh *dm, ParticleData *pa)
{
	MVert *mverts;
	MFace *mface;
	float mapfw[4];
	int mapindex;
	float *co1 = NULL, *co2 = NULL, *co3 = NULL, *co4 = NULL;
	float vec[3];
	float w[4];
	
	if (!psys_get_index_on_dm(psys, dm, pa, &mapindex, mapfw))
		return false;
	
	mface = dm->getTessFaceData(dm, mapindex, CD_MFACE);
	mverts = dm->getVertDataArray(dm, CD_MVERT);
	
	co1 = mverts[mface->v1].co;
	co2 = mverts[mface->v2].co;
	co3 = mverts[mface->v3].co;
	
	if (mface->v4) {
		co4 = mverts[mface->v4].co;
		
		interp_v3_v3v3v3v3(vec, co1, co2, co3, co4, mapfw);
	}
	else {
		interp_v3_v3v3v3(vec, co1, co2, co3, mapfw);
	}
	
	/* test both triangles of the face */
	interp_weights_face_v3(w, co1, co2, co3, NULL, vec);
	if (w[0] <= 1.0f && w[1] <= 1.0f && w[2] <= 1.0f) {
		sample->orig_verts[0] = mface->v1;
		sample->orig_verts[1] = mface->v2;
		sample->orig_verts[2] = mface->v3;
	
		copy_v3_v3(sample->orig_weights, w);
		return true;
	}
	else if (mface->v4) {
		interp_weights_face_v3(w, co3, co4, co1, NULL, vec);
		sample->orig_verts[0] = mface->v3;
		sample->orig_verts[1] = mface->v4;
		sample->orig_verts[2] = mface->v1;
	
		copy_v3_v3(sample->orig_weights, w);
		return true;
	}
	else
		return false;
}

bool BKE_mesh_sample_to_particle(MSurfaceSample *sample, ParticleSystem *UNUSED(psys), DerivedMesh *dm, BVHTreeFromMesh *bvhtree, ParticleData *pa)
{
	BVHTreeNearest nearest;
	float vec[3], nor[3], tang[3];
	
	BKE_mesh_sample_eval(dm, sample, vec, nor, tang);
	
	nearest.index = -1;
	nearest.dist_sq = FLT_MAX;
	BLI_bvhtree_find_nearest(bvhtree->tree, vec, &nearest, bvhtree->nearest_callback, bvhtree);
	if (nearest.index >= 0) {
		MFace *mface = dm->getTessFaceData(dm, nearest.index, CD_MFACE);
		MVert *mverts = dm->getVertDataArray(dm, CD_MVERT);
		
		float *co1 = mverts[mface->v1].co;
		float *co2 = mverts[mface->v2].co;
		float *co3 = mverts[mface->v3].co;
		float *co4 = mface->v4 ? mverts[mface->v4].co : NULL;
		
		pa->num = nearest.index;
		pa->num_dmcache = DMCACHE_NOTFOUND;
		
		interp_weights_face_v3(pa->fuv, co1, co2, co3, co4, vec);
		pa->foffset = 0.0f; /* XXX any sensible way to reconstruct this? */
		
		return true;
	}
	else
		return false;
}

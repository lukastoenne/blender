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

bool BKE_mesh_sample_is_volume_sample(const MeshSample *sample)
{
	return sample->orig_verts[0] == 0 && sample->orig_verts[1] == 0;
}

bool BKE_mesh_sample_eval(DerivedMesh *dm, const MeshSample *sample, float loc[3], float nor[3], float tang[3])
{
	MVert *mverts = dm->getVertArray(dm);
	unsigned int totverts = (unsigned int)dm->getNumVerts(dm);
	MVert *v1, *v2, *v3;
	
	zero_v3(loc);
	zero_v3(nor);
	zero_v3(tang);
	
	if (BKE_mesh_sample_is_volume_sample(sample)) {
		/* VOLUME SAMPLE */
		copy_v3_v3(loc, sample->orig_weights);
	}
	else {
		/* SURFACE SAMPLE */
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
	
	normalize_v3(nor);
	}
	
	return true;
}

bool BKE_mesh_sample_shapekey(Key *key, KeyBlock *kb, const MeshSample *sample, float loc[3])
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

BLI_INLINE unsigned int hibit(unsigned int n) {
	n |= (n >>  1);
	n |= (n >>  2);
	n |= (n >>  4);
	n |= (n >>  8);
	n |= (n >> 16);
	return n ^ (n >> 1);
}

static void *buffer_reserve_exp(void *buffer, unsigned int *alloc, size_t elemsize, unsigned int tot)
{
	if (tot > (*alloc)) {
		*alloc = hibit(tot) << 1;
		buffer = MEM_reallocN(buffer, (size_t)(*alloc) * elemsize);
	}
	return buffer;
}

/* ------------------------------------------------------------------------- */

BLI_INLINE void mesh_sample_weights_from_loc(MeshSample *sample, DerivedMesh *dm, int face_index, const float loc[3])
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

typedef void (*GeneratorFreeFp)(struct MeshSampleGenerator *gen);
typedef bool (*GeneratorMakeSampleFp)(struct MeshSampleGenerator *gen, struct MeshSample *sample);

typedef struct MeshSampleGenerator
{
	GeneratorFreeFp free;
	GeneratorMakeSampleFp make_sample;
} MeshSampleGenerator;

static void sample_generator_init(MeshSampleGenerator *gen, GeneratorFreeFp free, GeneratorMakeSampleFp make_sample)
{
	gen->free = free;
	gen->make_sample = make_sample;
}

/* ------------------------------------------------------------------------- */

//#define USE_DEBUG_COUNT

typedef struct MSurfaceSampleGenerator_Random {
	MeshSampleGenerator base;
	
	DerivedMesh *dm;
	RNG *rng;
	float *face_weights;
	float *vertex_weights;
	
#ifdef USE_DEBUG_COUNT
	int *debug_count;
#endif
} MSurfaceSampleGenerator_Random;

static void generator_random_free(MSurfaceSampleGenerator_Random *gen)
{
#ifdef USE_DEBUG_COUNT
	if (gen->debug_count) {
		if (gen->face_weights) {
			int num = gen->dm->getNumTessFaces(gen->dm);
			int i;
			int totsamples = 0;
			
			printf("Surface Sampling (n=%d):\n", num);
			for (i = 0; i < num; ++i)
				totsamples += gen->debug_count[i];
			
			for (i = 0; i < num; ++i) {
				float weight = i > 0 ? gen->face_weights[i] - gen->face_weights[i-1] : gen->face_weights[i];
				int samples = gen->debug_count[i];
				printf("  %d: W = %f, N = %d/%d = %f\n", i, weight, samples, totsamples, (float)samples / (float)totsamples);
			}
		}
		MEM_freeN(gen->debug_count);
	}
#endif
	if (gen->face_weights)
		MEM_freeN(gen->face_weights);
	if (gen->vertex_weights)
		MEM_freeN(gen->vertex_weights);
	if (gen->rng)
		BLI_rng_free(gen->rng);
	MEM_freeN(gen);
}

/* Find the index in "sum" array before "value" is crossed. */
BLI_INLINE int weight_array_binary_search(const float *sum, int size, float value)
{
	int mid, low = 0, high = size - 1;
	
	if (value <= 0.0f)
		return 0;
	
	while (low < high) {
		mid = (low + high) >> 1;
		
		if (sum[mid] < value && value <= sum[mid+1])
			return mid;
		
		if (sum[mid] >= value)
			high = mid - 1;
		else if (sum[mid] < value)
			low = mid + 1;
		else
			return mid;
	}
	
	return low;
}

static bool generator_random_make_sample(MSurfaceSampleGenerator_Random *gen, MeshSample *sample)
{
	DerivedMesh *dm = gen->dm;
	RNG *rng = gen->rng;
	MFace *mfaces = dm->getTessFaceArray(dm);
	int totfaces = dm->getNumTessFaces(dm);
	int totweights = totfaces * 2;
	
	int faceindex, triindex, tri;
	float a, b;
	MFace *mface;
	
	if (gen->face_weights)
		triindex = weight_array_binary_search(gen->face_weights, totweights, BLI_rng_get_float(rng));
	else
		triindex = BLI_rng_get_int(rng) % totweights;
	faceindex = triindex >> 1;
#ifdef USE_DEBUG_COUNT
	if (gen->debug_count)
		gen->debug_count[faceindex] += 1;
#endif
	tri = triindex % 2;
	a = BLI_rng_get_float(rng);
	b = BLI_rng_get_float(rng);
	
	mface = &mfaces[faceindex];
	
	if (tri == 0) {
		sample->orig_verts[0] = mface->v1;
		sample->orig_verts[1] = mface->v2;
		sample->orig_verts[2] = mface->v3;
	}
	else {
		sample->orig_verts[0] = mface->v1;
		sample->orig_verts[1] = mface->v3;
		sample->orig_verts[2] = mface->v4;
	}
	
	if (a + b > 1.0f) {
		a = 1.0f - a;
		b = 1.0f - b;
	}
	sample->orig_weights[0] = 1.0f - (a + b);
	sample->orig_weights[1] = a;
	sample->orig_weights[2] = b;
	
	return true;
}

BLI_INLINE void face_weight(DerivedMesh *dm, MFace *face, MeshSampleVertexWeightFp vertex_weight_cb, void *userdata, float weight[2])
{
	MVert *mverts = dm->getVertArray(dm);
	MVert *v1 = &mverts[face->v1];
	MVert *v2 = &mverts[face->v2];
	MVert *v3 = &mverts[face->v3];
	MVert *v4 = NULL;
	
	weight[0] = area_tri_v3(v1->co, v2->co, v3->co);
	
	if (face->v4) {
		v4 = &mverts[face->v4];
		weight[1] = area_tri_v3(v1->co, v3->co, v4->co);
	}
	else
		weight[1] = 0.0f;
	
	if (vertex_weight_cb) {
		float w1 = vertex_weight_cb(dm, v1, face->v1, userdata);
		float w2 = vertex_weight_cb(dm, v2, face->v2, userdata);
		float w3 = vertex_weight_cb(dm, v3, face->v3, userdata);
		
		weight[0] *= (w1 + w2 + w3) / 3.0f;
		
		if (v4) {
			float w4 = vertex_weight_cb(dm, v4, face->v4, userdata);
			weight[1] *= (w1 + w3 + w4) / 3.0f;
		}
	}
}

MeshSampleGenerator *BKE_mesh_sample_gen_surface_random_ex(DerivedMesh *dm, unsigned int seed,
                                                           MeshSampleVertexWeightFp vertex_weight_cb, void *userdata, bool use_facearea)
{
	MSurfaceSampleGenerator_Random *gen;
	
	DM_ensure_normals(dm);
	DM_ensure_tessface(dm);
	
	if (dm->getNumTessFaces(dm) == 0)
		return NULL;
	
	gen = MEM_callocN(sizeof(MSurfaceSampleGenerator_Random), "MSurfaceSampleGenerator_Random");
	sample_generator_init(&gen->base, (GeneratorFreeFp)generator_random_free, (GeneratorMakeSampleFp)generator_random_make_sample);
	
	gen->dm = dm;
	gen->rng = BLI_rng_new(seed);
	
	if (use_facearea) {
		int numfaces = dm->getNumTessFaces(dm);
		int numweights = numfaces * 2;
		MFace *mfaces = dm->getTessFaceArray(dm);
		MFace *mf;
		int i;
		float totweight;
		
		gen->face_weights = MEM_mallocN(sizeof(float) * (size_t)numweights, "mesh sample face weights");
		
		/* accumulate weights */
		totweight = 0.0f;
		for (i = 0, mf = mfaces; i < numfaces; ++i, ++mf) {
			int index = i << 1;
			float weight[2];
			
			face_weight(dm, mf, vertex_weight_cb, userdata, weight);
			gen->face_weights[index] = totweight;
			totweight += weight[0];
			gen->face_weights[index+1] = totweight;
			totweight += weight[1];
		}
		
		/* normalize */
		if (totweight > 0.0f) {
			float norm = 1.0f / totweight;
			for (i = 0, mf = mfaces; i < numfaces; ++i, ++mf) {
				int index = i << 1;
				gen->face_weights[index] *= norm;
				gen->face_weights[index+1] *= norm;
			}
		}
		else {
			/* invalid weights, remove to avoid invalid binary search */
			MEM_freeN(gen->face_weights);
			gen->face_weights = NULL;
		}
		
#ifdef USE_DEBUG_COUNT
		gen->debug_count = MEM_callocN(sizeof(int) * (size_t)numweights, "surface sample debug counts");
#endif
	}
	
	return &gen->base;
}

MeshSampleGenerator *BKE_mesh_sample_gen_surface_random(DerivedMesh *dm, unsigned int seed)
{
	return BKE_mesh_sample_gen_surface_random_ex(dm, seed, NULL, NULL, true);
}

/* ------------------------------------------------------------------------- */

typedef struct MSurfaceSampleGenerator_RayCast {
	MeshSampleGenerator base;
	
	DerivedMesh *dm;
	BVHTreeFromMesh bvhdata;
	
	MeshSampleRayFp ray_cb;
	void *userdata;
} MSurfaceSampleGenerator_RayCast;

static void generator_raycast_free(MSurfaceSampleGenerator_RayCast *gen)
{
	free_bvhtree_from_mesh(&gen->bvhdata);
	MEM_freeN(gen);
}

static bool generator_raycast_make_sample(MSurfaceSampleGenerator_RayCast *gen, MeshSample *sample)
{
	float ray_start[3], ray_end[3], ray_dir[3], dist;
	BVHTreeRayHit hit;
	
	if (!gen->ray_cb(gen->userdata, ray_start, ray_end))
		return false;
	
	sub_v3_v3v3(ray_dir, ray_end, ray_start);
	dist = normalize_v3(ray_dir);
	
	hit.index = -1;
	hit.dist = dist;

	if (BLI_bvhtree_ray_cast(gen->bvhdata.tree, ray_start, ray_dir, 0.0f,
	                         &hit, gen->bvhdata.raycast_callback, &gen->bvhdata) >= 0) {
		
		mesh_sample_weights_from_loc(sample, gen->dm, hit.index, hit.co);
		
		return true;
	}
	else
		return false;
}

MeshSampleGenerator *BKE_mesh_sample_gen_surface_raycast(DerivedMesh *dm, MeshSampleRayFp ray_cb, void *userdata)
{
	MSurfaceSampleGenerator_RayCast *gen;
	BVHTreeFromMesh bvhdata;
	
	DM_ensure_tessface(dm);
	
	if (dm->getNumTessFaces(dm) == 0)
		return NULL;
	
	memset(&bvhdata, 0, sizeof(BVHTreeFromMesh));
	bvhtree_from_mesh_faces(&bvhdata, dm, 0.0f, 4, 6);
	if (!bvhdata.tree)
		return NULL;
	
	gen = MEM_callocN(sizeof(MSurfaceSampleGenerator_RayCast), "MSurfaceSampleGenerator_RayCast");
	sample_generator_init(&gen->base, (GeneratorFreeFp)generator_raycast_free, (GeneratorMakeSampleFp)generator_raycast_make_sample);
	
	gen->dm = dm;
	memcpy(&gen->bvhdata, &bvhdata, sizeof(gen->bvhdata));
	gen->ray_cb = ray_cb;
	gen->userdata = userdata;
	
	return &gen->base;
}

/* ------------------------------------------------------------------------- */

/* Poisson Disk dart throwing algorithm as described in
 * Cline, David, et al. "Dart throwing on surfaces." Computer Graphics Forum. Vol. 28. No. 4, 2009
 * and extended by
 * Geng, Bo, et al. "Approximate Poisson disk sampling on mesh." Science China Information Sciences 56.9 (2013)
 */

/* The maximum useful number of logarithmic levels for single precision floats, see
 * White, Kenric B., David Cline, and Parris K. Egbert. "Poisson disk point sets by hierarchical dart throwing."
 * IEEE Symposium on Interactive Raytracing, 2007.
 */
#define MAX_LEVELS 23

typedef struct Triangle {
	unsigned int poly;
	unsigned int vert[3];
	float co[3][3];
	float area;
} Triangle;

BLI_INLINE void triangle_init(Triangle *tri, const MVert *mverts, const MLoop *mloops, const MLoopTri *lt)
{
	unsigned int v0 = mloops[lt->tri[0]].v;
	unsigned int v1 = mloops[lt->tri[1]].v;
	unsigned int v2 = mloops[lt->tri[2]].v;
	const float *co[3];
	
	tri->vert[0] = v0;
	tri->vert[1] = v1;
	tri->vert[2] = v2;
	tri->poly = lt->poly;
	
	co[0] = mverts[v0].co;
	co[1] = mverts[v1].co;
	co[2] = mverts[v2].co;
	copy_v3_v3(tri->co[0], co[0]);
	copy_v3_v3(tri->co[1], co[1]);
	copy_v3_v3(tri->co[2], co[2]);
	
	tri->area = area_tri_v3(co[0], co[1], co[2]);
}

typedef struct TriangleList {
	struct Triangle *triangles;
	unsigned int numtri, alloctri;
	
	float Bmin, Bmax; /* area bounds for triangles in this list */
	
	float totarea; /* current sum of triangle areas */
} TriangleList;

BLI_INLINE void tri_list_init(TriangleList *list, float Bmin, float Bmax)
{
	list->alloctri = 0;
	list->numtri = 0;
	list->triangles = NULL;
	list->Bmin = Bmin;
	list->Bmax = Bmax;
	list->totarea = 0.0f;
}

BLI_INLINE Triangle *tri_list_append(TriangleList *list)
{
	list->numtri += 1;
	list->triangles = buffer_reserve_exp(list->triangles, &list->alloctri, sizeof(Triangle), list->numtri);
	
	return &list->triangles[list->numtri - 1];
}

typedef struct TriangleIndex {
	struct TriangleList lists[MAX_LEVELS];
	
	float area_max; /* upper area bound */
} TriangleIndex;

BLI_INLINE unsigned int tri_area_bin(const TriangleIndex *index, float area)
{
	/* XXX there may be a faster way because
	 * floor(log_b(x)) == floor(log_b(floor(x)))
	 * for real numbers x, i.e. we can convert to int first and
	 * use efficient bit shifting for log2 (?) - lukas
	 */
	return (unsigned int)log2(index->area_max / area);
}

BLI_INLINE Triangle *tri_index_insert(TriangleIndex *index, const Triangle *tri)
{
	Triangle *ptri;
	TriangleList *list;
	unsigned int bin = tri_area_bin(index, tri->area);
	
	/* discard too tiny triangles */
	if (bin >= MAX_LEVELS)
		return NULL;
	
	list = &index->lists[bin];
	
	ptri = tri_list_append(list);
	*ptri = tri;
	
	list->totarea += tri->area;
	
	return ptri;
}

BLI_INLINE void tri_index_init(TriangleIndex *index, DerivedMesh *dm)
{
	const MVert *mverts = dm->getVertArray(dm);
	const MLoop *mloops = dm->getLoopArray(dm);
	const MLoopTri *mlooptris = dm->getLoopTriArray(dm);
	unsigned int numlooptris = (unsigned int)dm->getNumLoopTri(dm);
	
	const MLoopTri *lt;
	unsigned int i;
	float Bmax;
	
	/* find largest triangle area */
	index->area_max = 0.0f;
	for (i = 0, lt = mlooptris; i < numlooptris; ++i, ++lt) {
		Triangle tri;
		triangle_init(&tri, mverts, mloops, lt);
		
		if (tri.area > index->area_max)
			index->area_max = tri.area;
	}
	
	/* init triangle lists */
	Bmax = index->area_max;
	for (i = 0; i < MAX_LEVELS; ++i) {
		/* triangle area range is halved with each level */
		float Bmin = Bmax * 0.5f;
		
		tri_list_init(&index->lists[i], Bmin, Bmax);
		
		Bmax = Bmin;
	}
	
	/* fill triangles into bins */
	for (i = 0, lt = mlooptris; i < numlooptris; ++i, ++lt) {
		Triangle tri;
		
		triangle_init(&tri, mverts, mloops, lt);
		tri_index_insert(index, &tri);
	}
}

BLI_INLINE void tri_index_free(TriangleIndex *index)
{
	int i;
	for (i = 0; i < MAX_LEVELS; ++i) {
		if (index->lists[i].triangles)
			MEM_freeN(index->lists[i].triangles);
	}
}

#undef MAX_LEVELS

typedef struct MSurfaceSampleGenerator_PoissonDisk {
	MeshSampleGenerator base;
	
	RNG *rng;
	TriangleIndex index;
} MSurfaceSampleGenerator_PoissonDisk;

static void generator_poissondisk_free(MSurfaceSampleGenerator_PoissonDisk *gen)
{
	BLI_rng_free(gen->rng);
	tri_index_free(&gen->index);
	MEM_freeN(gen);
}

static bool generator_poissondisk_make_sample(MSurfaceSampleGenerator_PoissonDisk *gen, MeshSample *sample)
{
	return false;
}

MeshSampleGenerator *BKE_mesh_sample_gen_surface_poissondisk(DerivedMesh *dm, unsigned int seed)
{
	MSurfaceSampleGenerator_PoissonDisk *gen;
	
	DM_ensure_looptri(dm);
	
	if (dm->getNumLoopTri(dm) == 0)
		return NULL;
	
	gen = MEM_callocN(sizeof(MSurfaceSampleGenerator_PoissonDisk), "MSurfaceSampleGenerator_PoissonDisk");
	sample_generator_init(&gen->base, (GeneratorFreeFp)generator_poissondisk_free, (GeneratorMakeSampleFp)generator_poissondisk_make_sample);
	
	gen->rng = BLI_rng_new(seed);
	tri_index_init(&gen->index, dm);
	
	return &gen->base;
}

/* ------------------------------------------------------------------------- */

typedef struct MVolumeSampleGenerator_Random {
	MeshSampleGenerator base;
	
	DerivedMesh *dm;
	BVHTreeFromMesh bvhdata;
	RNG *rng;
	float min[3], max[3], extent[3], volume;
	float density;
	int max_samples_per_ray;
	
	/* current ray intersections */
	BVHTreeRayHit *ray_hits;
	unsigned int tothits, allochits;
	
	/* current segment index and sample number */
	unsigned int cur_seg, cur_tot, cur_sample;
} MVolumeSampleGenerator_Random;

static void generator_volume_random_free(MVolumeSampleGenerator_Random *gen)
{
	if (gen->rng)
		BLI_rng_free(gen->rng);
	free_bvhtree_from_mesh(&gen->bvhdata);
	
	if (gen->ray_hits)
		MEM_freeN(gen->ray_hits);
	
	MEM_freeN(gen);
}

static void generator_volume_ray_cb(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	MVolumeSampleGenerator_Random *gen = userdata;
	
	gen->bvhdata.raycast_callback(&gen->bvhdata, index, ray, hit);
	
	if (hit->index >= 0) {
		++gen->tothits;
		gen->ray_hits = buffer_reserve_exp(gen->ray_hits, &gen->allochits, sizeof(BVHTreeRayHit), gen->tothits);
		
		memcpy(&gen->ray_hits[gen->tothits-1], hit, sizeof(BVHTreeRayHit));
	}
}

static void generator_volume_random_cast_ray(MVolumeSampleGenerator_Random *gen)
{
	/* bounding box margin to get clean ray intersections */
	static const float margin = 0.01f;
	
	RNG *rng = gen->rng;
	float ray_start[3], ray_end[3], ray_dir[3];
	int axis;
	
	ray_start[0] = BLI_rng_get_float(rng);
	ray_start[1] = BLI_rng_get_float(rng);
	ray_start[2] = 0.0f;
	ray_end[0] = BLI_rng_get_float(rng);
	ray_end[1] = BLI_rng_get_float(rng);
	ray_end[2] = 1.0f;
	
	axis = BLI_rng_get_int(rng) % 3;
	switch (axis) {
		case 0: break;
		case 1:
			SHIFT3(float, ray_start[0], ray_start[1], ray_start[2]);
			SHIFT3(float, ray_end[0], ray_end[1], ray_end[2]);
			break;
		case 2:
			SHIFT3(float, ray_start[2], ray_start[1], ray_start[0]);
			SHIFT3(float, ray_end[2], ray_end[1], ray_end[0]);
			break;
	}
	
	mul_v3_fl(ray_start, 1.0f + 2.0f*margin);
	add_v3_fl(ray_start, -margin);
	mul_v3_fl(ray_end, 1.0f + 2.0f*margin);
	add_v3_fl(ray_end, -margin);
	
	madd_v3_v3v3v3(ray_start, gen->min, ray_start, gen->extent);
	madd_v3_v3v3v3(ray_end, gen->min, ray_end, gen->extent);
	
	sub_v3_v3v3(ray_dir, ray_end, ray_start);
	
	gen->tothits = 0;
	BLI_bvhtree_ray_cast_all(gen->bvhdata.tree, ray_start, ray_dir, 0.0f,
	                         generator_volume_ray_cb, gen);
	
	gen->cur_seg = 0;
	gen->cur_tot = 0;
	gen->cur_sample = 0;
}

static void generator_volume_init_segment(MVolumeSampleGenerator_Random *gen)
{
	BVHTreeRayHit *a, *b;
	float length;
	
	BLI_assert(gen->cur_seg + 1 < gen->tothits);
	a = &gen->ray_hits[gen->cur_seg];
	b = &gen->ray_hits[gen->cur_seg + 1];
	
	length = len_v3v3(a->co, b->co);
	gen->cur_tot = (unsigned int)min_ii(gen->max_samples_per_ray, (int)ceilf(length * gen->density));
	gen->cur_sample = 0;
}

static bool generator_volume_random_make_sample(MVolumeSampleGenerator_Random *gen, MeshSample *sample)
{
	RNG *rng = gen->rng;
	BVHTreeRayHit *a, *b;
	
	if (gen->cur_seg + 1 >= gen->tothits) {
		generator_volume_random_cast_ray(gen);
		if (gen->tothits < 2)
			return false;
	}
	
	if (gen->cur_sample >= gen->cur_tot) {
		gen->cur_seg += 2;
		
		if (gen->cur_seg + 1 >= gen->tothits) {
			generator_volume_random_cast_ray(gen);
			if (gen->tothits < 2)
				return false;
		}
		
		generator_volume_init_segment(gen);
	}
	a = &gen->ray_hits[gen->cur_seg];
	b = &gen->ray_hits[gen->cur_seg + 1];
	
	if (gen->cur_sample < gen->cur_tot) {
		float t;
		
		sample->orig_verts[0] = 0;
		sample->orig_verts[1] = 0;
		sample->orig_verts[2] = 0;
		
		t = BLI_rng_get_float(rng);
		interp_v3_v3v3(sample->orig_weights, a->co, b->co, t);
		
		gen->cur_sample += 1;
		
		return true;
	}
	
	return false;
}

MeshSampleGenerator *BKE_mesh_sample_gen_volume_random_bbray(DerivedMesh *dm, unsigned int seed, float density)
{
	MVolumeSampleGenerator_Random *gen;
	BVHTreeFromMesh bvhdata;
	
	gen = MEM_callocN(sizeof(MVolumeSampleGenerator_Random), "MVolumeSampleGenerator_Random");
	sample_generator_init(&gen->base, (GeneratorFreeFp)generator_volume_random_free,
	                      (GeneratorMakeSampleFp)generator_volume_random_make_sample);
	
	DM_ensure_tessface(dm);
	
	if (dm->getNumTessFaces(dm) == 0)
		return NULL;
	
	memset(&bvhdata, 0, sizeof(BVHTreeFromMesh));
	bvhtree_from_mesh_faces(&bvhdata, dm, 0.0f, 4, 6);
	if (!bvhdata.tree)
		return NULL;
	
	gen->dm = dm;
	memcpy(&gen->bvhdata, &bvhdata, sizeof(gen->bvhdata));
	gen->rng = BLI_rng_new(seed);
	
	INIT_MINMAX(gen->min, gen->max);
	dm->getMinMax(dm, gen->min, gen->max);
	sub_v3_v3v3(gen->extent, gen->max, gen->min);
	gen->volume = gen->extent[0] * gen->extent[1] * gen->extent[2];
	gen->density = density;
	gen->max_samples_per_ray = max_ii(1, (int)powf(gen->volume, 1.0f/3.0f)) >> 1;
	
	gen->ray_hits = buffer_reserve_exp(gen->ray_hits, &gen->allochits, sizeof(BVHTreeRayHit), 64);
	
	return &gen->base;
}

/* ------------------------------------------------------------------------- */

void BKE_mesh_sample_free_generator(MeshSampleGenerator *gen)
{
	gen->free(gen);
}

bool BKE_mesh_sample_generate(MeshSampleGenerator *gen, struct MeshSample *sample)
{
	return gen->make_sample(gen, sample);
}

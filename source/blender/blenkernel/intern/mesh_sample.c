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

#include "BKE_bvhutils.h"
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


/* ==== Sample Storage ==== */

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


/* ==== Sample Generators ==== */

/* -------- Uniform Random Sampling -------- */

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
		
		if (!dst->store_sample(dst->data, dst->capacity, i, &sample))
			break;
	}
	
	BLI_rng_free(rng);
}

/* -------- Poisson Disk Sampling -------- */

/* Implementation based on
 * "Poisson Disk Point Sets by Hierarchical Dart Throwing" (White, Cline, Egbert 2007)
 */

static void mesh_sample_boundbox(DerivedMesh *dm, float bbmin[3], float bbmax[3])
{
	MVert *mverts = dm->getVertArray(dm);
	MVert *mv;
	int totvert = dm->getNumVerts(dm);
	int i;
	
	if (totvert == 0) {
		zero_v3(bbmin);
		zero_v3(bbmax);
	}
	else {
		mv = mverts;
		copy_v3_v3(bbmin, mv->co);
		copy_v3_v3(bbmax, mv->co);
		
		++mv;
		for (i = 1; i < totvert; ++i, ++mv) {
			CLAMP_MAX(bbmin[0], mv->co[0]);
			CLAMP_MAX(bbmin[1], mv->co[1]);
			CLAMP_MAX(bbmin[2], mv->co[2]);
			CLAMP_MIN(bbmax[0], mv->co[0]);
			CLAMP_MIN(bbmax[1], mv->co[1]);
			CLAMP_MIN(bbmax[2], mv->co[2]);
		}
	}
}

typedef struct PoissonCell {
	short index[3];
} PoissonCell;

typedef struct PoissonGrid {
	/* constants */
	PoissonCell *cells;
	int alloc;
	float cellsize, cellvolume;
	
	/* variables */
	int size;
	float totvolume;
} PoissonGrid;

/* Note: according to the original paper the maximum number of grid levels
 * is bounded by the floating point precision,
 * i.e. 23 for single precision float, 52 for double precision.
 * In practice discarding grid levels before this limit is also fine.
 */
#define MAX_GRID_LEVELS 16

#define GRID_CELL_ALLOC 256 /* XXX arbitrary */

static void poisson_grid_add_cell(PoissonGrid *grid, short i, short j, short k)
{
	PoissonCell *cell;
	
	if (grid->size == grid->alloc) {
		grid->alloc += GRID_CELL_ALLOC;
		grid->cells = MEM_reallocN(grid->cells, sizeof(PoissonCell) * (size_t)grid->alloc);
	}
	
	cell = &grid->cells[grid->size];
	cell->index[0] = i;
	cell->index[1] = j;
	cell->index[2] = k;
	
	++grid->size;
}

static void poisson_grid_remove_cell(PoissonGrid *grid, int index)
{
	PoissonCell *cell = &grid->cells[index];
	if (index < grid->size - 1) {
		PoissonCell *cell_move = &grid->cells[grid->size - 1];
		
		/* move last cell to the new location */
		memcpy(cell, cell_move, sizeof(PoissonCell));
	}
	
	--grid->size;
}

static void poisson_grid_reserve(PoissonGrid *grid, int totcells)
{
	if (grid->alloc < totcells) {
		int blocks = (totcells + GRID_CELL_ALLOC - 1) / GRID_CELL_ALLOC;
		grid->alloc = blocks * GRID_CELL_ALLOC;
		grid->cells = MEM_reallocN(grid->cells, sizeof(PoissonCell) * (size_t)grid->alloc);
	}
}

static void poisson_grids_init(PoissonGrid *grids, float basesize)
{
	float cellsize = basesize;
	float cellvolume = basesize * basesize * basesize;
	int i;
	for (i = 0; i < MAX_GRID_LEVELS; ++i) {
		memset(&grids[i], 0, sizeof(PoissonGrid));
		grids[i].cellsize = cellsize;
		grids[i].cellvolume = cellvolume;
		
		cellsize /= 2.0f;
		cellvolume /= 8.0f;
	}
}

static void poisson_grids_free(PoissonGrid *grids)
{
	int i;
	for (i = 0; i < MAX_GRID_LEVELS; ++i) {
		if (grids[i].cells)
			MEM_freeN(grids[i].cells);
	}
}

static float poisson_grids_update_volume(PoissonGrid *grids)
{
	float totvolume = 0.0f;
	int i;
	for (i = 0; i < MAX_GRID_LEVELS; ++i) {
		grids[i].totvolume = totvolume;
		totvolume += grids[i].cellvolume * (float)grids[i].size;
	}
	return totvolume;
}

/* Note: linear search is well suited to finding a grid list,
 * because the number of lists is fixed and fairly small.
 */
static PoissonGrid *poisson_grid_search(PoissonGrid *grids, float volume)
{
	int i;
	PoissonGrid *grid;
	for (i = 0, grid = grids; i < MAX_GRID_LEVELS - 1; ++i, ++grid) {
		if ((grid+1)->totvolume > volume)
			break;
	}
	return grid;
}
#if 0 /* unused binary search */
static PoissonGrid *poisson_grid_search(PoissonGrid *grids, float volume)
{
	int low = 0, high = MAX_GRID_LEVELS;
	
	if (volume == 0.0f)
		return &grids[0];
	
	while (low <= high) {
		int mid = (low + high) >> 1;
		PoissonGrid *grid = &grids[mid];
		
		if (grid->totvolume < volume && volume <= (grid+1)->totvolume)
			return grid;
		else if (grid->totvolume >= volume)
			high = mid - 1;
		else if (grid->totvolume < volume)
			low = mid + 1;
		else
			return grid;
	}
	
	return &grids[low];
}
#endif

BLI_INLINE bool isect_aabb_v3(const float bbmin[3], const float bbmax[3], const float v[3])
{
	return (v[0] >= bbmin[0] && v[1] >= bbmin[1] && v[2] >= bbmin[2] &&
	        v[0] <= bbmax[0] && v[1] <= bbmax[1] && v[2] <= bbmax[2]);
}

BLI_INLINE bool isect_aabb_face(const float bbmin[3], const float bbmax[3], const MVert *mverts, const MFace *mf)
{
	bool overlap = isect_aabb_v3(bbmin, bbmax, mverts[mf->v1].co);
	overlap |= isect_aabb_v3(bbmin, bbmax, mverts[mf->v2].co);
	overlap |= isect_aabb_v3(bbmin, bbmax, mverts[mf->v3].co);
	if (mf->v4)
		overlap |= isect_aabb_v3(bbmin, bbmax, mverts[mf->v4].co);
	return overlap;
}

#if 0
typedef struct BoxFace {
	int index;
	float v1[3], v2[3], v3[3];
	
	float area;
} BoxFace;

static int isect_mesh_box(DerivedMesh *dm, const float bbmin[3], const float bbmax[3], BoxFace *r_faces)
{
	MVert *mverts = dm->getVertArray(dm);
	MFace *mfaces = dm->getTessFaceArray(dm), *mf;
//	int totverts = dm->getNumVerts(dm);
	int totfaces = dm->getNumTessFaces(dm), i;
	BoxFace *isect_face;
	int numfaces;
	
	numfaces = 0;
	isect_face = r_faces;
	for (i = 0, mf = mfaces; i < totfaces; ++i, ++mf) {
		if (isect_aabb_face(bbmin, bbmax, mverts, mf)) {
			*isect_face->index = i;
			
			++numfaces;
			++isect_face;
		}
	}
	
	return numfaces;
}
#endif

static int stupid_isect_select_face(DerivedMesh *dm, const float bbmin[3], const float bbmax[3])
{
	MVert *mverts = dm->getVertArray(dm);
	MFace *mfaces = dm->getTessFaceArray(dm), *mf;
//	int totverts = dm->getNumVerts(dm);
	int totfaces = dm->getNumTessFaces(dm), i;
	
	for (i = 0, mf = mfaces; i < totfaces; ++i, ++mf) {
		if (isect_aabb_face(bbmin, bbmax, mverts, mf)) {
			return i;
		}
	}
	
	return -1;
}

#if 1
BLI_INLINE bool mesh_sample_project_barycentric(const float co[3], const float a[3], const float b[3], const float c[3], float w[3])
{
	float dir1[3], dir2[3], len1, len2, vec[3];
	float co2[2], a2[2], b2[2], c2[2];
	
	sub_v3_v3v3(vec, co, a);
	sub_v3_v3v3(dir1, b, a);
	len1 = normalize_v3(dir1);
	sub_v3_v3v3(dir2, c, a);
	len2 = normalize_v3(dir2);
	
	if (len1 > 0.0f && len2 > 0.0f) {
		co2[0] = dot_v3v3(vec, dir1);
		co2[1] = dot_v3v3(vec, dir2);
		a2[0] = 0.0f;
		a2[1] = 0.0f;
		b2[0] = len1;
		b2[1] = 0.0f;
		c2[0] = 0.0f;
		c2[1] = len2;
	}
	else {
		zero_v3(w);
	}
	
	return barycentric_coords_v2(a2, b2, c2, co2, w);
}
#endif

typedef struct PoissonDiskSampleData {
	MSurfaceSampleStorage *dst;
	int dst_index;
	
	DerivedMesh *dm;
	float bbmin[3], bbmax[3];
	BVHTreeFromMesh bvhdata;
	
	PoissonGrid grids[MAX_GRID_LEVELS];
	RNG *rng;
} PoissonDiskSampleData;

typedef enum ePoissonDiskSampleResult {
	PDS_SAMPLE_FAILED,			/* could not store sample (abort) */
	PDS_CELL_EMPTY,				/* no geometry in the cell, no samples can be generated */
	PDS_CELL_COVERED,			/* cell is fully covered alread, no sample can be added */
	PDS_CELL_NEW_SAMPLE,		/* new sample has been added to the cell */
} ePoissonDiskSampleResult;

static ePoissonDiskSampleResult poisson_disk_gen_sample(PoissonDiskSampleData *data, PoissonGrid *grid, PoissonCell *cell)
{
#if 0
	float bbmin[3], bbmax[3];
	int index;
	
	bbmin[0] = data->bbmin[0] + grid->cellsize * (float)cell->index[0];
	bbmin[1] = data->bbmin[1] + grid->cellsize * (float)cell->index[1];
	bbmin[2] = data->bbmin[2] + grid->cellsize * (float)cell->index[2];
	bbmax[0] = data->bbmin[0] + grid->cellsize * (float)(cell->index[0] + 1);
	bbmax[1] = data->bbmin[1] + grid->cellsize * (float)(cell->index[1] + 1);
	bbmax[2] = data->bbmin[2] + grid->cellsize * (float)(cell->index[2] + 1);
	
	index = stupid_isect_select_face(data->dm, bbmin, bbmax);
	if (index >= 0) {
		MFace *mf = &data->dm->getTessFaceArray(data->dm)[index];
//		MVert *mverts = data->dm->getVertArray(data->dm);
		MSurfaceSample sample;
		
		sample.orig_verts[0] = mf->v1;
		sample.orig_verts[1] = mf->v2;
		sample.orig_verts[2] = mf->v3;
		sample.orig_weights[0] = 1.0f/3.0f;
		sample.orig_weights[1] = 1.0f/3.0f;
		sample.orig_weights[2] = 1.0f/3.0f;
		
		if (data->dst->store_sample(data->dst->data, data->dst->capacity, data->dst_index++, &sample))
			return PDS_CELL_NEW_SAMPLE;
		else
			return PDS_SAMPLE_FAILED;
	}
	
	return PDS_CELL_EMPTY;
	
#else
	float co[3];
	BVHTreeNearest nearest = {0};
	nearest.index = -1;
	nearest.dist_sq = 10.0f;
	
	co[0] = data->bbmin[0] + ((float)cell->index[0] + 0.5f) * grid->cellsize;
	co[1] = data->bbmin[1] + ((float)cell->index[1] + 0.5f) * grid->cellsize;
	co[2] = data->bbmin[2] + ((float)cell->index[2] + 0.5f) * grid->cellsize;
	
	BLI_bvhtree_find_nearest(data->bvhdata.tree, co, &nearest, data->bvhdata.nearest_callback, &data->bvhdata);
	
	if (nearest.index != -1) {
		MFace *mf = &data->dm->getTessFaceArray(data->dm)[nearest.index];
		MVert *mverts = data->dm->getVertArray(data->dm);
		MSurfaceSample sample;
		float *v1, *v2, *v3, *v4;
		float w[3];
		
		v1 = mverts[mf->v1].co;
		v2 = mverts[mf->v2].co;
		v3 = mverts[mf->v3].co;
		v4 = mf->v4 ? mverts[mf->v4].co : NULL;
		
		if (mesh_sample_project_barycentric(nearest.co, v1, v2, v3, w)) {
			sample.orig_verts[0] = mf->v1;
			sample.orig_verts[1] = mf->v2;
			sample.orig_verts[2] = mf->v3;
			copy_v3_v3(sample.orig_weights, w);
			
			if (data->dst->store_sample(data->dst->data, data->dst->capacity, data->dst_index++, &sample))
				return PDS_CELL_NEW_SAMPLE;
			else
				return PDS_SAMPLE_FAILED;
		}
		
		if (v4 && mesh_sample_project_barycentric(nearest.co, v3, v4, v1, w)) {
			sample.orig_verts[0] = mf->v3;
			sample.orig_verts[1] = mf->v4;
			sample.orig_verts[2] = mf->v1;
			copy_v3_v3(sample.orig_weights, w);
			
			if (data->dst->store_sample(data->dst->data, data->dst->capacity, data->dst_index++, &sample))
				return PDS_CELL_NEW_SAMPLE;
			else
				return PDS_SAMPLE_FAILED;
		}
	}
	
	return PDS_CELL_EMPTY;
#endif
}

/* if return value is true, continue generating samples */
static bool mesh_sample_poisson_disk_step(PoissonDiskSampleData *data, float *active_volume)
{
	float u = BLI_rng_get_float(data->rng);
	float search_volume = u * (*active_volume);
	PoissonGrid *grid = poisson_grid_search(data->grids, search_volume);
	int cell_index = (int)((search_volume - grid->totvolume) / grid->cellvolume);
	PoissonCell *cell = &(grid->cells[cell_index]);
	ePoissonDiskSampleResult result;
	
	result = poisson_disk_gen_sample(data, grid, cell);
	
	/* deactivate the cell */
	/* warning: this invalidates the cell_index, do this after evaluating cell geometry! */
	poisson_grid_remove_cell(grid, cell_index);
	
	switch (result) {
		case PDS_SAMPLE_FAILED:
			return false;
		case PDS_CELL_NEW_SAMPLE:
		case PDS_CELL_COVERED:
			break; /* TODO */
		case PDS_CELL_EMPTY:
			/* nothing further to do for this cell */
			break;
	}
	
	/* recalculate volumes */
	*active_volume = poisson_grids_update_volume(data->grids);
	
	return true;
}

static bool mesh_sample_poisson_disk_has_active_cells(PoissonDiskSampleData *data)
{
	int i;
	PoissonGrid *grid;
	for (i = 0, grid = data->grids; i < MAX_GRID_LEVELS; ++i, ++grid) {
		if (grid->size > 0)
			return true;
	}
	return false;
}

static void mesh_sample_generate_poisson_disk(MSurfaceSampleStorage *dst, DerivedMesh *dm, unsigned int seed, float bbmin[3], float bbmax[3], float radius)
{
	PoissonDiskSampleData data;
	float extent[3];
	float basesize;
	int totgrid, gridsize[3];
	short i, j, k;
	
	data.dst = dst;
	data.dst_index = 0;
	data.dm = dm;
	copy_v3_v3(data.bbmin, bbmin);
	copy_v3_v3(data.bbmax, bbmax);
	data.rng = BLI_rng_new(seed);
	
	/* create BVH tree */
	DM_ensure_tessface(dm);
	memset(&data.bvhdata, 0, sizeof(data.bvhdata));
	bvhtree_from_mesh_faces(&data.bvhdata, dm, 0.0f, 4, 8);
	
	/* minimum size of base grid cubes (must be covered by radius) */
	sub_v3_v3v3(extent, bbmax, bbmin);
	for (i = 0; i < 3; ++i) {
		float size = extent[i] / ceilf(extent[i] * sqrtf(3.0f) / radius);
		if (i == 0 || size < basesize)
			basesize = size;
	}
	poisson_grids_init(data.grids, basesize);
	
	/* actual number of base grid entries */
	totgrid = 1;
	for (i = 0; i < 3; ++i) {
		gridsize[i] = (int)ceilf(extent[i] / basesize);
		totgrid *= gridsize[i];
	}
	
	/* activate all base grid cells */
	poisson_grid_reserve(&data.grids[0], totgrid);
	for (k = 0; k < gridsize[2]; ++k) {
		for (j = 0; j < gridsize[1]; ++j) {
			for (i = 0; i < gridsize[0]; ++i) {
				poisson_grid_add_cell(&data.grids[0], i, j, k);
			}
		}
	}
	
	/* main poisson disk sampling loop */
	{
		float active_volume = poisson_grids_update_volume(data.grids);
		while (mesh_sample_poisson_disk_has_active_cells(&data)) {
			if (!mesh_sample_poisson_disk_step(&data, &active_volume))
				break;
		}
	}
	
	free_bvhtree_from_mesh(&data.bvhdata);
	poisson_grids_free(data.grids);
	BLI_rng_free(data.rng);
}

#undef MAX_GRID_LEVELS

void BKE_mesh_sample_generate_poisson_disk_by_radius(MSurfaceSampleStorage *dst, DerivedMesh *dm, unsigned int seed, float radius)
{
	float bbmin[3], bbmax[3];
	
	/* spatial extent of the grid */
	mesh_sample_boundbox(dm, bbmin, bbmax);
	
	mesh_sample_generate_poisson_disk(dst, dm, seed, bbmin, bbmax, radius);
}

void BKE_mesh_sample_generate_poisson_disk_by_amount(MSurfaceSampleStorage *dst, DerivedMesh *dm, unsigned int seed, int totsample)
{
	float bbmin[3], bbmax[3];
	
	float extent[3], volume, radius;
	
	/* spatial extent of the grid */
	mesh_sample_boundbox(dm, bbmin, bbmax);
	sub_v3_v3v3(extent, bbmax, bbmin);
	
	/* minimum radius, based on maximal sphere packing
	 * http://en.wikipedia.org/wiki/Close-packing_of_equal_spheres
	 * acquired by assuming that n spheres of volume V = 4/3*pi*r^3
	 * fill a total volume of V_tot = n * V * pi/(3*sqrt(2))
	 */
	volume = extent[0] * extent[1] * extent[2];
	radius = sqrt3f( volume / ((float)totsample * 4.0f * sqrtf(2.0f)) );
	
	mesh_sample_generate_poisson_disk(dst, dm, seed, bbmin, bbmax, radius);
}

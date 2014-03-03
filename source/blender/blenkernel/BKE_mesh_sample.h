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

#ifndef __BKE_MESH_SAMPLE_H__
#define __BKE_MESH_SAMPLE_H__

/** \file BKE_mesh_sample.h
 *  \ingroup bke
 */

struct MSurfaceSample;

/* Evaluate */


/* Iterators */

#if 0
struct MSampleIterator;

typedef void (*MSampleIteratorNextFunc)(MSampleIterator *iter);
typedef bool (*MSampleIteratorValidFunc)(MSampleIterator *iter);
typedef void (*MSampleIteratorFreeFunc)(MSampleIterator *iter);

typedef struct MSampleIterator {
	MSampleIteratorNextFunc next;
	MSampleIteratorValidFunc valid;
	MSampleIteratorFreeFunc free;
} MSampleIterator;

typedef struct MSampleArrayIterator {
	MSampleIterator base;
	
	MSurfaceSample *cur;
	int remaining;
} MSampleArrayIterator;

void BKE_mesh_sample_surface_array_begin(MSurfaceSampleArrayIterator *iter, MSurfaceSample *array, int totarray);
#endif

/* Sampling */

typedef enum eMSurfaceSampleRNG {
	MSS_RNG_UNIFORM
} eMSurfaceSampleRNG;

typedef struct MSurfaceSampleInfo {
	struct DerivedMesh *dm;
	
	eMSurfaceSampleRNG rng;
	unsigned int seed;
} MSurfaceSampleInfo;

void BKE_mesh_sample_surface_array(const struct MSurfaceSampleInfo *info, struct MSurfaceSample *samples, int totsample);

#endif  /* __BKE_MESH_SAMPLE_H__ */

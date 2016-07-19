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

#ifndef __BKE_STRANDS_H__
#define __BKE_STRANDS_H__

/** \file blender/blenkernel/BKE_strands.h
 *  \ingroup bke
 */

#include "BLI_utildefines.h"

#include "DNA_strand_types.h"

struct BMesh;
struct BMVert;
struct DerivedMesh;
struct GPUStrandsConverter;
struct GPUStrandsShader;
struct GPUDrawStrands;

static const unsigned int STRAND_INDEX_NONE = 0xFFFFFFFF;

struct Strands *BKE_strands_new(void);
struct Strands *BKE_strands_copy(struct Strands *strands);
void BKE_strands_free(struct Strands *strands);

bool BKE_strands_get_location(const struct StrandCurve *curve, struct DerivedMesh *root_dm, float loc[3]);
bool BKE_strands_get_vectors(const struct StrandCurve *curve, struct DerivedMesh *root_dm,
                             float loc[3], float nor[3], float tang[3]);
bool BKE_strands_get_matrix(const struct StrandCurve *curve, struct DerivedMesh *root_dm, float mat[4][4]);

bool BKE_strands_get_fiber_location(const struct StrandFiber *fiber, struct DerivedMesh *root_dm, float loc[3]);
bool BKE_strands_get_fiber_vectors(const struct StrandFiber *fiber, struct DerivedMesh *root_dm,
                                   float loc[3], float nor[3], float tang[3]);
bool BKE_strands_get_fiber_matrix(const struct StrandFiber *fiber, struct DerivedMesh *root_dm, float mat[4][4]);

/* ------------------------------------------------------------------------- */

typedef struct StrandCurveCache {
	float (*verts)[3];
	float (*normals)[3];
	float (*tangents)[3];
	int maxverts;
} StrandCurveCache;

struct StrandCurveCache *BKE_strand_curve_cache_create(const struct Strands *strands, int subdiv);
struct StrandCurveCache *BKE_strand_curve_cache_create_bm(struct BMesh *bm, int subdiv);
void BKE_strand_curve_cache_free(struct StrandCurveCache *cache);
int BKE_strand_curve_cache_calc(const struct StrandVertex *orig_verts, int orig_num_verts,
                                struct StrandCurveCache *cache, float rootmat[4][4], int subdiv);
int BKE_strand_curve_cache_calc_bm(struct BMVert *root, int orig_num_verts,
                                   struct StrandCurveCache *cache, float rootmat[4][4], int subdiv);
int BKE_strand_curve_cache_size(int orig_num_verts, int subdiv);
int BKE_strand_curve_cache_totverts(int orig_totverts, int orig_totcurves, int subdiv);

/* ------------------------------------------------------------------------- */

void BKE_strands_test_init(struct Strands *strands, struct DerivedMesh *scalp,
                           int totcurves, int maxverts,
                           unsigned int seed);


void BKE_strands_scatter(struct Strands *strands,
                         struct DerivedMesh *scalp, unsigned int amount,
                         unsigned int seed);
void BKE_strands_free_fibers(struct Strands *strands);

void BKE_strands_free_drawdata(struct GPUDrawStrands *gpu_buffer);

/* ------------------------------------------------------------------------- */

void BKE_strands_invalidate_shader(struct Strands *strands);

struct GPUStrandsConverter *BKE_strands_get_gpu_converter(struct Strands *strands, struct DerivedMesh *root_dm,
                                                          int subdiv, int fiber_primitive, bool use_geomshader);

#endif

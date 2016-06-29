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

struct DerivedMesh;
struct GPUStrands;

static const unsigned int STRAND_INDEX_NONE = 0xFFFFFFFF;

struct Strands *BKE_strands_new(void);
struct Strands *BKE_strands_copy(struct Strands *strands);
void BKE_strands_free(struct Strands *strands);

void BKE_strands_test_init(struct Strands *strands, struct DerivedMesh *scalp,
                           int totcurves, int maxverts,
                           unsigned int seed);

struct StrandInfo *BKE_strands_scatter(struct DerivedMesh *scalp, unsigned int amount,
                                       const StrandCurve *controls, unsigned int num_controls,
                                       unsigned int seed);

#if 0
typedef struct StrandCurveParams {
	struct DerivedMesh *scalp;
	unsigned int max_verts;
} StrandCurveParams;

struct StrandData *BKE_strand_data_interpolate(struct StrandInfo *strands, unsigned int num_strands,
                                               const StrandCurve *controls, struct StrandCurveParams *params);
void BKE_strand_data_free(struct StrandData *data);
#endif

#endif

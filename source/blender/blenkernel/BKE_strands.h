/*
 * Copyright 2015, Blender Foundation.
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
 */

#ifndef __BKE_STRANDS_H__
#define __BKE_STRANDS_H__

#include "BLI_utildefines.h"

typedef struct StrandsVertex {
	float co[3];
	float time;
	float weight;
	
	/* utility data */
	float nor[3]; /* normals (edge directions) */
} StrandsVertex;

typedef struct StrandsMotionState {
	float co[3];
	float vel[3];
	
	/* utility data */
	float nor[3]; /* normals (edge directions) */
} StrandsMotionState;

typedef struct StrandsCurve {
	int numverts;
} StrandsCurve;

typedef struct Strands {
	StrandsCurve *curves;
	StrandsVertex *verts;
	int totcurves, totverts;
	
	/* optional */
	StrandsMotionState *state;
} Strands;

struct Strands *BKE_strands_new(int strands, int verts);
void BKE_strands_free(struct Strands *strands);

void BKE_strands_add_motion_state(struct Strands *strands);
void BKE_strands_state_copy_rest_positions(Strands *strands);
void BKE_strands_state_copy_root_positions(Strands *strands);
void BKE_strands_state_clear_velocities(Strands *strands);

void BKE_strands_ensure_normals(struct Strands *strands);

/* ------------------------------------------------------------------------- */

typedef struct StrandIterator {
	int index, tot;
	StrandsCurve *curve;
	StrandsVertex *verts;
	StrandsMotionState *state;
} StrandIterator;

BLI_INLINE void BKE_strand_iter_init(StrandIterator *iter, Strands *strands)
{
	iter->tot = strands->totcurves;
	iter->index = 0;
	iter->curve = strands->curves;
	iter->verts = strands->verts;
	iter->state = strands->state;
}

BLI_INLINE bool BKE_strand_iter_valid(StrandIterator *iter)
{
	return iter->index < iter->tot;
}

BLI_INLINE void BKE_strand_iter_next(StrandIterator *iter)
{
	const int numverts = iter->curve->numverts;
	
	++iter->index;
	++iter->curve;
	iter->verts += numverts;
	if (iter->state)
		iter->state += numverts;
}


typedef struct StrandVertexIterator {
	int index, tot;
	StrandsVertex *vertex;
	StrandsMotionState *state;
} StrandVertexIterator;

BLI_INLINE void BKE_strand_vertex_iter_init(StrandVertexIterator *iter, StrandIterator *strand_iter)
{
	iter->tot = strand_iter->curve->numverts;
	iter->index = 0;
	iter->vertex = strand_iter->verts;
	iter->state = strand_iter->state;
}

BLI_INLINE bool BKE_strand_vertex_iter_valid(StrandVertexIterator *iter)
{
	return iter->index < iter->tot;
}

BLI_INLINE void BKE_strand_vertex_iter_next(StrandVertexIterator *iter)
{
	++iter->vertex;
	if (iter->state)
		++iter->state;
	++iter->index;
}

#endif  /* __BKE_STRANDS_H__ */

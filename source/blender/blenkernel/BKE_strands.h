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
void BKE_strands_remove_motion_state(struct Strands *strands);
void BKE_strands_state_copy_rest_positions(struct Strands *strands);
void BKE_strands_state_clear_velocities(struct Strands *strands);

void BKE_strands_ensure_normals(struct Strands *strands);

void BKE_strands_get_minmax(struct Strands *strands, float min[3], float max[3], bool use_motion_state);

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

BLI_INLINE size_t BKE_strand_iter_curve_offset(Strands *strands, StrandIterator *iter)
{
	return iter->curve - strands->curves;
}

BLI_INLINE size_t BKE_strand_iter_vertex_offset(Strands *strands, StrandIterator *iter)
{
	return iter->verts - strands->verts;
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

BLI_INLINE size_t BKE_strand_vertex_iter_vertex_offset(Strands *strands, StrandVertexIterator *iter)
{
	return iter->vertex - strands->verts;
}


typedef struct StrandEdgeIterator {
	int index, tot;
	StrandsVertex *vertex0, *vertex1;
	StrandsMotionState *state0, *state1;
} StrandEdgeIterator;

BLI_INLINE void BKE_strand_edge_iter_init(StrandEdgeIterator *iter, StrandIterator *strand_iter)
{
	iter->tot = strand_iter->curve->numverts - 1;
	iter->index = 0;
	iter->vertex0 = strand_iter->verts;
	iter->state0 = strand_iter->state;
	iter->vertex1 = strand_iter->verts + 1;
	iter->state1 = strand_iter->state + 1;
}

BLI_INLINE bool BKE_strand_edge_iter_valid(StrandEdgeIterator *iter)
{
	return iter->index < iter->tot;
}

BLI_INLINE void BKE_strand_edge_iter_next(StrandEdgeIterator *iter)
{
	++iter->vertex0;
	++iter->vertex1;
	if (iter->state0) {
		++iter->state0;
		++iter->state1;
	}
	++iter->index;
}

BLI_INLINE size_t BKE_strand_edge_iter_vertex0_offset(Strands *strands, StrandEdgeIterator *iter)
{
	return iter->vertex0 - strands->verts;
}

BLI_INLINE size_t BKE_strand_edge_iter_vertex1_offset(Strands *strands, StrandEdgeIterator *iter)
{
	return iter->vertex1 - strands->verts;
}

#endif  /* __BKE_STRANDS_H__ */

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

#include "DNA_strands_types.h"

struct Strands *BKE_strands_new(int strands, int verts);
void BKE_strands_free(struct Strands *strands);

void BKE_strands_add_motion_state(struct Strands *strands);
void BKE_strands_remove_motion_state(struct Strands *strands);
void BKE_strands_state_copy_rest_positions(struct Strands *strands);
void BKE_strands_state_clear_velocities(struct Strands *strands);

void BKE_strands_ensure_normals(struct Strands *strands);

void BKE_strands_get_minmax(struct Strands *strands, float min[3], float max[3], bool use_motion_state);


struct StrandsChildren *BKE_strands_children_new(int strands, int verts);
void BKE_strands_children_free(struct StrandsChildren *strands);

void BKE_strands_children_deform(struct StrandsChildren *strands, struct Strands *parents, bool use_motion);

void BKE_strands_children_ensure_normals(struct StrandsChildren *strands);

void BKE_strands_children_get_minmax(struct StrandsChildren *strands, float min[3], float max[3]);

/* ------------------------------------------------------------------------- */
/* Strand Curves Iterator */

typedef struct StrandIterator {
	int index, tot;
	struct StrandsCurve *curve;
	struct StrandsVertex *verts;
	struct StrandsMotionState *state;
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

/* ------------------------------------------------------------------------- */
/* Strand Vertices Iterator */

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

/* ------------------------------------------------------------------------- */
/* Strand Edges Iterator */

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

/* ------------------------------------------------------------------------- */
/* Strand Bends Iterator */

typedef struct StrandBendIterator {
	int index, tot;
	StrandsVertex *vertex0, *vertex1, *vertex2;
	StrandsMotionState *state0, *state1, *state2;
} StrandBendIterator;

BLI_INLINE void BKE_strand_bend_iter_init(StrandBendIterator *iter, StrandIterator *strand_iter)
{
	iter->tot = strand_iter->curve->numverts - 2;
	iter->index = 0;
	iter->vertex0 = strand_iter->verts;
	iter->state0 = strand_iter->state;
	iter->vertex1 = strand_iter->verts + 1;
	iter->state1 = strand_iter->state + 1;
	iter->vertex2 = strand_iter->verts + 2;
	iter->state2 = strand_iter->state + 2;
}

BLI_INLINE bool BKE_strand_bend_iter_valid(StrandBendIterator *iter)
{
	return iter->index < iter->tot;
}

BLI_INLINE void BKE_strand_bend_iter_next(StrandBendIterator *iter)
{
	++iter->vertex0;
	++iter->vertex1;
	++iter->vertex2;
	if (iter->state0) {
		++iter->state0;
		++iter->state1;
		++iter->state2;
	}
	++iter->index;
}

BLI_INLINE size_t BKE_strand_bend_iter_vertex0_offset(Strands *strands, StrandBendIterator *iter)
{
	return iter->vertex0 - strands->verts;
}

BLI_INLINE size_t BKE_strand_bend_iter_vertex1_offset(Strands *strands, StrandBendIterator *iter)
{
	return iter->vertex1 - strands->verts;
}

BLI_INLINE size_t BKE_strand_bend_iter_vertex2_offset(Strands *strands, StrandBendIterator *iter)
{
	return iter->vertex2 - strands->verts;
}

void BKE_strand_bend_iter_transform_rest(StrandBendIterator *iter, float mat[3][3]);
void BKE_strand_bend_iter_transform_state(StrandBendIterator *iter, float mat[3][3]);

/* ------------------------------------------------------------------------- */
/* Strand Child Curves Iterator */

typedef struct StrandChildIterator {
	int index, tot;
	StrandsChildCurve *curve;
	StrandsChildVertex *verts;
} StrandChildIterator;

BLI_INLINE void BKE_strand_child_iter_init(StrandChildIterator *iter, StrandsChildren *strands)
{
	iter->tot = strands->totcurves;
	iter->index = 0;
	iter->curve = strands->curves;
	iter->verts = strands->verts;
}

BLI_INLINE bool BKE_strand_child_iter_valid(StrandChildIterator *iter)
{
	return iter->index < iter->tot;
}

BLI_INLINE void BKE_strand_child_iter_next(StrandChildIterator *iter)
{
	const int numverts = iter->curve->numverts;
	
	++iter->index;
	++iter->curve;
	iter->verts += numverts;
}

BLI_INLINE size_t BKE_strand_child_iter_curve_offset(StrandsChildren *strands, StrandChildIterator *iter)
{
	return iter->curve - strands->curves;
}

BLI_INLINE size_t BKE_strand_child_iter_vertex_offset(StrandsChildren *strands, StrandChildIterator *iter)
{
	return iter->verts - strands->verts;
}

/* ------------------------------------------------------------------------- */
/* Strand Child Vertices Iterator */

typedef struct StrandChildVertexIterator {
	int index, tot;
	StrandsChildVertex *vertex;
} StrandChildVertexIterator;

BLI_INLINE void BKE_strand_child_vertex_iter_init(StrandChildVertexIterator *iter, StrandChildIterator *strand_iter)
{
	iter->tot = strand_iter->curve->numverts;
	iter->index = 0;
	iter->vertex = strand_iter->verts;
}

BLI_INLINE bool BKE_strand_child_vertex_iter_valid(StrandChildVertexIterator *iter)
{
	return iter->index < iter->tot;
}

BLI_INLINE void BKE_strand_child_vertex_iter_next(StrandChildVertexIterator *iter)
{
	++iter->vertex;
	++iter->index;
}

BLI_INLINE size_t BKE_strand_child_vertex_iter_vertex_offset(StrandsChildren *strands, StrandChildVertexIterator *iter)
{
	return iter->vertex - strands->verts;
}

/* ------------------------------------------------------------------------- */
/* Strand Child Edges Iterator */

typedef struct StrandChildEdgeIterator {
	int index, tot;
	StrandsChildVertex *vertex0, *vertex1;
} StrandChildEdgeIterator;

BLI_INLINE void BKE_strand_child_edge_iter_init(StrandChildEdgeIterator *iter, StrandChildIterator *strand_iter)
{
	iter->tot = strand_iter->curve->numverts - 1;
	iter->index = 0;
	iter->vertex0 = strand_iter->verts;
	iter->vertex1 = strand_iter->verts + 1;
}

BLI_INLINE bool BKE_strand_child_edge_iter_valid(StrandChildEdgeIterator *iter)
{
	return iter->index < iter->tot;
}

BLI_INLINE void BKE_strand_child_edge_iter_next(StrandChildEdgeIterator *iter)
{
	++iter->vertex0;
	++iter->vertex1;
	++iter->index;
}

BLI_INLINE size_t BKE_strand_child_edge_iter_vertex0_offset(StrandsChildren *strands, StrandChildEdgeIterator *iter)
{
	return iter->vertex0 - strands->verts;
}

BLI_INLINE size_t BKE_strand_child_edge_iter_vertex1_offset(StrandsChildren *strands, StrandChildEdgeIterator *iter)
{
	return iter->vertex1 - strands->verts;
}

#endif  /* __BKE_STRANDS_H__ */

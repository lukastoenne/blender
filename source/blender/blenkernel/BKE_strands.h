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
} StrandsVertex;

typedef struct StrandsCurve {
	int numverts;
} StrandsCurve;

typedef struct Strands {
	StrandsCurve *curves;
	StrandsVertex *verts;
	int totcurves, totverts;
} Strands;

struct Strands *BKE_strands_new(int strands, int verts);
void BKE_strands_free(struct Strands *strands);

/* ------------------------------------------------------------------------- */

typedef struct StrandIterator {
	int index, tot;
	StrandsCurve *curve;
	StrandsVertex *verts;
} StrandIterator;

BLI_INLINE void BKE_strand_iter_init(StrandIterator *iter, Strands *strands)
{
	iter->tot = strands->totcurves;
	iter->index = 0;
	iter->curve = strands->curves;
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
}


typedef struct StrandVertexIterator {
	int index, tot;
	StrandsVertex *vertex;
} StrandVertexIterator;

BLI_INLINE void BKE_strand_vertex_iter_init(StrandVertexIterator *iter, StrandIterator *strand_iter)
{
	iter->tot = strand_iter->curve->numverts;
	iter->index = 0;
	iter->vertex = strand_iter->verts;
}

BLI_INLINE bool BKE_strand_vertex_iter_valid(StrandVertexIterator *iter)
{
	return iter->index < iter->tot;
}

BLI_INLINE void BKE_strand_vertex_iter_next(StrandVertexIterator *iter)
{
	++iter->vertex;
	++iter->index;
}

#endif  /* __BKE_STRANDS_H__ */

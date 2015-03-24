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

#define STRANDS_BLOCK_SIZE                          (1 << 14) /* 16384 */
#define STRANDS_INDEX_TO_BLOCK(i)                   ((i) >> 14)
#define STRANDS_INDEX_TO_BLOCK_OFFSET(i)            ((i) - STRANDS_INDEX_TO_BLOCK((i)))
#define STRANDS_BLOCK_TO_INDEX(block, offset)       ((block) * STRANDS_BLOCK_SIZE + (offset))

typedef struct StrandsVertexBlock {
	float co[STRANDS_BLOCK_SIZE][3];
	float vel[STRANDS_BLOCK_SIZE][3];
	float rot[STRANDS_BLOCK_SIZE][4];
	float col[STRANDS_BLOCK_SIZE][3];
	float time[STRANDS_BLOCK_SIZE];
} StrandsVertexBlock;

typedef struct StrandsBlock {
	int numverts[STRANDS_BLOCK_SIZE];
	int parent[STRANDS_BLOCK_SIZE];
} StrandsBlock;

typedef struct Strands {
	StrandsBlock *strands;
	StrandsVertexBlock *verts;
	int totstrands, totverts;
} Strands;

struct Strands *BKE_strands_new(int strands, int verts);
void BKE_strands_free(struct Strands *strands);

/* ------------------------------------------------------------------------- */

#if 0
typedef struct StrandsIterator {
	int totstrands; /* total number of strands to loop over */
	int strand_index, strand_block; /* index of current strand and index in the block */
	int vertex_start, vertex_block_next;
	
	int *numverts, *parent;
} StrandsIterator;

BLI_INLINE void BKE_strands_iter_init(StrandsIterator *iter, Strands *strands)
{
	iter->strand_index = 0;
	iter->strand_block = 0;
	iter->totstrands = strands->totstrands;
	
	iter->vertex_start = 0;
	iter->vertex_block_next = 0;
	
	iter->numverts = strands->strands->numverts;
	iter->parent = strands->strands->parent;
}

BLI_INLINE bool BKE_strands_iter_valid(StrandsIterator *iter)
{
	return (iter->strand_index < iter->totstrands);
}

BLI_INLINE void BKE_strand_iter_next(StrandsIterator *iter)
{
	int numverts = *iter->numverts;
	
	++iter->strand_index;
	++iter->strand_block;
	if (iter->strand_block > STRANDS_BLOCK_SIZE)
		iter->strand_block -= STRANDS_BLOCK_SIZE;
	
	iter->vertex_start += numverts;
	iter->vertex_block_next += numverts;
	if (iter->vertex_block_next > STRANDS_BLOCK_SIZE)
		iter->vertex_block_next = 0;
}
#endif


#if 0
typedef struct StrandsIterator {
	int strand_index, vertex_index;
	int strand_block, vertex_block;
	int totstrands, totverts;
	float *co[3], *vel[3], *rot[4], *col[3], *time;
} StrandsIterator;

BLI_INLINE void BKE_strands_iter_init(StrandsIterator *iter, Strands *strands)
{
	iter->strand_index = 0;
	iter->strand_block = 0;
	iter->totstrands = strands->totstrands;
	iter->totverts = strands->totverts;
	iter->vertex_index = 0;
	iter->vertex_block = 0;
	iter->co = strands->verts->co;
	iter->vel = strands->verts->vel;
	iter->rot = strands->verts->rot;
	iter->col = strands->verts->col;
	iter->time = strands->verts->time;
}

BLI_INLINE bool BKE_strands_iter_valid(StrandsIterator *iter)
{
	return (iter->strand_index < iter->totstrands) && (iter->vertex_index < iter->totverts);
}

BLI_INLINE void BKE_strand_iter_next(StrandsIterator *iter)
{
	++iter->vertex_index;
	++iter->vertex_block;
}
#endif

#endif  /* __BKE_STRANDS_H__ */

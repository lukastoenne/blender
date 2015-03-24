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

#include "MEM_guardedalloc.h"

#include "BKE_strands.h"

Strands *BKE_strands_new(int strands, int verts)
{
	int num_strand_blocks = STRANDS_INDEX_TO_BLOCK(strands);
	int num_vertex_blocks = STRANDS_INDEX_TO_BLOCK(verts);
	
	Strands *s = MEM_mallocN(sizeof(Strands), "strands");
	
	s->totstrands = strands;
	s->strands = MEM_mallocN(sizeof(StrandsBlock) * num_strand_blocks, "strand strands");
	
	s->totverts = verts;
	s->verts = MEM_mallocN(sizeof(StrandsVertexBlock) * num_vertex_blocks, "strand vertices");
	
	return s;
}

void BKE_strands_free(Strands *strands)
{
	if (strands) {
		if (strands->strands)
			MEM_freeN(strands->strands);
		if (strands->verts)
			MEM_freeN(strands->verts);
		MEM_freeN(strands);
	}
}

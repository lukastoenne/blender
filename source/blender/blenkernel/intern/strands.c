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

#include "BLI_math.h"

#include "BKE_strands.h"

Strands *BKE_strands_new(int curves, int verts)
{
	Strands *s = MEM_mallocN(sizeof(Strands), "strands");
	
	s->totcurves = curves;
	s->curves = MEM_mallocN(sizeof(StrandsCurve) * curves, "strand curves");
	
	s->totverts = verts;
	s->verts = MEM_mallocN(sizeof(StrandsVertex) * verts, "strand vertices");
	
	/* must be added explicitly */
	s->state = NULL;
	
	return s;
}

void BKE_strands_free(Strands *strands)
{
	if (strands) {
		if (strands->curves)
			MEM_freeN(strands->curves);
		if (strands->verts)
			MEM_freeN(strands->verts);
		MEM_freeN(strands);
	}
}

void BKE_strands_add_motion_state(Strands *strands)
{
	if (!strands->state) {
		strands->state = MEM_mallocN(sizeof(StrandsMotionState) * strands->totverts, "strand motion states");
	}
}

static void calc_normals(Strands *strands, bool use_motion_state)
{
	StrandIterator it_strand;
	for (BKE_strand_iter_init(&it_strand, strands); BKE_strand_iter_valid(&it_strand); BKE_strand_iter_next(&it_strand)) {
		StrandVertexIterator it_vert;
		BKE_strand_vertex_iter_init(&it_vert, &it_strand);
		
		if (BKE_strand_vertex_iter_valid(&it_vert)) {
			const float *co_prev = use_motion_state ? it_vert.state->co : it_vert.vertex->co;
			
			BKE_strand_vertex_iter_next(&it_vert);
			
			for (; BKE_strand_vertex_iter_valid(&it_vert); BKE_strand_vertex_iter_next(&it_vert)) {
				const float *co = use_motion_state ? it_vert.state->co : it_vert.vertex->co;
				float *nor = use_motion_state ? it_vert.state->nor : it_vert.vertex->nor;
				
				sub_v3_v3v3(nor, co, co_prev);
				normalize_v3(nor);
			}
		}
	}
}

void BKE_strands_ensure_normals(Strands *strands)
{
	const bool use_motion_state = (strands->state);
	
	calc_normals(strands, false);
	
	if (use_motion_state)
		calc_normals(strands, true);
}

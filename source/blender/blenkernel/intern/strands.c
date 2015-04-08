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
#include "BLI_string.h"

#include "BKE_strands.h"

Strands *BKE_strands_new(int curves, int verts)
{
	Strands *strands = MEM_mallocN(sizeof(Strands), "strands");
	
	strands->totcurves = curves;
	strands->curves = MEM_mallocN(sizeof(StrandsCurve) * curves, "strand curves");
	
	strands->totverts = verts;
	strands->verts = MEM_mallocN(sizeof(StrandsVertex) * verts, "strand vertices");
	
	/* must be added explicitly */
	strands->state = NULL;
	
	return strands;
}

void BKE_strands_free(Strands *strands)
{
	if (strands) {
		if (strands->curves)
			MEM_freeN(strands->curves);
		if (strands->verts)
			MEM_freeN(strands->verts);
		if (strands->state)
			MEM_freeN(strands->state);
		MEM_freeN(strands);
	}
}

/* copy the rest positions to initialize the motion state */
void BKE_strands_state_copy_rest_positions(Strands *strands)
{
	if (strands->state) {
		int i;
		for (i = 0; i < strands->totverts; ++i) {
			copy_v3_v3(strands->state[i].co, strands->verts[i].co);
		}
	}
}

/* copy the rest positions to initialize the motion state */
void BKE_strands_state_clear_velocities(Strands *strands)
{
	if (strands->state) {
		int i;
		
		for (i = 0; i < strands->totverts; ++i) {
			zero_v3(strands->state[i].vel);
		}
	}
}

void BKE_strands_add_motion_state(Strands *strands)
{
	if (!strands->state) {
		int i;
		
		strands->state = MEM_mallocN(sizeof(StrandsMotionState) * strands->totverts, "strand motion states");
		
		BKE_strands_state_copy_rest_positions(strands);
		BKE_strands_state_clear_velocities(strands);
		
		/* initialize normals */
		for (i = 0; i < strands->totverts; ++i) {
			copy_v3_v3(strands->state[i].nor, strands->verts[i].nor);
		}
	}
}

void BKE_strands_remove_motion_state(Strands *strands)
{
	if (strands) {
		if (strands->state) {
			MEM_freeN(strands->state);
			strands->state = NULL;
		}
	}
}

static void calc_normals(Strands *strands, bool use_motion_state)
{
	StrandIterator it_strand;
	for (BKE_strand_iter_init(&it_strand, strands); BKE_strand_iter_valid(&it_strand); BKE_strand_iter_next(&it_strand)) {
		StrandEdgeIterator it_edge;
		if (use_motion_state) {
			for (BKE_strand_edge_iter_init(&it_edge, &it_strand); BKE_strand_edge_iter_valid(&it_edge); BKE_strand_edge_iter_next(&it_edge)) {
				sub_v3_v3v3(it_edge.state0->nor, it_edge.state1->co, it_edge.state0->co);
				normalize_v3(it_edge.state0->nor);
			}
			if (it_strand.tot > 0) {
				copy_v3_v3(it_edge.state0->nor, (it_edge.state0-1)->nor);
			}
		}
		else {
			for (BKE_strand_edge_iter_init(&it_edge, &it_strand); BKE_strand_edge_iter_valid(&it_edge); BKE_strand_edge_iter_next(&it_edge)) {
				sub_v3_v3v3(it_edge.vertex0->nor, it_edge.vertex1->co, it_edge.vertex0->co);
				normalize_v3(it_edge.vertex0->nor);
			}
			if (it_strand.tot > 0) {
				copy_v3_v3(it_edge.vertex0->nor, (it_edge.vertex0-1)->nor);
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

void BKE_strands_get_minmax(Strands *strands, float min[3], float max[3], bool use_motion_state)
{
	int numverts = strands->totverts;
	int i;
	
	if (use_motion_state && strands->state) {
		for (i = 0; i < numverts; ++i) {
			minmax_v3v3_v3(min, max, strands->state[i].co);
		}
	}
	else {
		for (i = 0; i < numverts; ++i) {
			minmax_v3v3_v3(min, max, strands->verts[i].co);
		}
	}
}

/* ------------------------------------------------------------------------- */

StrandsChildren *BKE_strands_children_new(int curves, int verts)
{
	StrandsChildren *strands = MEM_mallocN(sizeof(StrandsChildren), "strands children");
	
	strands->totcurves = curves;
	strands->curves = MEM_mallocN(sizeof(StrandsChildCurve) * curves, "strand children curves");
	
	strands->totverts = verts;
	strands->verts = MEM_mallocN(sizeof(StrandsChildVertex) * verts, "strand children vertices");
	
	return strands;
}

void BKE_strands_children_free(StrandsChildren *strands)
{
	if (strands) {
		if (strands->curves)
			MEM_freeN(strands->curves);
		if (strands->verts)
			MEM_freeN(strands->verts);
		MEM_freeN(strands);
	}
}

static void calc_child_normals(StrandsChildren *strands)
{
	StrandChildIterator it_strand;
	for (BKE_strand_child_iter_init(&it_strand, strands); BKE_strand_child_iter_valid(&it_strand); BKE_strand_child_iter_next(&it_strand)) {
		StrandChildEdgeIterator it_edge;
		for (BKE_strand_child_edge_iter_init(&it_edge, &it_strand); BKE_strand_child_edge_iter_valid(&it_edge); BKE_strand_child_edge_iter_next(&it_edge)) {
			sub_v3_v3v3(it_edge.vertex0->nor, it_edge.vertex1->co, it_edge.vertex0->co);
			normalize_v3(it_edge.vertex0->nor);
		}
		if (it_strand.tot > 0) {
			copy_v3_v3(it_edge.vertex0->nor, (it_edge.vertex0-1)->nor);
		}
	}
}

void BKE_strands_children_ensure_normals(StrandsChildren *strands)
{
	calc_child_normals(strands);
}

void BKE_strands_children_get_minmax(StrandsChildren *strands, float min[3], float max[3])
{
	int numverts = strands->totverts;
	int i;
	
	for (i = 0; i < numverts; ++i) {
		minmax_v3v3_v3(min, max, strands->verts[i].co);
	}
}

/* ------------------------------------------------------------------------- */

void BKE_strand_bend_iter_transform_rest(StrandBendIterator *iter, float mat[3][3])
{
	float dir0[3], dir1[3];
	
	sub_v3_v3v3(dir0, iter->vertex1->co, iter->vertex0->co);
	sub_v3_v3v3(dir1, iter->vertex2->co, iter->vertex1->co);
	normalize_v3(dir0);
	normalize_v3(dir1);
	
	/* rotation between segments */
	rotation_between_vecs_to_mat3(mat, dir0, dir1);
}

void BKE_strand_bend_iter_transform_state(StrandBendIterator *iter, float mat[3][3])
{
	if (iter->state0) {
		float dir0[3], dir1[3];
		
		sub_v3_v3v3(dir0, iter->state1->co, iter->state0->co);
		sub_v3_v3v3(dir1, iter->state2->co, iter->state1->co);
		normalize_v3(dir0);
		normalize_v3(dir1);
		
		/* rotation between segments */
		rotation_between_vecs_to_mat3(mat, dir0, dir1);
	}
	else
		unit_m3(mat);
}

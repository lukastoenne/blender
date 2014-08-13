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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/hair.c
 *  \ingroup bke
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "DNA_hair_types.h"

#include "BKE_hair.h"
#include "BKE_mesh_sample.h"

#include "HAIR_capi.h"

HairSystem *BKE_hairsys_new(void)
{
	HairSystem *hsys = MEM_callocN(sizeof(HairSystem), "hair system");
	
	hsys->params.substeps_forces = 30;
	hsys->params.substeps_damping = 10;
	hsys->params.stretch_stiffness = 2000.0f;
	hsys->params.stretch_damping = 10.0f;
	hsys->params.bend_stiffness = 40.0f;
	hsys->params.bend_damping = 10.0f;
	
	hsys->params.num_render_hairs = 100;
	hsys->params.curl_smoothing = 1.0f;
	
	return hsys;
}

void BKE_hairsys_free(HairSystem *hsys)
{
	int totcurves = hsys->totcurves, i;
	if (hsys->curves) {
		for (i = 0; i < totcurves; ++i)
			MEM_freeN(hsys->curves[i].points);
		MEM_freeN(hsys->curves);
	}
	
	if (hsys->display.drawdata)
		MEM_freeN(hsys->display.drawdata);
	
	MEM_freeN(hsys);
}

HairSystem *BKE_hairsys_copy(HairSystem *hsys)
{
	int totcurves = hsys->totcurves, i;
	HairSystem *thsys = MEM_dupallocN(hsys);
	
	thsys->curves = MEM_dupallocN(hsys->curves);
	for (i = 0; i < totcurves; ++i)
		thsys->curves[i].points = MEM_dupallocN(hsys->curves[i].points);
	
	thsys->display.drawdata = NULL;
	
	return thsys;
}


HairCurve *BKE_hair_curve_add(HairSystem *hsys)
{
	return BKE_hair_curve_add_multi(hsys, 1);
}

HairCurve *BKE_hair_curve_add_multi(HairSystem *hsys, int num)
{
	if (num <= 0)
		return NULL;
	
	hsys->totcurves += num;
	hsys->curves = MEM_recallocN_id(hsys->curves, sizeof(HairCurve) * hsys->totcurves, "hair system curve data");
	
	return &hsys->curves[hsys->totcurves-num];
}

void BKE_hair_curve_remove(HairSystem *hsys, HairCurve *hair)
{
	HairCurve *ncurves;
	int pos, ntotcurves;
	
	pos = (int)(hair - hsys->curves);
	BLI_assert(pos >= 0 && pos < hsys->totcurves);
	
	ntotcurves = hsys->totcurves - 1;
	ncurves = ntotcurves > 0 ? MEM_mallocN(sizeof(HairCurve) * ntotcurves, "hair system curve data") : NULL;
	
	if (pos >= 1) {
		memcpy(ncurves, hsys->curves, sizeof(HairCurve) * pos);
	}
	if (pos < hsys->totcurves - 1) {
		memcpy(ncurves + pos, hsys->curves + (pos + 1), sizeof(HairCurve) * (hsys->totcurves - (pos + 1)));
	}
	
	MEM_freeN(hair->points);
	MEM_freeN(hsys->curves);
	hsys->curves = ncurves;
	hsys->totcurves = ntotcurves;
}

HairPoint *BKE_hair_point_append(HairSystem *hsys, HairCurve *hair)
{
	return BKE_hair_point_append_multi(hsys, hair, 1);
}

HairPoint *BKE_hair_point_append_multi(HairSystem *UNUSED(hsys), HairCurve *hair, int num)
{
	if (num <= 0)
		return NULL;
	
	hair->totpoints += num;
	hair->points = MEM_recallocN_id(hair->points, sizeof(HairPoint) * hair->totpoints, "hair point data");
	
	return &hair->points[hair->totpoints-num];
}

HairPoint *BKE_hair_point_insert(HairSystem *hsys, HairCurve *hair, int pos)
{
	return BKE_hair_point_insert_multi(hsys, hair, pos, 1);
}

HairPoint *BKE_hair_point_insert_multi(HairSystem *UNUSED(hsys), HairCurve *hair, int pos, int num)
{
	HairPoint *npoints;
	int ntotpoints;
	
	if (num <= 0)
		return NULL;
	
	ntotpoints = hair->totpoints + num;
	npoints = ntotpoints > 0 ? MEM_callocN(sizeof(HairPoint) * ntotpoints, "hair point data") : NULL;
	
	CLAMP(pos, 0, ntotpoints-1);
	if (pos >= 1) {
		memcpy(npoints, hair->points, sizeof(HairPoint) * pos);
	}
	if (pos < hair->totpoints - num) {
		memcpy(npoints + (pos + num), hair->points + pos, hair->totpoints - pos);
	}
	
	MEM_freeN(hair->points);
	hair->points = npoints;
	hair->totpoints = ntotpoints;
	
	return &hair->points[pos];
}

void BKE_hair_point_remove(HairSystem *hsys, HairCurve *hair, HairPoint *point)
{
	BKE_hair_point_remove_position(hsys, hair, (int)(point - hair->points));
}

void BKE_hair_point_remove_position(HairSystem *UNUSED(hsys), HairCurve *hair, int pos)
{
	HairPoint *npoints;
	int ntotpoints;
	
	BLI_assert(pos >= 0 && pos < hair->totpoints);
	
	ntotpoints = hair->totpoints - 1;
	npoints = ntotpoints > 0 ? MEM_mallocN(sizeof(HairPoint) * ntotpoints, "hair point data") : NULL;
	
	if (pos >= 1) {
		memcpy(npoints, hair->points, sizeof(HairPoint) * pos);
	}
	if (pos < hair->totpoints - 1) {
		memcpy(npoints + pos, hair->points + (pos + 1), hair->totpoints - (pos + 1));
	}
	
	MEM_freeN(hair->points);
	hair->points = npoints;
	hair->totpoints = ntotpoints;
}

void BKE_hair_calculate_rest(HairSystem *hsys)
{
	HairCurve *hair;
	int i;
	
	for (i = 0, hair = hsys->curves; i < hsys->totcurves; ++i, ++hair) {
		HairPoint *point;
		int k;
		float tot_rest_length;
		
		tot_rest_length = 0.0f;
		for (k = 1, point = hair->points + 1; k < hair->totpoints; ++k, ++point) {
			tot_rest_length += len_v3v3((point-1)->rest_co, point->rest_co);
		}
		if (hair->totpoints > 1)
			hair->avg_rest_length = tot_rest_length / (float)(hair->totpoints-1);
	}
}

void BKE_hair_debug_data_free(HairDebugData *debug_data)
{
	if (debug_data) {
		if (debug_data->points)
			MEM_freeN(debug_data->points);
		if (debug_data->contacts)
			MEM_freeN(debug_data->contacts);
		MEM_freeN(debug_data);
	}
}


/* ================ Render ================ */

static int hair_maxpoints(HairSystem *hsys)
{
	HairCurve *hair;
	int i;
	int maxpoints = 0;
	for (i = 0, hair = hsys->curves; i < hsys->totcurves; ++i, ++hair) {
		if (hair->totpoints > maxpoints)
			maxpoints = hair->totpoints;
	}
	return maxpoints;
}

static HairRenderChildData *hair_gen_child_data(HairParams *params, unsigned int seed)
{
	int num_render_hairs = params->num_render_hairs;
	HairRenderChildData *hair, *data = MEM_mallocN(sizeof(HairRenderChildData) * num_render_hairs, "hair render data");
	RNG *rng;
	int i;
	
	rng = BLI_rng_new(seed);
	
	for (i = 0, hair = data; i < num_render_hairs; ++i, ++hair) {
		hair->u = BLI_rng_get_float(rng)*2.0f - 1.0f;
		hair->v = BLI_rng_get_float(rng)*2.0f - 1.0f;
	}
	
	BLI_rng_free(rng);
	
	return data;
}

static void get_hair_root_frame(HairCurve *hair, float frame[3][3])
{
	const float up[3] = {0.0f, 0.0f, 1.0f};
	float normal[3];
	
	if (hair->totpoints >= 2) {
		sub_v3_v3v3(normal, hair->points[1].co, hair->points[0].co);
		normalize_v3(normal);
		
		copy_v3_v3(frame[0], normal);
		madd_v3_v3v3fl(frame[1], up, normal, -dot_v3v3(up, normal));
		normalize_v3(frame[1]);
		cross_v3_v3v3(frame[2], frame[0], frame[1]);
	}
	else {
		unit_m3(frame);
	}
}

static void hair_precalc_cache(HairRenderIterator *iter)
{
	struct HAIR_FrameIterator *frame_iter = HAIR_frame_iter_new();
	HairPointRenderCache *cache = iter->hair_cache;
	float initial_frame[3][3];
	
	get_hair_root_frame(iter->hair, initial_frame);
	
	for (HAIR_frame_iter_init(frame_iter, iter->hair, iter->hair->avg_rest_length, iter->hsys->params.curl_smoothing, initial_frame);
	     HAIR_frame_iter_valid(frame_iter);
	     HAIR_frame_iter_next(frame_iter)) {
		int k = HAIR_frame_iter_index(frame_iter);
		
		HAIR_frame_iter_get(frame_iter, cache->nor, cache->tan, cache->cotan);
		
		/* for rendering, rotate frames half-way to the next segment */
		if (k > 0) {
			add_v3_v3((cache-1)->nor, cache->nor);
			mul_v3_fl((cache-1)->nor, 0.5f);
			normalize_v3((cache-1)->nor);
			
			add_v3_v3((cache-1)->tan, cache->tan);
			mul_v3_fl((cache-1)->tan, 0.5f);
			normalize_v3((cache-1)->tan);
			
			add_v3_v3((cache-1)->cotan, cache->cotan);
			mul_v3_fl((cache-1)->cotan, 0.5f);
			normalize_v3((cache-1)->cotan);
		}
		
		++cache;
	}
}

void BKE_hair_render_iter_init(HairRenderIterator *iter, HairSystem *hsys)
{
	int maxpoints = hair_maxpoints(hsys);
	
	iter->hsys = hsys;
	iter->steps_per_point = 1; // XXX TODO!
	iter->maxsteps = (maxpoints - 1) * iter->steps_per_point + 1;
	iter->hair_cache = MEM_mallocN(sizeof(HairPointRenderCache) * maxpoints, "hair render cache data");
	
	iter->maxchildren = hsys->params.num_render_hairs;
	iter->child_data = hair_gen_child_data(&hsys->params, 12345); /* TODO handle seeds properly here ... */
	
	iter->hair = hsys->curves;
	iter->i = 0;
	iter->totchildren = hsys->params.num_render_hairs;
	iter->child = 0;
}

void BKE_hair_render_iter_init_hair(HairRenderIterator *iter)
{
	iter->point = iter->hair->points;
	iter->k = 0;
	
	iter->totsteps = (iter->hair->totpoints - 1) * iter->steps_per_point + 1;
	iter->step = 0;
	
	/* actual new hair or just next child? */
	if (iter->child >= iter->totchildren) {
		iter->totchildren = iter->hsys->params.num_render_hairs; /* XXX in principle could differ per hair */
		iter->child = 0;
		
		/* fill the hair cache to avoid redundant per-child calculations */
		hair_precalc_cache(iter);
	}
}

void BKE_hair_render_iter_end(HairRenderIterator *iter)
{
	if (iter->hair_cache)
		MEM_freeN(iter->hair_cache);
	
	if (iter->child_data)
		MEM_freeN(iter->child_data);
}

bool BKE_hair_render_iter_valid_hair(HairRenderIterator *iter)
{
	return iter->i < iter->hsys->totcurves;
}

bool BKE_hair_render_iter_valid_step(HairRenderIterator *iter)
{
	return iter->step < iter->totsteps;
}

void BKE_hair_render_iter_next(HairRenderIterator *iter)
{
	++iter->step;
	
	if (iter->step >= iter->totsteps) {
		++iter->child;
		
		if (iter->child >= iter->totchildren) {
			++iter->hair;
			++iter->i;
		}
	}
	else if (iter->step % iter->steps_per_point == 0) {
		++iter->point;
		++iter->k;
	}
}

void BKE_hair_render_iter_get(HairRenderIterator *iter, float r_co[3], float *r_radius)
{
	HairPoint *pt0 = iter->point;
	float tan[3], cotan[3];
	float co[3];
	float radius;
	
	copy_v3_v3(co, pt0->co);
	radius = pt0->radius;
	
	copy_v3_v3(tan, iter->hair_cache[iter->k].tan);
	copy_v3_v3(cotan, iter->hair_cache[iter->k].cotan);
	
	if (iter->step < iter->totsteps - 1) {
		HairPoint *pt1 = pt0 + 1;
		int i = iter->step % iter->steps_per_point;
		float t = (float)i / (float)iter->steps_per_point;
		float mt = 1.0f - t;
		
		interp_v3_v3v3(co, co, pt1->co, t);
		radius = radius * mt + pt1->radius * t;
		
		interp_v3_v3v3(tan, tan, iter->hair_cache[iter->k + 1].tan, t);
		interp_v3_v3v3(cotan, cotan, iter->hair_cache[iter->k + 1].cotan, t);
	}
	
	/* child offset */
	{
		HairRenderChildData *child_data = iter->child_data + iter->child;
		
		madd_v3_v3fl(co, tan, child_data->u * radius);
		madd_v3_v3fl(co, cotan, child_data->v * radius);
	}
	
	if (r_co) copy_v3_v3(r_co, co);
	if (r_radius) *r_radius = radius;
}

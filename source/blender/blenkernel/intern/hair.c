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
#include "BLI_ghash.h"

#include "DNA_hair_types.h"
#include "DNA_object_force.h"

#include "BKE_effect.h"
#include "BKE_hair.h"
#include "BKE_mesh_sample.h"

void BKE_hair_params_init(HairParams *params)
{
	params->substeps = 5;
	params->stretch_stiffness = 2000.0f;
	params->stretch_damping = 10.0f;
	params->bend_stiffness = 40.0f;
	params->bend_damping = 10.0f;
	
	params->effector_weights = BKE_add_effector_weights(NULL);
}

void BKE_hair_params_free(HairParams *params)
{
	if (params->effector_weights) {
		MEM_freeN(params->effector_weights);
		params->effector_weights = NULL;
	}
}

void BKE_hair_params_copy(HairParams *to, HairParams *from)
{
	memcpy(to, from, sizeof(HairParams));
	
	if (from->effector_weights)
		to->effector_weights = MEM_dupallocN(from->effector_weights);
}

HairSystem *BKE_hair_system_new(void)
{
	HairSystem *hsys = MEM_callocN(sizeof(HairSystem), "hair system");
	HairParams *params = &hsys->params;
	HairRenderSettings *render = &params->render;
	
	BKE_hair_params_init(params);
	render->flag = HAIR_RENDER_CLOSE_TIP;
	render->num_render_hairs = 100;
	render->interpolation_steps = 4;
	
	render->curl_smoothing = 1.0f;
	render->radius_scale = 0.01f;
	render->root_width = 1.0f;
	render->tip_width = 0.0f;
	render->shape = 0.0f;
	
	return hsys;
}

void BKE_hair_system_free(HairSystem *hsys)
{
	BKE_hair_params_free(&hsys->params);
	
	BKE_hairsys_clear(hsys);
	MEM_freeN(hsys);
}

HairSystem *BKE_hair_system_copy(HairSystem *hsys)
{
	int totcurves = hsys->totcurves, i;
	HairSystem *thsys = MEM_dupallocN(hsys);
	
	thsys->curves = MEM_dupallocN(hsys->curves);
	for (i = 0; i < totcurves; ++i)
		thsys->curves[i].points = MEM_dupallocN(hsys->curves[i].points);
	
	BKE_hair_params_copy(&thsys->params, &hsys->params);
	
	thsys->render_iter = NULL;
	
	return thsys;
}

void BKE_hairsys_clear(HairSystem *hsys)
{
	int totcurves = hsys->totcurves, i;
	if (hsys->curves) {
		for (i = 0; i < totcurves; ++i)
			MEM_freeN(hsys->curves[i].points);
		MEM_freeN(hsys->curves);
		hsys->curves = NULL;
		hsys->totcurves = 0;
	}
	
	if (hsys->render_iter) {
		MEM_freeN(hsys->render_iter);
		hsys->render_iter = NULL;
	}
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

static void get_hair_frame(float frame[3][3], const float normal[3], const float tangent[3])
{
	copy_v3_v3(frame[2], normal);
	
	if (tangent) {
		copy_v3_v3(frame[0], tangent);
	}
	else {
		const float up[3] = {0.0f, 0.0f, 1.0f};
		
		madd_v3_v3v3fl(frame[0], up, normal, -dot_v3v3(up, normal));
		normalize_v3(frame[0]);
	}
	
	cross_v3_v3v3(frame[1], frame[2], frame[0]);
}

void BKE_hair_get_mesh_frame(struct DerivedMesh *dm, HairCurve *curve, float frame[3][3])
{
	float vloc[3], vnor[3];
	
	bool ok = BKE_mesh_sample_eval(dm, &curve->root, vloc, vnor);
	if (ok)
		get_hair_frame(frame, vnor, NULL);
	else
		unit_m3(frame);
}

void BKE_hair_calculate_rest(struct DerivedMesh *dm, HairCurve *curve)
{
	HairPoint *point;
	int k;
	float tot_rest_length;
	HairFrameIterator iter;
	float frame[3][3], rot[3][3];
	
	if (curve->totpoints < 2) {
		point = curve->points;
		for (k = 0; k < curve->totpoints; ++k, ++point) {
			point->rest_length = 0.0f;
			zero_v3(point->rest_target);
		}
		
		curve->avg_rest_length = 0.0f;
		zero_m3(curve->root_rest_frame);
		
		return;
	}
	
	tot_rest_length = 0.0f;
	point = curve->points;
	for (k = 0; k < curve->totpoints - 1; ++k, ++point) {
		point->rest_length = len_v3v3(point->rest_co, (point+1)->rest_co);
		tot_rest_length += point->rest_length;
	}
	curve->avg_rest_length = tot_rest_length / (float)(curve->totpoints-1);
	
	/* frame starts in root rest position
	 * note: not using obmat here, doesn't matter for rest calculation
	 */
	BKE_hair_get_mesh_frame(dm, curve, curve->root_rest_frame);
	copy_m3_m3(frame, curve->root_rest_frame);
	
	/* initialize frame iterator */
	BKE_hair_frame_init(&iter, frame[2]);
	
	point = curve->points;
	/* target is the edge vector in frame space */
	sub_v3_v3v3(point->rest_target, (point+1)->rest_co, point->rest_co);
	mul_transposed_m3_v3(frame, point->rest_target);
	
	++point;
	for (k = 1; k < curve->totpoints - 1; ++k, ++point) {
		/* transport the frame to the next segment */
		BKE_hair_frame_next_from_points(&iter, (point-1)->rest_co, point->rest_co, rot);
		mul_m3_m3m3(frame, rot, frame);
		
		/* target is the edge vector in frame space */
		sub_v3_v3v3(point->rest_target, (point+1)->rest_co, point->rest_co);
		mul_transposed_m3_v3(frame, point->rest_target);
	}
	
	/* last point */
	point->rest_length = 0.0f;
	zero_v3(point->rest_target);
}

/* ==== Hair Framing ==== */

void BKE_hair_frame_init(HairFrameIterator *iter, const float dir0[3])
{
	copy_v3_v3(iter->prev_dir, dir0);
	copy_v3_v3(iter->dir, dir0);
}

void BKE_hair_frame_init_from_points(HairFrameIterator *iter, const float x0[3], const float x1[3])
{
	float dir0[3];
	sub_v3_v3v3(dir0, x1, x0);
	normalize_v3(dir0);
	
	BKE_hair_frame_init(iter, dir0);
}

void BKE_hair_frame_next(HairFrameIterator *iter, const float dir[3], float rot[3][3])
{
	/* TODO implement optional smoothing function here, as described in "Artistic Simulation of Curly Hair" */
	
	/* rotation between segments */
	rotation_between_vecs_to_mat3(rot, iter->dir, dir);
	
	/* advance iterator state */
	copy_v3_v3(iter->prev_dir, iter->dir);
	copy_v3_v3(iter->dir, dir);
}

void BKE_hair_frame_next_from_points(HairFrameIterator *iter, const float x0[3], const float x1[3], float rot[3][3])
{
	float dir[3];
	sub_v3_v3v3(dir, x1, x0);
	normalize_v3(dir);
	
	BKE_hair_frame_next(iter, dir, rot);
}

/* ================ Render Hair Iterator ================ */

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
	int num_render_hairs = params->render.num_render_hairs;
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

static void hair_precalc_cache(HairRenderIterator *iter)
{
	HairRenderSettings *render = &iter->hsys->params.render;
	struct HAIR_FrameIterator *frame_iter;
	HairPointRenderCache *cache;
	float initial_frame[3][3];
	
	if (!BKE_hair_render_iter_valid_hair(iter))
		return;
	
#if 0 // TODO
	frame_iter = HAIR_frame_iter_new();
	get_hair_root_frame(iter->hair, initial_frame);
	
	cache = iter->hair_cache;
	for (HAIR_frame_iter_init(frame_iter, iter->hair, iter->hair->avg_rest_length, render->curl_smoothing, initial_frame);
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
#endif
}

void BKE_hair_render_iter_init(HairRenderIterator *iter, HairSystem *hsys)
{
	HairRenderSettings *render = &hsys->params.render;
	int maxpoints = hair_maxpoints(hsys);
	
	iter->hsys = hsys;
	iter->steps_per_point = render->interpolation_steps;
	iter->maxsteps = (maxpoints - 1) * iter->steps_per_point + 1;
	iter->hair_cache = MEM_mallocN(sizeof(HairPointRenderCache) * maxpoints, "hair render cache data");
	
	iter->maxchildren = render->num_render_hairs;
	iter->child_data = hair_gen_child_data(&hsys->params, 12345); /* TODO handle seeds properly here ... */
	
	iter->hair = hsys->curves;
	iter->i = 0;
	iter->totchildren = render->num_render_hairs;
	iter->child = 0;
	
	/* fill the hair cache to avoid redundant per-child calculations */
	hair_precalc_cache(iter);
}

BLI_INLINE void forward_diff_precalc(HairRenderIterator *iter, float t,
                                     float p0[3], float p1[3], float p2[3], float p3[3],
                                     int i)
{
	float t_sq = t * t;
	float fdd_per_2, fddd_per_2;
	
	/* hermite spline interpolation tangents */
#if 1
	/* Catmull-Rom tangents */
	float m1 = 0.5f * (p2[i] - p0[i]);
	float m2 = 0.5f * (p3[i] - p1[i]);
#else
	/* Cardinal tangents */
	float c = 0.5f; /* tension parameter */
	float m1 = c * 0.5f * (p2[i] - p0[i]);
	float m2 = c * 0.5f * (p3[i] - p1[i]);
#endif
	
	iter->f[i] = p1[i];
	iter->fd[i] = m1 * t;
	iter->fdd_per_2[i] = fdd_per_2 = (3.0f * (p2[i] - p1[i]) - 2.0f * m1 - m2) * t_sq;
	iter->fddd_per_2[i] = fddd_per_2 = 3.0f * (2.0f * (p1[i] - p2[i]) + m1 + m2) * t_sq * t;
	
	iter->fddd[i] = fddd_per_2 + fddd_per_2;
	iter->fdd[i] = fdd_per_2 + fdd_per_2;
	iter->fddd_per_6[i] = fddd_per_2 * (1.0f / 3.0f);
}

static void hair_render_iter_init_point(HairRenderIterator *iter)
{
	int totpoints = iter->hair->totpoints;
	int i;
	
	int k = iter->k;
	HairPoint *pt1 = iter->point;
	HairPoint *pt0 = k > 0 ? pt1 - 1 : pt1;
	HairPoint *pt2 = k < totpoints - 1 ? pt1 + 1 : pt1;
	HairPoint *pt3 = k < totpoints - 2 ? pt1 + 2 : pt2;
	
	for (i = 0; i < 3; ++i)
		forward_diff_precalc(iter, 1.0f / (float)iter->steps_per_point, pt0->co, pt1->co, pt2->co, pt3->co, i);
}

void BKE_hair_render_iter_init_hair(HairRenderIterator *iter)
{
	iter->point = iter->hair->points;
	iter->k = 0;
	
	iter->totsteps = (iter->hair->totpoints - 1) * iter->steps_per_point + 1;
	iter->step = 0;
	
	/* actual new hair or just next child? */
	if (iter->child >= iter->totchildren) {
		iter->totchildren = iter->hsys->params.render.num_render_hairs; /* XXX in principle could differ per hair */
		iter->child = 0;
		
		/* fill the hair cache to avoid redundant per-child calculations */
		hair_precalc_cache(iter);
	}
	
	/* init first point */
	if (iter->hair->totpoints > 0)
		hair_render_iter_init_point(iter);
}

void BKE_hair_render_iter_end(HairRenderIterator *iter)
{
	if (iter->hair_cache) {
		MEM_freeN(iter->hair_cache);
		iter->hair_cache = NULL;
	}
	
	if (iter->child_data) {
		MEM_freeN(iter->child_data);
		iter->child_data = NULL;
	}
	
	iter->hair = NULL; /* indicates uninitialized iterator */
}

bool BKE_hair_render_iter_initialized(HairRenderIterator *iter)
{
	return iter->hair != NULL || iter->hair_cache || iter->child_data;
}

void BKE_hair_render_iter_count(HairRenderIterator *iter, int *r_tothairs, int *r_totsteps)
{
	int tothairs, totsteps;
	HairCurve *hair;
	int i;
	
	tothairs = iter->hsys->totcurves;
	
	totsteps = 0;
	for (i = 0, hair = iter->hsys->curves; i < iter->hsys->totcurves; ++i, ++hair) {
		totsteps += (hair->totpoints - 1) * iter->steps_per_point + 1;
	}
	
	if (r_tothairs) *r_tothairs = tothairs;
	if (r_totsteps) *r_totsteps = totsteps;
}

bool BKE_hair_render_iter_valid_hair(HairRenderIterator *iter)
{
	return iter->i < iter->hsys->totcurves;
}

bool BKE_hair_render_iter_valid_step(HairRenderIterator *iter)
{
	return iter->step < iter->totsteps;
}

void BKE_hair_render_iter_next_hair(HairRenderIterator *iter)
{
	++iter->child;
	
	if (iter->child >= iter->totchildren) {
		++iter->hair;
		++iter->i;
	}
}

BLI_INLINE void forward_diff_step(HairRenderIterator *iter, int i)
{
	iter->f[i] = iter->f[i] + iter->fd[i] + iter->fdd_per_2[i] + iter->fddd_per_6[i];
	iter->fd[i] = iter->fd[i] + iter->fdd[i] + iter->fddd_per_2[i];
	iter->fdd[i] = iter->fdd[i] + iter->fddd[i];
	iter->fdd_per_2[i] = iter->fdd_per_2[i] + iter->fddd_per_2[i];
}

void BKE_hair_render_iter_next_step(HairRenderIterator *iter)
{
	int i;
	
	/* calculate next interpolation value */
	for (i = 0; i < 3; ++i)
		forward_diff_step(iter, i);
	
	++iter->step;
	
	if (iter->step < iter->totsteps && iter->step % iter->steps_per_point == 0) {
		++iter->point;
		++iter->k;
		
		/* init next point */
		hair_render_iter_init_point(iter);
	}
}

void BKE_hair_render_iter_get(HairRenderIterator *iter, float r_co[3], float *r_radius)
{
	HairPointRenderCache *cache0 = iter->hair_cache + iter->k;
	HairPoint *pt0 = iter->point;
	float tan[3], cotan[3];
	float co[3];
	float radius;
	
	/* spline interpolation */
	
	copy_v3_v3(co, iter->f);
	
	
	/* linear interpolation */
	
	radius = pt0->radius;
	
	copy_v3_v3(tan, cache0->tan);
	copy_v3_v3(cotan, cache0->cotan);
	
	if (iter->step < iter->totsteps - 1) {
		HairPointRenderCache *cache1 = cache0 + 1;
		HairPoint *pt1 = pt0 + 1;
		int i = iter->step % iter->steps_per_point;
		float t = (float)i / (float)iter->steps_per_point;
		float mt = 1.0f - t;
		
		radius = radius * mt + pt1->radius * t;
		
		interp_v3_v3v3(tan, tan, cache1->tan, t);
		interp_v3_v3v3(cotan, cotan, cache1->cotan, t);
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

void BKE_hair_render_iter_get_frame(HairRenderIterator *iter, float nor[3], float tan[3], float cotan[3])
{
	HairPointRenderCache *cache0 = iter->hair_cache + iter->k;
	copy_v3_v3(nor, cache0->nor);
	copy_v3_v3(tan, cache0->tan);
	copy_v3_v3(cotan, cache0->cotan);
	
	if (iter->step < iter->totsteps - 1) {
		HairPointRenderCache *cache1 = cache0 + 1;
		int i = iter->step % iter->steps_per_point;
		float t = (float)i / (float)iter->steps_per_point;
		
		interp_v3_v3v3(nor, nor, cache1->nor, t);
		interp_v3_v3v3(tan, tan, cache1->tan, t);
		interp_v3_v3v3(cotan, cotan, cache1->cotan, t);
	}
}

float BKE_hair_render_iter_param(HairRenderIterator *iter)
{
	return iter->totsteps > 1 ? (float)iter->step / (float)(iter->totsteps-1) : 0.0f;
}

/* ==== Hair Modifier ==== */

void BKE_hair_mod_verify_debug_data(HairModifierData *hmd)
{
	if (hmd) {
		if (!(hmd->debug_flag & MOD_HAIR_DEBUG_SHOW)) {
			if (hmd->debug_data) {
				BKE_sim_debug_data_free(hmd->debug_data);
				hmd->debug_data = NULL;
			}
		}
		else {
			if (!hmd->debug_data) {
				hmd->debug_data = BKE_sim_debug_data_new();
			}
		}
	}
}

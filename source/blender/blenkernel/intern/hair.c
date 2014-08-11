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
#include "BLI_utildefines.h"

#include "DNA_hair_types.h"

#include "BKE_hair.h"
#include "BKE_mesh_sample.h"

HairSystem *BKE_hairsys_new(void)
{
	HairSystem *hsys = MEM_callocN(sizeof(HairSystem), "hair system");
	
	hsys->params.substeps_forces = 30;
	hsys->params.substeps_damping = 10;
	hsys->params.stretch_stiffness = 2000.0f;
	hsys->params.stretch_damping = 10.0f;
	hsys->params.bend_stiffness = 40.0f;
	hsys->params.bend_damping = 10.0f;
	
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
	MEM_freeN(hsys);
}

HairSystem *BKE_hairsys_copy(HairSystem *hsys)
{
	int totcurves = hsys->totcurves, i;
	HairSystem *thsys = MEM_dupallocN(hsys);
	
	thsys->curves = MEM_dupallocN(hsys->curves);
	for (i = 0; i < totcurves; ++i)
		thsys->curves[i].points = MEM_dupallocN(hsys->curves[i].points);
	
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

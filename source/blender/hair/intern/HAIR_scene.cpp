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

extern "C" {
#include "DNA_hair_types.h"
#include "DNA_object_types.h"
}

#include "HAIR_curve.h"
#include "HAIR_math.h"
#include "HAIR_memalloc.h"
#include "HAIR_scene.h"
#include "HAIR_solver.h"

HAIR_NAMESPACE_BEGIN

SolverData *SceneConverter::build_solver_data(Scene *scene, Object *ob, HairSystem *hsys)
{
	HairCurve *hair;
	int i;
	
	Transform mat = Transform(ob->obmat);
	
	/* count points */
	int totpoints = 0;
	for (hair = hsys->curves, i = 0; i < hsys->totcurves; ++hair, ++i) {
		totpoints += hair->totpoints;
	}
	
	/* allocate data */
	SolverData *data = new SolverData(hsys->totcurves, totpoints);
	Curve *solver_curves = data->curves;
	Point *solver_points = data->points;
	
	/* copy scene data to solver data */
	Point *point = solver_points;
	for (hair = hsys->curves, i = 0; i < hsys->totcurves; ++hair, ++i) {
		solver_curves[i] = Curve(hair->totpoints, point);
		
		for (int k = 0; k < hair->totpoints; ++k, ++point) {
			HairPoint *hair_pt = hair->points + k;
			
			*point = Point(transform_point(mat, hair_pt->rest_co));
			point->cur.co = transform_point(mat, hair_pt->co);
			point->cur.vel = transform_direction(mat, hair_pt->vel);
		}
	}
	
	/* finalize */
	data->precompute_rest_bend();
	
	return data;
}

HAIR_NAMESPACE_END

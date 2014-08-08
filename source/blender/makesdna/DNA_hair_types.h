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
 * The Original Code is: all of this file.
 *
 * Contributor(s): Blender Foundation,
 *                 Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_hair_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_HAIR_TYPES_H__
#define __DNA_HAIR_TYPES_H__

#include "DNA_meshdata_types.h"

typedef struct HairPoint {
	float rest_co[3];           /* rest location in object space */
	float co[3];                /* location in object space */
	float vel[3];               /* velocity */
	float radius;               /* thickness of a hair wisp */
	
	int pad[2];
} HairPoint;

typedef struct HairCurve {
	HairPoint *points;          /* point data */
	int totpoints;              /* number of points in the curve */
	int pad;
	
	MSurfaceSample root;
	float rest_nor[3];          /* rest normal */
	float rest_tan[3];          /* rest tangent */
} HairCurve;

typedef struct HairParams {
	int substeps_forces;
	int substeps_damping;
	
	float stretch_stiffness;
	float stretch_damping;
	float bend_stiffness;
	float bend_damping;
	float bend_smoothing;
	
	float drag;
	
	/* collision settings */
	float restitution;
	float friction;
} HairParams;

typedef struct HairSystem {
	HairCurve *curves;              /* curve data */
	int totcurves;                  /* number of curves */
	int pad;
	
	HairParams params;
} HairSystem;

#endif

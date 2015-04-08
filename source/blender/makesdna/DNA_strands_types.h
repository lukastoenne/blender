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

/** \file DNA_strands_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_STRANDS_TYPES_H__
#define __DNA_STRANDS_TYPES_H__

#include "DNA_defs.h"

typedef struct StrandsVertex {
	float co[3];
	float time;
	float weight;
	
	/* utility data */
	float nor[3]; /* normals (edge directions) */
} StrandsVertex;

typedef struct StrandsMotionState {
	float co[3];
	float vel[3];
	
	/* utility data */
	float nor[3]; /* normals (edge directions) */
} StrandsMotionState;

typedef struct StrandsCurve {
	int numverts;
	float root_matrix[3][3];
} StrandsCurve;

typedef struct Strands {
	StrandsCurve *curves;
	StrandsVertex *verts;
	int totcurves, totverts;
	
	/* optional */
	StrandsMotionState *state;
} Strands;


typedef struct StrandsChildCurve {
	int numverts;
} StrandsChildCurve;

typedef struct StrandsChildVertex {
	float co[3];
	float time;
	
	/* utility data */
	float nor[3]; /* normals (edge directions) */
} StrandsChildVertex;

typedef struct StrandsChildren {
	StrandsChildCurve *curves;
	StrandsChildVertex *verts;
	int totcurves, totverts;
} StrandsChildren;

#endif  /* __DNA_STRANDS_TYPES_H__ */

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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenjit/BJIT_types.h
 *  \ingroup bjit
 */

#ifndef __BJIT_TYPES_H__
#define __BJIT_TYPES_H__

#include "bjit_llvm.h"

namespace bjit {

using namespace llvm;

typedef types::ieee_float vec2_t[2];
typedef types::ieee_float vec3_t[3];
typedef types::ieee_float vec4_t[4];

typedef vec2_t mat2_t[2];
typedef vec3_t mat3_t[3];
typedef vec4_t mat4_t[4];

/* ------------------------------------------------------------------------- */

typedef struct EffectorEvalInput {
	vec3_t loc;
	vec3_t vel;
} EffectorEvalInput;

typedef struct EffectorEvalResult {
	vec3_t force;
	vec3_t impulse;
} EffectorEvalResult;

typedef struct EffectorEvalSettings {
	mat4_t tfm;
	mat4_t itfm;

	int flag;			/* general settings flag */

	short falloff;		/* fall-off type */
	short shape;		/* point, plane or surface */
	
	/* Main effector values */
	float f_strength;	/* The strength of the force (+ or - ) */
	float f_damp;		/* Damping ratio of the harmonic effector */
	float f_flow;		/* How much force is converted into "air flow", i.e. force used as the velocity of surrounding medium. */
	
	float f_size;		/* Noise size for noise effector, restlength for harmonic effector */
	
	/* fall-off */
	float f_power;		/* The power law - real gravitation is 2 (square) */
	float maxdist;		/* if indicated, use this maximum */
	float mindist;		/* if indicated, use this minimum */
	float f_power_r;	/* radial fall-off power */
	float maxrad;		/* radial versions of above */
	float minrad;
	
	float absorption;	/* used for forces */
} EffectorEvalSettings;

typedef enum EffectorEvalSettings_Flag {
	EFF_FIELD_USE_MIN               = (1 << 0),
	EFF_FIELD_USE_MAX               = (1 << 1),
	EFF_FIELD_USE_MIN_RAD           = (1 << 2),
	EFF_FIELD_USE_MAX_RAD           = (1 << 3),
} EffectorEvalSettings_Flag;

typedef enum EffectorEvalSettings_FalloffType {
	EFF_FIELD_FALLOFF_SPHERE        = 0,
	EFF_FIELD_FALLOFF_TUBE          = 1,
	EFF_FIELD_FALLOFF_CONE          = 2,
} EffectorEvalSettings_FalloffType;

typedef enum EffectorEvalSettings_Shape{
	EFF_FIELD_SHAPE_POINT           = 0,
	EFF_FIELD_SHAPE_PLANE           = 1,
	EFF_FIELD_SHAPE_SURFACE         = 2,
	EFF_FIELD_SHAPE_POINTS          = 3,
} EffectorEvalSettings_Shape;

} /* namespace bjit */

#endif

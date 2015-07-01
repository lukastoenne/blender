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

/** \file blender/blenjit/BJIT_forcefield.h
 *  \ingroup bjit
 */

#ifndef __BJIT_FORCEFIELD_H__
#define __BJIT_FORCEFIELD_H__

#ifdef __cplusplus
extern "C" {
#endif

struct EffectorContext;

typedef struct EffectorEvalInput {
	float loc[3];
	float vel[3];
} EffectorEvalInput;

typedef struct EffectorEvalResult {
	float force[3];
	float impulse[3];
} EffectorEvalResult;

typedef struct EffectorEvalSettings {
	float tfm[4][4];
	float itfm[4][4];
} EffectorEvalSettings;

void BJIT_build_effector_module(void);
void BJIT_free_effector_module(void);

void BJIT_build_effector_function(struct EffectorContext *effctx);
void BJIT_free_effector_function(struct EffectorContext *effctx);

#ifdef __cplusplus
}
#endif

#endif

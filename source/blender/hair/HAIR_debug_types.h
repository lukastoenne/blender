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

#ifndef __HAIR_DEBUG_TYPES_H__
#define __HAIR_DEBUG_TYPES_H__

/** Struct types for debugging info,
 *  shared between C and C++ code to avoid redundancy
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HAIR_SolverDebugContact {
	float coA[3], coB[3];
} HAIR_SolverDebugContact;

typedef struct HAIR_SolverDebugPoint {
	int index;
	float co[3];
	float rest_bend[3];
	float bend[3];
	float frame[3][3];
} HAIR_SolverDebugPoint;

#ifdef __cplusplus
}
#endif

#endif

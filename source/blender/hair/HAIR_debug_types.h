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

typedef struct HAIR_SolverDebugElement {
	int type;
	int hash;
	float color[3];
	
	float a[3], b[3];
} HAIR_SolverDebugElement;

typedef enum eHAIR_SolverDebugElement_Type {
	HAIR_DEBUG_ELEM_DOT,
	HAIR_DEBUG_ELEM_LINE,
	HAIR_DEBUG_ELEM_VECTOR,
} eHAIR_SolverDebugElement_Type;

typedef struct HAIR_SolverDebugVoxel {
	float r;
} HAIR_SolverDebugVoxel;

#ifdef __cplusplus
}
#endif

#endif

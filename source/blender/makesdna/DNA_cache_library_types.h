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
 * The Original Code is Copyright (C) 2015 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_cache_library_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_CACHE_LIBRARY_TYPES_H__
#define __DNA_CACHE_LIBRARY_TYPES_H__

#include "DNA_defs.h"
#include "DNA_ID.h"
#include "DNA_listBase.h"

#define MAX_CACHE_GROUP_LEVEL 8

typedef enum eCacheLibrary_SourceMode {
	CACHE_LIBRARY_SOURCE_SCENE      = 0, /* use generated scene data as input */
	CACHE_LIBRARY_SOURCE_CACHE      = 1, /* use cached data as input*/
} eCacheLibrary_SourceMode;

typedef enum eCacheLibrary_DisplayMode {
	CACHE_LIBRARY_DISPLAY_SOURCE    = 0, /* display source data */
	CACHE_LIBRARY_DISPLAY_RESULT    = 1, /* display result data */
} eCacheLibrary_DisplayMode;

typedef enum eCacheLibrary_EvalMode {
	CACHE_LIBRARY_EVAL_REALTIME     = (1 << 0), /* evaluate data with realtime settings */
	CACHE_LIBRARY_EVAL_RENDER       = (1 << 1), /* evaluate data with render settings */
} eCacheLibrary_EvalMode;

typedef enum eCacheDataType {
	CACHE_TYPE_OBJECT               = (1 << 0),
	CACHE_TYPE_DERIVED_MESH         = (1 << 1),
	CACHE_TYPE_HAIR                 = (1 << 2),
	CACHE_TYPE_HAIR_PATHS           = (1 << 3),
	CACHE_TYPE_PARTICLES            = (1 << 4),
	
	CACHE_TYPE_ALL                  = CACHE_TYPE_OBJECT | CACHE_TYPE_DERIVED_MESH | CACHE_TYPE_HAIR | CACHE_TYPE_HAIR_PATHS | CACHE_TYPE_PARTICLES,
} eCacheDataType;

typedef enum eCacheReadSampleResult {
	CACHE_READ_SAMPLE_INVALID         = 0,	/* no valid result can be retrieved */
	CACHE_READ_SAMPLE_EARLY           = 1,	/* request time before first sample */
	CACHE_READ_SAMPLE_LATE            = 2,	/* request time after last sample */
	CACHE_READ_SAMPLE_EXACT           = 3,	/* found sample for requested frame */
	CACHE_READ_SAMPLE_INTERPOLATED    = 4,	/* no exact sample, but found enclosing samples for interpolation */
} eCacheReadSampleResult;

typedef enum eCacheLibrary_Flag {
	CACHE_LIBRARY_BAKING              = (1 << 0), /* perform modifier evaluation when evaluating */
} eCacheLibrary_Flag;

typedef enum eCacheLibrary_DisplayFlag {
	CACHE_LIBRARY_DISPLAY_MOTION      = (1 << 0), /* display motion state result from simulation, if available */
	CACHE_LIBRARY_DISPLAY_CHILDREN    = (1 << 1), /* display child strands, if available */
} eCacheLibrary_DisplayFlag;

typedef enum eCacheLibrary_RenderFlag {
	CACHE_LIBRARY_RENDER_MOTION      = (1 << 0), /* render motion state result from simulation, if available */
	CACHE_LIBRARY_RENDER_CHILDREN    = (1 << 1), /* render child strands, if available */
} eCacheLibrary_RenderFlag;

typedef struct CacheLibrary {
	ID id;
	
	int flag;
	short eval_mode;
	short source_mode;
	short display_mode;
	short pad;
	int display_flag;
	int render_flag;
	int data_types;
	
	char input_filepath[1024]; /* 1024 = FILE_MAX */
	char output_filepath[1024]; /* 1024 = FILE_MAX */
	
	ListBase modifiers;
} CacheLibrary;

/* ========================================================================= */

/* XXX here be dragons ...
 * stuff below is a production hack,
 * should not be considered a permanent solution ...
 */
typedef struct CacheModifier {
	struct CacheModifier *next, *prev;
	
	short type, pad;
	int flag;
	char name[64]; /* MAX_NAME */
} CacheModifier;

typedef enum eCacheModifier_Type {
	eCacheModifierType_None                         = 0,
	
	eCacheModifierType_HairSimulation               = 1,
	eCacheModifierType_ForceField                   = 2,
	
	NUM_CACHE_MODIFIER_TYPES
} eCacheModifier_Type;

typedef struct HairSimParams {
	int flag;
	float timescale;
	int substeps;
	int pad;
	
	struct EffectorWeights *effector_weights;
	
	float mass;
	float drag;
	float goal_stiffness, goal_damping;
	struct CurveMapping *goal_stiffness_mapping;
	float stretch_stiffness, stretch_damping;
	float bend_stiffness, bend_damping;
} HairSimParams;

typedef enum eHairSimParams_Flag {
	eHairSimParams_Flag_UseGoalStiffnessCurve        = (1 << 0),
} eHairSimParams_Flag;

typedef struct HairSimCacheModifier {
	CacheModifier modifier;
	
	struct Object *object;
	int hair_system;
	int pad;
	
	HairSimParams sim_params;
} HairSimCacheModifier;

typedef struct ForceFieldCacheModifier {
	CacheModifier modifier;
	
	struct Object *object;
} ForceFieldCacheModifier;

#endif

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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Joshua Leung
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * "Operation Contexts" are used to pass state info (scene, parameter info, cfra)
 * as well as the temporary data structure(s) that operations should perform their
 * operations on. Thus, instead of operations potentially messing up state in places
 * they shouldn't be touching, they are just provided with thread-safe micro-environments
 * in which to work.
 */

#ifndef __DEPSGRAPH_EVAL_TYPES_H__
#define __DEPSGRAPH_EVAL_TYPES_H__

/* ****************************************** */
/* Operation Contexts */

/* Generic Operations Context 
 *
 * This contains standard information that most/all
 * operations will inevitably need at some point.
 */
typedef struct DEG_OperationsContext {
	Main *bmain;          /* scene database to query data from (if needed) */
	Scene *scene;         /* current scene we're working with */
	
	double cfra;          /* current frame (including subframe offset stuff) */
	size_t type;          /* (eDepsNode_Type.OuterNodes) type of context (for debug purposes) */
} DEG_OperationsContext;

/* Component Contexts ========================= */

/* Parameters */
typedef struct DEG_ParametersContext {
	DEG_OperationsContext ctx;       /* standard header */
	
	PointerRNA ptr;                  /* pointer to struct where parameters live */
	// XXX: ptr to data instance?
} DEG_ParametersContext;


/* Animation */
typedef struct DEG_AnimationContext {
	DEG_OperationsContext ctx;       /* standard header */
	
	ID *id;                          /* ID block to evaluate AnimData for */
	AnimData *adt;                   /* id->adt to be evaluated */
	
	// TODO: accumulation buffers for NLA?
} DEG_AnimationContext;


/* Transform */
// XXX: assume for now that this is object's only...
typedef struct DEG_TransformContext {
	DEG_OperationsContext ctx;       /* standard header */
	
	float matrix[4][4];              /* 4x4 matrix where results go */
	
	Object *ob;                      /* object that we're evaluating */
	struct bConstraintOb *cob;       /* constraint evaluation temp object/context */
} DEG_TransformContext;


/* Geometry */
typedef struct DEG_GeometryContext {
	DEG_OperationsContext ctx;      /* standard header */
	
	/* Output buffers - Only one of these should need to be used */
	struct DerivedMesh *dm;         /* mesh output */
	struct Displist *dl;            /* curves output */
	struct Path *path;              /* parametric curve */  // XXX...
	
	/* Source Geometry */
	ID *source;
	
	/* Assorted settings */
	uint64_t customdata_mask;       /* customdata mask */
} DEG_GeometryContext;


/* Pose Evaluation */
typedef struct DEG_PoseContext {
	DEG_OperationsContext ctx;      /* standard header */
	
	/* Source Data */
	/* NOTE: "iktrees" are stored on the bones as they're being evaluated... */
	Object *ob;                     /* object that pose resides on */
	bPose *pose;                    /* pose object that is being "solved" */
} DEG_PoseContext;

/* ****************************************** */

#endif // __DEPSGRAPH_EVAL_TYPES_H__

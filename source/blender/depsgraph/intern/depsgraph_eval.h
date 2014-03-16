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

struct Depsgraph;
struct DepsNode;

/* ****************************************** */
/* Operation Contexts */

/* Generic Operations Context 
 *
 * This contains standard information that most/all
 * operations will inevitably need at some point.
 */
typedef struct DEG_OperationsContext {
	struct Main *bmain;   /* scene database to query data from (if needed) */
	struct Scene *scene;  /* current scene we're working with */
	
	double cfra;          /* current frame (including subframe offset stuff) */
	
	int type;             /* (eDepsNode_Type.OuterNodes) component type <-> context type (for debug purposes) */
	short utype;          /* (eDEG_OperationContext_UserType) evaluation user type */
	short flag;           /* (eDEG_OperationContext_Flag) extra settings */
} DEG_OperationsContext;

/* Settings */
typedef enum eDEG_OperationContext_Flag {
	/* we're dealing with an instanced item... */
	// XXX: review this...
	DEG_OPCONTEXT_FLAG_INSTANCE = (1 << 0),
} eDEG_OperationContext_Flag;

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
	
	struct ID *id;                   /* ID block to evaluate AnimData for */
	struct AnimData *adt;            /* id->adt to be evaluated */
	
	// TODO: accumulation buffers for NLA?
} DEG_AnimationContext;


/* Transform */
typedef struct DEG_TransformContext {
	DEG_OperationsContext ctx;       /* standard header */
	
	float matrix[4][4];              /* 4x4 matrix where results go */
	
	struct Object *ob;               /* object that we're evaluating */
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
	struct Object *ob;              /* object that pose resides on */
	struct bPose *pose;             /* pose object that is being "solved" */
} DEG_PoseContext;

/* ****************************************** */
/* Tagging */

/* Tag a specific node as needing updates */
void DEG_node_tag_update(Depsgraph *graph, DepsNode *node);

/* ****************************************** */

#endif // __DEPSGRAPH_EVAL_TYPES_H__

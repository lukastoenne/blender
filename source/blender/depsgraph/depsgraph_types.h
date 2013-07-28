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
 * Datatypes for internal use in the Depsgraph
 * 
 * All of these datatypes are only really used within the "core" depsgraph.
 * In particular, node types declared here form the structure of operations
 * in the graph.
 */

#ifndef __DEPSGRAPH_TYPES_H__
#define __DEPSGRAPH_TYPES_H__

/* Maximum length of identifier names used in Depsgraph */
#define DEG_MAX_ID_NAME     128

/* ************************************* */
/* Relationships Between Nodes */

/* B depends on A (A -> B) */
struct DepsRelation {
	DepsRelation *next, *prev;
	
	/* the nodes in the relationship (since this is shared between the nodes) */
	DepsNode *from;               /* A */
	DepsNode *to;                 /* B */
	
	/* relationship attributes */
	char name[DEG_MAX_ID_NAME];   /* label for debugging */
	
	int type;                     /* (eDepsRelation_Type) */
	int flag;                     /* (eDepsRelation_Flag) */
};


/* Types of relationships between nodes 
 *
 * This is used to provide additional hints to use when filtering
 * the graph, so that we can go without doing more extensive
 * data-level checks...
 */
typedef enum eDepsRelation_Type {
	/* reationship type unknown/irrelevant */
	DEPSREL_TYPE_STANDARD = 0,
	
	/* root -> active scene or entity (screen, image, etc.) */
	DEPSREL_TYPE_ROOT_TO_ACTIVE,
	
	/* general datablock dependency */
	DEPSREL_TYPE_DATABLOCK,
	
	/* time dependency */
	DEPSREL_TYPE_TIME,
	
	/* component depends on results of another */
	DEPSREL_TYPE_COMPONENT_ORDER,
	
	/* relationship is just used to enforce ordering of operations
	 * (e.g. "init()" callback done before "exec() and "cleanup()")
	 */
	DEPSREL_TYPE_OPERATION,
	
	/* relationship results from a property driver affecting property */
	DEPSREL_TYPE_DRIVER,
	
	/* relationship is something driver depends on */
	DEPSREL_TYPE_DRIVER_TARGET,
	
	/* relationship is used for transform stack 
	 * (e.g. parenting, user transforms, constraints)
	 */
	DEPSREL_TYPE_TRANSFORM,
	
	/* relationship is used for geometry evaluation 
	 * (e.g. metaball "motherball" or modifiers)
	 */
	DEPSREL_TYPE_GEOMETRY_EVAL,
	
	/* relationship is used to trigger a post-change validity updates */
	DEPSREL_TYPE_UPDATE,
	
	/* relationship is used to trigger editor/screen updates */
	DEPSREL_TYPE_UPDATE_UI,
} eDepsRelation_Type;


/* Settings/Tags on Relationship */
typedef enum eDepsRelation_Flag {
	/* "pending" tag is used whenever "to" node is still waiting on this relation to be valid */
	DEPSREL_FLAG_PENDING    = (1 << 0),
	
	/* "touched" tag is used when filtering, to know which to collect */
	DEPSREL_FLAG_TEMP_TAG   = (1 << 1)
} eDepsRelation_Flag;

/* ************************************* */
/* Base-Defines for Nodes in Depsgraph */

/* All nodes in Despgraph are descended from this */
struct DepsNode {
	DepsNode *next, *prev;		/* linked-list of siblings (from same parent node) */
	DepsNode *owner;            /* mainly for inner-nodes to see which outer/data node they came from */
	
	char name[DEG_MAX_ID_NAME]; /* identifier - mainly for debugging purposes... */
	
	ListBase inlinks;           /* (LinkData : DepsRelation) nodes which this one depends on */
	ListBase outlinks;          /* (LinkData : DepsRelation) ndoes which depend on this one */
	
	short type;                 /* (eDepsNode_Type) structural type of node */
	short class;                /* (eDepsNode_Class) type of data/behaviour represented by node... */
	
	short  color;               /* (eDepsNode_Color) stuff for tagging nodes (for algorithmic purposes) */
	short  flag;                /* (eDepsNode_Flag) dirty/visited tags */
	
	size_t valency;             /* how many inlinks are we still waiting on before we can be evaluated... */
	int lasttime;               /* for keeping track of whether node has been evaluated yet, without performing full purge of flags first */
};

/* Metatype of Nodes - The general "level" in the graph structure the node serves */
typedef enum eDepsNode_Class {
	DEPSNODE_CLASS_GENERIC         = 0,        /* Types generally unassociated with user-visible entities, but needed for graph functioning */
	
	DEPSNODE_CLASS_COMPONENT       = 1,        /* [Outer Node] An "aspect" of evaluating/updating an ID-Block, requiring certain types of evaluation behaviours */    
	DEPSNODE_CLASS_OPERATION       = 2,        /* [Inner Node] A glorified function-pointer/callback for scheduling up evaluation operations for components, subject to relationship requirements */
} eDepsNode_Class;

/* Types of Nodes */
typedef enum eDepsNode_Type {
	/* Generic Types */
	DEPSNODE_TYPE_ROOT             = 0,        /* "Current Scene" - basically whatever kicks off the ealuation process */
	DEPSNODE_TYPE_TIMESOURCE       = 1,        /* Time-Source */
	DEPSNODE_TYPE_ID_REF           = 2,        /* ID-Block reference - used as landmarks/collection point for components, but not usually part of main graph */
	
	
	/* Outer Types */
	DEPSNODE_TYPE_PARAMETERS       = 10,       /* Parameters Component - Default when nothing else fits (i.e. just SDNA property setting) */
	DEPSNODE_TYPE_PROXY            = 11,       /* Generic "Proxy-Inherit" Component */   // XXX: Also for instancing of subgraphs?
	DEPSNODE_TYPE_ANIMATION        = 12,       /* Animation Component */                 // XXX: merge in with parameters?
	DEPSNODE_TYPE_TRANSFORM        = 13,       /* Transform Component (Parenting/Constraints) */
	DEPSNODE_TYPE_GEOMETRY         = 14,       /* Geometry Component (DerivedMesh/Displist) */
	
	/* Evaluation-Related Outer Types (with Subdata) */
	DEPSNODE_TYPE_EVAL_POSE        = 20,       /* Pose Component - Owner/Container of Bones Eval */
	DEPSNODE_TYPE_EVAL_PARTICLES   = 21,       /* Particle Systems Component */
	
	
	/* Inner Types */
	DEPSNODE_TYPE_OP_PARAMETER     = 100,      /* Parameter Evaluation Operation */
	DEPSNODE_TYPE_OP_PROXY         = 101,      /* Proxy Evaluation Operation */
	DEPSNODE_TYPE_OP_ANIMATION     = 102,      /* Animation Evaluation Operation */
	DEPSNODE_TYPE_OP_TRANSFORM     = 103,      /* Transform Evaluation Operation (incl. constraints, parenting, anim-to-matrix) */
	DEPSNODE_TYPE_OP_GEOMETRY      = 104,      /* Geometry Evaluation Operation (incl. modifiers) */
	
	DEPSNODE_TYPE_OP_UPDATE        = 105,      /* Property Update Evaluation Operation [Parameter] */
	DEPSNODE_TYPE_OP_DRIVER        = 106,      /* Driver Evaluation Operation [Parameter] */
	
	DEPSNODE_TYPE_OP_BONE          = 107,      /* Bone Evaluation [Transform/Pose] */
	DEPSNODE_TYPE_OP_PARTICLE      = 108,      /* Particles Evaluation [Particle] */
	DEPSNODE_TYPE_OP_RIGIDBODY     = 109,      /* Rigidbody Sim (Step) Evaluation */
} eDepsNode_Type;


/* "Colors" for use in depsgraph topology algorithms */
typedef enum eDepsNode_Color {
	DEPSNODE_WHITE = 0,
	DEPSNODE_GRAY  = 1,
	DEPSNODE_BLACK = 2
} eDepsNode_Color;

/* Flags for Depsgraph Nodes */
typedef enum eDepsNode_Flag {
	/* node needs to be updated */
	DEPSNODE_FLAG_NEEDS_UPDATE       = (1 << 0),
	
	/* node was directly modified, causing need for update */
	/* XXX: intention is to make it easier to tell when we just need to take subgraphs */
	DEPSNODE_FLAG_DIRECTLY_MODIFIED  = (1 << 1),
	
	/* node was visited/handled already in traversal... */
	DEPSNODE_FLAG_TEMP_TAG           = (1 << 2),
} eDepsNode_Flag;

/* ************************************* */
/* Node Types - to be customised during graph building */

/* Generic Nodes ======================= */

/* Root Node */
/* Super(DepsNode) */
typedef struct RootDepsNode {
	DepsNode nd;                    /* standard header */
	
	Scene *scene;                   /* scene that this corresponds to */
	TimeSourceDepsNode time_source; /* entrypoint node for time-changed */
} RootDepsNode;

/* Time Source Node */
/* Super(DepsNode) */
typedef struct TimeSourceDepsNode {
	DepsNode nd;                   /* standard header */
	
	// XXX: how do we keep track of the chain of time sources for propagation of delays?
	
	double cfra;                    /* new "current time" */
	double offset;                  /* time-offset relative to the "official" time source that this one has */
} TimeSourceDepsNode;


/* ID-Block Reference */
/* Super(DepsNode) */
typedef struct IDDepsNode {
	DepsNode nd;             /* standard header */
	
	ID *id;                  /* ID Block referenced */
	GHash *component_hash;   /* <eDepsNode_Type, ComponentDepsNode*> hash to make it faster to look up components */
} IDDepsNode;

/* Outer Nodes ========================= */

/* ID Component - Base type for all components */
/* Super(DepsNode) */
typedef struct ComponentDepsNode {
	DepsNode nd;             /* standard header */
	
	ListBase ops;            /* ([OperationDepsNode]) inner nodes for this component */
	GHash *ophash;           /* <String, OperationDepsNode> quicker lookups for inner nodes attached here by name/identifier */
	
	void *context;           /* (DEG_OperationsContext) context passed to evaluation functions, where required operations are determined */
	void *result_data;       /*  where the data for this component goes when done */
	
	// XXX: a poll() callback to check if component's first node can be started?
} ComponentDepsNode;

/* ---------------------------------------- */

/* Pose Evaluation - Sub-data needed */
/* Super(ComponentDepsNode) */
typedef struct PoseComponentDepsNode {
	/* ComponentDepsNode */
	DepsNode nd;             /* standard header */
	
	ListBase ops;            /* ([OperationDepsNode]) inner nodes for this component */
	GHash *ophash;           /* <String, OperationDepsNode> quicker lookups for inner nodes attached here by name/identifier */
	
	void *context;           /* (DEG_OperationsContext) context passed to evaluation functions, where required operations are determined */
	void *result_data;       /*  where the data for this component goes when done */
	
	/* PoseComponentDepsNode */
	GHash *bone_hash;        /* <String, BoneDepsNode> hash for quickly finding node(s) associated with bone */
} PoseComponentDepsNode;

/* Inner Nodes ========================= */

/* Atomic Operation - Base type for all operations */
/* Super(DepsNode) */
typedef struct OperationDepsNode {
	DepsNode nd;             /* standard header */
	
	/* Evaluation Operation for atomic operation 
	 * < context: (ComponentEvalContext) context containing data necessary for performing this operation
	 *            Results can generally be written to the context directly...
	 * < (item): (ComponentDepsNode/PointerRNA) the specific entity involved, where applicable
	 */
	void (*evaluate)(void *context, void *item);
} OperationDepsNode;

/* ************************************* */
/* Depsgraph */

/* Dependency Graph object */
struct Depsgraph {
	GHash *id_hash;          /* <ID : IDDepsNode> mapping from ID blocks to nodes representing these blocks (for quick lookups) */
	DepsNode *root_node;     /* "root" node - the one where all evaluation enters from */
	
	// XXX: other data...
	// XXX: what about free-floating non-id-related nodes? where do "they" go?
};

/* ************************************* */

#endif // __DEPSGRAPH_TYPES_H__

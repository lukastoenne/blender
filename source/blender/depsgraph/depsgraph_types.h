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

/* ************************************* */
/* Relationships Between Nodes */

/* B depends on A (A -> B) */
struct DepsRelation {
	DepsRelation *next, *prev;
	
	/* the nodes in the relationship (since this is shared between the nodes) */
	DepsNode *from;     /* A */
	DepsNode *to;       /* B */
	
	/* relationship attributes */
	const char *name;   /* label for debugging */
	
	int type;           /* (eDepsRelation_Type) */
	int flag;           /* (eDepsRelation_Flag) */
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
	
	/* relationship is just used to enforce ordering of operations
	 * (e.g. "init()" callback done before "exec() and "cleanup()")
	 */
	DEPSREL_TYPE_OPERATION,
	
	/* relationship results from a property driver */
	DEPSREL_TYPE_DRIVER,
	
	/* relationship results from a driver related to transforms */
	DEPSREL_TYPE_DRIVER_TRANSFORM,
	
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
	
	const char *name;           /* identifier - mainly for debugging purposes... */
	
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
	DEPSNODE_TYPE_EVAL_POSE        = 15,       /* Pose Component - Owner/Container of Bones Eval */
	DEPSNODE_TYPE_EVAL_PARTICLES   = 16,       /* Particle Systems Component */
	
	
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
	
	/* node's name needs to be freed (when node is freed, as it is on heap) */
	DEPSNODE_FLAG_NAME_NEEDS_FREE    = (1 << 3),
} eDepsNode_Flag;

/* ************************************* */
/* "Generic" Node Types */

/* Outer Nodes ========================= */

/* Note about ID vs ID-Group Nodes:
 * For simplicity, we could just merge ID and ID-Group types into a single
 * type of node, since ID nodes are simply a special subcase of group nodes 
 * where n = 1. However, that'd mean we'd end up incurring all the costs
 * associated with groups, even when that might not be needed. Hopefully,
 * operations code won't really find this too painful...
 */

/* "Standard" Outer-Node Node Header
 * NOTE: All outer nodes should follow this ordering so that inner nodes
 *       can easily get to the list of inner-nodes hosted by it without
 *       resorting to special typechecks or needing extra API's for access.
 */
/* @Super(DepsNode) */
typedef struct OuterDepsNodeTemplate {
	DepsNode nd;         /* standard node header */
	
	ListBase nodes;      /* ([DepsNode]) "inner" nodes ready to be executed */
} OuterDepsNodeTemplate;

/* "Standard" Datablock Node Header 
 * NOTE: This is used as the start of both IDDepsNode and GroupDepsNode,
 *       so any changes here need to be propagated down to both of these.
 *       We do this so that ID nodes and Groups can be used interchangably.
 */
/* @Super(OuterDepsNodeTemplate) */
typedef struct OuterIdDepsNodeTemplate {
	DepsNode nd;          /* standard node header */
	
	ListBase nodes;       /* ([DepsNode]) "inner" nodes ready to be executed */
	ListBase subdata;     /* ([DepsNode]) sub-data "data" nodes - where appropriate */
} OuterIdDepsNodeTemplate;


/* "ID" Datablock Node */
/* @Super(OuterIdDepsNodeTemplate) */
typedef struct IDDepsNode {
	/* OuterIdDepsNodeTemplate... */
	DepsNode nd;           /* standard node header */
	
	ListBase nodes;        /* ([DepsNode]) "inner" nodes ready to be executed */
	ListBase subdata;      /* ([DepsNode]) sub-datablock "data" nodes - where appropriate */
	
	
	/* ID-Node specific data */
	ID *id;                /* ID-block that this node represents */
} IDDepsNode;


/* "ID Group" Node */
/* @Super(OuterIdDepsNodeTemplate) */
typedef struct GroupDepsNode {
	/* OuterIdDepsNodeTemplate... */
	DepsNode nd;           /* standard node header */
	
	ListBase nodes;        /* ([DepsNode]) "inner" nodes ready to be executed */
	ListBase subdata;      /* ([DepsNode]) sub-datablock "data" nodes - where appropriate */
	
	
	/* Headline Section - Datablocks which cannot be evaluated separately from each other */
	ListBase id_blocks;    /* (LinkData : ID) ID datablocks involved in group */
	
	/* Cycle Resolution Stuff */
	/* ... TODO: this will only really be needed if/when we get bugreports about this ... */
} GroupDepsNode;


/* Inter-datablock Operation (e.g. Rigidbody Sim)
 * Basically, a glorified outer-node wrapper around an atomic operation
 */
// XXX: there probably isn't really any good reason yet why we couldn't use that directly...
/* @Super(OuterDepsNodeTemplate) */
typedef struct InterblockDepsNode {
	DepsNode nd;      /* standard node header */
	
	ListBase nodes;   /* ([DepsNode]) operation(s) "inner" nodes to be executed for this step */
	/* ... TODO: extra metadata for tagging the kinds of events this can accept? ... */
} InterblockDepsNode;

/* Sub-ID Data Nodes =================== */

/* A sub ID-block "data" node used to represent dependencies between such entities
 * which may be slightly coarser than the operations that are needed (or less coarse).
 */
// TODO: later on, we'll need to review whether this is really needed (or whether it just adds bloat)
//       - this will all really become more obvious when we have a bit more "real" logic blocked in
/* @Super(OuterDepsNodeTemplate) */
typedef struct DataDepsNode {
	DepsNode nd;        /* standard node header */
	
	PointerRNA ptr;     /* pointer for declaring the type/ref of data that we're referring to... */
	ListBase nodes;     /* ([DepsNode]) "inner" nodes ready to be executed, which represent this node */
} DataDepsNode;

/* Inner Nodes ========================= */

/* Atomic Operation Callback */
// FIXME: args to be passed to operation callbacks needs fleshing out...
typedef void (*DEG_AtomicEvalOperation_Cb)(PointerRNA *ptr, void *state);


/* Atomic Operation Node - The smallest execution unit that can be performed */
// Potential TODO's?
//    - special flags to make it easier to test if operation is of a "certain" type
/* @Super(OuterDepsNodeTemplate) */
typedef struct AtomicOperationDepsNode {
	DepsNode nd;                      /* standard node header */
	
	DEG_AtomicEvalOperation_Cb exec;  /* operation to perform */
	PointerRNA ptr;                   /* for holding info about the type/nature of the data node operates on */
} AtomicOperationDepsNode;

/* ************************************* */
/* Depsgraph */

/* Dependency Graph object */
struct Depsgraph {
	ListBase nodes;		    /* ([DepsNode]) sorted set of top-level outer-nodes */
	ListBase relations;     /* ([DepsRelation]) list of all relationships in the graph */
	
	DepsNode *root_node;    /* "root" node - the one where all evaluation enters from */
	
	GHash *nodehash;        /* (<ID : DepsNode>) mapping from ID blocks to outer nodes, for quicker retrievals */
	
	size_t num_nodes;       /* total number of nodes present in the system */
	int type;               /* type of Depsgraph - generic or specialised... */ // XXX: needed?
	
	void *instance_data;    /* this is the datastore (a "context" object of sorts) where data referred to lives */
};

/* ************************************* */

#endif // __DEPSGRAPH_TYPES_H__

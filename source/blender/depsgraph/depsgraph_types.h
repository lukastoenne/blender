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

/* A depends on B (A -> B) */
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
	DEG_RELATION_UNKNOWN = 0,
	
	/* relationship is just used to enforce ordering of operations
	 * (e.g. "init()" callback done before "exec() and "cleanup()")
	 */
	DEG_RELATION_OPERATION,
	
	/* relationship results from a property driver */
	DEG_RELATION_DRIVER,
	
	/* relationship results from a driver related to transforms */
	DEG_RELATION_DRIVER_TRANSFORM,
	
	/* relationship is used for transform stack 
	 * (e.g. parenting, user transforms, constraints)
	 */
	DEG_RELATION_TRANSFORM,
	
	/* relationship is used for geometry evaluation 
	 * (e.g. metaball "motherball" or modifiers)
	 */
	DEG_RELATION_GEOMETRY_EVAL,
	
	/* relationship is used to trigger a post-change validity updates */
	DEG_RELATION_UPDATE,
	
	/* relationship is used to trigger editor/screen updates */
	DEG_RELATION_UPDATE_UI,
	
	/* general datablock dependency */
	DEG_RELATION_DATABLOCK,
} eDepsRelation_Type;


/* Settings/Tags on Relationship */
typedef enum eDepsRelation_Flag {
	/* "pending" tag is used whenever "to" node is still waiting on this relation to be valid */
	DEG_RELATION_FLAG_PENDING    = (1 << 0),
	
	/* "touched" tag is used when filtering, to know which to collect */
	DEG_RELATION_FLAG_TEMP_TAG   = (1 << 1)
} eDepsRelation_Flag;

/* ************************************* */
/* Base-Defines for Nodes in Depsgraph */

/* All nodes in Despgraph are descended from this */
// XXX: this probably needs some way of representing what data it affects? 
//      so that it can be used in queries. But will this be too much overhead?
struct DepsNode {
	DepsNode *next, *prev;		/* linked-list of siblings (from same parent node) */
	DepsNode *owner;            /* mainly for inner-nodes to see which outer/data node they came from */
	
	ListBase inlinks;           /* (LinkData : DepsRelation) nodes which this one depends on */
	ListBase outlinks;          /* (LinkData : DepsRelation) ndoes which depend on this one */
	
	short nodetype;             /* (eDepsNode_Type) structural type of node */
	short datatype;             /* (???) type of data/behaviour represented by node... */
	
	short  color;               /* (eDepsNode_Color) stuff for tagging nodes (for algorithmic purposes) */
	short  flag;                /* (eDepsNode_Flag) dirty/visited tags */
	
	size_t valency;             /* how many inlinks are we still waiting on before we can be evaluated... */
	int lasttime;               /* for keeping track of whether node has been evaluated yet, without performing full purge of flags first */
};


/* Types of Nodes */
typedef enum eDepsNode_Type {
	/* Outer Types */
	DEPSNODE_TYPE_OUTER_ID    = 0,        /* Datablock */
	DEPSNODE_TYPE_OUTER_GROUP = 1,        /* ID Group */
	DEPSNODE_TYPE_OUTER_OP    = 2,        /* Inter-datablock operation */
	
	/* "Data" Nodes (sub-datablock level) */
	DEPSNODE_TYPE_MID_DATA    = 10,       /* Sub-datablock "data" (i.e. */
	
	/* Inner Types */
	DEPSNODE_TYPE_INNER_ATOM  = 100,      /* Atomic Operation */
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
	DEPSNODE_NEEDS_UPDATE       = (1 << 0),
	
	/* node was directly modified, causing need for update */
	/* XXX: intention is to make it easier to tell when we just need to take subgraphs */
	DEPSNODE_DIRECTLY_MODIFIED  = (1 << 1),
	
	/* node was visited/handled already in traversal... */
	DEPSNODE_TEMP_TAG           = (1 << 2)
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

/* "ID" Datablock Node */
typedef struct DatablockDepsNode {
	DepsNode nd;           /* standard node header */
	
	/* Sub-Graph 
	 * NOTE: Keep this in sync with GroupDepsNode!
	 */
	ListBase subdata;      /* ([DepsNode]) sub-datablock "data" nodes - where appropriate */
	ListBase nodes;        /* ([DepsNode]) "inner" nodes ready to be executed */
} DatablockDepsNode;


/* "ID Group" Node */
typedef struct GroupDepsNode {
	DepsNode nd;           /* standard node header */
	
	/* Sub-Graph
	 * WARNING: Keep this in sync with DatablockDepsNode!
	 */
	ListBase subdata;      /* ([DepsNode]) sub-datablock "data" nodes - where appropriate */
	ListBase nodes;        /* ([DepsNode]) "inner" nodes ready to be executed */
	
	/* Headline Section - Datablocks which cannot be evaluated separately from each other */
	ListBase id_blocks;    /* (LinkData : ID) ID datablocks involved in group */
	
	/* Cycle Resolution Stuff */
	/* ... TODO: this will only really be needed if/when we get bugreports about this ... */
} GroupDepsNode;

/* Sub-ID Data Nodes =================== */

/* A sub ID-block "data" node used to represent dependencies between such entities
 * which may be slightly coarser than the operations that are needed (or less coarse).
 */
typedef struct DataDepsNode {
	DepsNode nd;        /* standard node header */
	
	PointerRNA ptr;     /* pointer for declaring the type/ref of data that we're referring to... */
	ListBase nodes;     /* ([DepsNode]) "inner" nodes ready to be executed, which represent this node */
} DataDepsNode;

/* Inner Nodes ========================= */

/* Atomic Operation Callback */
// FIXME: args to be passed to operation callbacks needs fleshing out...
typedef void (*DEG_AtomicEvalOperation_Cb)(void *state);


/* Atomic Operation Node - The smallest execution unit that can be performed */
// Potential TODO's?
//    - special flags to make it easier to test if operation is of a "certain" type
//    - "name" derived from callback function used - for easier debugging? Maybe in header instead?
typedef struct AtomicOperationDepsNode {
	DepsNode nd;                      /* standard node header */
	
	DEG_AtomicEvalOperation_Cb exec;  /* operation to perform */
	PointerRNA ptr;                   /* for holding info about the type/nature of the data node operates on */
} AtomicOperationDepsNode;

/* ************************************* */
/* Depsgraph */

/* Dependency Graph object */
struct Depsgraph {
	ListBase nodes;		/* ([DepsNode]) sorted set of top-level outer-nodes */
	ListBase relations; /* ([DepsRelation]) list of all relationships in the graph */
	
	size_t num_nodes;   /* total number of nodes present in the system */
	int type;           /* type of Depsgraph - generic or specialised... */ // XXX: needed?
	
	void *instance_data; /* this is the datastore (a "context" object of sorts) where data referred to lives */
};

/* ************************************* */

#endif // __DEPSGRAPH_TYPES_H__

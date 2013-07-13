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
/* Base-Defines for Nodes in Depsgraph */

/* All nodes in Despgraph are descended from this */
struct DepsNode {
	DepsNode *next, *prev;		/* linked-list of siblings (from same parent node) */
	DepsNode *owner;            /* mainly for inner-nodes to see which outer/data node they came from */
	
	short type;                 /* (eDepsNode_Type) type of node */
	char  color;                /* (eDepsNode_Color) stuff for tagging nodes (for algorithmic purposes) */
	
	char  flag;                 /* (eDepsNode_Flag) dirty/visited tags */
	int lasttime;               /* for keeping track of whether node has been evaluated yet, without performing full purge of flags first */
};


/* Types of Nodes */
typedef enum eDepsNode_Type {
	/* Outer Types */
	DEPSNODE_TYPE_OUTER_ID    = 0,        /* Datablock */
	DEPSNODE_TYPE_OUTER_GROUP = 1,        /* ID Group */
	DEPSNODE_TYPE_OUTER_OP    = 2,        /* Inter-datablock operation */
	
	/* Inner Types */
	DEPSNODE_TYPE_INNER_ATOM  = 100,      /* Atomic Operation */
	DEPSNODE_TYPE_INNER_COMBO = 101,      /* Optimised cluster of atomic operations - unexploded */
} eDepsNode_Type;


/* "Colors" for use in depsgraph topology algorithms */
typedef enum eDepsNode_Color {
	DAG_WHITE = 0,
	DAG_GRAY  = 1,
	DAG_BLACK = 2
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

/* "ID Group" Node */
typedef struct GroupDepsNode {
	DepsNode nodedata;  /* standard node header */
	
} GroupDepsNode;

/* Inner Nodes ========================= */

/* Atomic Operation Callback */
// FIXME: args to be passed to operation callbacks needs fleshing out...
typedef void (*DEG_AtomicEvalOperation_Cb)(void *state);


/* Atomic Operation Node - The smallest execution unit that can be performed */
// Potential TODO's?
//    - special flags to make it easier to test if operation is of a "certain" type
//    - "name" derived from callback function used - for easier debugging? Maybe in header instead?
typedef struct AtomicOperationDepsNode {
	DepsNode nodedata;                /* standard node header */
	
	DEG_AtomicEvalOperation_Cb exec;  /* operation to perform */
	PointerRNA ptr;                   /* for holding info about the type/nature of the data node operates on */
} AtomicOperationDepsNode;

/* ************************************* */
/* Depsgraph */

/* Dependency Graph object */
struct Depsgraph {
	ListBase nodes;		/*([DepsNode]) sorted set of top-level outer-nodes */
	
	size_t num_nodes;   /* total number of nodes present in the system */
	int type;           /* type of Depsgraph - generic or specialised... */ // XXX: needed?
	
	void *instance_data; /* this is the datastore (a "context" object of sorts) where data referred to lives */
};

/* ************************************* */

#endif // __DEPSGRAPH_TYPES_H__

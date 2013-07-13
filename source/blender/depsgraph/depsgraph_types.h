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
 */

#ifndef __DEPSGRAPH_TYPES_H__
#define __DEPSGRAPH_TYPES_H__

/* ************************************* */
/* Nodes in Depsgraph */

/* Types of Nodes ---------------------- */

typedef enum eDepsNode_Type {
	/* Outer Types */
	DEPSNODE_OUTER_TYPE_ID    = 0,        /* Datablock */
	DEPSNODE_OUTER_TYPE_GROUP = 1,        /* ID Group */
	DEPSNODE_OUTER_TYPE_OP    = 2,        /* Inter-datablock operation */
	
	/* Inner Types */
	DEPSNODE_INNER_TYPE_ATOM  = 100,      /* Atomic Operation */
	DEPSNODE_INNER_TYPE_COMBO = 101,      /* Optimised cluster of atomic operations - unexploded */
} eDepsNode_Type;

/* Base Node Type ---------------------- */

/* All nodes are descended from this */
struct DepsNode {
	DepsNode *next, *prev;	/* "natural order" that nodes can live in */
	DepsNode *parent;       /* node which "owns" this one */
	
	eDepsNode_Type type;    /* type of node */
	
	eDepsNode_Color color;  /* for internal algorithmic usage... */
	short time;             /* for tracking whether node has been visited twice - cycle detection */
	
	short needs_update;     /* (bool) whether node is tagged for updating */
};


/* ************************************* */
/* Depsgraph */

/* Dependency Graph object */
struct Depsgraph {
	ListBase nodes;		/*([DepsNode]) sorted set of top-level outer-nodes */
	
	size_t num_nodes;   /* total number of nodes present in the system */
	int type;           /* type of Depsgraph - generic or specialised... */
};

/* ************************************* */

#endif // __DEPSGRAPH_TYPES_H__

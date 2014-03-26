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

#ifndef __DEPSGRAPH_H__
#define __DEPSGRAPH_H__

#include <vector>

#include "MEM_guardedalloc.h"

#include "depsgraph_types.h"
#include "depsgraph_util_map.h"
#include "depsgraph_util_set.h"

#pragma message("DEPSGRAPH PORTING XXX: DNA_defs.h only included for unnecessary MAX_NAME macro, remove later")
#include "DNA_defs.h"

using std::vector;

struct DepsNode;


/* ************************************* */
/* Relationships Between Nodes */

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
	/* "touched" tag is used when filtering, to know which to collect */
	DEPSREL_FLAG_TEMP_TAG   = (1 << 0),
	
	/* "cyclic" link - when detecting cycles, this relationship was the one
	 * which triggers a cyclic relationship to exist in the graph
	 */
	DEPSREL_FLAG_CYCLIC     = (1 << 1),
} eDepsRelation_Flag;

/* B depends on A (A -> B) */
struct DepsRelation {
	/* the nodes in the relationship (since this is shared between the nodes) */
	DepsNode *from;               /* A */
	DepsNode *to;                 /* B */
	
	/* relationship attributes */
	char name[DEG_MAX_ID_NAME];   /* label for debugging */
	
	eDepsRelation_Type type;      /* type */
	int flag;                     /* (eDepsRelation_Flag) */
	
	DepsRelation(DepsNode *from, DepsNode *to, eDepsRelation_Type type, const char *description);
	~DepsRelation();
	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("DEG:DepsNode")
#endif
};

/* ************************************* */
/* Depsgraph */

/* Dependency Graph object */
struct Depsgraph {
	typedef unordered_map<const ID *, IDDepsNode *> IDNodeMap;
	typedef unordered_set<SubgraphDepsNode *> Subgraphs;
	typedef unordered_set<DepsNode *> EntryTags;
	typedef vector<DepsNode *> OperationNodes;
	
	Depsgraph();
	~Depsgraph();
	
	/* Find node which matches the specified description
	 *
	 * < id: ID block that is associated with this
	 * < (subdata): identifier used for sub-ID data (e.g. bone)
	 * < type: type of node we're dealing with
	 * < (name): custom identifier assigned to node 
	 *
	 * > returns: A node matching the required characteristics if it exists
	 *            OR NULL if no such node exists in the graph
	 */
	DepsNode *find_node(const ID *id, const char subdata[MAX_NAME], 
	                    eDepsNode_Type type, const char name[DEG_MAX_ID_NAME]);
	IDDepsNode *find_id_node(const ID *id) const;
	/* Determine node-querying criteria for finding a suitable node,
	 * given a RNA Pointer (and optionally, a property too)
	 *
	 * < ptr: pointer to the data that node will represent
	 * < (prop): optional property affected - providing this effectively results in inner nodes being returned
	 *
	 * > id: ID-block for node lookup/creation in
	 * > subdata: identifier used for sub-ID data (e.g. bone)
	 * > type: Node Type required
	 * > name: buffer to dump name to use for lookup clarification
	 */
	static void find_node_criteria_from_pointer(const PointerRNA *ptr, const PropertyRNA *prop,
	                                            ID **id, char subdata[MAX_NAME],
	                                            eDepsNode_Type *type, char name[DEG_MAX_ID_NAME]);
	/* Convenience wrapper to find node given just pointer + property
	 * < ptr: pointer to the data that node will represent
	 * < (prop): optional property affected - providing this effectively results in inner nodes being returned
	 *
	 * > returns: A node matching the required characteristics if it exists
	 *            OR NULL if no such node exists in the graph
	 */
	DepsNode *find_node_from_pointer(const PointerRNA *ptr, const PropertyRNA *prop);
	
	/* Create or find a node with data matching the requested characteristics
	 * ! New nodes are created if no matching nodes exist...
	 * ! Arguments are as for DEG_find_node()
	 *
	 * > returns: A node matching the required characteristics that exists in the graph
	 */
	DepsNode *get_node(const ID *id, const char *subdata,
	                   eDepsNode_Type type, const char name[DEG_MAX_ID_NAME]);
	/* Get the most appropriate node referred to by pointer + property 
	 * < graph: Depsgraph to find node from
	 * < ptr: RNA Pointer to the data that we're supposed to find a node for
	 * < (prop): optional RNA Property that is affected
	 */
	// XXX: returns matching outer node only, except for drivers
	DepsNode *get_node_from_pointer(const PointerRNA *ptr, const PropertyRNA *prop);
	/* Get the most appropriate node referred to by data path
	 * < graph: Depsgraph to find node from
	 * < id: ID-Block that path is rooted on
	 * < path: RNA-Path to resolve
	 * > returns: (IDDepsNode | DataDepsNode) as appropriate
	 */
	DepsNode *get_node_from_rna_path(const ID *id, const char path[]);
	
	/* Create a new node and add to graph
	 * ! Arguments are as for DEG_find_node()
	 *
	 * > returns: The new node created (of the specified type) which now exists in the graph already
	 *            (i.e. even if an ID node was created first, the inner node would get created first)
	 */
	DepsNode *add_new_node(const ID *id, const char *subdata,
	                       eDepsNode_Type type, const char name[DEG_MAX_ID_NAME]);
	
	/* Remove node from graph, but don't free any of its data */
	void remove_node(DepsNode *node);
	
	
	/* Core Graph Functionality ........... */
	IDNodeMap id_hash;          /* <ID : IDDepsNode> mapping from ID blocks to nodes representing these blocks (for quick lookups) */
	RootDepsNode *root_node;    /* "root" node - the one where all evaluation enters from */
	
	Subgraphs subgraphs;        /* subgraphs referenced in tree... */
	
	/* Quick-Access Temp Data ............. */
	EntryTags entry_tags;       /* nodes which have been tagged as "directly modified" */
	
	/* Convenience Data ................... */
	OperationNodes all_opnodes; /* all operation nodes, sorted in order of single-thread traversal order */
	
	// XXX: additional stuff like eval contexts, mempools for allocating nodes from, etc.
	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("DEG:DepsNode")
#endif
};

/* Add new relationship between two nodes */
DepsRelation *DEG_add_new_relation(DepsNode *from, DepsNode *to,
                                   eDepsRelation_Type type, 
                                   const char description[DEG_MAX_ID_NAME]);

/* ************************************* */

#endif // __DEPSGRAPH_H__

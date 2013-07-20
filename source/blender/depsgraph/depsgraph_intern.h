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
 * API's for internal use in the Depsgraph
 *
 * XXX: is this file really needed? Or does its purpose overlap with others?
 *      For now, let's keep this on the assumption that these API's aren't
 *      good for any of the other headers.
 */ 
 
#ifndef __DEPSGRAPH_INTERN_H__
#define __DEPSGRAPH_INTERN_H__

/* includes for safety! */
#include <stdarg.h>

/* Graph Building/Low-Level Querying =============================== */

/* Basics ---------------------------------------------------------- */

/* Always add a new (outer) node 
 * < graph: dependency graph that node will be part of
 * < type: type of outer-node to create. Inner nodes cannot be created using this method
 * < id: ID block that is associated with this data [<-- XXX: may not be supported/needed for ops?]
 *
 * < (data): sub-ID data that node refers to (if applicable)
 * < (srna): typeinfo for data, which makes it easier to keep track of what it is (if applicable)
 *
 * > returns: The new node created (of the specified type) which now exists in the graph already
 *            (i.e. even if an ID node was created first, the inner node would get created first)
 */
DepsNode *DEG_add_node(Depsgraph *graph, eDepsNode_Type type, ID *id, StructRNA *srna, void *data);


/* Find an outer node with characteristics matching the specified info 
 * ! Arguments are as for DEG_add_node()
 *
 * > returns: A node matching the required characteristics if it exists
 *            OR NULL if no such node exists in the graph
 */
DepsNode *DEG_find_node(Depsgraph *graph, eDepsNode_Type type, ID *id, StructRNA *srna, void *data);


/* Get the (outer) node with data matching the requested characteristics
 * ! New nodes are created if no matching nodes exist...
 * ! Arguments are as for DEG_add_node()
 *
 * > returns: A node matching the required characteristics that exists in the graph
 */
DepsNode *DEG_get_node(Depsgraph *graph, eDepsNode_Type type, ID *id, StructRNA *srna, void *data);


/* Groups ------------------------------------------------------------- */

/* Make a group from the two given outer nodes 
 * < node1: (DatablockDepsNode | GroupDepsNode)   // XXX: should we really allow existing groups as first arg, to organically "grow" them?
 * < node2: (DatablockDepsNode)
 * > return: (GroupDepsNode) either a new group node, or node1 if that was a group already
 */
DepsNode *DEG_group_cyclic_nodes(Depsgraph *graph, DepsNode *node1, DepsNode *node2);


/* Node Types Handling ================================================= */

/* "Typeinfo" for Node Types ------------------------------------------- */

/* Typeinfo Struct (nti) */
typedef struct DepsNodeTypeInfo {
	/* Identification ................................. */
	eDepsNode_Type type;           /* DEPSNODE_TYPE_### */
	size_t size;                   /* size in bytes of the struct */
	char name[MAX_NAME];           /* name of node type */
	
	/* Data Management ................................ */
	/* Initialise node-specific data - the node already exists */
	void (*init_data)(DepsNode *node, ID *id, StructRNA *srna, void *data);
	
	/* Add node to graph */
	void (*add_to_graph)(Depsgraph *graph, DepsNode *node);
	
	/* Free node-specific data, but not node itself */
	void (*free_data)(DepsNode *node);
	
	/* Make a copy of "src" node's data over to "dst" node */
	void (*copy_data)(DepsNode *dst, const DepsNode *src);
	
	/* Querying ...................................... */
	/* Does node match the (outer-node) query conditions? */
	bool (*is_match_outer)(DepsNode *node, ID *id, StructRNA *srna, void *data);
	
	// ...
	
	/* Graph Building (Outer nodes only) ............. */
	/* Generate atomic operation nodes (inner nodes subgraph) */
	void (*build_subgraph)(DepsNode *node);
	
	// TODO: perform special pruning operations to cull branches which don't do anything?
} DepsNodeTypeInfo;

/* Typeinfo Management -------------------------------------------------- */

/* Register node type */
void DEG_register_node_typeinfo(DepsNodeTypeInfo *typeinfo);

/* Get typeinfo for specified type */
DepsNodeTypeInfo *DEG_get_typeinfo(eDepsNode_Type type);

/* Get typeinfo for provided node */
DepsNodeTypeInfo *DEG_node_get_typeinfo(DepsNode *node);


#endif // __DEPSGRAPH_INTERN_H__

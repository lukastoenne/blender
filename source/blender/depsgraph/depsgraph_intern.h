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

/* Node Querying --------------------------------------------------- */

/* Find an outer node with characteristics matching the specified info 
 
 * < graph: dependency graph that node will be part of
 * < type: type of outer-node to create. Inner nodes cannot be created using this method
 * < id: ID block that is associated with this data [<-- XXX: may not be supported/needed for ops?]
 *
 * < (data): sub-ID data that node refers to (if applicable)
 * < (srna): typeinfo for data, which makes it easier to keep track of what it is (if applicable)
 *
 * > returns: A node matching the required characteristics if it exists
 *            OR NULL if no such node exists in the graph
 */
DepsNode *DEG_find_node(Depsgraph *graph, eDepsNode_Type type, ID *id, StructRNA *srna, void *data);


/* Get the (outer) node with data matching the requested characteristics
 * ! New nodes are created if no matching nodes exist...
 * ! Arguments are as for DEG_find_node()
 *
 * > returns: A node matching the required characteristics that exists in the graph
 */
DepsNode *DEG_get_node(Depsgraph *graph, eDepsNode_Type type, ID *id, StructRNA *srna, void *data);

/* Node Management ---------------------------------------------------- */

/* Create a new node, but don't do anything else with it yet... 
 * ! Ensuring that the node is properly initialised is the responsibility
 *   of whoever is calling this...
 *
 * > returns: The new node created (of the specified type), but which hasn't been added to
 *            the graph yet (callers need to do this manually, as well as other initialisations)
 */
DepsNode *DEG_create_node(eDepsNode_Type type);

/* Add given node to graph */
void DEG_add_node(Depsgraph *graph, DepsNode *node);

/* Create a new (outer) node and add to graph
 * ! Arguments are as for DEG_find_node()
 *
 * > returns: The new node created (of the specified type) which now exists in the graph already
 *            (i.e. even if an ID node was created first, the inner node would get created first)
 */
DepsNode *DEG_add_new_node(Depsgraph *graph, eDepsNode_Type type, ID *id, StructRNA *srna, void *data);

/* Make a (deep) copy of provided node and it's little subgraph
 * ! Newly created node is not added to the existing graph
 */
DepsNode *DEG_copy_node(const DepsNode *node);

/* Remove node from graph, but don't free any of its data */
void DEG_remove_node(Depsgraph *graph, DepsNode *node);

/* Free node data but not node itself 
 * ! Node data must be separately freed by caller
 * ! DEG_remove_node() should be called before calling this...
 */
void DEG_free_node(DepsNode *node)

/* Groups ------------------------------------------------------------- */

/* Make a group from the two given outer nodes 
 * < node1: (DatablockDepsNode | GroupDepsNode)
 * < node2: (DatablockDepsNode)
 * > return: (GroupDepsNode) either a new group node, or node1 if that was a group already
 */
DepsNode *DEG_group_cyclic_node_pair(Depsgraph *graph, DepsNode *node1, DepsNode *node2);


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
	
	/* Free node-specific data, but not node itself */
	// XXX: note - this should not try to call remove_from_graph()...
	void (*free_data)(DepsNode *node);
	
	/* Make a copy of "src" node's data over to "dst" node */
	void (*copy_data)(DepsNode *dst, const DepsNode *src);
	
	/* Add node to graph */
	void (*add_to_graph)(Depsgraph *graph, DepsNode *node, ID *id);
	
	/* Remove node from graph - Only use when node is to be replaced... */
	void (*remove_from_graph)(Depsgraph *graph, DepsNode *node);
	
	/* Querying ...................................... */
	
	/* Does node match the (outer-node) data-type requirements? */
	bool (*match_outer)(DepsNode *node, ID *id, StructRNA *srna, void *data);
	
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
DepsNodeTypeInfo *DEG_get_node_typeinfo(eDepsNode_Type type);

/* Get typeinfo for provided node */
DepsNodeTypeInfo *DEG_node_get_typeinfo(DepsNode *node);


#endif // __DEPSGRAPH_INTERN_H__

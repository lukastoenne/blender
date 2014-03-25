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
 * - Also, defines for "Node Type Info"
 */ 
 
#ifndef __DEPSGRAPH_INTERN_H__
#define __DEPSGRAPH_INTERN_H__

extern "C" {
#include "BLI_utildefines.h"
} /* extern "C" */

#include "depsgraph_types.h"

struct Main;
struct Group;
struct Scene;

/* Low-Level Querying ============================================== */

/* Node Querying --------------------------------------------------- */

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
void DEG_find_node_criteria_from_pointer(const PointerRNA *ptr, const PropertyRNA *prop,
                                         ID **id, char subdata[MAX_NAME],
                                         eDepsNode_Type *type, char name[DEG_MAX_ID_NAME]);
                                         
/* Convenience wrapper to find node given just pointer + property
 * < ptr: pointer to the data that node will represent
 * < (prop): optional property affected - providing this effectively results in inner nodes being returned
 *
 * > returns: A node matching the required characteristics if it exists
 *            OR NULL if no such node exists in the graph
 */
DepsNode *DEG_find_node_from_pointer(Depsgraph *graph, const PointerRNA *ptr, const PropertyRNA *prop);

/* Node Getting --------------------------------------------------- */

/* Create or find a node with data matching the requested characteristics
 * ! New nodes are created if no matching nodes exist...
 * ! Arguments are as for DEG_find_node()
 *
 * > returns: A node matching the required characteristics that exists in the graph
 */
DepsNode *DEG_get_node(Depsgraph *graph, const ID *id, const char subdata[MAX_NAME],
                       eDepsNode_Type type, const char name[DEG_MAX_ID_NAME]);


/* Get the most appropriate node referred to by pointer + property 
 * < graph: Depsgraph to find node from
 * < ptr: RNA Pointer to the data that we're supposed to find a node for
 * < (prop): optional RNA Property that is affected
 */
// XXX: returns matching outer node only, except for drivers
DepsNode *DEG_get_node_from_pointer(Depsgraph *graph, const PointerRNA *ptr, const PropertyRNA *prop);

/* Get the most appropriate node referred to by data path
 * < graph: Depsgraph to find node from
 * < id: ID-Block that path is rooted on
 * < path: RNA-Path to resolve
 * > returns: (IDDepsNode | DataDepsNode) as appropriate
 */
DepsNode *DEG_get_node_from_rna_path(Depsgraph *graph, const ID *id, const char path[]);

/* Graph Building ===================================================== */
/* Node Management ---------------------------------------------------- */

/* Create a new node and add to graph
 * ! Arguments are as for DEG_find_node()
 *
 * > returns: The new node created (of the specified type) which now exists in the graph already
 *            (i.e. even if an ID node was created first, the inner node would get created first)
 */
DepsNode *DEG_add_new_node(Depsgraph *graph, const ID *id, const char subdata[MAX_NAME],
                           eDepsNode_Type type, const char name[DEG_MAX_ID_NAME]);

/* Remove node from graph, but don't free any of its data */
void DEG_remove_node(Depsgraph *graph, DepsNode *node);

/* Convenience API ------------------------------------------------- */

/* Create a new node for representing an operation and add this to graph
 * ! If an existing node is found, it will be modified. This helps when node may 
 *   have been partially created earlier (e.g. parent ref before parent item is added)
 *
 * < id: ID-Block that operation will be performed on
 * < (subdata): identifier for sub-ID data that this is for (e.g. bones)
 * < type: Operation node type (corresponding to context/component that it operates in)
 * < optype: Role that operation plays within component (i.e. where in eval process)
 * < op: The operation to perform
 * < name: Identifier for operation - used to find/locate it again
 */
OperationDepsNode *DEG_add_operation(Depsgraph *graph, ID *id, const char subdata[MAX_NAME],
                                     eDepsNode_Type type, eDepsOperation_Type optype, 
                                     DepsEvalOperationCb op, const char name[DEG_MAX_ID_NAME]);

/* Graph Validity -------------------------------------------------- */

/* Ensure that all implicit constraints between nodes are satisfied 
 * (e.g. components are only allowed to be executed in a certain order)
 */
void DEG_graph_validate_links(Depsgraph *graph);


/* Sort nodes to determine evaluation order for operation nodes
 * where dependency relationships won't get violated.
 */
void DEG_graph_sort(Depsgraph *graph);


/* Relationships Handling ============================================== */

/* Convenience Macros -------------------------------------------------- */

/* Helper macros for interating over set of relationship
 * links incident on each node.
 *
 * NOTE: it is safe to perform removal operations here...
 *
 * < relations_set: (DepsNode::Relations) set of relationships (in/out links)
 * > relation:  (DepsRelation *) identifier where DepsRelation that we're currently accessing comes up
 */
#define DEPSNODE_RELATIONS_ITER_BEGIN(relations_set_, relation_)                          \
	{                                                                                \
		DepsNode::Relations::const_iterator __rel_iter = relations_set_.begin();     \
		while (__rel_iter != relations_set_.end()) {                                 \
			DepsRelation *relation_ = *__rel_iter;                                   \
			++__rel_iter;

			/* ... code for iterator body can be written here ... */

#define DEPSNODE_RELATIONS_ITER_END                                                  \
		}                                                                            \
	}

/* API Methods --------------------------------------------------------- */

/* Add new relationship between two nodes */
DepsRelation *DEG_add_new_relation(DepsNode *from, DepsNode *to,
                                   eDepsRelation_Type type, 
                                   const char description[DEG_MAX_ID_NAME]);

/* Graph Building ======================================================== */

/* Build depsgraph for the given group, and dump results in given graph container 
 * This is usually used for building subgraphs for groups to use...
 */
void DEG_graph_build_from_group(Depsgraph *graph, struct Main *bmain, struct Group *group);

/* Build subgraph for group */
DepsNode *DEG_graph_build_group_subgraph(Depsgraph *graph_main, struct Main *bmain, struct Group *group);

/* Graph Copying ========================================================= */
/* (Part of the Filtering API) */

/* Depsgraph Copying Context (dcc)
 *
 * Keeps track of node relationships/links/etc. during the copy 
 * operation so that they can be safely remapped...
 */
typedef struct DepsgraphCopyContext {
	struct GHash *nodes_hash;   /* <DepsNode, DepsNode> mapping from src node to dst node */
	struct GHash *rels_hash;    // XXX: same for relationships?
	
	// XXX: filtering criteria...
} DepsgraphCopyContext;

/* Internal Filtering API ---------------------------------------------- */

/* Create filtering context */
// XXX: needs params for conditions?
DepsgraphCopyContext *DEG_filter_init(void);

/* Free filtering context once filtering is done */
void DEG_filter_cleanup(DepsgraphCopyContext *dcc);


/* Data Copy Operations ------------------------------------------------ */

/* Make a (deep) copy of provided node and it's little subgraph
 * ! Newly created node is not added to the existing graph
 * < dcc: Context info for helping resolve links
 */
DepsNode *DEG_copy_node(DepsgraphCopyContext *dcc, const DepsNode *src);

/* Node Types Handling ================================================= */

/* "Typeinfo" for Node Types ------------------------------------------- */

/* Typeinfo Struct (nti) */
struct DepsNodeFactory {
	virtual eDepsNode_Type type() const = 0;
	virtual eDepsNode_Class tclass() const = 0;
	virtual const char *tname() const = 0;
	
	virtual DepsNode *create_node(const ID *id, const char *subdata, const char *name) const = 0;
	virtual DepsNode *copy_node(DepsgraphCopyContext *dcc, const DepsNode *copy) const = 0;
};

template <class NodeType>
struct DepsNodeFactoryImpl : public DepsNodeFactory {
	eDepsNode_Type type() const { return NodeType::typeinfo.type; }
	eDepsNode_Class tclass() const { return NodeType::typeinfo.tclass; }
	const char *tname() const { return NodeType::typeinfo.tname; }
	
	DepsNode *create_node(const ID *id, const char *subdata, const char *name) const
	{
		DepsNode *node = new NodeType();
		
		/* populate base node settings */
		node->type = type();
		node->tclass = tclass();
		
		if (name && name[0])
			/* set name if provided ... */
			BLI_strncpy(node->name, name, DEG_MAX_ID_NAME);
		else
			/* ... otherwise use default type name */
			BLI_strncpy(node->name, tname(), DEG_MAX_ID_NAME);
		
		node->init(id, subdata);
		
		return node;
	}
	
	virtual DepsNode *copy_node(DepsgraphCopyContext *dcc, const DepsNode *copy) const
	{
		BLI_assert(copy->type == type());
		DepsNode *node = new NodeType();
		
		/* populate base node settings */
		node->type = type();
		node->tclass = tclass();
		// XXX: need to review the name here, as we can't have exact duplicates...
		BLI_strncpy(node->name, copy->name, DEG_MAX_ID_NAME);
		
		node->copy(dcc, static_cast<const NodeType *>(copy));
		
		return node;
	}
};

/* Typeinfo Management -------------------------------------------------- */

/* Get typeinfo for specified type */
DepsNodeFactory *DEG_get_node_factory(const eDepsNode_Type type);

/* Get typeinfo for provided node */
DepsNodeFactory *DEG_node_get_factory(const DepsNode *node);

/* Debugging ========================================================= */

void DEG_debug_build_node_added(const DepsNode *node);
void DEG_debug_build_relation_added(const DepsRelation *rel);


#endif // __DEPSGRAPH_INTERN_H__

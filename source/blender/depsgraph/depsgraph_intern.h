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

struct Main;
struct Group;
struct Scene;

/* Low-Level Querying ============================================== */

/* Node Querying --------------------------------------------------- */

/* Find node which matches the specified description
 *
 * < graph: dependency graph that node will be part of
 * < id: ID block that is associated with this
 * < (subdata): identifier used for sub-ID data (e.g. bone)
 * < type: type of node we're dealing with
 * < (name): custom identifier assigned to node 
 *
 * > returns: A node matching the required characteristics if it exists
 *            OR NULL if no such node exists in the graph
 */
DepsNode *DEG_find_node(Depsgraph *graph, const ID *id, const char subdata[MAX_NAME], 
                        eDepsNode_Type type, const char name[DEG_MAX_ID_NAME]);


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

/* Create a new node, but don't do anything else with it yet... 
 * ! Ensuring that the node is properly initialised is the responsibility
 *   of whoever is calling this...
 *
 * > returns: The new node created (of the specified type), but which hasn't been added to
 *            the graph yet (callers need to do this manually, as well as other initialisations)
 */
DepsNode *DEG_create_node(eDepsNode_Type type);

/* Add given node to graph 
 * < (id): ID-Block that node is associated with (if applicable)
 */
void DEG_add_node(Depsgraph *graph, DepsNode *node, const ID *id);

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

/* Free node data but not node itself 
 * ! Node data must be separately freed by caller
 * ! DEG_remove_node() should be called before calling this...
 */
void DEG_free_node(DepsNode *node);

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
 * NOTE: Since each relationship is shared between the two nodes
 *       involved, each node must use "LinkData" to reference
 *       the nodes nearby...
 *
 * NOTE: it is safe to perform removal operations here...
 *
 * < first_link: (LinkData *) first LinkData in list of relationships (in/out links)
 * > rel:  (DepsRelation *) identifier where DepsRelation that we're currently accessing comes up
 */
#define DEPSNODE_RELATIONS_ITER_BEGIN(first_link, relation_)                          \
	{                                                                                \
		LinkData *__rel_iter, *__rel_next;                                           \
		for (__rel_iter = first_link; __rel_iter; __rel_iter = __rel_next) {         \
			DepsRelation *relation_ = (DepsRelation *)__rel_iter->data;              \
			__rel_next = __rel_iter->next;

			/* ... code for iterator body can be written here ... */

#define DEPSNODE_RELATIONS_ITER_END                                                  \
		}                                                                            \
	}

/* API Methods --------------------------------------------------------- */

/* Create new relationship object, but don't add it to graph yet */
DepsRelation *DEG_create_new_relation(DepsNode *from, DepsNode *to, 
                                      eDepsRelation_Type type, 
                                      const char description[DEG_MAX_ID_NAME]);

/* Add given relationship to the graph */
void DEG_add_relation(DepsRelation *rel);


/* Add new relationship between two nodes */
DepsRelation *DEG_add_new_relation(DepsNode *from, DepsNode *to,
                                   eDepsRelation_Type type, 
                                   const char description[DEG_MAX_ID_NAME]);


/* Remove relationship from graph, but don't free it yet */
void DEG_remove_relation(Depsgraph *graph, DepsRelation *rel);

/* Free relationship's data 
 * ! Assumes that it isn't part of graph anymore (DEG_remove_relation() called)
 * ! Relationship itself *is* freed...
 */
void DEG_free_relation(DepsRelation *rel);

/* Graph Building ======================================================== */

/* Build depsgraph for the given group, and dump results in given graph container 
 * This is usually used for building subgraphs for groups to use...
 */
void DEG_graph_build_from_group(Depsgraph *graph, struct Main *bmain, struct Group *group);

/* Build subgraph for group */
DepsNode *DEG_graph_build_group_subgraph(Depsgraph *graph_main, struct Main *bmain, struct Group *group);

/* Build depsgraph for the given scene, and dump results in given graph container */
void DEG_graph_build_from_scene(Depsgraph *graph, struct Main *bmain, struct Scene *scene);

/* Graph Copying ========================================================= */
/* (Part of the Filtering API) */

/* Depsgraph Copying Context (dcc)
 *
 * Keeps track of node relationships/links/etc. during the copy 
 * operation so that they can be safely remapped...
 */
typedef struct DepsgraphCopyContext {
	GHash *nodes_hash;   /* <DepsNode, DepsNode> mapping from src node to dst node */
	GHash *rels_hash;    // XXX: same for relationships?
	
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

/* Make a copy of given relationship */
DepsRelation *DEG_copy_relation(const DepsRelation *src);

/* Node Types Handling ================================================= */

/* "Typeinfo" for Node Types ------------------------------------------- */

/* Typeinfo Struct (nti) */
typedef struct DepsNodeTypeInfo {
	/* Identification ................................. */
	eDepsNode_Type type;           /* DEPSNODE_TYPE_### */
	size_t size;                   /* size in bytes of the struct */
	char name[DEG_MAX_ID_NAME];    /* name of node type */
	
	/* Data Management ................................ */
	/* Initialise node-specific data - the node already exists */
	void (*init_data)(DepsNode *node, const ID *id, const char subdata[MAX_NAME]);
	
	/* Free node-specific data, but not node itself 
	 * NOTE: data should already have been removed from graph!
	 */
	void (*free_data)(DepsNode *node);
	
	/* Make a copy of "src" node's data over to "dst" node */
	// TODO: perhaps copying needs to be a two-pass operation?
	void (*copy_data)(DepsgraphCopyContext *dcc, DepsNode *dst, const DepsNode *src);
	
	/* Graph/Connection Management .................... */
	
	/* Add node to graph - Will add additional inbetween nodes as needed */
	void (*add_to_graph)(Depsgraph *graph, DepsNode *node, const ID *id);
	
	/* Remove node from graph - Only use when node is to be replaced... */
	void (*remove_from_graph)(Depsgraph *graph, DepsNode *node);
	
	
	/* Recursively ensure that all implicit/builtin link rules have been applied */
	/* i.e. init()/cleanup() callbacks as last items for components + component ordering rules obeyed */
	void (*validate_links)(Depsgraph *graph, DepsNode *node);
	
	/* Evaluation Context Management .................. */
	/* (Components Only) */
	
	/* Initialise component's evaluation context used for the specified purpose */
	void (*eval_context_init)(ComponentDepsNode *node, eEvaluationContextType context_type);
	
	/* Free data in component's evaluation context which is used for the specified purpose 
	 * NOTE: this does not free the actual context in question
	 */
	void (*eval_context_free)(ComponentDepsNode *node, eEvaluationContextType context_type);
} DepsNodeTypeInfo;

/* Typeinfo Management -------------------------------------------------- */

/* Register node type */
void DEG_register_node_typeinfo(DepsNodeTypeInfo *typeinfo);

/* Get typeinfo for specified type */
DepsNodeTypeInfo *DEG_get_node_typeinfo(const eDepsNode_Type type);

/* Get typeinfo for provided node */
DepsNodeTypeInfo *DEG_node_get_typeinfo(const DepsNode *node);


#endif // __DEPSGRAPH_INTERN_H__

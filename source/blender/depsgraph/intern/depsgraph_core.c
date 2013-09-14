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
 * Core routines for how the Depsgraph works
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_defs.h"
#include "DNA_ID.h"
#include "DNA_listBase.h"

#include "BKE_depsgraph.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "depsgraph_types.h"
#include "depsgraph_intern.h"

/* ************************************************** */
/* Validity + Integrity */

/* Ensure that all implicit constraints between nodes are satisfied 
 * (e.g. components are only allowed to be executed in a certain order)
 */
void DEG_graph_validate_links(Depsgraph *graph)
{
	GHashIterator hashIter;
	
	BLI_assert((graph != NULL) && (graph->id_hash != NULL));
	
	/* go over each ID node to recursively call validate_links()
	 * on it, which should be enough to ensure that all of those
	 * subtrees are valid
	 */
	GHASH_ITER(hashIter, graph->id_hash) {
		DepsNode *node = (DepsNode *)BLI_ghashIterator_getValue(&hashIter);
		DepsNodeTypeInfo *nti = DEG_node_get_typeinfo(node);
		
		if (nti && nti->validate_links) {
			nti->validate_links(graph, node);
		}
	}
}

/* ************************************************** */
/* Low-Level Graph Traversal and Sorting */

/* Sort nodes to determine evaluation order for operation nodes
 * where dependency relationships won't get violated.
 */
void DEG_graph_sort(Depsgraph *graph)
{
#if 0
	void *ctx = NULL; // XXX: temp struct for keeping track of visited nodes, etc.?
	
	/* 1) traverse graph from root
	 *   - note when each graph was visited (within its peers)
	 *   - tag/knock out relationships leading to cyclic dependencies
	 */
	DEG_graph_traverse(graph, DEG_Filter_ExecutableNodes, NULL, 
	                          tag_nodes_for_sorting,      ctx); 
	

	/* 2) tweak order of nodes within each set of links */
#endif	
}

/* ************************************************** */
/* Node Management */

/* Get Node ----------------------------------------- */

/* Get a matching node, creating one if need be */
DepsNode *DEG_get_node(Depsgraph *graph, const ID *id, const char subdata[MAX_NAME],
                       eDepsNode_Type type, const char name[DEG_MAX_ID_NAME])
{
	DepsNode *node;
	
	/* firstly try to get an existing node... */
	node = DEG_find_node(graph, id, subdata, type, name);
	if (node == NULL) {
		/* nothing exists, so create one instead! */
		node = DEG_add_new_node(graph, id, subdata, type, name);
	}
	
	/* return the node - it must exist now... */
	return node;
}

/* Get the most appropriate node referred to by pointer + property */
DepsNode *DEG_get_node_from_pointer(Depsgraph *graph, const PointerRNA *ptr, const PropertyRNA *prop)
{
	DepsNode *node = NULL;
	
	ID *id;
	eDepsNode_Type type;
	char subdata[MAX_NAME];
	char name[DEG_MAX_ID_NAME];
	
	/* get querying conditions */
	DEG_find_node_criteria_from_pointer(ptr, prop, &id, subdata, &type, name);
	
	/* use standard lookup mechanisms... */
	node = DEG_get_node(graph, id, subdata, type, name);
	return node;
}

/* Get DepsNode referred to by data path */
DepsNode *DEG_get_node_from_rna_path(Depsgraph *graph, const ID *id, const char path[])
{
	PointerRNA id_ptr, ptr;
	PropertyRNA *prop = NULL;
	DepsNode *node = NULL;
	
	/* create ID pointer for root of path lookup */
	RNA_id_pointer_create((ID *)id, &id_ptr);
	
	/* try to resolve path... */
	if (RNA_path_resolve(&id_ptr, path, &ptr, &prop)) {
		/* get matching node... */
		node = DEG_get_node_from_pointer(graph, &ptr, prop);
	}
	
	/* return node found */
	return node;
}

/* Add ------------------------------------------------ */

/* Create a new node, but don't do anything else with it yet... */
DepsNode *DEG_create_node(eDepsNode_Type type)
{
	const DepsNodeTypeInfo *nti = DEG_get_node_typeinfo(type);
	DepsNode *node;
	
	/* create node data... */
	node = MEM_callocN(nti->size, nti->name);
	
	/* populate base node settings */
	node->type = type;
	
	/* node.class 
	 * ! KEEP IN SYNC wtih eDepsNode_Type
	 */
	if (type < DEPSNODE_TYPE_PARAMETERS) {
		node->class = DEPSNODE_CLASS_GENERIC;
	}
	else if (type < DEPSNODE_TYPE_OP_PARAMETER) {
		node->class = DEPSNODE_CLASS_COMPONENT;
	}
	else {
		node->class = DEPSNODE_CLASS_OPERATION;
	}
	
	/* node.name */
	// XXX: placeholder for now...
	BLI_strncpy(node->name, nti->name, DEG_MAX_ID_NAME);
	
	/* return newly created node data for more specialisation... */
	return node;
}

/* Add given node to graph */
void DEG_add_node(Depsgraph *graph, DepsNode *node, const ID *id)
{
	const DepsNodeTypeInfo *nti = DEG_node_get_typeinfo(node);
	
	if (node && nti) {
		nti->add_to_graph(graph, node, id);
	}
}

/* Add a new node */
DepsNode *DEG_add_new_node(Depsgraph *graph, const ID *id, const char subdata[MAX_NAME],
                           eDepsNode_Type type, const char name[DEG_MAX_ID_NAME])
{
	const DepsNodeTypeInfo *nti = DEG_get_node_typeinfo(type);
	DepsNode *node;
	
	BLI_assert(nti != NULL);
	
	/* create node data... */
	node = DEG_create_node(type);
	
	/* set name if provided */
	if (name && name[0]) {
		BLI_strncpy(node->name, name, DEG_MAX_ID_NAME);
	}
	
	/* type-specific data init
	 * NOTE: this is not included as part of create_node() as
	 *       some methods may want/need to override this step
	 */
	if (nti->init_data) {
		nti->init_data(node, id, subdata);
	}
	
	/* add node to graph 
	 * NOTE: additional nodes may be created in order to add this node to the graph
	 *       (i.e. parent/owner nodes) where applicable...
	 */
	DEG_add_node(graph, node, id);
	
	/* add node to operation-node list if it plays a part in the evaluation process */
	if (ELEM(node->class, DEPSNODE_CLASS_GENERIC, DEPSNODE_CLASS_OPERATION)) {
		BLI_addtail(&graph->all_opnodes, BLI_genericNodeN(node));
		graph->num_nodes++;
	}
	
	/* return the newly created node matching the description */
	return node;
}

/* Remove/Free ---------------------------------------- */

/* Remove node from graph, but don't free any of its data */
void DEG_remove_node(Depsgraph *graph, DepsNode *node)
{
	const DepsNodeTypeInfo *nti = DEG_node_get_typeinfo(node);
	
	if (node == NULL)
		return;
	
	/* relationships 
	 * - remove these, since they're at the same level as the
	 *   node itself (inter-relations between sub-nodes will
	 *   still remain and/or can still work that way)
	 */
	DEPSNODE_RELATIONS_ITER_BEGIN(node->inlinks.first, rel)
	{
		DEG_remove_relation(graph, rel);
	}
	DEPSNODE_RELATIONS_ITER_END;
	
	DEPSNODE_RELATIONS_ITER_BEGIN(node->outlinks.first, rel)
	{
		DEG_remove_relation(graph, rel);
	}
	DEPSNODE_RELATIONS_ITER_END;
	
	/* remove node from graph - handle special data the node might have */
	if (nti && nti->remove_from_graph) {
		nti->remove_from_graph(graph, node);
	}
}

/* Free node data but not node itself
 * - Used when removing/replacing old nodes, but also when cleaning up graph 
 */
void DEG_free_node(DepsNode *node)
{
	const DepsNodeTypeInfo *nti = DEG_node_get_typeinfo(node);
	
	if (node) {
		/* free any special type-specific data */
		if (nti && nti->free_data) {
			nti->free_data(node);
		}
		
		/* free links */
		// XXX: review how this works!
		BLI_freelistN(&node->inlinks);
		BLI_freelistN(&node->outlinks);
	}
}

/* Convenience Functions ---------------------------- */

/* Create a new node for representing an operation and add this to graph */
OperationDepsNode *DEG_add_operation(Depsgraph *graph, ID *id, const char subdata[MAX_NAME],
                                     eDepsNode_Type type, eDepsOperation_Type optype, 
                                     DepsEvalOperationCb op, const char name[DEG_MAX_ID_NAME])
{
	OperationDepsNode *op_node = NULL;
	eDepsNode_Type component_type;
	
	/* sanity check */
	if (ELEM3(NULL, graph, id, op))
		return NULL;
	
	/* create operation node (or find an existing but perhaps on partially completed one) */
	op_node = (OperationDepsNode *)DEG_get_node(graph, id, subdata, type, name);
	BLI_assert(op_node != NULL);
	
	/* attach extra data... */
	op_node->evaluate = op;
	op_node->optype = optype;
	
	/* return newly created node */
	return op_node;
}

/* ************************************************** */
/* Relationships Management */

/* Create new relationship that between two nodes, but don't link it in */
DepsRelation *DEG_create_new_relation(DepsNode *from, DepsNode *to,
                                      eDepsRelation_Type type,
                                      const char description[DEG_MAX_ID_NAME])
{
	DepsRelation *rel;
	
	/* create new relationship */
	if (description)
		rel = MEM_callocN(sizeof(DepsRelation), description);
	else
		rel = MEM_callocN(sizeof(DepsRelation), "DepsRelation");
	
	/* populate data */
	rel->from = from;
	rel->to = to;
	
	rel->type = type;
	BLI_strncpy(rel->name, description, DEG_MAX_ID_NAME);
	
	/* return */
	return rel;
}

/* Add relationship to graph */
void DEG_add_relation(DepsRelation *rel)
{
	/* hook it up to the nodes which use it */
	BLI_addtail(&rel->from->outlinks, BLI_genericNodeN(rel));
	BLI_addtail(&rel->to->inlinks,    BLI_genericNodeN(rel));
}

/* Add new relationship between two nodes */
DepsRelation *DEG_add_new_relation(DepsNode *from, DepsNode *to, 
                                   eDepsRelation_Type type, 
                                   const char description[DEG_MAX_ID_NAME])
{
	/* create new relation, and add it to the graph */
	DepsRelation *rel = DEG_create_new_relation(from, to, type, description);
	DEG_add_relation(rel);
	
	return rel;
}

/* Remove relationship from graph */
void DEG_remove_relation(Depsgraph *graph, DepsRelation *rel)
{
	LinkData *ld;
	
	/* sanity check */
	if (ELEM3(NULL, rel, rel->from, rel->to)) {
		return;
	}
	
	/* remove it from the nodes that use it */
	ld = BLI_findptr(&rel->from->outlinks, rel, offsetof(LinkData, data));
	if (ld) {
		BLI_freelinkN(&rel->from->outlinks, ld);
		ld = NULL;
	}
	
	ld = BLI_findptr(&rel->to->inlinks, rel, offsetof(LinkData, data));
	if (ld) {
		BLI_freelinkN(&rel->to->inlinks, rel);
		ld = NULL;
	}
}

/* Free relation and its data */
void DEG_free_relation(DepsRelation *rel)
{
	BLI_assert(rel != NULL);
	BLI_assert((rel->next == rel->prev) && (rel->next == NULL));
	
	/* for now, assume that relation has no data of its own... */
	MEM_freeN(rel);
}

/* ************************************************** */
/* Update Tagging/Flushing */

/* Low-Level Tagging -------------------------------- */

/* Tag a specific node as needing updates */
void DEG_node_tag_update(Depsgraph *graph, DepsNode *node)
{
	/* sanity check */
	if (ELEM(NULL, graph, node))
		return;
		
	/* tag for update, but also not that this was the source of an update */
	node->flag |= (DEPSNODE_FLAG_NEEDS_UPDATE | DEPSNODE_FLAG_DIRECTLY_MODIFIED);
	
	/* add to graph-level set of directly modified nodes to start searching from
	 * NOTE: this is necessary since we have several thousand nodes to play with...
	 */
	BLI_addtail(&graph->entry_tags, BLI_genericNodeN(node));
}

/* Data-Based Tagging ------------------------------- */

/* Tag all nodes in ID-block for update 
 * ! This is a crude measure, but is most convenient for old code
 */
void DEG_id_tag_update(Depsgraph *graph, const ID *id)
{
	DepsNode *node = DEG_find_node(graph, id, NULL, DEPSNODE_TYPE_ID_REF, NULL);
	DEG_node_tag_update(graph, node);
}

/* Tag nodes related to a specific piece of data */
void DEG_data_tag_update(Depsgraph *graph, const PointerRNA *ptr)
{
	DepsNode *node = DEG_find_node_from_pointer(graph, ptr, NULL);
	DEG_node_tag_update(graph, node);
}

/* Tag nodes related to a specific property */
void DEG_property_tag_update(Depsgraph *graph, const PointerRNA *ptr, const PropertyRNA *prop)
{
	DepsNode *node = DEG_find_node_from_pointer(graph, ptr, prop);
	DEG_node_tag_update(graph, node);
}

/* Update Flushing ---------------------------------- */

/* Flush updates from tagged nodes outwards until all affected nodes are tagged */
void DEG_graph_flush_updates(Depsgraph *graph)
{
	LinkData *ld;
	
	/* sanity check */
	if (graph == NULL)
		return;
	
	/* clear count of number of nodes needing updates */
	graph->tagged_count = 0;
	
	/* starting from the tagged "entry" nodes, flush outwards... */
	// XXX: perhaps instead of iterating, we should just push these onto the queue of nodes to check?
	// NOTE: also need to ensure that for each of these, there is a path back to root, or else they won't be done
	// NOTE: count how many nodes we need to handle - entry nodes may be component nodes which don't count for this purpose!
	for (ld = graph->entry_tags.first; ld; ld = ld->next) {
		DepsNode *node = (DepsNode *)ld->data;
		
		/* flush to sub-nodes... */
		// NOTE: if flushing to subnodes, we should then proceed to remove tag(s) from self, as only the subnode tags matter
		
		/* flush to nodes along links... */
		
	}
	
	/* clear entry tags, since all tagged nodes should now be reachable from root */
	BLI_freelistN(&graph->entry_tags);
}

/* Clear tags from all operation nodes */
void DEG_graph_clear_tags(Depsgraph *graph)
{
	LinkData *ld;
	
	/* go over all operation nodes, clearing tags */
	for (ld = graph->all_opnodes.first; ld; ld = ld->next) {
		DepsNode *node = (DepsNode *)ld->data;
		
		/* clear node's "pending update" settings */
		node->flag &= ~(DEPSNODE_FLAG_DIRECTLY_MODIFIED | DEPSNODE_FLAG_NEEDS_UPDATE);
		node->valency = 0; /* reset so that it can be bumped up again */
	}
	
	/* clear any entry tags which haven't been flushed */
	BLI_freelistN(&graph->entry_tags);
}

/* ************************************************** */
/* Public Graph API */

/* Init --------------------------------------------- */

/* Initialise a new Depsgraph */
Depsgraph *DEG_graph_new()
{
	Depsgraph *graph = MEM_callocN(sizeof(Depsgraph), "Depsgraph");
	
	/* initialise hash used to quickly find node associated with a particular ID block */
	graph->id_hash = BLI_ghash_ptr_new("Depsgraph ID NodeHash");
	
	/* return new graph */
	return graph;
}

/* Freeing ------------------------------------------- */

/* wrapper around DEG_free_node() so that it can be used to free nodes stored in hash... */
static void deg_graph_free__node_wrapper(void *node_p)
{
	DEG_free_node((DepsNode *)node_p);
	MEM_freeN(node_p);
}

/* Free graph's contents and graph itself */
void DEG_graph_free(Depsgraph *graph)
{
	/* free node hash */
	BLI_ghash_free(graph->id_hash, NULL, deg_graph_free__node_wrapper);
	graph->id_hash = NULL;
	
	/* free root node - it won't have been freed yet... */
	if (graph->root_node) {
		DEG_free_node(graph->root_node);
		graph->root_node = NULL;
	}
	
	/* free entrypoint tag cache... */
	BLI_freelistN(&graph->entry_tags);
	
	/* finally, graph itself */
	MEM_freeN(graph);
}

/* ************************************************** */

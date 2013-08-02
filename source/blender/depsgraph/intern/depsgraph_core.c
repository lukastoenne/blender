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
	BLI_assert((graph != NULL) && (graph->id_hash != NULL));
	
	/* go over each ID node to recursively call validate_links()
	 * on it, which should be enough to ensure that all of those
	 * subtrees are valid
	 */
	GHASH_ITER(hashIter, graph->id_hash) {
		DepsNode *node = (DepsNode *)BLI_ghashIterator_getValue(hashIter);
		DepsNodeTypeInfo *nti = DEG_node_get_typeinfo(node);
		
		if (nti && nti->validate_links) {
			nti->validate_links(graph, node);
		}
	}
	BLI_ghashIterator_free(hashIter);
}

/* ************************************************** */
/* Low-Level Graph Traversal and Sorting */

/* Sort nodes to determine evaluation order for operation nodes
 * where dependency relationships won't get violated.
 */
void DEG_graph_sort(Depsgraph *graph)
{

}

/* ************************************************** */
/* Node Management */

/* Node Finding ------------------------------------- */
// XXX: should all this node finding stuff be part of low-level query api?

/* helper for finding inner nodes by their names */
DepsNode *deg_find_inner_node(Depsgraph *graph, ID *id, eDepsNode_Type component_type, 
                              eDepsNode_Type type, const char name[DEG_MAX_ID_NAME])
{
	ComponentDepsNode *component = DEG_find_node(graph, component_type, id, NULL);
	
	if (component) {
		/* lookup node with matching name... */
		DepsNode *node = BLI_ghash_lookup(component->ophash, name);
		
		if (node) {
			/* make sure type matches too... just in case */
			BLI_assert(node->type == type);
			return node;
		}
	}
	
	/* no match... */
	return NULL;
}

/* Find matching node */
DepsNode *DEG_find_node(Depsgraph *graph, ID *id, eDepsNode_Type type, const char name[DEG_MAX_ID_NAME])
{
	DepsNode *result = NULL;
	
	/* each class of types requires a different search strategy... */
	switch (type) {
		/* "Generic" Types -------------------------- */
		case DEPSNODE_TYPE_ROOT:   /* NOTE: this case shouldn't need to exist, but just in case... */
			result = graph->root_node;
			break;
			
		case DEPSNODE_TYPE_TIMESOURCE: /* Time Source */
		{
			/* search for one attached to a particular ID? */
			if (id) {
				/* check if it was added as a component 
				 * (as may be done for subgraphs needing timeoffset) 
				 */
				// XXX: review this
				IDDepsNode *id_node = BLI_ghash_lookup(graph->id_hash, id);
				
				if (id_node) {
					result = BLI_ghash_lookup(id_node->component_hash,
					                          SET_INT_IN_POINTER(type));
				}
			}
			else {
				/* use "official" timesource */
				RootDepsNode *root_node = (RootDepsNode *)graph->root_node;
				result = root_node->time_source;
			}
		}
			break;
		
		case DEPSNODE_TYPE_ID_REF: /* ID Block Index/Reference */
		{
			/* lookup relevant ID using nodehash */
			result = BLI_ghash_lookup(graph->id_hash, id);
		}	
			break;
			
		/* "Outer" Nodes ---------------------------- */
		
		case DEPSNODE_TYPE_PARAMETERS: /* Components... */
		case DEPSNODE_TYPE_PROXY:
		case DEPSNODE_TYPE_ANIMATION:
		case DEPSNODE_TYPE_TRANSFORM:
		case DEPSNODE_TYPE_GEOMETRY:
		case DEPSNODE_TYPE_EVAL_POSE:
		case DEPSNODE_TYPE_EVAL_PARTICLES:
		{
			/* Each ID-Node knows the set of components that are associated with it */
			IDDepsNode *id_node = BLI_ghash_lookup(graph->id_hash, id);
			
			if (id_node) {
				result = BLI_ghash_lookup(id_node->component_hash, type);
			}
		}
			break;
		
		/* "Inner" Nodes ---------------------------- */
		
		case DEPSNODE_TYPE_OP_PARAMETER:  /* Parameter Related Ops */
			result = deg_find_inner_node(graph, id, DEPSNODE_TYPE_PARAMETERS, type, name);
			break;
		case DEPSNODE_TYPE_OP_PROXY:      /* Proxy Ops */
			result = deg_find_inner_node(graph, id, DEPSNODE_TYPE_PROXY, type, name);
			break;
		case DEPSNODE_TYPE_OP_TRANSFORM:  /* Transform Ops */
			result = deg_find_inner_node(graph, id, DEPSNODE_TYPE_TRANSFORM, type, name);
			break;
		case DEPSNODE_TYPE_OP_ANIMATION:  /* Animation Ops */
			result = deg_find_inner_node(graph, id, DEPSNODE_TYPE_ANIMATION, type, name);
			break;
		case DEPSNODE_TYPE_OP_GEOMETRY:   /* Geometry Ops */
			result = deg_find_inner_node(graph, id, DEPSNODE_TYPE_GEOMETRY, type, name);
			break;
			
		case DEPSNODE_TYPE_OP_UPDATE:     /* Updates */
			result = deg_find_inner_node(graph, id, DEPSNODE_TYPE_PARAMETERS, type, name);
			break;
		case DEPSNODE_TYPE_OP_DRIVER:     /* Drivers */
			result = deg_find_inner_node(graph, id, DEPSNODE_TYPE_PARAMETERS, type, name);
			break;
			
		case DEPSNODE_TYPE_OP_BONE:       /* Bone */
			result = deg_find_inner_node(graph, id, DEPSNODE_TYPE_EVAL_POSE, type, name);
			break;
		case DEPSNODE_TYPE_OP_PARTICLE:  /* Particle System/Step */
			result = deg_find_inner_node(graph, id, DEPSNODE_TYPE_EVAL_PARTICLE, type, name);
			break;
			
		case DEPSNODE_TYPE_OP_RIGIDBODY: /* Rigidbody Sim */
			result = deg_find_inner_node(graph, id, DEPSNODE_TYPE_TRANSFORM, type, name); // XXX: needs review
			break;
		
		default:
			/* Unhandled... */
			printf("%s(): Unknown node type %d\n", __func__, type);
			break;
	}
	
	return result;
}

/* Get Node ----------------------------------------- */

/* Get a matching node, creating one if need be */
DepsNode *DEG_get_node(Depsgraph *graph, ID *id, eDepsNode_Type type, const char name[DEG_MAX_ID_NAME])
{
	DepsNode *node;
	
	/* firstly try to get an existing node... */
	node = DEG_find_node(graph, type, id, name);
	if (node == NULL) {
		/* nothing exists, so create one instead! */
		node = DEG_add_new_node(graph, type, id, name);
	}
	
	/* return the node - it must exist now... */
	return node;
}

/* Get DepsNode referred to by data path.
 *
 * < graph: Depsgraph to find node from
 * < id: ID-Block that path is rooted on
 * < path: RNA-Path to resolve
 * > returns: (IDDepsNode | DataDepsNode) as appropriate
 */
DepsNode *DEG_get_node_from_rna_path(Depsgraph *graph, const ID *id, const char path[])
{
	PointerRNA id_ptr, ptr;
	DepsNode *node = NULL;
	
	/* create ID pointer for root of path lookup */
	RNA_id_pointer_create(id, &id_ptr);
	
	/* try to resolve path... */
	if (RNA_path_resolve(&id_ptr, path, &ptr, NULL)) {
#if 0
		/* exact type of data to query depends on type of ptr we've got (search code is dumb!) */
		if (RNA_struct_is_ID(ptr.type)) {
			node = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, ptr.id, NULL, NULL);
		}
		else {
			node = DEG_get_node(graph, DEPSNODE_TYPE_DATA, ptr.id, ptr.type, ptr.data);
		}
#endif
	}
	
	/* return node found */
	return node;
}

/* Add/Remove/Copy/Free ------------------------------- */

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
void DEG_add_node(Depsgraph *graph, DepsNode *node, ID *id)
{
	const DepsNodeTypeInfo *nti = DEG_node_get_typeinfo(node);
	
	if (node && nti) {
		nti->add_to_graph(graph, node, id);
	}
}

/* Add a new node */
DepsNode *DEG_add_new_node(Depsgraph *graph, ID *id, eDepsNode_Type type, const char name[DEG_MAX_ID_NAME])
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
		nti->init_data(node, id);
	}
	
	/* add node to graph 
	 * NOTE: additional nodes may be created in order to add this node to the graph
	 *       (i.e. parent/owner nodes) where applicable...
	 */
	DEG_add_node(graph, node, id);
	
	/* return the newly created node matching the description */
	return node;
}

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
OperationDepsNode *DEG_add_operation(Depsgraph *graph, ID *id, eDepsNode_Type type,
                                     eDepsOperation_Type optype, DepsEvalOperationCb op,
                                     const char name[DEG_MAX_ID_NAME])
{
	OperationDepsNode *op_node = NULL;
	eDepsNode_Type component_type;
	
	/* sanity check */
	if (ELEM3(NULL, graph, id, op))
		return NULL;
	
	/* create operation node (or find an existing but perhaps on partially completed one) */
	op_node = (OperationDepsNode *)DEG_get_node(graph, id, type, name);
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
	rel->from = form;
	rel->to = to;
	
	rel->type = type;
	BLI_strncpy(rel->description, description, DEG_MAX_ID_NAME);
	
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
		BLI_freelinkN(&rel->to->inlinks, rel, offsetof(LinkData, data));
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
	DepsNode *node = DEG_find_node(graph, id, DEPSNODE_TYPE_ID_REF, NULL);
	DEG_node_tag_update(graph, node);
}

/* Tag nodes related to a specific piece of data */
void DEG_data_tag_update(Depsgraph *graph, const PointerRNA *ptr)
{
	// ... querying api needed ...
}

/* Tag nodes related to a specific property */
void DEG_property_tag_update(Depsgraph *graph, const PointerRNA *ptr, const PropertyRNA *prop)
{
	// ... querying api needed ...
}

/* Update Flushing ---------------------------------- */

/* Flush updates from tagged nodes outwards until all affected nodes are tagged */
void DEG_graph_flush_updates(Depsgraph *graph)
{
	LinkData *ld;
	
	/* sanity check */
	if (graph == NULL)
		return;
	
	/* starting from the tagged "entry" nodes, flush outwards... */
	// NOTE: also need to ensure that for each of these, there is a path back to root, or else they won't be done
	for (ld = graph->entry_tags; ld; ld = ld->next) {
		DepsNode *node = ld->data;
		
		/* flush to sub-nodes... */
		
		/* flush to nodes along links... */
		
	}
	
	/* clear entry tags, since all tagged nodes should now be reachable from root */
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
	graph->id_hash = BLI_ghash_ptr_new("Depsgraph NodeHash");
	
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

/* Free graph's contents, but not graph itself */
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
}

/* ************************************************** */

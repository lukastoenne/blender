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
#include "BLI_utildefines.h"

#include "BKE_depsgraph.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "depsgraph_types.h"
#include "depsgraph_intern.h"

/* ************************************************** */
/* Low-Level Dependency Graph Traversal / Sorting / Validity + Integrity */

/* ************************************************** */
/* Node Management */

/* Node Finding ------------------------------------- */
// XXX: should all this node finding stuff be part of low-level query api?

/* find node where type matches (and outer-match function returns true) */
static DepsNode *deg_find_node__generic_type_match(Depsgraph *graph, eDepsNode_Type type, ID *id, StructRNA *srna, void *data)
{
	DepsNodeTypeInfo *nti = DEG_get_node_typeinfo(type);
	DepsNode *node;
	
	for (node = graph->nodes.first; node; node = node->next) {
		if (node->type == type) {
			if (nti->match_outer) {
				if (nti->match_outer(node, id, srna, data)) {
					/* match! */
					return node;
				}
				/* else: not found yet... */
			}
			else {
				/* assume match - with only one of this sort */
				return node;
			}
		}
	}
	
	/* not found */
	return NULL;
}

/* find node where id matches or is contained in the node... */
static DepsNode *deg_find_node__id_match(Depsgraph *graph, ID *id)
{
	/* use graph's ID-hash to quickly jump to relevant node */
	return BLI_ghash_lookup(graph->nodehash, id);
}

/* find node where data matches */
static DepsNode *deg_find_node__data_match(Depsgraph *graph, ID *id, StructRNA *srna, void *data)
{
	/* first, narrow-down the search space to the ID-block this data is attached to */
	DepsNode *id_node = deg_find_node__id_match(graph, id);
	
	if (id_node) {
		const OuterIdDepsNodeTemplate *outer_data = (OuterIdDepsNodeTemplate *)id_node;
		const DepsNodeTypeInfo *nti = DEG_get_node_typeinfo(DEPSNODE_TYPE_DATA);
		DepsNode *node;
		
		/* find data-node within this ID-block which matches this */
		for (node = outer_data->subdata.first; node; node = node->next) {
			BLI_assert(node->type == DEPSNODE_TYPE_DATA);
			
			if (nti->match_outer(node, id, srna, data)) {
				/* match! */
				return node;
			}
		}
	}
	
	/* no matching nodes found */
	return NULL;
}


/* Find matching node */
DepsNode *DEG_find_node(Depsgraph *graph, eDepsNode_Type type, ID *id, StructRNA *srna, void *data)
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
			/* there can only be one "official" timesource for now */
			// XXX: what happens if we want the timesource for a subgraph?
			RootDepsNode *root_node = (RootDepsNode *)graph->root_node;
			result = root_node->time_source;
		}
			break;
			
		/* "Outer" Nodes ---------------------------- */
		
		case DEPSNODE_TYPE_OUTER_GROUP: /* Group Nodes */
		case DEPSNODE_TYPE_OUTER_OP:    /* Generic Operation Wrapper */
		{
			result = deg_find_node__generic_type_match(graph, type, id, srna, data);
		}
			break;
			
		case DEPSNODE_TYPE_OUTER_ID:    /* ID or Group nodes */
		{
			/* return the ID or Group node, not just ID, 
			 * as ID may have already been added to a group
			 * due to cycles appearing
			 */
			result = deg_find_node__id_match(graph, id);
		}
			break;
			
		case DEPSNODE_TYPE_DATA:        /* Data (i.e. Bones, Drivers, etc.) */
		{
			result = deg_find_node__data_match(graph, id, srna, data);
		}
			break;
			
		default:
			/* Unhandled... */
			break;
	}
	
	return result;
}

/* Get Node ----------------------------------------- */

/* Get a matching node, creating one if need be */
DepsNode *DEG_get_node(Depsgraph *graph, eDepsNode_Type type, ID *id, StructRNA *srna, void *data)
{
	DepsNode *node;
	
	/* firstly try to get an existing node... */
	node = DEG_find_node(graph, type, id, srna, data);
	if (node == NULL) {
		/* nothing exists, so create one instead! */
		node = DEG_add_new_node(graph, type, id, srna, data);
	}
	
	/* return the node - it must exist now... */
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
	node->type = type;
	
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

/* Add a new outer node */
DepsNode *DEG_add_new_node(Depsgraph *graph, eDepsNode_Type type, ID *id, StructRNA *srna, void *data)
{
	const DepsNodeTypeInfo *nti = DEG_get_node_typeinfo(type);
	DepsNode *node;
	
	BLI_assert(nti != NULL);
	
	/* create node data... */
	node = deg_create_node(type);
	
	/* type-specific data init
	 * NOTE: this is not included as part of create_node() as
	 *       some methods may want/need to override this step
	 */
	if (nti->init_data) {
		nti->init_data(node, id, srna, data);
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
	
	if (node && nti) {
		/* relationships */
		// XXX: for now, they're just left in place...
		
		/* remove node from graph - general */
		nti->remove_from_graph(graph, node);
	}
}

/* Create a copy of provided node */
// FIXME: the handling of sub-nodes and links will need to be subject to filtering options...
// FIXME: copying nodes is probably more at the heart of the querying + filtering API
DepsNode *DEG_copy_node(const DepsNode *src)
{
	const DepsNodeTypeInfo *nti = DEG_get_node_typeinfo(type);
	DepsNode *dst;
	
	/* sanity check */
	if (src == NULL)
		return NULL;
	
	/* allocate new node, and brute-force copy over all "basic" data */
	dst = deg_create_node(src->type);
	memcpy(dst, src, nti->size);
	
	/* now, fix up any links in standard "node header" (i.e. DepsNode struct, that all 
	 * all others are derived from) that are now corrupt 
	 */
	{
		/* not assigned to graph... */
		dst->next = dst->prev = NULL;
		dst->owner = NULL;
		
		/* make a new copy of name (if necessary) */
		if (dst->flag & DEPSNODE_FLAG_NAME_NEEDS_FREE) {
			dst->name = BLI_strdup(dst->name);
		}
		
		/* relationships to other nodes... */
		// FIXME: how to handle links? We may only have partial set of all nodes still?
		// XXX: the exact details of how to handle this are really part of the querying API...
		
		// XXX: BUT, for copying subgraphs, we'll need to define an API for doing this stuff anyways
		// (i.e. for resolving and patching over links that exist within subtree...)
		dst->inlinks.first = dst->inlinks.last = NULL;
		dst->outlinks.first = dst->outlinks.last = NULL;
		
		/* clear traversal data */
		dst->valency = 0;
		dst->lasttime = 0;
	}
	
	/* fix up type-specific data (and/or subtree...) */
	if (nti->copy_data) {
		nti->copy_data(dst, src);
	}
	
	/* return copied node */
	return dst;
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
		
		/* free custom name */
		if (node->flag & DEPSNODE_FLAG_NAME_NEEDS_FREE) {
			if (node->name) 
				MEM_freeN(node->name);
			node->name = NULL;
		}
	}
}

/* ************************************************** */
/* Relationships Management */

/* Create new relationship that between two nodes, but don't link it in */
DepsRelation *DEG_create_new_relation(DepsNode *from, DepsNode *to,
                                      eDepsRelation_Type type,
                                      const char *description)
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
	rel->description = description;
	
	/* return */
	return rel;
}

/* Add relationship to graph */
void DEG_add_relation(Depsgraph *graph, DepsRelation *rel)
{
	/* hook it up to the nodes which use it */
	BLI_addtail(&from->outlinks, BLI_genericNodeN(rel));
	BLI_addtail(&to->inlinks, BLI_genericNodeN(rel));
	
	/* store copy of the relation at top-level for safekeeping */
	// XXX: is there any good reason to keep doing this?
	BLI_addtail(&graph->relations, rel);
}

/* Add new relationship between two nodes */
DepsRelation *DEG_add_new_relation(Depsgraph *graph, DepsNode *from, DepsNode *to,
                                   eDepsRelation_Type type, const char *description)
{
	/* create new relation, and add it to the graph */
	DepsRelation *rel = DEG_create_new_relation(from, to, type, description);
	DEG_add_relation(graph, rel);
	
	return rel;
}

/* Make a copy of a relationship */
DepsRelation DEG_copy_relation(const DepsRelation *src)
{
	DepsRelation *dst = MEM_dupallocN(src);
	
	/* clear out old pointers which no-longer apply */
	dst->next = dst->prev = NULL;
	
	/* return copy */
	return dst;
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
	
	/* remove relation from the graph... */
	BLI_remlink(&graph->relations, rel);
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
/* Public API */

/* Initialise a new Depsgraph */
Depsgraph *DEG_graph_new()
{
	Depsgraph *graph = MEM_callocN(sizeof(Depsgraph), "Depsgraph");
	
	/* initialise nodehash - hash table for quickly finding outer nodes corresponding to ID's */
	graph->nodehash = BLI_ghash_ptr_new("Depsgraph NodeHash");
	
	/* type of depsgraph */
	graph->type = 0; // XXX: in time, this can be fleshed out, but for now, this means "main scene/eval one"
	
	/* return new graph */
	return graph;
}

/* Free graph's contents, but not graph itself */
void DEG_graph_free(Depsgraph *graph)
{
	
	/* free relationships */
	
	/* free nodes */
	
}


/* ************************************************** */
/* Evaluation */


/* ************************************************** */

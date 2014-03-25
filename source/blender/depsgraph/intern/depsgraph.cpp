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
 
//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>

#include "MEM_guardedalloc.h"

extern "C" {
//#include "BLI_blenlib.h"
//#include "BLI_string.h"
//#include "BLI_utildefines.h"

//#include "DNA_defs.h"
//#include "DNA_ID.h"
//#include "DNA_listBase.h"

//#include "DEG_depsgraph.h"

//#include "RNA_access.h"
//#include "RNA_types.h"
} /* extern "C" */

#include "depsgraph.h" /* own include */
//#include "depsgraph_eval.h"
//#include "depsgraph_types.h"
#include "depsgraph_intern.h"


/* ************************************************** */
/* Node Management */

IDDepsNode *Depsgraph::find_id_node(const ID *id) const
{
	IDNodeMap::const_iterator it = this->id_hash.find(id);
	return it != this->id_hash.end() ? it->second : NULL;
}

/* Get Node ----------------------------------------- */

/* Get a matching node, creating one if need be */
DepsNode *DEG_get_node(Depsgraph *graph, const ID *id, const char *subdata,
                       eDepsNode_Type type, const char name[DEG_MAX_ID_NAME])
{
	DepsNode *node;
	
	/* firstly try to get an existing node... */
	node = graph->find_node(id, subdata, type, name);
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

/* Add a new node */
DepsNode *DEG_add_new_node(Depsgraph *graph, const ID *id, const char *subdata,
                           eDepsNode_Type type, const char name[DEG_MAX_ID_NAME])
{
	DepsNode *node;
	
	DepsNodeFactory *factory = DEG_get_node_factory(type);
	BLI_assert(factory != NULL);
	
	/* create node data... */
	node = factory->create_node(id, subdata, name);
	
	/* add node to graph 
	 * NOTE: additional nodes may be created in order to add this node to the graph
	 *       (i.e. parent/owner nodes) where applicable...
	 */
	node->add_to_graph(graph, id);
	
	/* add node to operation-node list if it plays a part in the evaluation process */
	if (ELEM(node->tclass, DEPSNODE_CLASS_GENERIC, DEPSNODE_CLASS_OPERATION)) {
		graph->all_opnodes.push_back(node);
	}
	
	DEG_debug_build_node_added(node);
	
	/* return the newly created node matching the description */
	return node;
}

/* Remove/Free ---------------------------------------- */

/* Remove node from graph, but don't free any of its data */
void DEG_remove_node(Depsgraph *graph, DepsNode *node)
{
	if (node == NULL)
		return;
	
	/* relationships 
	 * - remove these, since they're at the same level as the
	 *   node itself (inter-relations between sub-nodes will
	 *   still remain and/or can still work that way)
	 */
	DEPSNODE_RELATIONS_ITER_BEGIN(node->inlinks, rel)
		delete rel;
	DEPSNODE_RELATIONS_ITER_END;
	
	DEPSNODE_RELATIONS_ITER_BEGIN(node->outlinks, rel)
		delete rel;
	DEPSNODE_RELATIONS_ITER_END;
	
	/* remove node from graph - handle special data the node might have */
	node->remove_from_graph(graph);
}

/* Convenience Functions ---------------------------- */

/* Create a new node for representing an operation and add this to graph */
OperationDepsNode *DEG_add_operation(Depsgraph *graph, ID *id, const char subdata[MAX_NAME],
                                     eDepsNode_Type type, eDepsOperation_Type optype, 
                                     DepsEvalOperationCb op, const char name[DEG_MAX_ID_NAME])
{
	OperationDepsNode *op_node = NULL;
	
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

DepsRelation::DepsRelation(DepsNode *from, DepsNode *to, eDepsRelation_Type type, const char *description)
{
	this->from = from;
	this->to = to;
	this->type = type;
	BLI_strncpy(this->name, description, DEG_MAX_ID_NAME);
	
	/* hook it up to the nodes which use it */
	from->outlinks.insert(this);
	to->inlinks.insert(this);
}

DepsRelation::~DepsRelation()
{
	/* sanity check */
	BLI_assert(this->from && this->to);
	/* remove it from the nodes that use it */
	this->from->outlinks.erase(this);
	this->to->inlinks.erase(this);
}

/* Add new relationship between two nodes */
DepsRelation *DEG_add_new_relation(DepsNode *from, DepsNode *to, 
                                   eDepsRelation_Type type, 
                                   const char description[DEG_MAX_ID_NAME])
{
	/* create new relation, and add it to the graph */
	DepsRelation *rel = new DepsRelation(from, to, type, description);
	
	DEG_debug_build_relation_added(rel);
	
	return rel;
}

/* ************************************************** */
/* Public Graph API */

Depsgraph::Depsgraph()
{
	this->root_node = NULL;
}

Depsgraph::~Depsgraph()
{
	/* free node hash */
	for (Depsgraph::IDNodeMap::const_iterator it = this->id_hash.begin(); it != this->id_hash.end(); ++it) {
		DepsNode *node = it->second;
		delete node;
	}
	
	/* free root node - it won't have been freed yet... */
	if (this->root_node) {
		delete this->root_node;
	}
}

/* Init --------------------------------------------- */

/* Initialise a new Depsgraph */
Depsgraph *DEG_graph_new()
{
	return new Depsgraph;
}

/* Freeing ------------------------------------------- */

/* Free graph's contents and graph itself */
void DEG_graph_free(Depsgraph *graph)
{
	delete graph;
}

/* ************************************************** */

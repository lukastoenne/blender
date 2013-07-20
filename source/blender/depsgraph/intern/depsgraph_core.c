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

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_depsgraph.h"

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
	DepsNodeTypeInfo *nti = DEG_get_typeinfo(type);
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
	// XXX: remember to remove existing entries when grouping nodes...
	return BLI_ghash_lookup(graph->nodehash, id);
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
			DepsNode *id_node = deg_find_node__id_match(graph, id);
			
			if (id_node) {
				/* find data-node which matches this */
				
			}
		}
			break;
	}
	
	return result;
}

/* Get/Add Node ----------------------------------------- */

/* Add a new outer node */
DepsNode *DEG_add_node(Depsgraph *graph, eDepsNode_Type type, ID *id, StructRNA *srna, void *data)
{
	const DepsNodeTypeInfo *nti = DEG_get_typeinfo(type);
	DepsNode *node = NULL;
	
	BLI_assert(nti != NULL);
	
	/* create node data... */
	node = MEM_callocN(nti->size, nti->name);
	node->type = type;
	
	/* node-specific data init */
	if (nti->init_data) {
		nti->init_data(node, id, srna, data);
	}
	
	/* add node to graph */
	// XXX: remember to add to both nodelist + nodehash (when adding to toplevel)
	nti->add_to_graph(graph, node);
	
	/* return the newly created node matching the description */
	return node;
}

/* Get a matching node, creating one if need be */
DepsNode *DEG_get_node(Depsgraph *graph, eDepsNode_Type type, ID *id, StructRNA *srna, void *data)
{
	DepsNode *node;
	
	/* firstly try to get an existing node... */
	node = DEG_find_node(graph, type, id, srna, data);
	if (node == NULL) {
		/* nothing exists, so create one instead! */
		node = DEG_add_node(graph, type, id, srna, data);
	}
	
	/* return the node - it must exist now... */
	return node;
}

/* ************************************************** */
/* Public API */


/* ************************************************** */
/* Evaluation */


/* ************************************************** */

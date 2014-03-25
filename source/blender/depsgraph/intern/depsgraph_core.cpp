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

extern "C" {
#include "BLI_blenlib.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_defs.h"
#include "DNA_ID.h"
#include "DNA_listBase.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_types.h"
} /* extern "C" */

#include "depsgraph.h"
#include "depsgraph_eval.h"
#include "depsgraph_types.h"
#include "depsgraph_intern.h"

/* ************************************************** */
/* Validity + Integrity */

/* Ensure that all implicit constraints between nodes are satisfied 
 * (e.g. components are only allowed to be executed in a certain order)
 */
void DEG_graph_validate_links(Depsgraph *graph)
{
	BLI_assert(graph != NULL);
	
	/* go over each ID node to recursively call validate_links()
	 * on it, which should be enough to ensure that all of those
	 * subtrees are valid
	 */
	for (Depsgraph::IDNodeMap::const_iterator it = graph->id_hash.begin(); it != graph->id_hash.end(); ++it) {
		DepsNode *node = it->second;
		node->validate_links(graph);
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

/* Add ------------------------------------------------ */

DepsNode::TypeInfo::TypeInfo(eDepsNode_Type type_, const char *tname_)
{
	this->type = type_;
	if (type_ < DEPSNODE_TYPE_PARAMETERS)
		this->tclass = DEPSNODE_CLASS_GENERIC;
	else if (type_ < DEPSNODE_TYPE_OP_PARAMETER)
		this->tclass = DEPSNODE_CLASS_COMPONENT;
	else
		this->tclass = DEPSNODE_CLASS_OPERATION;
	this->tname = tname_;
}

DepsNode::DepsNode()
{
	this->name[0] = '\0';
}

DepsNode::~DepsNode()
{
	/* free links
	 * note: deleting relations will remove them from the node relations set,
	 * but only touch the same position as we are using here, which is safe.
	 */
	DEPSNODE_RELATIONS_ITER_BEGIN(this->inlinks, rel)
		delete rel;
	DEPSNODE_RELATIONS_ITER_END;
	
	DEPSNODE_RELATIONS_ITER_BEGIN(this->outlinks, rel)
		delete rel;
	DEPSNODE_RELATIONS_ITER_END;
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
	graph->entry_tags.insert(node);
}

/* Data-Based Tagging ------------------------------- */

/* Tag all nodes in ID-block for update 
 * ! This is a crude measure, but is most convenient for old code
 */
void DEG_id_tag_update(Depsgraph *graph, const ID *id)
{
	DepsNode *node = graph->find_node(id, NULL, DEPSNODE_TYPE_ID_REF, NULL);
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
	/* sanity check */
	if (graph == NULL)
		return;
	
	/* starting from the tagged "entry" nodes, flush outwards... */
	// XXX: perhaps instead of iterating, we should just push these onto the queue of nodes to check?
	// NOTE: also need to ensure that for each of these, there is a path back to root, or else they won't be done
	// NOTE: count how many nodes we need to handle - entry nodes may be component nodes which don't count for this purpose!
	for (Depsgraph::EntryTags::const_iterator it = graph->entry_tags.begin(); it != graph->entry_tags.end(); ++it) {
		DepsNode *node = *it;
		
		/* flush to sub-nodes... */
		// NOTE: if flushing to subnodes, we should then proceed to remove tag(s) from self, as only the subnode tags matter
		
		/* flush to nodes along links... */
		
	}
	
	/* clear entry tags, since all tagged nodes should now be reachable from root */
	graph->entry_tags.clear();
}

/* Clear tags from all operation nodes */
void DEG_graph_clear_tags(Depsgraph *graph)
{
	/* go over all operation nodes, clearing tags */
	for (Depsgraph::OperationNodes::const_iterator it = graph->all_opnodes.begin(); it != graph->all_opnodes.end(); ++it) {
		DepsNode *node = *it;
		
		/* clear node's "pending update" settings */
		node->flag &= ~(DEPSNODE_FLAG_DIRECTLY_MODIFIED | DEPSNODE_FLAG_NEEDS_UPDATE);
		node->num_links_pending = 0; /* reset so that it can be bumped up again */
	}
	
	/* clear any entry tags which haven't been flushed */
	graph->entry_tags.clear();
}

/* ************************************************** */

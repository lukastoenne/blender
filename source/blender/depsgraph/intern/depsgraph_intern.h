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

#include <stdlib.h>

extern "C" {
#include "BLI_string.h"
#include "BLI_utildefines.h"
} /* extern "C" */

#include "depsgraph.h"
#include "depsnode.h"
#include "depsgraph_types.h"

struct Main;
struct Group;
struct Scene;

/* Low-Level Querying ============================================== */

/* Graph Validity -------------------------------------------------- */

/* Ensure that all implicit constraints between nodes are satisfied 
 * (e.g. components are only allowed to be executed in a certain order)
 */
void DEG_graph_validate_links(Depsgraph *graph);


/* Sort nodes to determine evaluation order for operation nodes
 * where dependency relationships won't get violated.
 */
void DEG_graph_sort(Depsgraph *graph);

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
	virtual const string &tname() const = 0;
	
	virtual DepsNode *create_node(const ID *id, const string &subdata, const string &name) const = 0;
	virtual DepsNode *copy_node(DepsgraphCopyContext *dcc, const DepsNode *copy) const = 0;
};

template <class NodeType>
struct DepsNodeFactoryImpl : public DepsNodeFactory {
	eDepsNode_Type type() const { return NodeType::typeinfo.type; }
	eDepsNode_Class tclass() const { return NodeType::typeinfo.tclass; }
	const string &tname() const { return NodeType::typeinfo.tname; }
	
	DepsNode *create_node(const ID *id, const string &subdata, const string &name) const
	{
		DepsNode *node = new NodeType();
		
		/* populate base node settings */
		node->type = type();
		node->tclass = tclass();
		
		if (!name.empty())
			/* set name if provided ... */
			node->name = name;
		else
			/* ... otherwise use default type name */
			node->name = tname();
		
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
		node->name = copy->name;
		
		node->copy(dcc, static_cast<const NodeType *>(copy));
		
		return node;
	}
};

/* Typeinfo Management -------------------------------------------------- */

/* Register typeinfo */
void DEG_register_node_typeinfo(DepsNodeFactory *factory);

/* Get typeinfo for specified type */
DepsNodeFactory *DEG_get_node_factory(const eDepsNode_Type type);

/* Get typeinfo for provided node */
DepsNodeFactory *DEG_node_get_factory(const DepsNode *node);

/* Debugging ========================================================= */

void DEG_debug_build_node_added(const DepsNode *node);
void DEG_debug_build_relation_added(const DepsRelation *rel);


#endif // __DEPSGRAPH_INTERN_H__

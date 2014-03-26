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

#include <stdlib.h>

#include "BLI_utildefines.h"

extern "C" {
#include "DNA_ID.h"
}

#include "depsnode.h" /* own include */
#include "depsnode_component.h"
#include "depsgraph.h"
#include "depsgraph_intern.h"

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


/* ******************************************************** */
/* Generic Nodes */

/* Root Node ============================================== */

/* Add 'root' node to graph */
void RootDepsNode::add_to_graph(Depsgraph *graph, const ID *UNUSED(id))
{
	BLI_assert(graph->root_node == NULL);
	graph->root_node = this;
}

/* Remove 'root' node from graph */
void RootDepsNode::remove_from_graph(Depsgraph *graph)
{
	BLI_assert(graph->root_node == this);
	graph->root_node = NULL;
}

DEG_DEPSNODE_DEFINE(RootDepsNode, DEPSNODE_TYPE_ROOT, "Root DepsNode");
static DepsNodeFactoryImpl<RootDepsNode> DNTI_ROOT;

/* Time Source Node ======================================= */

/* Add 'time source' node to graph */
void TimeSourceDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	/* determine which node to attach timesource to */
	if (id) {
		/* get ID node */
//		DepsNode *id_node = graph->get_node(id, NULL, DEPSNODE_TYPE_ID_REF, NULL);
		
		/* depends on what this is... */
		switch (GS(id->name)) {
			case ID_SCE: /* Scene - Usually sequencer strip causing time remapping... */
			{
				// TODO...
			}
			break;
			
			case ID_GR: /* Group */
			{
				// TODO...
			}
			break;
			
			// XXX: time source...
			
			default:     /* Unhandled */
				printf("%s(): Unhandled ID - %s \n", __func__, id->name);
				break;
		}
	}
	else {
		/* root-node */
		graph->root_node->time_source = this;
		this->owner = graph->root_node;
	}
}

/* Remove 'time source' node from graph */
void TimeSourceDepsNode::remove_from_graph(Depsgraph *graph)
{
	BLI_assert(this->owner != NULL);
	
	switch(this->owner->type) {
		case DEPSNODE_TYPE_ROOT: /* root node - standard case */
		{
			graph->root_node->time_source = NULL;
			this->owner = NULL;
		}
		break;
		
		// XXX: ID node - as needed...
		
		default: /* unhandled for now */
			break;
	}
}

DEG_DEPSNODE_DEFINE(TimeSourceDepsNode, DEPSNODE_TYPE_TIMESOURCE, "Time Source");
static DepsNodeFactoryImpl<TimeSourceDepsNode> DNTI_TIMESOURCE;

/* ID Node ================================================ */

/* Initialise 'id' node - from pointer data given */
void IDDepsNode::init(const ID *id, const char *UNUSED(subdata))
{
	/* store ID-pointer */
	BLI_assert(id != NULL);
	this->id = (ID *)id;
	
	/* NOTE: components themselves are created if/when needed.
	 * This prevents problems with components getting added 
	 * twice if an ID-Ref needs to be created to house it...
	 */
}

/* Free 'id' node */
IDDepsNode::~IDDepsNode()
{
	for (IDDepsNode::ComponentMap::const_iterator it = this->components.begin(); it != this->components.end(); ++it) {
		const ComponentDepsNode *comp = it->second;
		delete comp;
	}
}

/* Copy 'id' node */
void IDDepsNode::copy(DepsgraphCopyContext *dcc, const IDDepsNode *src)
{
	/* iterate over items in original hash, adding them to new hash */
	for (IDDepsNode::ComponentMap::const_iterator it = this->components.begin(); it != this->components.end(); ++it) {
		/* get current <type : component> mapping */
		eDepsNode_Type c_type   = it->first;
		DepsNode *old_component = it->second;
		
		/* make a copy of component */
		ComponentDepsNode *component     = (ComponentDepsNode *)DEG_copy_node(dcc, old_component);
		
		/* add new node to hash... */
		this->components[c_type] = component;
	}
	
	// TODO: perform a second loop to fix up links?
}

/* Add 'id' node to graph */
void IDDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	/* add to hash so that it can be found */
	graph->id_hash[id] = this;
}

/* Remove 'id' node from graph */
void IDDepsNode::remove_from_graph(Depsgraph *graph)
{
	/* remove toplevel node and hash entry, but don't free... */
	graph->id_hash.erase(this->id);
}

/* Validate links between components */
void IDDepsNode::validate_links(Depsgraph *graph)
{
#if 0
	/* XXX WARNING: using ListBase is dangerous for virtual C++ classes,
	 * loses vtable info!
	 * Disabled for now due to unclear purpose, later use a std::vector or similar here
	 */
	
	ListBase dummy_list = {NULL, NULL}; // XXX: perhaps this should live in the node?
	
	/* get our components ......................................................................... */
	ComponentDepsNode *params = find_component(DEPSNODE_TYPE_PARAMETERS);
	ComponentDepsNode *anim = find_component(DEPSNODE_TYPE_ANIMATION);
	ComponentDepsNode *trans = find_component(DEPSNODE_TYPE_TRANSFORM);
	ComponentDepsNode *geom = find_component(DEPSNODE_TYPE_GEOMETRY);
	ComponentDepsNode *proxy = find_component(DEPSNODE_TYPE_PROXY);
	ComponentDepsNode *pose = find_component(DEPSNODE_TYPE_EVAL_POSE);
	ComponentDepsNode *psys = find_component(DEPSNODE_TYPE_EVAL_PARTICLES);
	ComponentDepsNode *seq = find_component(DEPSNODE_TYPE_SEQUENCER);
	
	/* enforce (gross) ordering of these components................................................. */
	// TODO: create relationships to do this...
	
	/* parameters should always exist... */
	#pragma message("DEPSGRAPH PORTING XXX: params not always created, assert disabled for now")
//	BLI_assert(params != NULL);
	BLI_addhead(&dummy_list, params);
	
	/* anim before params */
	if (anim && params) {
		BLI_addhead(&dummy_list, anim);
	}
	
	/* proxy before anim (or params) */
	if (proxy) {
		BLI_addhead(&dummy_list, proxy);
	}
	
	/* transform after params */
	if (trans) {
		BLI_addtail(&dummy_list, trans);
	}
	
	/* geometry after transform */
	if (geom) {
		BLI_addtail(&dummy_list, geom);
	}
	
	/* pose eval after transform */
	if (pose) {
		BLI_addtail(&dummy_list, pose);
	}
#endif
	
	/* for each component, validate it's internal nodes ............................................ */
	
	/* NOTE: this is done after the component-level restrictions are done,
	 * so that we can take those restrictions as a guide for our low-level
	 * component restrictions...
	 */
	for (IDDepsNode::ComponentMap::const_iterator it = this->components.begin(); it != this->components.end(); ++it) {
		DepsNode *component = it->second;
		component->validate_links(graph);
	}
}

ComponentDepsNode *IDDepsNode::find_component(eDepsNode_Type type) const
{
	ComponentMap::const_iterator it = components.find(type);
	return it != components.end() ? it->second : NULL;
}

DEG_DEPSNODE_DEFINE(IDDepsNode, DEPSNODE_TYPE_ID_REF, "ID Node");
static DepsNodeFactoryImpl<IDDepsNode> DNTI_ID_REF;

/* Subgraph Node ========================================== */

/* Initialise 'subgraph' node - from pointer data given */
void SubgraphDepsNode::init(const ID *id, const char *UNUSED(subdata))
{
	/* store ID-ref if provided */
	this->root_id = (ID *)id;
	
	/* NOTE: graph will need to be added manually,
	 * as we don't have any way of passing this down
	 */
}

/* Free 'subgraph' node */
SubgraphDepsNode::~SubgraphDepsNode()
{
	/* only free if graph not shared, of if this node is the first reference to it... */
	// XXX: prune these flags a bit...
	if ((this->flag & SUBGRAPH_FLAG_FIRSTREF) || !(this->flag & SUBGRAPH_FLAG_SHARED)) {
		/* free the referenced graph */
		DEG_graph_free(this->graph);
		this->graph = NULL;
	}
}

/* Copy 'subgraph' node - Assume that the subgraph doesn't get copied for now... */
void SubgraphDepsNode::copy(DepsgraphCopyContext *dcc, const SubgraphDepsNode *src)
{
	//const SubgraphDepsNode *src_node = (const SubgraphDepsNode *)src;
	//SubgraphDepsNode *dst_node       = (SubgraphDepsNode *)dst;
	
	/* for now, subgraph itself isn't copied... */
}

/* Add 'subgraph' node to graph */
void SubgraphDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	/* add to subnodes list */
	graph->subgraphs.insert(this);
	
	/* if there's an ID associated, add to ID-nodes lookup too */
	if (id) {
#if 0 /* XXX subgraph node is NOT a true IDDepsNode - what is this supposed to do? */
		// TODO: what to do if subgraph's ID has already been added?
		BLI_assert(!graph->find_id_node(id));
		graph->id_hash[id] = this;
#endif
	}
}

/* Remove 'subgraph' node from graph */
void SubgraphDepsNode::remove_from_graph(Depsgraph *graph)
{
	/* remove from subnodes list */
	graph->subgraphs.erase(this);
	
	/* remove from ID-nodes lookup */
	if (this->root_id) {
#if 0 /* XXX subgraph node is NOT a true IDDepsNode - what is this supposed to do? */
		BLI_assert(graph->find_id_node(this->root_id) == this);
		graph->id_hash.erase(this->root_id);
#endif
	}
}

/* Validate subgraph links... */
void SubgraphDepsNode::validate_links(Depsgraph *graph)
{
	
}

DEG_DEPSNODE_DEFINE(SubgraphDepsNode, DEPSNODE_TYPE_SUBGRAPH, "Subgraph Node");
static DepsNodeFactoryImpl<SubgraphDepsNode> DNTI_SUBGRAPH;


void DEG_register_base_depsnodes()
{
	DEG_register_node_typeinfo(&DNTI_ROOT);
	DEG_register_node_typeinfo(&DNTI_TIMESOURCE);
	
	DEG_register_node_typeinfo(&DNTI_ID_REF);
	DEG_register_node_typeinfo(&DNTI_SUBGRAPH);
}

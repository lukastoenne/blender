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
 * Defines and code for core node types
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_utildefines.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_types.h"
} /* extern "C" */

#include "depsgraph_types.h"
#include "depsgraph_intern.h"

#include "stubs.h" // XXX: THIS MUST BE REMOVED WHEN THE DEPSGRAPH REFACTOR IS DONE

void BKE_animsys_eval_driver(void *context, void *item) {}

void BKE_constraints_evaluate(void *context, void *item) {}
void BKE_pose_iktree_evaluate(void *context, void *item) {}
void BKE_pose_splineik_evaluate(void *context, void *item) {}
void BKE_pose_eval_bone(void *context, void *item) {}

void BKE_pose_rebuild_op(void *context, void *item) {}
void BKE_pose_eval_init(void *context, void *item) {}
void BKE_pose_eval_flush(void *context, void *item) {}

void BKE_particle_system_eval(void *context, void *item) {}

void BKE_rigidbody_rebuild_sim(void *context, void *item) {}
void BKE_rigidbody_eval_simulation(void *context, void *item) {}
void BKE_rigidbody_object_sync_transforms(void *context, void *item) {}

void BKE_object_eval_local_transform(void *context, void *item) {}
void BKE_object_eval_parent(void *context, void *item) {}

void BKE_mesh_eval_geometry(void *context, void *item) {}
void BKE_mball_eval_geometry(void *context, void *item) {}
void BKE_curve_eval_geometry(void *context, void *item) {}
void BKE_curve_eval_path(void *context, void *item) {}
void BKE_lattice_eval_geometry(void *context, void *item) {}

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
static DepsNodeTypeInfoImpl<RootDepsNode> DNTI_ROOT;

#if 0
/* Root Node Type Info */
static DepsNodeTypeInfo DNTI_ROOT = {
	/* type */               DEPSNODE_TYPE_ROOT,
	/* size */               sizeof(RootDepsNode),
	/* name */               "Root DepsNode",
	
	/* init_data() */        NULL,
	/* free_data() */        NULL,
	/* copy_data() */        NULL,
	
	/* add_to_graph() */     dnti_root__add_to_graph,
	/* remove_from_graph()*/ dnti_root__remove_from_graph,
	
	/* validate_links() */   NULL,
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

/* Time Source Node ======================================= */

/* Add 'time source' node to graph */
void TimeSourceDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	/* determine which node to attach timesource to */
	if (id) {
		/* get ID node */
//		DepsNode *id_node = DEG_get_node(graph, id, NULL, DEPSNODE_TYPE_ID_REF, NULL);
		
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
static DepsNodeTypeInfoImpl<TimeSourceDepsNode> DNTI_TIMESOURCE;

#if 0
/* Time Source Type Info */
static DepsNodeTypeInfo DNTI_TIMESOURCE = {
	/* type */               DEPSNODE_TYPE_TIMESOURCE,
	/* size */               sizeof(TimeSourceDepsNode),
	/* name */               "Time Source",
	
	/* init_data() */        NULL,
	/* free_data() */        NULL,
	/* copy_data() */        NULL,
	
	/* add_to_graph() */     dnti_timesource__add_to_graph,
	/* remove_from_graph()*/ dnti_timesource__remove_from_graph,
	
	/* validate_links() */   NULL, // XXX?
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

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
	BLI_ghash_insert(graph->id_hash, (ID *)id, this);
}

/* Remove 'id' node from graph */
void IDDepsNode::remove_from_graph(Depsgraph *graph)
{
	/* remove toplevel node and hash entry, but don't free... */
	BLI_ghash_remove(graph->id_hash, this->id, NULL, NULL);
}

/* Validate links between components */
void IDDepsNode::validate_links(Depsgraph *graph)
{
	ListBase dummy_list = {NULL, NULL}; // XXX: perhaps this should live in the node?
	
	GHashIterator hashIter;
	
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
static DepsNodeTypeInfoImpl<IDDepsNode> DNTI_ID_REF;

#if 0
/* ID Node Type Info */
static DepsNodeTypeInfo DNTI_ID_REF = {
	/* type */               DEPSNODE_TYPE_ID_REF,
	/* size */               sizeof(IDDepsNode),
	/* name */               "ID Node",
	
	/* init_data() */        dnti_id_ref__init_data,
	/* free_data() */        dnti_id_ref__free_data,
	/* copy_data() */        dnti_id_ref__copy_data,
	
	/* add_to_graph() */     dnti_id_ref__add_to_graph,
	/* remove_from_graph()*/ dnti_id_ref__remove_from_graph,
	
	/* validate_links() */   dnti_id_ref__validate_links,
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

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
	BLI_addtail(&graph->subgraphs, this);
	
	/* if there's an ID associated, add to ID-nodes lookup too */
	if (id) {
		// TODO: what to do if subgraph's ID has already been added?
		BLI_assert(BLI_ghash_haskey(graph->id_hash, id) == false);
		BLI_ghash_insert(graph->id_hash, (ID *)id, this);
	}
}

/* Remove 'subgraph' node from graph */
void SubgraphDepsNode::remove_from_graph(Depsgraph *graph)
{
	/* remove from subnodes list */
	BLI_remlink(&graph->subgraphs, this);
	
	/* remove from ID-nodes lookup */
	if (this->root_id) {
		BLI_assert(BLI_ghash_lookup(graph->id_hash, this->root_id) == this);
		BLI_ghash_remove(graph->id_hash, this->root_id, NULL, NULL);
	}
}

/* Validate subgraph links... */
void SubgraphDepsNode::validate_links(Depsgraph *graph)
{
	
}

DEG_DEPSNODE_DEFINE(SubgraphDepsNode, DEPSNODE_TYPE_SUBGRAPH, "Subgraph Node");
static DepsNodeTypeInfoImpl<SubgraphDepsNode> DNTI_SUBGRAPH;

#if 0
/* Subgraph Type Info */
static DepsNodeTypeInfo DNTI_SUBGRAPH = {
	/* type */               DEPSNODE_TYPE_SUBGRAPH,
	/* size */               sizeof(SubgraphDepsNode),
	/* name */               "Subgraph Node",
	
	/* init_data() */        dnti_subgraph__init_data,
	/* free_data() */        dnti_subgraph__free_data,
	/* copy_data() */        dnti_subgraph__copy_data,
	
	/* add_to_graph() */     dnti_subgraph__add_to_graph,
	/* remove_from_graph()*/ dnti_subgraph__remove_from_graph,
	
	/* validate_links() */   dnti_subgraph__validate_links,
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

/* ******************************************************** */
/* Outer Nodes */

/* Standard Component Methods ============================= */

/* Initialise 'component' node - from pointer data given */
void ComponentDepsNode::init(const ID *id, const char *subdata)
{
	/* create op-node hash */
	this->op_hash = BLI_ghash_str_new("DepsNode Component - Operations Hash");
	
	/* hook up eval context? */
	// XXX: maybe this needs a special API?
}

/* Copy 'component' node */
void ComponentDepsNode::copy(DepsgraphCopyContext *dcc, const ComponentDepsNode *src)
{
	/* create new op-node hash (to host the copied data) */
	this->op_hash = BLI_ghash_str_new("DepsNode Component - Operations Hash (Copy)");
	
	/* duplicate list of operation nodes */
	BLI_listbase_clear(&this->ops);
	
	for (DepsNode *src_op = (DepsNode *)src->ops.first; src_op; src_op = src_op->next) {
		/* recursive copy */
		DepsNodeTypeInfo *nti = DEG_node_get_typeinfo(src_op);
		DepsNode *dst_op = nti->copy_node(dcc, src_op);
		BLI_addtail(&this->ops, dst_op);
			
		/* fix links... */
		// ...
	}
	
	/* copy evaluation contexts */
	//
}

/* Free 'component' node */
ComponentDepsNode::~ComponentDepsNode()
{
	DepsNode *op, *next;
	
	/* free nodes and list of nodes */
	for (op = (DepsNode *)this->ops.first; op; op = next) {
		next = op->next;
		
		BLI_remlink(&this->ops, op);
		delete op;
	}
	
	/* free hash too - no need to free as it should be empty now */
	BLI_ghash_free(this->op_hash, NULL, NULL);
	this->op_hash = NULL;
}

/* Add 'component' node to graph */
void ComponentDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	/* find ID node that we belong to (and create it if it doesn't exist!) */
	IDDepsNode *id_node = (IDDepsNode *)DEG_get_node(graph, id, NULL, DEPSNODE_TYPE_ID_REF, NULL);
	BLI_assert(id_node != NULL);
	
	/* add component to id */
	id_node->components[this->type] = this;
	this->owner = id_node;
}

/* Remove 'component' node from graph */
void ComponentDepsNode::remove_from_graph(Depsgraph *graph)
{
	/* detach from owner (i.e. id-ref) */
	if (this->owner) {
		IDDepsNode *id_node = (IDDepsNode *)this->owner;
		id_node->components.erase(this->type);
		this->owner = NULL;
	}
	
	/* NOTE: don't need to do anything about relationships,
	 * as those are handled via the standard mechanism
	 */
}

/* Parameter Component Defines ============================ */

DEG_DEPSNODE_DEFINE(ParametersComponentDepsNode, DEPSNODE_TYPE_PARAMETERS, "Parameters Component");
static DepsNodeTypeInfoImpl<ParametersComponentDepsNode> DNTI_PARAMETERS;

#if 0
/* Parameters */
static DepsNodeTypeInfo DNTI_PARAMETERS = {
	/* type */               DEPSNODE_TYPE_PARAMETERS,
	/* size */               sizeof(ComponentDepsNode),
	/* name */               "Parameters Component",
	
	/* init_data() */        dnti_component__init_data,
	/* free_data() */        dnti_component__free_data,
	/* copy_data() */        dnti_component__copy_data,
	
	/* add_to_graph() */     dnti_component__add_to_graph,
	/* remove_from_graph()*/ dnti_component__remove_from_graph,
	
	/* validate_links() */   NULL, // XXX: ensure cleanup ops are first/last and hooked to whatever depends on us
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

/* Animation Component Defines ============================ */

DEG_DEPSNODE_DEFINE(AnimationComponentDepsNode, DEPSNODE_TYPE_ANIMATION, "Animation Component");
static DepsNodeTypeInfoImpl<AnimationComponentDepsNode> DNTI_ANIMATION;

#if 0
/* Animation */
static DepsNodeTypeInfo DNTI_ANIMATION = {
	/* type */               DEPSNODE_TYPE_ANIMATION,
	/* size */               sizeof(ComponentDepsNode),
	/* name */               "Animation Component",
	
	/* init_data() */        dnti_component__init_data,
	/* free_data() */        dnti_component__free_data,
	/* copy_data() */        dnti_component__copy_data,
	
	/* add_to_graph() */     dnti_component__add_to_graph,
	/* remove_from_graph()*/ dnti_component__remove_from_graph,
	
	/* validate_links() */   NULL, // XXX: ensure cleanup ops are first/last and hooked to whatever depends on us
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

/* Transform Component Defines ============================ */

DEG_DEPSNODE_DEFINE(TransformComponentDepsNode, DEPSNODE_TYPE_TRANSFORM, "Transform Component");
static DepsNodeTypeInfoImpl<TransformComponentDepsNode> DNTI_TRANSFORM;

#if 0
/* Transform */
static DepsNodeTypeInfo DNTI_TRANSFORM = {
	/* type */               DEPSNODE_TYPE_TRANSFORM,
	/* size */               sizeof(ComponentDepsNode),
	/* name */               "Transform Component",
	
	/* init_data() */        dnti_component__init_data,
	/* free_data() */        dnti_component__free_data,
	/* copy_data() */        dnti_component__copy_data,
	
	/* add_to_graph() */     dnti_component__add_to_graph,
	/* remove_from_graph()*/ dnti_component__remove_from_graph,
	
	/* validate_links() */   NULL, // XXX: ensure cleanup ops are first/last and hooked to whatever depends on us
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

/* Proxy Component Defines ================================ */

DEG_DEPSNODE_DEFINE(ProxyComponentDepsNode, DEPSNODE_TYPE_PROXY, "Proxy Component");
static DepsNodeTypeInfoImpl<ProxyComponentDepsNode> DNTI_PROXY;

#if 0
/* Proxy */
static DepsNodeTypeInfo DNTI_PROXY = {
	/* type */               DEPSNODE_TYPE_PROXY,
	/* size */               sizeof(ComponentDepsNode),
	/* name */               "Proxy Component",
	
	/* init_data() */        dnti_component__init_data,
	/* free_data() */        dnti_component__free_data,
	/* copy_data() */        dnti_component__copy_data,
	
	/* add_to_graph() */     dnti_component__add_to_graph,
	/* remove_from_graph()*/ dnti_component__remove_from_graph,
	
	/* validate_links() */   NULL, // XXX: ensure cleanup ops are first/last and hooked to whatever depends on us
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

/* Geometry Component Defines ============================= */

DEG_DEPSNODE_DEFINE(GeometryComponentDepsNode, DEPSNODE_TYPE_GEOMETRY, "Geometry Component");
static DepsNodeTypeInfoImpl<GeometryComponentDepsNode> DNTI_GEOMETRY;

#if 0
/* Geometry */
static DepsNodeTypeInfo DNTI_GEOMETRY = {
	/* type */               DEPSNODE_TYPE_GEOMETRY,
	/* size */               sizeof(ComponentDepsNode),
	/* name */               "Geometry Component",
	
	/* init_data() */        dnti_component__init_data,
	/* free_data() */        dnti_component__free_data,
	/* copy_data() */        dnti_component__copy_data,
	
	/* add_to_graph() */     dnti_component__add_to_graph,
	/* remove_from_graph()*/ dnti_component__remove_from_graph,
	
	/* validate_links() */   NULL, // XXX: ensure cleanup ops are first/last and hooked to whatever depends on us
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

/* Sequencer Component Defines ============================ */

DEG_DEPSNODE_DEFINE(SequencerComponentDepsNode, DEPSNODE_TYPE_SEQUENCER, "Sequencer Component");
static DepsNodeTypeInfoImpl<SequencerComponentDepsNode> DNTI_SEQUENCER;

#if 0
/* Sequencer */
static DepsNodeTypeInfo DNTI_SEQUENCER = {
	/* type */               DEPSNODE_TYPE_SEQUENCER,
	/* size */               sizeof(ComponentDepsNode),
	/* name */               "Sequencer Component",
	
	/* init_data() */        dnti_component__init_data,
	/* free_data() */        dnti_component__free_data,
	/* copy_data() */        dnti_component__copy_data,
	
	/* add_to_graph() */     dnti_component__add_to_graph,
	/* remove_from_graph()*/ dnti_component__remove_from_graph,
	
	/* validate_links() */   NULL, // XXX: ensure cleanup ops are first/last and hooked to whatever depends on us
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

/* Pose Component ========================================= */

/* Initialise 'pose eval' node - from pointer data given */
void PoseComponentDepsNode::init(const ID *id, const char *subdata)
{
	/* generic component-node... */
	ComponentDepsNode::init(id, subdata);
	
	/* pose-specific data... */
	this->bone_hash = BLI_ghash_str_new("Pose Component Bone Hash"); /* <String, BoneNode> */
}

/* Copy 'pose eval' node */
void PoseComponentDepsNode::copy(DepsgraphCopyContext *dcc, const PoseComponentDepsNode *src)
{
	/* generic component node... */
	ComponentDepsNode::copy(dcc, src);
	
	/* pose-specific data... */
	// copy bonehash...
}

/* Free 'pose eval' node */
PoseComponentDepsNode::~PoseComponentDepsNode()
{
	/* pose-specific data... */
	BLI_ghash_free(this->bone_hash, NULL, /*dnti_pose_eval__hash_free_bone*/NULL);
}

/* Validate links for pose evaluation */
void PoseComponentDepsNode::validate_links(Depsgraph *graph)
{
	GHashIterator hashIter;
	
	/* create our core operations... */
	if (BLI_ghash_size(this->bone_hash) || (this->ops.first)) {
		OperationDepsNode *rebuild_op, *init_op, *cleanup_op;
		IDDepsNode *owner_node = (IDDepsNode *)this->owner;
		Object *ob;
		ID *id;
		
		/* get ID-block that pose-component belongs to */
		BLI_assert(owner_node && owner_node->id);
		
		id = owner_node->id;
		ob = (Object *)id;
		
		/* create standard pose evaluation start/end hooks */
		rebuild_op = DEG_add_operation(graph, id, NULL, DEPSNODE_TYPE_OP_POSE,
		                               DEPSOP_TYPE_REBUILD, BKE_pose_rebuild_op,
		                               "Rebuild Pose");
		RNA_pointer_create(id, &RNA_Pose, ob->pose, &rebuild_op->ptr);
		
		init_op = DEG_add_operation(graph, id, NULL, DEPSNODE_TYPE_OP_POSE,
		                            DEPSOP_TYPE_INIT, BKE_pose_eval_init,
		                            "Init Pose Eval");
		RNA_pointer_create(id, &RNA_Pose, ob->pose, &init_op->ptr);
		
		cleanup_op = DEG_add_operation(graph, id, NULL, DEPSNODE_TYPE_OP_POSE,
		                               DEPSOP_TYPE_POST, BKE_pose_eval_flush,
		                               "Flush Pose Eval");
		RNA_pointer_create(id, &RNA_Pose, ob->pose, &cleanup_op->ptr);
		
		
		/* attach links between these operations */
		DEG_add_new_relation(rebuild_op, init_op,    DEPSREL_TYPE_COMPONENT_ORDER, "[Pose Rebuild -> Pose Init] DepsRel");
		DEG_add_new_relation(init_op,    cleanup_op, DEPSREL_TYPE_COMPONENT_ORDER, "[Pose Init -> Pose Cleanup] DepsRel");
		
		/* NOTE: bones will attach themselves to these endpoints */
	}
	
	/* ensure that each bone has been validated... */
	if (BLI_ghash_size(this->bone_hash)) {
		GHASH_ITER(hashIter, this->bone_hash) {
			DepsNode *bone_comp = (DepsNode *)BLI_ghashIterator_getValue(&hashIter);
			/* recursively validate the links within bone component */
			// NOTE: this ends up hooking up the IK Solver(s) here to the relevant final bone operations...
			bone_comp->validate_links(graph);
		}
	}
}

DEG_DEPSNODE_DEFINE(PoseComponentDepsNode, DEPSNODE_TYPE_EVAL_POSE, "Pose Eval Component");
static DepsNodeTypeInfoImpl<PoseComponentDepsNode> DNTI_EVAL_POSE;

#if 0
/* Pose Evaluation */
static DepsNodeTypeInfo DNTI_EVAL_POSE = {
	/* type */               DEPSNODE_TYPE_EVAL_POSE,
	/* size */               sizeof(PoseComponentDepsNode),
	/* name */               "Pose Eval Component",
	
	/* init_data() */        dnti_pose_eval__init_data,
	/* free_data() */        dnti_pose_eval__free_data,
	/* copy_data() */        dnti_pose_eval__copy_data,
	
	/* add_to_graph() */     dnti_component__add_to_graph,
	/* remove_from_graph()*/ dnti_component__remove_from_graph,
	
	/* validate_links() */   dnti_pose_eval__validate_links,
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

/* Bone Component ========================================= */

/* Initialise 'bone component' node - from pointer data given */
void BoneComponentDepsNode::init(const ID *id, const char *subdata)
{
	/* generic component-node... */
	ComponentDepsNode::init(id, subdata);
	
	/* name of component comes is bone name */
	BLI_strncpy(this->name, subdata, MAX_NAME);
	
	/* bone-specific node data */
	Object *ob = (Object *)id;
	this->pchan = BKE_pose_channel_find_name(ob->pose, subdata);
}

/* Add 'bone component' node to graph */
void BoneComponentDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	PoseComponentDepsNode *pose_node;
	
	/* find pose node that we belong to (and create it if it doesn't exist!) */
	pose_node = (PoseComponentDepsNode *)DEG_get_node(graph, id, NULL, DEPSNODE_TYPE_EVAL_POSE, NULL);
	BLI_assert(pose_node != NULL);
	
	/* add bone component to pose bone-hash */
	BLI_ghash_insert(pose_node->bone_hash, this->name, this);
	this->owner = (DepsNode *)pose_node;
}

/* Remove 'bone component' node from graph */
void BoneComponentDepsNode::remove_from_graph(Depsgraph *graph)
{
	/* detach from owner (i.e. pose component) */
	if (this->owner) {
		PoseComponentDepsNode *pose_node = (PoseComponentDepsNode *)this->owner;
		
		BLI_ghash_remove(pose_node->bone_hash, this->name, NULL, NULL);
		this->owner = NULL;
	}
	
	/* NOTE: don't need to do anything about relationships,
	 * as those are handled via the standard mechanism
	 */
}

/* Validate 'bone component' links... 
 * - Re-route all component-level relationships to the nodes 
 */
void BoneComponentDepsNode::validate_links(Depsgraph *graph)
{
	PoseComponentDepsNode *pcomp = (PoseComponentDepsNode *)this->owner;
	bPoseChannel *pchan = this->pchan;
	
	DepsNode *btrans_op = (DepsNode *)BLI_ghash_lookup(this->op_hash, "Bone Transforms");
	DepsNode *final_op = NULL;  /* normal final-evaluation operation */
	DepsNode *ik_op = NULL;     /* IK Solver operation */
	
	BLI_assert(btrans_op != NULL);
	
	/* link bone/component to pose "sources" if it doesn't have any obvious dependencies */
	if (pchan->parent == NULL) {
		DepsNode *pinit_op = (DepsNode *)BLI_ghash_lookup(pcomp->op_hash, "Init Pose Eval");
		DEG_add_new_relation(pinit_op, btrans_op, DEPSREL_TYPE_OPERATION, "PoseEval Source-Bone Link");
	}
	
	/* inlinks destination should all go to the "Bone Transforms" operation */
	DEPSNODE_RELATIONS_ITER_BEGIN(this->inlinks.first, rel)
	{
		/* redirect destination pointer */
		rel->to = btrans_op;
		
		/* ensure that transform operation knows it has this link now */
		/* XXX: for now, we preserve the link to the component, so that querying is easier
		 *      But, this also ends up making more work when flushing updates...
		 */
		DEG_add_relation(rel);
	}
	DEPSNODE_RELATIONS_ITER_END;
	
	
	/* outlink source target depends on what we might have:
	 * 1) Transform only - No constraints at all
	 * 2) Constraints node - Just plain old constraints
	 * 3) IK Solver node - If part of IK chain...
	 */
	if (pchan->constraints.first) {
		/* find constraint stack operation */
		final_op = (DepsNode *)BLI_ghash_lookup(this->op_hash, "Constraint Stack");
	}
	else {
		/* just normal transforms */
		final_op = btrans_op;
	}
	
	DEPSNODE_RELATIONS_ITER_BEGIN(this->outlinks.first, rel)
	{
		/* Technically, the last evaluation operation on these
		 * should be IK if present. Since, this link is actually
		 * present in the form of one or more of the ops, we'll
		 * take the first one that comes (during a first pass)
		 * (XXX: there's potential here for problems with forked trees) 
		 */
		if (strcmp(rel->name, "IK Solver Update") == 0) {
			ik_op = rel->to;
			break;
		}
	}
	DEPSNODE_RELATIONS_ITER_END;
	
	/* fix up outlink refs */
	DEPSNODE_RELATIONS_ITER_BEGIN(this->outlinks.first, rel)
	{
		if (ik_op) {
			/* bone is part of IK Chain... */
			if (rel->to == ik_op) {
				/* can't have ik to ik, so use final "normal" bone transform 
				 * as indicator to IK Solver that it is ready to run 
				 */
				rel->from = final_op;
			}
			else {
				/* everything which depends on result of this bone needs to know 
				 * about the IK result too!
				 */
				rel->from = ik_op;
			}
		}
		else {
			/* bone is not part of IK Chains... */
			rel->from = final_op;
		}
		
		/* XXX: for now, we preserve the link to the component, so that querying is easier
		 *      But, this also ends up making more work when flushing updates...
		 */
		DEG_add_relation(rel);
	}
	DEPSNODE_RELATIONS_ITER_END;
	
	/* link bone/component to pose "sinks" as final link, unless it has obvious quirks */
	{
		DepsNode *ppost_op = (DepsNode *)BLI_ghash_lookup(this->op_hash, "Cleanup Pose Eval");
		DEG_add_new_relation(final_op, ppost_op, DEPSREL_TYPE_OPERATION, "PoseEval Sink-Bone Link");
	}
}

DEG_DEPSNODE_DEFINE(BoneComponentDepsNode, DEPSNODE_TYPE_BONE, "Bone Component");
static DepsNodeTypeInfoImpl<BoneComponentDepsNode> DNTI_BONE;

#if 0
/* Bone Type Info */
static DepsNodeTypeInfo DNTI_BONE = {
	/* type */               DEPSNODE_TYPE_BONE,
	/* size */               sizeof(BoneComponentDepsNode),
	/* name */               "Bone Component Node",
	
	/* init_data() */        dnti_bone__init_data,
	/* free_data() */        dnti_component__free_data,
	/* copy_data() */        dnti_component__copy_data,
	
	/* add_to_graph() */     dnti_bone__add_to_graph,
	/* remove_from_graph()*/ dnti_bone__remove_from_graph,
	
	/* validate_links() */   dnti_bone__validate_links,
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif


/* ******************************************************** */
/* Inner Nodes */

/* Standard Operation Callbacks =========================== */
/* NOTE: some of these are just templates used by the others */

/* Helper to add 'operation' node to graph */
void OperationDepsNode::add_to_component_node(Depsgraph *graph, const ID *id, eDepsNode_Type component_type)
{
	/* get component node to add operation to */
	ComponentDepsNode *component = (ComponentDepsNode *)DEG_get_node(graph, id, NULL, component_type, NULL);
	
	/* add to hash and list */
	BLI_ghash_insert(component->op_hash, this->name, this);
	BLI_addtail(&component->ops, this);
	
	/* add backlink to component */
	this->owner = component;
}

/* Callback to remove 'operation' node from graph */
void OperationDepsNode::remove_from_graph(Depsgraph *UNUSED(graph))
{
	if (this->owner) {
		ComponentDepsNode *component = (ComponentDepsNode *)this->owner;
		
		/* remove node from hash and list */
		BLI_ghash_remove(component->op_hash, this->name, NULL, NULL);
		BLI_remlink(&component->ops, this);
		
		/* remove backlink */
		this->owner = NULL;
	}
}

/* Parameter Operation ==================================== */

/* Add 'parameter operation' node to graph */
void ParametersOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_PARAMETERS);
}

DEG_DEPSNODE_DEFINE(ParametersOperationDepsNode, DEPSNODE_TYPE_OP_PARAMETER, "Parameters Operation");
static DepsNodeTypeInfoImpl<ParametersOperationDepsNode> DNTI_OP_PARAMETERS;

#if 0
/* Parameter Operation Node */
static DepsNodeTypeInfo DNTI_OP_PARAMETER = {
	/* type */               DEPSNODE_TYPE_OP_PARAMETER,
	/* size */               sizeof(OperationDepsNode),
	/* name */               "Parameter Operation",
	
	/* init_data() */        NULL,
	/* free_data() */        NULL,
	/* copy_data() */        NULL,
	
	/* add_to_graph() */     dnti_op_parameter__add_to_graph,
	/* remove_from_graph()*/ dnti_operation__remove_from_graph,
	
	/* validate_links() */   NULL,
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

/* Proxy Operation ======================================== */

/* Add 'proxy operation' node to graph */
void ProxyOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_PROXY);
}

DEG_DEPSNODE_DEFINE(ProxyOperationDepsNode, DEPSNODE_TYPE_OP_PROXY, "Proxy Operation");
static DepsNodeTypeInfoImpl<ProxyOperationDepsNode> DNTI_OP_PROXY;

#if 0
/* Proxy Operation Node */
static DepsNodeTypeInfo DNTI_OP_PROXY = {
	/* type */               DEPSNODE_TYPE_OP_PROXY,
	/* size */               sizeof(OperationDepsNode),
	/* name */               "Proxy Operation",
	
	/* init_data() */        NULL,
	/* free_data() */        NULL,
	/* copy_data() */        NULL,
	
	/* add_to_graph() */     dnti_op_proxy__add_to_graph,
	/* remove_from_graph()*/ dnti_operation__remove_from_graph,
	
	/* validate_links() */   NULL,
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

/* Animation Operation ==================================== */

/* Add 'animation operation' node to graph */
void AnimationOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_ANIMATION);
}

DEG_DEPSNODE_DEFINE(AnimationOperationDepsNode, DEPSNODE_TYPE_OP_ANIMATION, "Animation Operation");
static DepsNodeTypeInfoImpl<AnimationOperationDepsNode> DNTI_OP_ANIMATION;

#if 0
/* Animation Operation Node */
static DepsNodeTypeInfo DNTI_OP_ANIMATION = {
	/* type */               DEPSNODE_TYPE_OP_ANIMATION,
	/* size */               sizeof(OperationDepsNode),
	/* name */               "Animation Operation",
	
	/* init_data() */        NULL,
	/* free_data() */        NULL,
	/* copy_data() */        NULL,
	
	/* add_to_graph() */     dnti_op_animation__add_to_graph,
	/* remove_from_graph()*/ dnti_operation__remove_from_graph,
	
	/* validate_links() */   NULL,
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

/* Transform Operation ==================================== */

/* Add 'transform operation' node to graph */
void TransformOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_TRANSFORM);
}

DEG_DEPSNODE_DEFINE(TransformOperationDepsNode, DEPSNODE_TYPE_OP_TRANSFORM, "Transform Operation");
static DepsNodeTypeInfoImpl<TransformOperationDepsNode> DNTI_OP_TRANSFORM;

#if 0
/* Transform Operation Node */
static DepsNodeTypeInfo DNTI_OP_TRANSFORM = {
	/* type */               DEPSNODE_TYPE_OP_TRANSFORM,
	/* size */               sizeof(OperationDepsNode),
	/* name */               "Transform Operation",
	
	/* init_data() */        NULL,
	/* free_data() */        NULL,
	/* copy_data() */        NULL,
	
	/* add_to_graph() */     dnti_op_transform__add_to_graph,
	/* remove_from_graph()*/ dnti_operation__remove_from_graph,
	
	/* validate_links() */   NULL,
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

/* Geometry Operation ===================================== */

/* Add 'geometry operation' node to graph */
void GeometryOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_GEOMETRY);
}

DEG_DEPSNODE_DEFINE(GeometryOperationDepsNode, DEPSNODE_TYPE_OP_GEOMETRY, "Geometry Operation");
static DepsNodeTypeInfoImpl<GeometryOperationDepsNode> DNTI_OP_GEOMETRY;

#if 0
/* Geometry Operation Node */
static DepsNodeTypeInfo DNTI_OP_GEOMETRY = {
	/* type */               DEPSNODE_TYPE_OP_GEOMETRY,
	/* size */               sizeof(OperationDepsNode),
	/* name */               "Geometry Operation",
	
	/* init_data() */        NULL,
	/* free_data() */        NULL,
	/* copy_data() */        NULL,
	
	/* add_to_graph() */     dnti_op_geometry__add_to_graph,
	/* remove_from_graph()*/ dnti_operation__remove_from_graph,
	
	/* validate_links() */   NULL,
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

/* Sequencer Operation ==================================== */

/* Add 'sequencer operation' node to graph */
void SequencerOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_SEQUENCER);
}

DEG_DEPSNODE_DEFINE(SequencerOperationDepsNode, DEPSNODE_TYPE_OP_SEQUENCER, "Sequencer Operation");
static DepsNodeTypeInfoImpl<SequencerOperationDepsNode> DNTI_OP_SEQUENCER;

#if 0
/* Sequencer Operation Node */
static DepsNodeTypeInfo DNTI_OP_SEQUENCER = {
	/* type */               DEPSNODE_TYPE_OP_SEQUENCER,
	/* size */               sizeof(OperationDepsNode),
	/* name */               "Sequencer Operation",
	
	/* init_data() */        NULL,
	/* free_data() */        NULL,
	/* copy_data() */        NULL,
	
	/* add_to_graph() */     dnti_op_sequencer__add_to_graph,
	/* remove_from_graph()*/ dnti_operation__remove_from_graph,
	
	/* validate_links() */   NULL,
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

/* Update Operation ======================================= */

/* Add 'update operation' node to graph */
void UpdateOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_PARAMETERS);
}

DEG_DEPSNODE_DEFINE(UpdateOperationDepsNode, DEPSNODE_TYPE_OP_UPDATE, "RNA Update Operation");
static DepsNodeTypeInfoImpl<UpdateOperationDepsNode> DNTI_OP_UPDATE;

#if 0
/* Update Operation Node */
static DepsNodeTypeInfo DNTI_OP_UPDATE = {
	/* type */               DEPSNODE_TYPE_OP_UPDATE,
	/* size */               sizeof(OperationDepsNode),
	/* name */               "RNA Update Operation",
	
	/* init_data() */        NULL,
	/* free_data() */        NULL,
	/* copy_data() */        NULL,
	
	/* add_to_graph() */     dnti_op_update__add_to_graph,
	/* remove_from_graph()*/ dnti_operation__remove_from_graph,
	
	/* validate_links() */   NULL,
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

/* Driver Operation ===================================== */
// XXX: some special tweaks may be needed for this one...

/* Add 'driver operation' node to graph */
void DriverOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_PARAMETERS);
}

DEG_DEPSNODE_DEFINE(DriverOperationDepsNode, DEPSNODE_TYPE_OP_DRIVER, "Driver Operation");
static DepsNodeTypeInfoImpl<DriverOperationDepsNode> DNTI_OP_DRIVER;

#if 0
/* Driver Operation Node */
static DepsNodeTypeInfo DNTI_OP_DRIVER = {
	/* type */               DEPSNODE_TYPE_OP_DRIVER,
	/* size */               sizeof(OperationDepsNode),
	/* name */               "Driver Operation",
	
	/* init_data() */        NULL,
	/* free_data() */        NULL,
	/* copy_data() */        NULL,
	
	/* add_to_graph() */     dnti_op_driver__add_to_graph,
	/* remove_from_graph()*/ dnti_operation__remove_from_graph,
	
	/* validate_links() */   NULL,
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

/* Pose Operation ========================================= */

/* Add 'pose operation' node to graph */
void PoseOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_EVAL_POSE);
}

DEG_DEPSNODE_DEFINE(PoseOperationDepsNode, DEPSNODE_TYPE_OP_POSE, "Pose Operation");
static DepsNodeTypeInfoImpl<PoseOperationDepsNode> DNTI_OP_POSE;

#if 0
/* Pose Operation Node */
static DepsNodeTypeInfo DNTI_OP_POSE = {
	/* type */               DEPSNODE_TYPE_OP_POSE,
	/* size */               sizeof(OperationDepsNode),
	/* name */               "Pose Operation",
	
	/* init_data() */        NULL,
	/* free_data() */        NULL,
	/* copy_data() */        NULL,
	
	/* add_to_graph() */     dnti_op_pose__add_to_graph,
	/* remove_from_graph()*/ dnti_operation__remove_from_graph,
	
	/* validate_links() */   NULL,
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

/* Bone Operation ========================================= */

/* Init local data for bone operation */
void BoneOperationDepsNode::init(const ID *id, const char *subdata)
{
	Object *ob;
	bPoseChannel *pchan;
	
	/* set up RNA Pointer to affected bone */
	ob = (Object *)id;
	pchan = BKE_pose_channel_find_name(ob->pose, subdata);
	
	RNA_pointer_create((ID *)id, &RNA_PoseBone, pchan, &this->ptr);
}

/* Add 'bone operation' node to graph */
void BoneOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	BoneComponentDepsNode *bone_comp;
	bPoseChannel *pchan;
	
	/* get bone component that owns this bone operation */
	BLI_assert(this->ptr.type == &RNA_PoseBone);
	pchan = (bPoseChannel *)this->ptr.data;
	
	bone_comp = (BoneComponentDepsNode *)DEG_get_node(graph, id, pchan->name, DEPSNODE_TYPE_BONE, NULL);
	
	/* add to hash and list as per usual */
	BLI_ghash_insert(bone_comp->op_hash, pchan->name, this);
	BLI_addtail(&bone_comp->ops, this);
	
	/* add backlink to component */
	this->owner = bone_comp;
}

DEG_DEPSNODE_DEFINE(BoneOperationDepsNode, DEPSNODE_TYPE_OP_BONE, "Bone Operation");
static DepsNodeTypeInfoImpl<BoneOperationDepsNode> DNTI_OP_BONE;

#if 0
/* Bone Operation Node */
static DepsNodeTypeInfo DNTI_OP_BONE = {
	/* type */               DEPSNODE_TYPE_OP_BONE,
	/* size */               sizeof(OperationDepsNode),
	/* name */               "Bone Operation",
	
	/* init_data() */        dnti_op_bone__init_data,
	/* free_data() */        NULL,
	/* copy_data() */        NULL,
	
	/* add_to_graph() */     dnti_op_bone__add_to_graph,
	/* remove_from_graph()*/ dnti_operation__remove_from_graph,
	
	/* validate_links() */   NULL,
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

/* Particle Operation ===================================== */

/* Add 'particle operation' node to graph */
void ParticlesOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_EVAL_PARTICLES);
}

/* Remove 'particle operation' node from graph */
void ParticlesOperationDepsNode::remove_from_graph(Depsgraph *graph)
{
	// XXX...
	OperationDepsNode::remove_from_graph(graph);
}

DEG_DEPSNODE_DEFINE(ParticlesOperationDepsNode, DEPSNODE_TYPE_OP_PARTICLE, "Particles Operation");
static DepsNodeTypeInfoImpl<ParticlesOperationDepsNode> DNTI_OP_PARTICLES;

#if 0
/* Particles Operation Node */
static DepsNodeTypeInfo DNTI_OP_PARTICLE = {
	/* type */               DEPSNODE_TYPE_OP_PARTICLE,
	/* size */               sizeof(OperationDepsNode),
	/* name */               "Particles Operation",
	
	/* init_data() */        NULL, // XXX
	/* free_data() */        NULL, // XXX
	/* copy_data() */        NULL, // XXX
	
	/* add_to_graph() */     dnti_op_particle__add_to_graph,
	/* remove_from_graph()*/ dnti_op_particle__remove_from_graph,
	
	/* validate_links() */   NULL, // XXX
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

/* RigidBody Operation ==================================== */
/* Note: RigidBody Operations are reserved for scene-level rigidbody sim steps */

/* Add 'rigidbody operation' node to graph */
void RigidBodyOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_TRANSFORM); // XXX
}

DEG_DEPSNODE_DEFINE(RigidBodyOperationDepsNode, DEPSNODE_TYPE_OP_RIGIDBODY, "RigidBody Operation");
static DepsNodeTypeInfoImpl<RigidBodyOperationDepsNode> DNTI_OP_RIGIDBODY;

#if 0
/* Rigidbody Operation Node */
static DepsNodeTypeInfo DNTI_OP_RIGIDBODY = {
	/* type */               DEPSNODE_TYPE_OP_RIGIDBODY,
	/* size */               sizeof(OperationDepsNode),
	/* name */               "Rigidbody Operation",
	
	/* init_data() */        NULL,
	/* free_data() */        NULL,
	/* copy_data() */        NULL,
	
	/* add_to_graph() */     dnti_op_rigidbody__add_to_graph,
	/* remove_from_graph()*/ dnti_operation__remove_from_graph,
	
	/* validate_links() */   NULL,
	
	/* eval_context_init()*/ NULL, 
	/* eval_context_free()*/ NULL
};
#endif

/* ******************************************************** */
/* External API */

/* Global type registry */

/* NOTE: For now, this is a hashtable not array, since the core node types
 * currently do not have contiguous ID values. Using a hash here gives us
 * more flexibility, albeit using more memory and also sacrificing a little
 * speed. Later on, when things stabilise we may turn this back to an array
 * since there are only just a few node types that an array would cope fine...
 */
static GHash *_depsnode_typeinfo_registry = NULL;

/* Registration ------------------------------------------- */

/* Register node type */
static void DEG_register_node_typeinfo(DepsNodeTypeInfo *factory)
{
	BLI_assert(factory != NULL);
	BLI_ghash_insert(_depsnode_typeinfo_registry, SET_INT_IN_POINTER(factory->type()), factory);
}

/* Register all node types */
void DEG_register_node_types(void)
{
	/* initialise registry */
	_depsnode_typeinfo_registry = BLI_ghash_int_new("Depsgraph Node Type Registry");
	
	/* register node types */
	/* GENERIC */
	DEG_register_node_typeinfo(&DNTI_ROOT);
	DEG_register_node_typeinfo(&DNTI_TIMESOURCE);
	
	DEG_register_node_typeinfo(&DNTI_ID_REF);
	DEG_register_node_typeinfo(&DNTI_SUBGRAPH);
	
	/* OUTER */
	DEG_register_node_typeinfo(&DNTI_PARAMETERS);
	DEG_register_node_typeinfo(&DNTI_PROXY);
	DEG_register_node_typeinfo(&DNTI_ANIMATION);
	DEG_register_node_typeinfo(&DNTI_TRANSFORM);
	DEG_register_node_typeinfo(&DNTI_GEOMETRY);
	DEG_register_node_typeinfo(&DNTI_SEQUENCER);
	
	DEG_register_node_typeinfo(&DNTI_EVAL_POSE);
	DEG_register_node_typeinfo(&DNTI_BONE);
	
	//DEG_register_node_typeinfo(&DNTI_EVAL_PARTICLES);
	
	/* INNER */
	DEG_register_node_typeinfo(&DNTI_OP_PARAMETERS);
	DEG_register_node_typeinfo(&DNTI_OP_PROXY);
	DEG_register_node_typeinfo(&DNTI_OP_ANIMATION);
	DEG_register_node_typeinfo(&DNTI_OP_TRANSFORM);
	DEG_register_node_typeinfo(&DNTI_OP_GEOMETRY);
	DEG_register_node_typeinfo(&DNTI_OP_SEQUENCER);
	
	DEG_register_node_typeinfo(&DNTI_OP_UPDATE);
	DEG_register_node_typeinfo(&DNTI_OP_DRIVER);
	
	DEG_register_node_typeinfo(&DNTI_OP_POSE);
	DEG_register_node_typeinfo(&DNTI_OP_BONE);
	
	DEG_register_node_typeinfo(&DNTI_OP_PARTICLES);
	DEG_register_node_typeinfo(&DNTI_OP_RIGIDBODY);
}

/* Free registry on exit */
void DEG_free_node_types(void)
{
	BLI_ghash_free(_depsnode_typeinfo_registry, NULL, NULL);
}

/* Getters ------------------------------------------------- */

/* Get typeinfo for specified type */
DepsNodeTypeInfo *DEG_get_node_typeinfo(const eDepsNode_Type type)
{
	/* look up type - at worst, it doesn't exist in table yet, and we fail */
	return (DepsNodeTypeInfo *)BLI_ghash_lookup(_depsnode_typeinfo_registry, SET_INT_IN_POINTER(type));
}

/* Get typeinfo for provided node */
DepsNodeTypeInfo *DEG_node_get_typeinfo(const DepsNode *node)
{
	DepsNodeTypeInfo *nti = NULL;
	
	if (node) {
		nti = DEG_get_node_typeinfo(node->type);
	}
	return nti;
}

/* ******************************************************** */

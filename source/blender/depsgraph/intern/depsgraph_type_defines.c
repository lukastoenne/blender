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

#include "depsgraph_types.h"
#include "depsgraph_intern.h"

#include "stubs.h" // XXX: THIS MUST BE REMOVED WHEN THE DEPSGRAPH REFACTOR IS DONE

/* ******************************************************** */
/* Generic Nodes */

/* Root Node ============================================== */

/* Add 'root' node to graph */
static void dnti_root__add_to_graph(Depsgraph *graph, DepsNode *node, const ID *UNUSED(id))
{
	graph->root_node = node;
}

/* Remove 'root' node from graph */
static void dnti_root__remove_from_graph(Depsgraph *graph, DepsNode *UNUSED(node))
{
	graph->root_node = NULL;
}

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

/* Time Source Node ======================================= */

/* Add 'time source' node to graph */
static void dnti_timesource__add_to_graph(Depsgraph *graph, DepsNode *node, const ID *id)
{
	TimeSourceDepsNode *ts_node = (TimeSourceDepsNode *)node;
	
	/* determine which node to attach timesource to */
	if (id) {
		/* get ID node */
		DepsNode *id_node = DEG_get_node(graph, id, NULL, DEPSNODE_TYPE_ID_REF, NULL);
		
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
		RootDepsNode *root_node = (RootDepsNode *)graph->root_node;
		
		root_node->time_source = ts_node;
		node->owner = graph->root_node;
	}
}

/* Remove 'time source' node from graph */
static void dnti_timesource__remove_from_graph(Depsgraph *graph, DepsNode *node)
{
	BLI_assert(node->owner != NULL);
	
	switch(node->owner->type) {
		case DEPSNODE_TYPE_ROOT: /* root node - standard case */
		{
			RootDepsNode *root_node = (RootDepsNode *)graph->root_node;
		
			root_node->time_source = NULL;
			node->owner = NULL;
		}
		break;
		
		// XXX: ID node - as needed...
		
		default: /* unhandled for now */
			break;
	}	
}

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

/* ID Node ================================================ */

/* Initialise 'id' node - from pointer data given */
static void dnti_id_ref__init_data(DepsNode *node, const ID *id, const char *UNUSED(subdata))
{
	IDDepsNode *id_node = (IDDepsNode *)node;
	
	/* store ID-pointer */
	BLI_assert(id != NULL);
	id_node->id = (ID *)id;
	
	/* init components hash - eDepsNode_Type : ComponentDepsNode */
	id_node->component_hash = BLI_ghash_int_new("IDDepsNode Component Hash");
	
	/* NOTE: components themselves are created if/when needed.
	 * This prevents problems with components getting added 
	 * twice if an ID-Ref needs to be created to house it...
	 */
}

/* Helper for freeing ID nodes - Used by component hash to free data... */
static void dnti_id_ref__hash_free_component(void *component_p)
{
	DEG_free_node((DepsNode *)component_p);
}

/* Free 'id' node */
static void dnti_id_ref__free_data(DepsNode *node)
{
	IDDepsNode *id_node = (IDDepsNode *)node;
	
	/* free components (and recursively, their data) while we empty the hash */
	BLI_ghash_free(id_node->component_hash, NULL, dnti_id_ref__hash_free_component);
}

/* Copy 'id' node */
static void dnti_id_ref__copy_data(DepsgraphCopyContext *dcc, DepsNode *dst, const DepsNode *src)
{
	const IDDepsNode *src_node = (const IDDepsNode *)src;
	IDDepsNode *dst_node       = (IDDepsNode *)dst;
	
	GHashIterator hashIter;
	
	/* create new hash for destination (src's one is still linked to it at this point) */
	dst_node->component_hash = BLI_ghash_int_new("IDDepsNode Component Hash Copy");
	
	/* iterate over items in original hash, adding them to new hash */
	GHASH_ITER(hashIter, src_node->component_hash) {
		/* get current <type : component> mapping */
		eDepsNode_Type c_type   = GET_INT_FROM_POINTER(BLI_ghashIterator_getKey(&hashIter));
		DepsNode *old_component = BLI_ghashIterator_getValue(&hashIter);
		
		/* make a copy of component */
		DepsNode *component     = DEG_copy_node(dcc, old_component);
		
		/* add new node to hash... */
		BLI_ghash_insert(dst_node->component_hash, SET_INT_IN_POINTER(c_type), old_component);
	}
	
	// TODO: perform a second loop to fix up links?
}

/* Add 'id' node to graph */
static void dnti_id_ref__add_to_graph(Depsgraph *graph, DepsNode *node, const ID *id)
{
	/* add to hash so that it can be found */
	BLI_ghash_insert(graph->id_hash, (ID *)id, node);
}

/* Remove 'id' node from graph */
static void dnti_id_ref__remove_from_graph(Depsgraph *graph, DepsNode *node)
{
	IDDepsNode *id_node = (IDDepsNode *)node;
	
	/* remove toplevel node and hash entry, but don't free... */
	BLI_ghash_remove(graph->id_hash, id_node->id, NULL, NULL);
}

/* Validate links between components */
static void dnti_id_ref__validate_links(Depsgraph *graph, DepsNode *node)
{
	IDDepsNode *id_node = (IDDepsNode *)node;
	ListBase dummy_list = {NULL, NULL}; // XXX: perhaps this should live in the node?
	
	GHashIterator hashIter;
	
	/* get our components ......................................................................... */
	ComponentDepsNode *params = BLI_ghash_lookup(id_node->component_hash, SET_INT_IN_POINTER(DEPSNODE_TYPE_PARAMETERS));
	ComponentDepsNode *anim = BLI_ghash_lookup(id_node->component_hash,   SET_INT_IN_POINTER(DEPSNODE_TYPE_ANIMATION));
	ComponentDepsNode *trans = BLI_ghash_lookup(id_node->component_hash,  SET_INT_IN_POINTER(DEPSNODE_TYPE_TRANSFORM));
	ComponentDepsNode *geom = BLI_ghash_lookup(id_node->component_hash,   SET_INT_IN_POINTER(DEPSNODE_TYPE_GEOMETRY));
	ComponentDepsNode *proxy = BLI_ghash_lookup(id_node->component_hash,  SET_INT_IN_POINTER(DEPSNODE_TYPE_PROXY));
	ComponentDepsNode *pose = BLI_ghash_lookup(id_node->component_hash,   SET_INT_IN_POINTER(DEPSNODE_TYPE_EVAL_POSE));
	ComponentDepsNode *psys = BLI_ghash_lookup(id_node->component_hash,   SET_INT_IN_POINTER(DEPSNODE_TYPE_EVAL_PARTICLES));
	ComponentDepsNode *seq = BLI_ghash_lookup(id_node->component_hash,    SET_INT_IN_POINTER(DEPSNODE_TYPE_SEQUENCER));
	
	/* enforce (gross) ordering of these components................................................. */
	// TODO: create relationships to do this...
	
	/* parameters should always exist... */
	BLI_assert(params != NULL); // XXX: not always created though!
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
	GHASH_ITER(hashIter, id_node->component_hash) {
		DepsNode *component = BLI_ghashIterator_getValue(&hashIter);
		DepsNodeTypeInfo *nti = DEG_node_get_typeinfo(component);
		
		if (nti && nti->validate_links) {
			nti->validate_links(graph, component);
		}
	}
}

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

/* Subgraph Node ========================================== */

/* Initialise 'subgraph' node - from pointer data given */
static void dnti_subgraph__init_data(DepsNode *node, const ID *id, const char *UNUSED(subdata))
{
	SubgraphDepsNode *sgn = (SubgraphDepsNode *)node;
	
	/* store ID-ref if provided */
	sgn->root_id = (ID *)id;
	
	/* NOTE: graph will need to be added manually,
	 * as we don't have any way of passing this down
	 */
}

/* Free 'subgraph' node */
static void dnti_subgraph__free_data(DepsNode *node)
{
	SubgraphDepsNode *sgn = (SubgraphDepsNode *)node;
	
	/* only free if graph not shared, of if this node is the first reference to it... */
	// XXX: prune these flags a bit...
	if ((sgn->flag & SUBGRAPH_FLAG_FIRSTREF) || !(sgn->flag & SUBGRAPH_FLAG_SHARED)) {
		/* free the referenced graph */
		DEG_graph_free(sgn->graph);
		sgn->graph = NULL;
	}
}

/* Copy 'subgraph' node - Assume that the subgraph doesn't get copied for now... */
static void dnti_subgraph__copy_data(DepsgraphCopyContext *dcc, DepsNode *dst, const DepsNode *src)
{
	//const SubgraphDepsNode *src_node = (const SubgraphDepsNode *)src;
	//SubgraphDepsNode *dst_node       = (SubgraphDepsNode *)dst;
	
	/* for now, subgraph itself isn't copied... */
}

/* Add 'subgraph' node to graph */
static void dnti_subgraph__add_to_graph(Depsgraph *graph, DepsNode *node, const ID *id)
{
	/* add to subnodes list */
	BLI_addtail(&graph->subgraphs, node);
	
	/* if there's an ID associated, add to ID-nodes lookup too */
	if (id) {
		// TODO: what to do if subgraph's ID has already been added?
		BLI_assert(BLI_ghash_haskey(graph->id_hash, id) == false);
		BLI_ghash_insert(graph->id_hash, (ID *)id, node);
	}
}

/* Remove 'subgraph' node from graph */
static void dnti_subgraph__remove_from_graph(Depsgraph *graph, DepsNode *node)
{
	SubgraphDepsNode *sgn = (SubgraphDepsNode *)node;
	
	/* remove from subnodes list */
	BLI_remlink(&graph->subgraphs, node);
	
	/* remove from ID-nodes lookup */
	if (sgn->root_id) {
		BLI_assert(BLI_ghash_lookup(graph->id_hash, sgn->root_id) == sgn);
		BLI_ghash_remove(graph->id_hash, sgn->root_id, NULL, NULL);
	}
}

/* Validate subgraph links... */
static void dnti_subgraph__validate_links(Depsgraph *graph, DepsNode *node)
{
	
}

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

/* ******************************************************** */
/* Outer Nodes */

/* Standard Component Methods ============================= */

/* Initialise 'component' node - from pointer data given */
static void dnti_component__init_data(DepsNode *node, const ID *UNUSED(id), const char *UNUSED(subdata))
{
	ComponentDepsNode *component = (ComponentDepsNode *)node;
	
	/* create op-node hash */
	component->op_hash = BLI_ghash_str_new("DepsNode Component - Operations Hash");
	
	/* hook up eval context? */
	// XXX: maybe this needs a special API?
}

/* Copy 'component' node */
static void dnti_component__copy_data(DepsgraphCopyContext *dcc, DepsNode *dst, const DepsNode *src)
{
	const ComponentDepsNode *src_node = (const ComponentDepsNode *)src;
	ComponentDepsNode *dst_node       = (ComponentDepsNode *)dst;
	
	DepsNode *dst_op, *src_op;
	
	/* create new op-node hash (to host the copied data) */
	dst_node->op_hash = BLI_ghash_str_new("DepsNode Component - Operations Hash (Copy)");
	
	/* duplicate list of operation nodes */
	BLI_duplicatelist(&dst_node->ops, &src_node->ops);
	
	for (dst_op = dst_node->ops.first, src_op = src_node->ops.first; 
	     dst_op && src_op; 
	     dst_op = dst_op->next,   src_op = src_op->next)
	{
		DepsNodeTypeInfo *nti = DEG_node_get_typeinfo(src_op);
		
		/* recursive copy */
		if (nti && nti->copy_data)
			nti->copy_data(dcc, dst_op, src_op);
			
		/* fix links... */
		// ...
	}
	
	/* copy evaluation contexts */
	//
}

/* Free 'component' node */
static void dnti_component__free_data(DepsNode *node)
{
	ComponentDepsNode *component = (ComponentDepsNode *)node;
	DepsNode *op, *next;
	
	/* free nodes and list of nodes */
	for (op = component->ops.first; op; op = next) {
		next = op->next;
		
		DEG_free_node(op);
		BLI_freelinkN(&component->ops, op);
	}
	
	/* free hash too - no need to free as it should be empty now */
	BLI_ghash_free(component->op_hash, NULL, NULL);
	component->op_hash = NULL;
}

/* Add 'component' node to graph */
static void dnti_component__add_to_graph(Depsgraph *graph, DepsNode *node, const ID *id)
{
	IDDepsNode *id_node;
	
	/* find ID node that we belong to (and create it if it doesn't exist!) */
	id_node = (IDDepsNode *)DEG_get_node(graph, id, NULL, DEPSNODE_TYPE_ID_REF, NULL);
	BLI_assert(id_node != NULL);
	
	/* add component to id */
	BLI_ghash_insert(id_node->component_hash, SET_INT_IN_POINTER(node->type), node);
	node->owner = (DepsNode *)id_node;
}

/* Remove 'component' node from graph */
static void dnti_component__remove_from_graph(Depsgraph *graph, DepsNode *node)
{
	/* detach from owner (i.e. id-ref) */
	if (node->owner) {
		IDDepsNode *id_node = (IDDepsNode *)node->owner;
		
		BLI_ghash_remove(id_node->component_hash, SET_INT_IN_POINTER(node->type), NULL, NULL);
		node->owner = NULL;
	}
	
	/* NOTE: don't need to do anything about relationships,
	 * as those are handled via the standard mechanism
	 */
}

/* Parameter Component Defines ============================ */

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

/* Animation Component Defines ============================ */

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

/* Transform Component Defines ============================ */

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

/* Proxy Component Defines ================================ */

/* Proxy */
static DepsNodeTypeInfo DNTI_PROXY = {
	/* type */               DEPSNODE_TYPE_TRANSFORM,
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

/* Geometry Component Defines ============================= */

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

/* Sequencer Component Defines ============================ */

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

/* Pose Component ========================================= */

/* Initialise 'pose eval' node - from pointer data given */
static void dnti_pose_eval__init_data(DepsNode *node, const ID *id, const char *UNUSED(subdata))
{
	PoseComponentDepsNode *pcomp = (PoseComponentDepsNode *)node;
	
	/* generic component-node... */
	dnti_component__init_data(node, id, NULL);
	
	/* pose-specific data... */
	pcomp->bone_hash = BLI_ghash_str_new("Pose Component Bone Hash"); /* <String, BoneNode> */
}

/* Copy 'pose eval' node */
static void dnti_pose_eval__copy_data(DepsgraphCopyContext *dcc, DepsNode *dst, const DepsNode *src)
{
	const PoseComponentDepsNode *src_node = (const PoseComponentDepsNode *)src;
	PoseComponentDepsNode *dst_node       = (PoseComponentDepsNode *)dst;
	
	/* generic component node... */
	dnti_component__copy_data(dcc, dst, src);
	
	/* pose-specific data... */
	// copy bonehash...
}

/* Free 'pose eval' node */
static void dnti_pose_eval__free_data(DepsNode *node)
{
	PoseComponentDepsNode *pcomp = (PoseComponentDepsNode *)node;
	
	/* pose-specific data... */
	BLI_ghash_free(pcomp->bone_hash, NULL, /*dnti_pose_eval__hash_free_bone*/NULL);
	
	/* generic component node... */
	dnti_component__free_data(node);
}

void BKE_pose_rebuild_op(void *UNUSED(context), void *UNUSED(item)) {}
void BKE_pose_eval_init(void *UNUSED(context), void *UNUSED(item)) {}
void BKE_pose_eval_flush(void *UNUSED(context), void *UNUSED(item)) {}

/* Validate links for pose evaluation */
static void dnti_pose_eval__validate_links(Depsgraph *graph, DepsNode *node)
{
	PoseComponentDepsNode *pcomp = (PoseComponentDepsNode *)node;
	GHashIterator hashIter;
	
	/* create our core operations... */
	if (BLI_ghash_size(pcomp->bone_hash) || (pcomp->ops.first)) {
		OperationDepsNode *rebuild_op, *init_op, *cleanup_op;
		IDDepsNode *owner_node = (IDDepsNode *)pcomp->nd.owner;
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
		DEG_add_new_relation(&rebuild_op->nd, &init_op->nd,    DEPSREL_TYPE_COMPONENT_ORDER, "[Pose Rebuild -> Pose Init] DepsRel");
		DEG_add_new_relation(&init_op->nd,    &cleanup_op->nd, DEPSREL_TYPE_COMPONENT_ORDER, "[Pose Init -> Pose Cleanup] DepsRel");
		
		/* NOTE: bones will attach themselves to these endpoints */
	}
	
	/* ensure that each bone has been validated... */
	if (BLI_ghash_size(pcomp->bone_hash)) {
		DepsNodeTypeInfo *nti = DEG_get_node_typeinfo(DEPSNODE_TYPE_BONE);
		
		BLI_assert(nti && nti->validate_links);
		
		GHASH_ITER(hashIter, pcomp->bone_hash) {
			DepsNode *bone_comp = BLI_ghashIterator_getValue(&hashIter);
			
			/* recursively validate the links within bone component */
			// NOTE: this ends up hooking up the IK Solver(s) here to the relevant final bone operations...
			nti->validate_links(graph, bone_comp);
		}
	}
}

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

/* Bone Component ========================================= */

/* Initialise 'bone component' node - from pointer data given */
static void dnti_bone__init_data(DepsNode *node, const ID *id, const char subdata[MAX_NAME])
{
	BoneComponentDepsNode *bone_node = (BoneComponentDepsNode *)node;
	Object *ob = (Object *)id;
	
	/* generic component-node... */
	dnti_component__init_data(node, id, subdata);
	
	/* name of component comes is bone name */
	BLI_strncpy(node->name, subdata, MAX_NAME);
	
	/* bone-specific node data */
	bone_node->pchan = BKE_pose_channel_find_name(ob->pose, subdata);
}

/* Add 'bone component' node to graph */
static void dnti_bone__add_to_graph(Depsgraph *graph, DepsNode *node, const ID *id)
{
	PoseComponentDepsNode *pose_node;
	
	/* find pose node that we belong to (and create it if it doesn't exist!) */
	pose_node = (PoseComponentDepsNode *)DEG_get_node(graph, id, NULL, DEPSNODE_TYPE_EVAL_POSE, NULL);
	BLI_assert(pose_node != NULL);
	
	/* add bone component to pose bone-hash */
	BLI_ghash_insert(pose_node->bone_hash, node->name, node);
	node->owner = (DepsNode *)pose_node;
}

/* Remove 'bone component' node from graph */
static void dnti_bone__remove_from_graph(Depsgraph *graph, DepsNode *node)
{
	/* detach from owner (i.e. pose component) */
	if (node->owner) {
		PoseComponentDepsNode *pose_node = (PoseComponentDepsNode *)node->owner;
		
		BLI_ghash_remove(pose_node->bone_hash, node->name, NULL, NULL);
		node->owner = NULL;
	}
	
	/* NOTE: don't need to do anything about relationships,
	 * as those are handled via the standard mechanism
	 */
}

/* Validate 'bone component' links... 
 * - Re-route all component-level relationships to the nodes 
 */
static void dnti_bone__validate_links(Depsgraph *graph, DepsNode *node)
{
	PoseComponentDepsNode *pcomp = (PoseComponentDepsNode *)node->owner;
	BoneComponentDepsNode *bcomp = (BoneComponentDepsNode *)node;
	bPoseChannel *pchan = bcomp->pchan;
	
	DepsNode *btrans_op = BLI_ghash_lookup(bcomp->op_hash, "Bone Transforms");
	DepsNode *final_op = NULL;  /* normal final-evaluation operation */
	DepsNode *ik_op = NULL;     /* IK Solver operation */
	
	/* link bone/component to pose "sources" if it doesn't have any obvious dependencies */
	if (pchan->parent == NULL) {
		DepsNode *pinit_op = BLI_ghash_lookup(pcomp->op_hash, "Init Pose Eval");
		DEG_add_new_relation(pinit_op, btrans_op, DEPSREL_TYPE_OPERATION, "PoseEval Source-Bone Link");
	}
	
	/* inlinks destination should all go to the "Bone Transforms" operation */
	DEPSNODE_RELATIONS_ITER_BEGIN(node->inlinks.first, rel)
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
		final_op = BLI_ghash_lookup(bcomp->op_hash, "Constraint Stack");
	}
	else {
		/* just normal transforms */
		final_op = btrans_op;
	}
	
	DEPSNODE_RELATIONS_ITER_BEGIN(node->outlinks.first, rel)
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
	DEPSNODE_RELATIONS_ITER_BEGIN(node->outlinks.first, rel)
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
		DepsNode *ppost_op = BLI_ghash_lookup(pcomp->op_hash, "Cleanup Pose Eval");
		DEG_add_new_relation(final_op, ppost_op, DEPSREL_TYPE_OPERATION, "PoseEval Sink-Bone Link");
	}
}

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


/* ******************************************************** */
/* Inner Nodes */

/* Standard Operation Callbacks =========================== */
/* NOTE: some of these are just templates used by the others */

/* Helper to add 'operation' node to graph */
static void dnti_operation__add_to_graph(Depsgraph *graph, DepsNode *node,
                                         const ID *id, eDepsNode_Type component_type)
{
	/* get component node to add operation to */
	DepsNode *comp_node = DEG_get_node(graph, id, NULL, component_type, NULL);
	ComponentDepsNode *component = (ComponentDepsNode *)comp_node;
	
	/* add to hash and list */
	BLI_ghash_insert(component->op_hash, node->name, node);
	BLI_addtail(&component->ops, node);
	
	/* add backlink to component */
	node->owner = comp_node;
}

/* Callback to remove 'operation' node from graph */
static void dnti_operation__remove_from_graph(Depsgraph *UNUSED(graph), DepsNode *node)
{
	if (node->owner) {
		ComponentDepsNode *component = (ComponentDepsNode *)node;
		
		/* remove node from hash and list */
		BLI_ghash_remove(component->op_hash, node->name, NULL, NULL);
		BLI_remlink(&component->ops, node);
		
		/* remove backlink */
		node->owner = NULL;
	}
}

/* Parameter Operation ==================================== */

/* Add 'parameter operation' node to graph */
static void dnti_op_parameter__add_to_graph(Depsgraph *graph, DepsNode *node, const ID *id)
{
	dnti_operation__add_to_graph(graph, node, id, DEPSNODE_TYPE_PARAMETERS);
}

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

/* Proxy Operation ======================================== */

/* Add 'proxy operation' node to graph */
static void dnti_op_proxy__add_to_graph(Depsgraph *graph, DepsNode *node, const ID *id)
{
	dnti_operation__add_to_graph(graph, node, id, DEPSNODE_TYPE_PROXY);
}

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

/* Animation Operation ==================================== */

/* Add 'animation operation' node to graph */
static void dnti_op_animation__add_to_graph(Depsgraph *graph, DepsNode *node, const ID *id)
{
	dnti_operation__add_to_graph(graph, node, id, DEPSNODE_TYPE_ANIMATION);
}

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

/* Transform Operation ==================================== */

/* Add 'transform operation' node to graph */
static void dnti_op_transform__add_to_graph(Depsgraph *graph, DepsNode *node, const ID *id)
{
	dnti_operation__add_to_graph(graph, node, id, DEPSNODE_TYPE_TRANSFORM);
}

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

/* Geometry Operation ===================================== */

/* Add 'geometry operation' node to graph */
static void dnti_op_geometry__add_to_graph(Depsgraph *graph, DepsNode *node, const ID *id)
{
	dnti_operation__add_to_graph(graph, node, id, DEPSNODE_TYPE_GEOMETRY);
}

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

/* Sequencer Operation ==================================== */

/* Add 'sequencer operation' node to graph */
static void dnti_op_sequencer__add_to_graph(Depsgraph *graph, DepsNode *node, const ID *id)
{
	dnti_operation__add_to_graph(graph, node, id, DEPSNODE_TYPE_SEQUENCER);
}

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

/* Update Operation ======================================= */

/* Add 'update operation' node to graph */
static void dnti_op_update__add_to_graph(Depsgraph *graph, DepsNode *node, const ID *id)
{
	dnti_operation__add_to_graph(graph, node, id, DEPSNODE_TYPE_PARAMETERS);
}

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

/* Driver Operation ===================================== */
// XXX: some special tweaks may be needed for this one...

/* Add 'driver operation' node to graph */
static void dnti_op_driver__add_to_graph(Depsgraph *graph, DepsNode *node, const ID *id)
{
	dnti_operation__add_to_graph(graph, node, id, DEPSNODE_TYPE_PARAMETERS);
}

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

/* Pose Operation ========================================= */

/* Add 'pose operation' node to graph */
static void dnti_op_pose__add_to_graph(Depsgraph *graph, DepsNode *node, const ID *id)
{
	dnti_operation__add_to_graph(graph, node, id, DEPSNODE_TYPE_EVAL_POSE);
}

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

/* Bone Operation ========================================= */

/* Init local data for bone operation */
static void dnti_op_bone__init_data(DepsNode *node, const ID *id, const char subdata[MAX_NAME])
{
	OperationDepsNode *bone_op = (OperationDepsNode *)node;
	Object *ob;
	bPoseChannel *pchan;
	
	/* set up RNA Pointer to affected bone */
	ob = (Object *)id;
	pchan = BKE_pose_channel_find_name(ob->pose, subdata);
	
	RNA_pointer_create((ID *)id, &RNA_PoseBone, pchan, &bone_op->ptr);
}

/* Add 'bone operation' node to graph */
static void dnti_op_bone__add_to_graph(Depsgraph *graph, DepsNode *node, const ID *id)
{
	OperationDepsNode *bone_op = (OperationDepsNode *)node;
	BoneComponentDepsNode *bone_comp;
	bPoseChannel *pchan;
	
	/* get bone component that owns this bone operation */
	BLI_assert(bone_op->ptr.type == &RNA_PoseBone);
	pchan = (bPoseChannel *)bone_op->ptr.data;
	
	bone_comp = (BoneComponentDepsNode *)DEG_get_node(graph, id, pchan->name, DEPSNODE_TYPE_BONE, NULL);
	
	/* add to hash and list as per usual */
	BLI_ghash_insert(bone_comp->op_hash, pchan->name, node);
	BLI_addtail(&bone_comp->ops, node);
	
	/* add backlink to component */
	node->owner = &bone_comp->nd;
}

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

/* Particle Operation ===================================== */

/* Add 'particle operation' node to graph */
static void dnti_op_particle__add_to_graph(Depsgraph *graph, DepsNode *node, const ID *id)
{
	dnti_operation__add_to_graph(graph, node, id, DEPSNODE_TYPE_EVAL_PARTICLES);
}

/* Remove 'particle operation' node from graph */
static void dnti_op_particle__remove_from_graph(Depsgraph *graph, DepsNode *node)
{
	// XXX...
	dnti_operation__remove_from_graph(graph, node);
}

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

/* RigidBody Operation ==================================== */
/* Note: RigidBody Operations are reserved for scene-level rigidbody sim steps */

/* Add 'rigidbody operation' node to graph */
static void dnti_op_rigidbody__add_to_graph(Depsgraph *graph, DepsNode *node, const ID *id)
{
	dnti_operation__add_to_graph(graph, node, id, DEPSNODE_TYPE_TRANSFORM); // XXX
}

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
void DEG_register_node_typeinfo(DepsNodeTypeInfo *typeinfo)
{
	if (typeinfo) {
		eDepsNode_Type type = typeinfo->type;
		BLI_ghash_insert(_depsnode_typeinfo_registry, SET_INT_IN_POINTER(type), typeinfo);
	}
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
	DEG_register_node_typeinfo(&DNTI_OP_PARAMETER);
	DEG_register_node_typeinfo(&DNTI_OP_PROXY);
	DEG_register_node_typeinfo(&DNTI_OP_ANIMATION);
	DEG_register_node_typeinfo(&DNTI_OP_TRANSFORM);
	DEG_register_node_typeinfo(&DNTI_OP_GEOMETRY);
	DEG_register_node_typeinfo(&DNTI_OP_SEQUENCER);
	
	DEG_register_node_typeinfo(&DNTI_OP_UPDATE);
	DEG_register_node_typeinfo(&DNTI_OP_DRIVER);
	
	DEG_register_node_typeinfo(&DNTI_OP_POSE);
	DEG_register_node_typeinfo(&DNTI_OP_BONE);
	
	DEG_register_node_typeinfo(&DNTI_OP_PARTICLE);
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
	return BLI_ghash_lookup(_depsnode_typeinfo_registry, SET_INT_IN_POINTER(type));
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

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

#include <string.h>

extern "C" {
#include "DNA_object_types.h"

#include "BKE_action.h"
} /* extern "C" */

#include "depsnode_component.h" /* own include */
#include "depsnode_operation.h"
#include "depsgraph_intern.h"

#include "stubs.h" // XXX: THIS MUST BE REMOVED WHEN THE DEPSGRAPH REFACTOR IS DONE

/* ******************************************************** */
/* Outer Nodes */

/* Standard Component Methods ============================= */

OperationDepsNode *ComponentDepsNode::find_operation(const char *name) const
{
	OperationMap::const_iterator it = this->operations.find(name);
	return it != this->operations.end() ? it->second : NULL;
}

/* Initialise 'component' node - from pointer data given */
void ComponentDepsNode::init(const ID *id, const char *subdata)
{
	/* hook up eval context? */
	// XXX: maybe this needs a special API?
}

/* Copy 'component' node */
void ComponentDepsNode::copy(DepsgraphCopyContext *dcc, const ComponentDepsNode *src)
{
	/* duplicate list of operation nodes */
	this->operations.clear();
	
	for (OperationMap::const_iterator it = src->operations.begin(); it != src->operations.end(); ++it) {
		const char *pchan_name = it->first;
		OperationDepsNode *src_op = it->second;
		
		/* recursive copy */
		DepsNodeFactory *factory = DEG_node_get_factory(src_op);
		OperationDepsNode *dst_op = (OperationDepsNode *)factory->copy_node(dcc, src_op);
		this->operations[pchan_name] = dst_op;
			
		/* fix links... */
		// ...
	}
	
	/* copy evaluation contexts */
	//
}

/* Free 'component' node */
ComponentDepsNode::~ComponentDepsNode()
{
	/* free nodes and list of nodes */
	for (OperationMap::const_iterator it = this->operations.begin(); it != this->operations.end(); ++it) {
		OperationDepsNode *op = it->second;
		delete op;
	}
}

/* Add 'component' node to graph */
void ComponentDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	/* find ID node that we belong to (and create it if it doesn't exist!) */
	IDDepsNode *id_node = (IDDepsNode *)graph->get_node(id, NULL, DEPSNODE_TYPE_ID_REF, NULL);
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
		IDDepsNode *id_node = this->owner;
		id_node->components.erase(this->type);
		this->owner = NULL;
	}
	
	/* NOTE: don't need to do anything about relationships,
	 * as those are handled via the standard mechanism
	 */
}

/* Parameter Component Defines ============================ */

DEG_DEPSNODE_DEFINE(ParametersComponentDepsNode, DEPSNODE_TYPE_PARAMETERS, "Parameters Component");
static DepsNodeFactoryImpl<ParametersComponentDepsNode> DNTI_PARAMETERS;

/* Animation Component Defines ============================ */

DEG_DEPSNODE_DEFINE(AnimationComponentDepsNode, DEPSNODE_TYPE_ANIMATION, "Animation Component");
static DepsNodeFactoryImpl<AnimationComponentDepsNode> DNTI_ANIMATION;

/* Transform Component Defines ============================ */

DEG_DEPSNODE_DEFINE(TransformComponentDepsNode, DEPSNODE_TYPE_TRANSFORM, "Transform Component");
static DepsNodeFactoryImpl<TransformComponentDepsNode> DNTI_TRANSFORM;

/* Proxy Component Defines ================================ */

DEG_DEPSNODE_DEFINE(ProxyComponentDepsNode, DEPSNODE_TYPE_PROXY, "Proxy Component");
static DepsNodeFactoryImpl<ProxyComponentDepsNode> DNTI_PROXY;

/* Geometry Component Defines ============================= */

DEG_DEPSNODE_DEFINE(GeometryComponentDepsNode, DEPSNODE_TYPE_GEOMETRY, "Geometry Component");
static DepsNodeFactoryImpl<GeometryComponentDepsNode> DNTI_GEOMETRY;

/* Sequencer Component Defines ============================ */

DEG_DEPSNODE_DEFINE(SequencerComponentDepsNode, DEPSNODE_TYPE_SEQUENCER, "Sequencer Component");
static DepsNodeFactoryImpl<SequencerComponentDepsNode> DNTI_SEQUENCER;

/* Pose Component ========================================= */

BoneComponentDepsNode *PoseComponentDepsNode::find_bone_component(const char *name) const
{
	BoneComponentMap::const_iterator it = this->bone_hash.find(name);
	return it != this->bone_hash.end() ? it->second : NULL;
}

/* Initialise 'pose eval' node - from pointer data given */
void PoseComponentDepsNode::init(const ID *id, const char *subdata)
{
	/* generic component-node... */
	ComponentDepsNode::init(id, subdata);
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
}

/* Validate links for pose evaluation */
void PoseComponentDepsNode::validate_links(Depsgraph *graph)
{
	/* create our core operations... */
	if (!this->bone_hash.empty() || !this->operations.empty()) {
		OperationDepsNode *rebuild_op, *init_op, *cleanup_op;
		IDDepsNode *owner_node = (IDDepsNode *)this->owner;
		Object *ob;
		ID *id;
		
		/* get ID-block that pose-component belongs to */
		BLI_assert(owner_node && owner_node->id);
		
		id = owner_node->id;
		ob = (Object *)id;
		
		/* create standard pose evaluation start/end hooks */
		rebuild_op = graph->add_operation(id, NULL, DEPSNODE_TYPE_OP_POSE,
		                                  DEPSOP_TYPE_REBUILD, BKE_pose_rebuild_op,
		                                  "Rebuild Pose");
		RNA_pointer_create(id, &RNA_Pose, ob->pose, &rebuild_op->ptr);
		
		init_op = graph->add_operation(id, NULL, DEPSNODE_TYPE_OP_POSE,
		                               DEPSOP_TYPE_INIT, BKE_pose_eval_init,
		                               "Init Pose Eval");
		RNA_pointer_create(id, &RNA_Pose, ob->pose, &init_op->ptr);
		
		cleanup_op = graph->add_operation(id, NULL, DEPSNODE_TYPE_OP_POSE,
		                                  DEPSOP_TYPE_POST, BKE_pose_eval_flush,
		                                  "Flush Pose Eval");
		RNA_pointer_create(id, &RNA_Pose, ob->pose, &cleanup_op->ptr);
		
		
		/* attach links between these operations */
		graph->add_new_relation(rebuild_op, init_op,    DEPSREL_TYPE_COMPONENT_ORDER, "[Pose Rebuild -> Pose Init] DepsRel");
		graph->add_new_relation(init_op,    cleanup_op, DEPSREL_TYPE_COMPONENT_ORDER, "[Pose Init -> Pose Cleanup] DepsRel");
		
		/* NOTE: bones will attach themselves to these endpoints */
	}
	
	/* ensure that each bone has been validated... */
	for (PoseComponentDepsNode::BoneComponentMap::const_iterator it = this->bone_hash.begin(); it != this->bone_hash.end(); ++it) {
		DepsNode *bone_comp = it->second;
		/* recursively validate the links within bone component */
		// NOTE: this ends up hooking up the IK Solver(s) here to the relevant final bone operations...
		bone_comp->validate_links(graph);
	}
}

DEG_DEPSNODE_DEFINE(PoseComponentDepsNode, DEPSNODE_TYPE_EVAL_POSE, "Pose Eval Component");
static DepsNodeFactoryImpl<PoseComponentDepsNode> DNTI_EVAL_POSE;

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
	pose_node = (PoseComponentDepsNode *)graph->get_node(id, NULL, DEPSNODE_TYPE_EVAL_POSE, NULL);
	BLI_assert(pose_node != NULL);
	
	/* add bone component to pose bone-hash */
	pose_node->bone_hash[this->name] = this;
#pragma message("DEPSGRAPH PORTING XXX: Type mismatch! bone components are actually sub-components, this should be a distinct class")
//	this->owner = pose_node;
}

/* Remove 'bone component' node from graph */
void BoneComponentDepsNode::remove_from_graph(Depsgraph *graph)
{
	/* detach from owner (i.e. pose component) */
	if (this->owner) {
		PoseComponentDepsNode *pose_node = (PoseComponentDepsNode *)this->owner;
		
		pose_node->bone_hash.erase(this->name);
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
	
	DepsNode *btrans_op = this->find_operation("Bone Transforms");
	DepsNode *final_op = NULL;  /* normal final-evaluation operation */
	DepsNode *ik_op = NULL;     /* IK Solver operation */
	
	BLI_assert(btrans_op != NULL);
	
	/* link bone/component to pose "sources" if it doesn't have any obvious dependencies */
	if (pchan->parent == NULL) {
		DepsNode *pinit_op = pcomp->find_operation("Init Pose Eval");
		graph->add_new_relation(pinit_op, btrans_op, DEPSREL_TYPE_OPERATION, "PoseEval Source-Bone Link");
	}
	
	/* inlinks destination should all go to the "Bone Transforms" operation */
	DEPSNODE_RELATIONS_ITER_BEGIN(this->inlinks, rel)
	{
		/* add equivalent relation to the bone transform operation */
		graph->add_new_relation(rel->from, btrans_op, rel->type, rel->name);
	}
	DEPSNODE_RELATIONS_ITER_END;
	
	
	/* outlink source target depends on what we might have:
	 * 1) Transform only - No constraints at all
	 * 2) Constraints node - Just plain old constraints
	 * 3) IK Solver node - If part of IK chain...
	 */
	if (pchan->constraints.first) {
		/* find constraint stack operation */
		final_op = this->find_operation("Constraint Stack");
	}
	else {
		/* just normal transforms */
		final_op = btrans_op;
	}
	
	DEPSNODE_RELATIONS_ITER_BEGIN(this->outlinks, rel)
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
	DEPSNODE_RELATIONS_ITER_BEGIN(this->outlinks, rel)
	{
		if (ik_op) {
			/* bone is part of IK Chain... */
			if (rel->to == ik_op) {
				/* can't have ik to ik, so use final "normal" bone transform 
				 * as indicator to IK Solver that it is ready to run 
				 */
				graph->add_new_relation(final_op, rel->to, rel->type, rel->name);
			}
			else {
				/* everything which depends on result of this bone needs to know 
				 * about the IK result too!
				 */
				graph->add_new_relation(ik_op, rel->to, rel->type, rel->name);
			}
		}
		else {
			/* bone is not part of IK Chains... */
			graph->add_new_relation(final_op, rel->to, rel->type, rel->name);
		}
	}
	DEPSNODE_RELATIONS_ITER_END;
	
	/* link bone/component to pose "sinks" as final link, unless it has obvious quirks */
	{
		DepsNode *ppost_op = this->find_operation("Cleanup Pose Eval");
		graph->add_new_relation(final_op, ppost_op, DEPSREL_TYPE_OPERATION, "PoseEval Sink-Bone Link");
	}
}

DEG_DEPSNODE_DEFINE(BoneComponentDepsNode, DEPSNODE_TYPE_BONE, "Bone Component");
static DepsNodeFactoryImpl<BoneComponentDepsNode> DNTI_BONE;


void DEG_register_component_depsnodes()
{
	DEG_register_node_typeinfo(&DNTI_PARAMETERS);
	DEG_register_node_typeinfo(&DNTI_PROXY);
	DEG_register_node_typeinfo(&DNTI_ANIMATION);
	DEG_register_node_typeinfo(&DNTI_TRANSFORM);
	DEG_register_node_typeinfo(&DNTI_GEOMETRY);
	DEG_register_node_typeinfo(&DNTI_SEQUENCER);
	
	DEG_register_node_typeinfo(&DNTI_EVAL_POSE);
	DEG_register_node_typeinfo(&DNTI_BONE);
	
	//DEG_register_node_typeinfo(&DNTI_EVAL_PARTICLES);
}

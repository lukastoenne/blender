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

extern "C" {
#include "BLI_utildefines.h"

#include "DNA_object_types.h"

#include "BKE_action.h"
} /* extern "C" */

#include "depsnode_operation.h" /* own include */
#include "depsnode_component.h"
#include "depsgraph.h"
#include "depsgraph_intern.h"

#include "stubs.h" // XXX: THIS MUST BE REMOVED WHEN THE DEPSGRAPH REFACTOR IS DONE

/* ******************************************************** */
/* Inner Nodes */

/* Standard Operation Callbacks =========================== */
/* NOTE: some of these are just templates used by the others */

/* Helper to add 'operation' node to graph */
void OperationDepsNode::add_to_component_node(Depsgraph *graph, const ID *id, eDepsNode_Type component_type)
{
	/* get component node to add operation to */
	ComponentDepsNode *component = (ComponentDepsNode *)graph->get_node(id, NULL, component_type, NULL);
	
	/* add to hash table */
	component->operations[this->name] = this;
	
	/* add backlink to component */
	this->owner = component;
}

/* Callback to remove 'operation' node from graph */
void OperationDepsNode::remove_from_graph(Depsgraph *UNUSED(graph))
{
	if (this->owner) {
		ComponentDepsNode *component = (ComponentDepsNode *)this->owner;
		
		/* remove node from hash table */
		component->operations.erase(this->name);
		
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
static DepsNodeFactoryImpl<ParametersOperationDepsNode> DNTI_OP_PARAMETERS;

/* Proxy Operation ======================================== */

/* Add 'proxy operation' node to graph */
void ProxyOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_PROXY);
}

DEG_DEPSNODE_DEFINE(ProxyOperationDepsNode, DEPSNODE_TYPE_OP_PROXY, "Proxy Operation");
static DepsNodeFactoryImpl<ProxyOperationDepsNode> DNTI_OP_PROXY;

/* Animation Operation ==================================== */

/* Add 'animation operation' node to graph */
void AnimationOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_ANIMATION);
}

DEG_DEPSNODE_DEFINE(AnimationOperationDepsNode, DEPSNODE_TYPE_OP_ANIMATION, "Animation Operation");
static DepsNodeFactoryImpl<AnimationOperationDepsNode> DNTI_OP_ANIMATION;

/* Transform Operation ==================================== */

/* Add 'transform operation' node to graph */
void TransformOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_TRANSFORM);
}

DEG_DEPSNODE_DEFINE(TransformOperationDepsNode, DEPSNODE_TYPE_OP_TRANSFORM, "Transform Operation");
static DepsNodeFactoryImpl<TransformOperationDepsNode> DNTI_OP_TRANSFORM;

/* Geometry Operation ===================================== */

/* Add 'geometry operation' node to graph */
void GeometryOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_GEOMETRY);
}

DEG_DEPSNODE_DEFINE(GeometryOperationDepsNode, DEPSNODE_TYPE_OP_GEOMETRY, "Geometry Operation");
static DepsNodeFactoryImpl<GeometryOperationDepsNode> DNTI_OP_GEOMETRY;

/* Sequencer Operation ==================================== */

/* Add 'sequencer operation' node to graph */
void SequencerOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_SEQUENCER);
}

DEG_DEPSNODE_DEFINE(SequencerOperationDepsNode, DEPSNODE_TYPE_OP_SEQUENCER, "Sequencer Operation");
static DepsNodeFactoryImpl<SequencerOperationDepsNode> DNTI_OP_SEQUENCER;

/* Update Operation ======================================= */

/* Add 'update operation' node to graph */
void UpdateOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_PARAMETERS);
}

DEG_DEPSNODE_DEFINE(UpdateOperationDepsNode, DEPSNODE_TYPE_OP_UPDATE, "RNA Update Operation");
static DepsNodeFactoryImpl<UpdateOperationDepsNode> DNTI_OP_UPDATE;

/* Driver Operation ===================================== */
// XXX: some special tweaks may be needed for this one...

/* Add 'driver operation' node to graph */
void DriverOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_PARAMETERS);
}

DEG_DEPSNODE_DEFINE(DriverOperationDepsNode, DEPSNODE_TYPE_OP_DRIVER, "Driver Operation");
static DepsNodeFactoryImpl<DriverOperationDepsNode> DNTI_OP_DRIVER;

/* Pose Operation ========================================= */

/* Add 'pose operation' node to graph */
void PoseOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_EVAL_POSE);
}

DEG_DEPSNODE_DEFINE(PoseOperationDepsNode, DEPSNODE_TYPE_OP_POSE, "Pose Operation");
static DepsNodeFactoryImpl<PoseOperationDepsNode> DNTI_OP_POSE;

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
	
	bone_comp = (BoneComponentDepsNode *)graph->get_node(id, pchan->name, DEPSNODE_TYPE_BONE, NULL);
	
	/* add to hash table */
	bone_comp->operations[pchan->name] = this;
	
	/* add backlink to component */
	this->owner = bone_comp;
}

DEG_DEPSNODE_DEFINE(BoneOperationDepsNode, DEPSNODE_TYPE_OP_BONE, "Bone Operation");
static DepsNodeFactoryImpl<BoneOperationDepsNode> DNTI_OP_BONE;

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
static DepsNodeFactoryImpl<ParticlesOperationDepsNode> DNTI_OP_PARTICLES;

/* RigidBody Operation ==================================== */
/* Note: RigidBody Operations are reserved for scene-level rigidbody sim steps */

/* Add 'rigidbody operation' node to graph */
void RigidBodyOperationDepsNode::add_to_graph(Depsgraph *graph, const ID *id)
{
	add_to_component_node(graph, id, DEPSNODE_TYPE_TRANSFORM); // XXX
}

DEG_DEPSNODE_DEFINE(RigidBodyOperationDepsNode, DEPSNODE_TYPE_OP_RIGIDBODY, "RigidBody Operation");
static DepsNodeFactoryImpl<RigidBodyOperationDepsNode> DNTI_OP_RIGIDBODY;

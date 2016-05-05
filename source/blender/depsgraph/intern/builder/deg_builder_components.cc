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
 * Contributor(s): Based on original depsgraph.c code - Blender Foundation (2005-2013)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/depsgraph_build_components.cc
 *  \ingroup depsgraph
 *
 * Build individual components out of operations and their relations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_blenlib.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_group_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_node_types.h"
#include "DNA_particle_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_animsys.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_effect.h"
#include "BKE_fcurve.h"
#include "BKE_idcode.h"
#include "BKE_group.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_mball.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_rigidbody.h"
#include "BKE_sound.h"
#include "BKE_texture.h"
#include "BKE_tracking.h"
#include "BKE_world.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "RNA_access.h"
#include "RNA_types.h"
} /* extern "C" */

#include "intern/builder/deg_builder.h"


namespace DEG {

/* **** Build functions for entity nodes **** */

void deg_build_scene(DepsgraphBuilder &context, Scene *scene)
{
	IDNodeBuilder builder(context, &scene->id);

	/* timesource */
	context.add_time_source();

	/* build subgraph for set, and link this in... */
	// XXX: depending on how this goes, that scene itself could probably store its
	//      own little partial depsgraph?
	if (scene->set) {
		deg_build_scene(context, scene->set);
		// TODO: link set to scene, especially our timesource...
	}

	/* scene objects */
	for (Base *base = (Base *)scene->base.first; base; base = base->next) {
		Object *ob = base->object;

		/* object itself */
		deg_build_object(context, scene, base, ob);

		/* object that this is a proxy for */
		// XXX: the way that proxies work needs to be completely reviewed!
		if (ob->proxy) {
			ob->proxy->proxy_from = ob;
			deg_build_object(context, scene, base, ob->proxy);
		}
	}

	/* rigidbody */
	if (scene->rigidbody_world) {
		deg_build_rigidbody(builder, scene);
	}

	/* scene's animation and drivers */
	if (scene->adt) {
		deg_build_animdata(builder, &scene->id);
	}

	/* world */
	if (scene->world) {
		deg_build_world(context, scene->world);
	}

	/* compo nodes */
	if (scene->nodetree) {
		deg_build_compositor(context, scene);
	}

	/* sequencer */
	// XXX...

	/* grease pencil */
	if (scene->gpd) {
		deg_build_gpencil(context, scene->gpd);
	}
}

void deg_build_group(DepsgraphBuilder &context,
                     Scene *scene,
                     Base *base,
                     Group *group)
{
	if (context.has_id(&group->id)) {
		return;
	}
	
	IDNodeBuilder builder(context, &group->id);
	ComponentBuilder geom_builder(builder, DEPSNODE_TYPE_GEOMETRY);
	
	for (GroupObject *go = (GroupObject *)group->gobject.first;
	     go != NULL;
	     go = go->next)
	{
		deg_build_object(context, scene, base, go->ob);
		
		geom_builder.add_id_dependency(DEPSREL_TYPE_GEOMETRY_EVAL, "Group member geometry",
		                               &go->ob->id, DEPSNODE_TYPE_GEOMETRY);
		geom_builder.add_id_dependency(DEPSREL_TYPE_TRANSFORM, "Group member transform",
		                               &go->ob->id, DEPSNODE_TYPE_TRANSFORM);
	}
}

void deg_build_subgraph(DepsgraphBuilder &context, Group *group)
{
	/* sanity checks */
	if (!group)
		return;

	/* create new subgraph's data */
	Depsgraph *subgraph = reinterpret_cast<Depsgraph *>(DEG_graph_new());

	DepsgraphBuilder subgraph_builder(subgraph);
	UNUSED_VARS(subgraph_builder);

	/* add group objects */
	for (GroupObject *go = (GroupObject *)group->gobject.first;
	     go != NULL;
	     go = go->next)
	{
		/*Object *ob = go->ob;*/

		/* Each "group object" is effectively a separate instance of the underlying
		 * object data. When the group is evaluated, the transform results and/or
		 * some other attributes end up getting overridden by the group
		 */
	}

	context.add_subgraph(subgraph, &group->id);
}

void deg_build_object(DepsgraphBuilder &context, Scene *scene, Base *base, Object *ob)
{
	if (context.has_id(&ob->id)) {
		return;
	}
	
	IDNodeBuilder builder(context, &ob->id);
	ComponentBuilder param_builder(builder, DEPSNODE_TYPE_PARAMETERS);
	
	builder.set_layers(base->lay);
	
	/* standard components */
	deg_build_object_transform(builder, scene, ob);

	/* ShapeKeys */
	Key *key = BKE_key_from_object(ob);
	if (key != NULL) {
		deg_build_shapekeys(context, key);
	}

	/* object data */
	if (ob->data) {
		/* type-specific data... */
		switch (ob->type) {
			case OB_MESH:     /* Geometry */
			case OB_CURVE:
			case OB_FONT:
			case OB_SURF:
			case OB_MBALL:
			case OB_LATTICE:
			{
				/* TODO(sergey): This way using this object's
				 * properties as driver target works fine.
				 *
				 * Does this depend on other nodes?
				 */
				param_builder.define_operation(DEPSOP_TYPE_POST,
				                               NULL,
				                               DEG_OPCODE_PLACEHOLDER, "Parameters Eval");

				deg_build_object_geom(builder, scene, ob);
				deg_build_obdata(context, scene, ob);
				
				/* materials */
				for (int a = 1; a <= ob->totcol; a++) {
					Material *ma = give_current_material(ob, a);
					
					if (ma) {
						// XXX?!
						deg_build_material(context, &ob->id, ma);
					}
				}
				
				break;
			}

			case OB_ARMATURE: /* Pose */
				if (ob->id.lib != NULL && ob->proxy_from != NULL) {
					deg_build_proxy_rig(builder, ob);
				}
				else {
					deg_build_rig(builder, scene, ob);
				}
				break;

			case OB_LAMP:   /* Lamp */
				deg_build_lamp(context, ob);
				break;

			case OB_CAMERA: /* Camera */
				deg_build_camera(context, ob);
				break;

			default:
			{
				ID *obdata = (ID *)ob->data;
				if (!context.has_id(obdata)) {
					IDNodeBuilder obdata_builder(context, obdata);
					deg_build_animdata(obdata_builder, obdata);
				}
				break;
			}
		}
	}

	/* Build animation data,
	 *
	 * Do it now because it's possible object data will affect
	 * on object's level animation, for example in case of rebuilding
	 * pose for proxy.
	 */
	deg_build_animdata(builder, &ob->id);

	/* particle systems */
	
	/* particle systems */
	if (ob->particlesystem.first) {
		/* particle settings */
		for (ParticleSystem *psys = (ParticleSystem *)ob->particlesystem.first; psys; psys = psys->next) {
			ParticleSettings *part = psys->part;
			if (!context.has_id(&part->id)) {
				IDNodeBuilder pset_builder(context, &part->id);
				deg_build_animdata(pset_builder, &part->id);
			}
		}
		
		deg_build_particles(builder, scene, ob);
	}

	/* grease pencil */
	if (ob->gpd) {
		deg_build_gpencil(context, ob->gpd);
	}

	/* Object dupligroup. */
	if (ob->dup_group) {
		deg_build_group(context, scene, base, ob->dup_group);
		
		ComponentBuilder geom_builder(builder, DEPSNODE_TYPE_GEOMETRY);
		geom_builder.add_dependency(DEPSREL_TYPE_GEOMETRY_EVAL, "Geometry transform",
		                            DEPSNODE_TYPE_TRANSFORM);
		geom_builder.add_id_dependency(DEPSREL_TYPE_GEOMETRY_EVAL, "Dupli group geometry",
		                               &ob->dup_group->id, DEPSNODE_TYPE_GEOMETRY);
	}
}

void deg_build_object_transform(IDNodeBuilder &context, Scene *scene, Object *ob)
{
	ComponentBuilder tfm_builder(context, DEPSNODE_TYPE_TRANSFORM);
	
	/* local transforms (from transform channels - loc/rot/scale + deltas) */
	Operation op_local = tfm_builder.define_operation(DEPSOP_TYPE_INIT,
	                                                  function_bind(BKE_object_eval_local_transform, _1, scene, ob),
	                                                  DEG_OPCODE_TRANSFORM_LOCAL);
	Operation op_base = op_local;

	tfm_builder.add_dependency(DEPSREL_TYPE_OPERATION, "Object Animation", DEPSNODE_TYPE_ANIMATION);
	

	/* object parent */
	if (ob->parent) {
		Operation op_parent = tfm_builder.define_operation(DEPSOP_TYPE_EXEC,
		                                                    function_bind(BKE_object_eval_parent, _1, scene, ob),
		                                                    DEG_OPCODE_TRANSFORM_PARENT);
		op_base = op_parent;
		
		tfm_builder.add_relation(DEPSREL_TYPE_COMPONENT_ORDER, "[ObLocal -> ObParent]", op_local, op_parent);
	}

	/* Temporary uber-update node, which does everything.
	 * It is for the being we're porting old dependencies into the new system.
	 * We'll get rid of this node as soon as all the granular update functions
	 * are filled in.
	 *
	 * TODO(sergey): Get rid of this node.
	 */
	Operation op_uber = tfm_builder.define_operation(DEPSOP_TYPE_EXEC,
	                                                 function_bind(BKE_object_eval_uber_transform, _1, scene, ob),
	                                                 DEG_OPCODE_OBJECT_UBEREVAL);

	/* object transform is done */
	Operation op_final = tfm_builder.define_operation(DEPSOP_TYPE_POST,
	                                                  function_bind(BKE_object_eval_done, _1, ob),
	                                                  DEG_OPCODE_TRANSFORM_FINAL);
	
	/* object constraints */
	if (ob->constraints.first) {
		Operation op_constraints = deg_build_object_constraints(context, scene, ob);
		
		/* operation order */
		tfm_builder.add_relation(DEPSREL_TYPE_COMPONENT_ORDER, "[ObBase-> Constraint Stack]", op_base, op_constraints);
		tfm_builder.add_relation(DEPSREL_TYPE_COMPONENT_ORDER, "[ObConstraints -> Done]", op_constraints, op_final);

		// XXX
		tfm_builder.add_relation(DEPSREL_TYPE_COMPONENT_ORDER, "Temp Ubereval", op_constraints, op_uber);
		tfm_builder.add_relation(DEPSREL_TYPE_COMPONENT_ORDER, "Temp Ubereval", op_uber, op_final);
	}
	else {
		/* operation order */
		tfm_builder.add_relation(DEPSREL_TYPE_COMPONENT_ORDER, "Object Transform", op_base, op_final);

		// XXX
		tfm_builder.add_relation(DEPSREL_TYPE_COMPONENT_ORDER, "Temp Ubereval", op_base, op_uber);
		tfm_builder.add_relation(DEPSREL_TYPE_COMPONENT_ORDER, "Temp Ubereval", op_uber, op_final);
	}
}

/**
 * Constraints Graph Notes
 *
 * For constraints, we currently only add a operation node to the Transform
 * or Bone components (depending on whichever type of owner we have).
 * This represents the entire constraints stack, which is for now just
 * executed as a single monolithic block. At least initially, this should
 * be sufficient for ensuring that the porting/refactoring process remains
 * manageable.
 *
 * However, when the time comes for developing "node-based" constraints,
 * we'll need to split this up into pre/post nodes for "constraint stack
 * evaluation" + operation nodes for each constraint (i.e. the contents
 * of the loop body used in the current "solve_constraints()" operation).
 *
 * -- Aligorith, August 2013
 */
Operation deg_build_object_constraints(IDNodeBuilder &context, Scene *scene, Object *ob)
{
	ComponentBuilder tfm_builder(context, DEPSNODE_TYPE_TRANSFORM);
	
	/* create node for constraint stack */
	return tfm_builder.define_operation(DEPSOP_TYPE_EXEC,
	                                    function_bind(BKE_object_eval_constraints, _1, scene, ob),
	                                    DEG_OPCODE_TRANSFORM_CONSTRAINTS);
}

Operation deg_build_pose_constraints(IDNodeBuilder &context, Object *ob, bPoseChannel *pchan)
{
	ComponentBuilder bone_builder(context, DEPSNODE_TYPE_BONE, pchan->name);
	
	/* create node for constraint stack */
	return bone_builder.define_operation(DEPSOP_TYPE_EXEC,
	                                     function_bind(BKE_pose_constraints_evaluate, _1, ob, pchan),
	                                     DEG_OPCODE_BONE_CONSTRAINTS);
}

/**
 * Build graph nodes for AnimData block
 * \param id: ID-Block which hosts the AnimData
 */
void deg_build_animdata(IDNodeBuilder &context, ID *id)
{
	ComponentBuilder anim_builder(context, DEPSNODE_TYPE_ANIMATION);
	AnimData *adt = BKE_animdata_from_id(id);

	if (adt == NULL)
		return;

	/* animation */
	if (adt->action || adt->nla_tracks.first || adt->drivers.first) {
		// XXX: Hook up specific update callbacks for special properties which may need it...

		/* actions and NLA - as a single unit for now, as it gets complicated to schedule otherwise */
		if ((adt->action) || (adt->nla_tracks.first)) {
			/* create the node */
			anim_builder.define_operation(DEPSOP_TYPE_EXEC,
			                              function_bind(BKE_animsys_eval_animdata, _1, id),
			                              DEG_OPCODE_ANIMATION, id->name);

			// TODO: for each channel affected, we might also want to add some support for running RNA update callbacks on them
			// (which will be needed for proper handling of drivers later)
		}

		/* drivers */
		for (FCurve *fcu = (FCurve *)adt->drivers.first; fcu; fcu = fcu->next) {
			/* create driver */
			deg_build_driver(context, id, fcu);
		}
	}
}

/**
 * Build graph node(s) for Driver
 * \param id: ID-Block that driver is attached to
 * \param fcu: Driver-FCurve
 */
void deg_build_driver(IDNodeBuilder &context, ID *id, FCurve *fcu)
{
	ComponentBuilder param_builder(context, DEPSNODE_TYPE_PARAMETERS);
	ChannelDriver *driver = fcu->driver;

	/* Create data node for this driver */
	/* TODO(sergey): Avoid creating same operation multiple times,
	 * in the future we need to avoid lookup of the operation as well
	 * and use some tagging magic instead.
	 */
	Operation driver_op = NULL;
	if (!param_builder.has_operation(DEG_OPCODE_DRIVER, deg_fcurve_id_name(fcu))) {
		driver_op = param_builder.define_operation(DEPSOP_TYPE_EXEC,
		                                           function_bind(BKE_animsys_eval_driver, _1, id, fcu),
		                                           DEG_OPCODE_DRIVER, deg_fcurve_id_name(fcu));
	}

	/* tag "scripted expression" drivers as needing Python (due to GIL issues, etc.) */
	if (driver->type == DRIVER_TYPE_PYTHON) {
		param_builder.set_operation_uses_python(driver_op);
	}
}

/* Recursively build graph for world */
void deg_build_world(DepsgraphBuilder &context, World *world)
{
	if (context.has_id(&world->id)) {
		return;
	}
	
	IDNodeBuilder builder(context, &world->id);

	/* world shading/params? */

	deg_build_animdata(builder, &world->id);

	/* TODO: other settings? */

	/* textures */
	deg_build_texture_stack(context, &world->id, world->mtex);

	/* world's nodetree */
	if (world->nodetree) {
		deg_build_nodetree(context, &world->id, world->nodetree);
	}
}

/* Rigidbody Simulation - Scene Level */
void deg_build_rigidbody(IDNodeBuilder &context, Scene *scene)
{
	RigidBodyWorld *rbw = scene->rigidbody_world;
	ComponentBuilder tfm_builder(context, DEPSNODE_TYPE_TRANSFORM);

	/**
	 * Rigidbody Simulation Nodes
	 * ==========================
	 *
	 * There are 3 nodes related to Rigidbody Simulation:
	 * 1) "Initialize/Rebuild World" - this is called sparingly, only when the simulation
	 *    needs to be rebuilt (mainly after file reload, or moving back to start frame)
	 * 2) "Do Simulation" - perform a simulation step - interleaved between the evaluation
	 *    steps for clusters of objects (i.e. between those affected and/or not affected by
	 *    the sim for instance)
	 *
	 * 3) "Pull Results" - grab the specific transforms applied for a specific object -
	 *    performed as part of object's transform-stack building
	 */

	/* create nodes ------------------------------------------------------------------------ */
	/* XXX: is this the right component, or do we want to use another one instead? */

	/* init/rebuild operation */
	/*Operation init_op =*/ tfm_builder.define_operation(DEPSOP_TYPE_REBUILD,
	                                                     function_bind(BKE_rigidbody_rebuild_sim, _1, scene),
	                                                     DEG_OPCODE_RIGIDBODY_REBUILD);

	/* do-sim operation */
	// XXX: what happens if we need to split into several groups?
	Operation sim_op = tfm_builder.define_operation(DEPSOP_TYPE_SIM,
	                                                function_bind(BKE_rigidbody_eval_simulation, _1, scene),
	                                                DEG_OPCODE_RIGIDBODY_SIM);

	/* XXX: For now, the sim node is the only one that really matters here. If any other
	 * sims get added later, we may have to remove these hacks...
	 */
	tfm_builder.set_entry_operation(sim_op);
	tfm_builder.set_exit_operation(sim_op);


	/* objects - simulation participants */
	if (rbw->group) {
		for (GroupObject *go = (GroupObject *)rbw->group->gobject.first; go; go = go->next) {
			Object *ob = go->ob;

			if (!ob || (ob->type != OB_MESH))
				continue;

			/* 2) create operation for flushing results */
			/* object's transform component - where the rigidbody operation lives */
			ComponentBuilder ob_tfm_builder(IDNodeBuilder(context.graph(), &ob->id), DEPSNODE_TYPE_TRANSFORM);
			
			ob_tfm_builder.define_operation(DEPSOP_TYPE_EXEC,
			                                function_bind(BKE_rigidbody_object_sync_transforms, _1, scene, ob),
			                                DEG_OPCODE_TRANSFORM_RIGIDBODY);
		}
	}
}

void deg_build_particles(IDNodeBuilder &context, Scene *scene, Object *ob)
{
	/**
	 * Particle Systems Nodes
	 * ======================
	 *
	 * There are two types of nodes associated with representing
	 * particle systems:
	 *  1) Component (EVAL_PARTICLES) - This is the particle-system
	 *     evaluation context for an object. It acts as the container
	 *     for all the nodes associated with a particular set of particle
	 *     systems.
	 *  2) Particle System Eval Operation - This operation node acts as a
	 *     blackbox evaluation step for one particle system referenced by
	 *     the particle systems stack. All dependencies link to this operation.
	 */

	/* component for all particle systems */
	ComponentBuilder psys_builder(context, DEPSNODE_TYPE_EVAL_PARTICLES);

	/* particle systems */
	for (ParticleSystem *psys = (ParticleSystem *)ob->particlesystem.first; psys; psys = psys->next) {
		/* this particle system */
		// TODO: for now, this will just be a placeholder "ubereval" node
		psys_builder.define_operation(DEPSOP_TYPE_EXEC,
		                              function_bind(BKE_particle_system_eval, _1, scene, ob, psys),
		                              DEG_OPCODE_PSYS_EVAL, psys->name);
	}

	/* pointcache */
	// TODO...
}

/* IK Solver Eval Steps */
void deg_build_ik_pose(ComponentBuilder &pose_builder, Scene *scene, Object *ob,
                       bPoseChannel *pchan, bConstraint *con)
{
	bKinematicConstraint *data = (bKinematicConstraint *)con->data;

	/* Find the chain's root. */
	bPoseChannel *rootchan = BKE_armature_ik_solver_find_root(pchan, data);

	if (!pose_builder.has_operation(DEG_OPCODE_POSE_IK_SOLVER)) {
		/* Operation node for evaluating/running IK Solver. */
		pose_builder.define_operation(DEPSOP_TYPE_SIM,
		                              function_bind(BKE_pose_iktree_evaluate, _1, scene, ob, rootchan),
		                              DEG_OPCODE_POSE_IK_SOLVER);
	}
}

/* Spline IK Eval Steps */
void deg_build_splineik_pose(ComponentBuilder &pose_builder, Scene *scene, Object *ob,
                             bPoseChannel *pchan, bConstraint *con)
{
	bSplineIKConstraint *data = (bSplineIKConstraint *)con->data;

	/* Find the chain's root. */
	bPoseChannel *rootchan = BKE_armature_splineik_solver_find_root(pchan, data);

	/* Operation node for evaluating/running Spline IK Solver.
	 * Store the "root bone" of this chain in the solver, so it knows where to start.
	 */
	pose_builder.define_operation(DEPSOP_TYPE_SIM,
	                              function_bind(BKE_pose_splineik_evaluate, _1, scene, ob, rootchan),
	                              DEG_OPCODE_POSE_SPLINE_IK_SOLVER);
}

/* Pose/Armature Bones Graph */
void deg_build_rig(IDNodeBuilder &context, Scene *scene, Object *ob)
{
	bArmature *arm = (bArmature *)ob->data;
	IDNodeBuilder arm_builder(context, &arm->id);

	/* animation and/or drivers linking posebones to base-armature used to define them
	 * NOTE: AnimData here is really used to control animated deform properties,
	 *       which ideally should be able to be unique across different instances.
	 *       Eventually, we need some type of proxy/isolation mechanism in-between here
	 *       to ensure that we can use same rig multiple times in same scene...
	 */
	deg_build_animdata(arm_builder, &arm->id);

	/* Rebuild pose if not up to date. */
	if (ob->pose == NULL || (ob->pose->flag & POSE_RECALC)) {
		BKE_pose_rebuild(ob, arm);
		/* XXX: Without this animation gets lost in certain circumstances
		 * after loading file. Need to investigate further since it does
		 * not happen with simple scenes..
		 */
		if (ob->adt) {
			ob->adt->recalc |= ADT_RECALC_ANIM;
		}
	}

	/* speed optimization for animation lookups */
	if (ob->pose) {
		BKE_pose_channels_hash_make(ob->pose);
		if (ob->pose->flag & POSE_CONSTRAINTS_NEED_UPDATE_FLAGS) {
			BKE_pose_update_constraint_flags(ob->pose);
		}
	}

	/* Make sure pose is up-to-date with armature updates. */
	ComponentBuilder arm_param_builder(arm_builder, DEPSNODE_TYPE_PARAMETERS);
	arm_param_builder.define_operation(DEPSOP_TYPE_EXEC,
	                                   NULL,
	                                   DEG_OPCODE_PLACEHOLDER, "Armature Eval");

	/**
	 * Pose Rig Graph
	 * ==============
	 *
	 * Pose Component:
	 * - Mainly used for referencing Bone components.
	 * - This is where the evaluation operations for init/exec/cleanup
	 *   (ik) solvers live, and are later hooked up (so that they can be
	 *   interleaved during runtime) with bone-operations they depend on/affect.
	 * - init_pose_eval() and cleanup_pose_eval() are absolute first and last
	 *   steps of pose eval process. ALL bone operations must be performed
	 *   between these two...
	 *
	 * Bone Component:
	 * - Used for representing each bone within the rig
	 * - Acts to encapsulate the evaluation operations (base matrix + parenting,
	 *   and constraint stack) so that they can be easily found.
	 * - Everything else which depends on bone-results hook up to the component only
	 *   so that we can redirect those to point at either the the post-IK/
	 *   post-constraint/post-matrix steps, as needed.
	 */

	ComponentBuilder pose_builder(context, DEPSNODE_TYPE_EVAL_POSE);

	/* pose eval context */
	pose_builder.define_operation(DEPSOP_TYPE_INIT,
	                              function_bind(BKE_pose_eval_init, _1, scene, ob, ob->pose),
	                              DEG_OPCODE_POSE_INIT);

	pose_builder.define_operation(DEPSOP_TYPE_POST,
	                              function_bind(BKE_pose_eval_flush, _1, scene, ob, ob->pose),
	                              DEG_OPCODE_POSE_DONE);

	/* bones */
	for (bPoseChannel *pchan = (bPoseChannel *)ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		ComponentBuilder bone_builder(context, DEPSNODE_TYPE_BONE, pchan->name);
		
		/* node for bone eval */
		bone_builder.define_operation(DEPSOP_TYPE_INIT,
		                              NULL, // XXX: BKE_pose_eval_bone_local
		                              DEG_OPCODE_BONE_LOCAL);

		bone_builder.define_operation(DEPSOP_TYPE_EXEC,
		                              function_bind(BKE_pose_eval_bone, _1, scene, ob, pchan), // XXX: BKE_pose_eval_bone_pose
		                              DEG_OPCODE_BONE_POSE_PARENT);

		bone_builder.define_operation(DEPSOP_TYPE_OUT,
		                              NULL, /* NOTE: dedicated noop for easier relationship construction */
		                              DEG_OPCODE_BONE_READY);

		bone_builder.define_operation(DEPSOP_TYPE_POST,
		                              function_bind(BKE_pose_bone_done, _1, pchan),
		                              DEG_OPCODE_BONE_DONE);

		/* constraints */
		if (pchan->constraints.first != NULL) {
			deg_build_pose_constraints(context, ob, pchan);
		}

		/**
		 * IK Solvers...
		 *
		 * - These require separate processing steps are pose-level
		 *   to be executed between chains of bones (i.e. once the
		 *   base transforms of a bunch of bones is done)
		 *
		 * Unsolved Issues:
		 * - Care is needed to ensure that multi-headed trees work out the same as in ik-tree building
		 * - Animated chain-lengths are a problem...
		 */
		for (bConstraint *con = (bConstraint *)pchan->constraints.first; con; con = con->next) {
			switch (con->type) {
				case CONSTRAINT_TYPE_KINEMATIC:
					deg_build_ik_pose(pose_builder, scene, ob, pchan, con);
					break;

				case CONSTRAINT_TYPE_SPLINEIK:
					deg_build_splineik_pose(pose_builder, scene, ob, pchan, con);
					break;

				default:
					break;
			}
		}
	}
}

void deg_build_proxy_rig(IDNodeBuilder &context, Object *ob)
{
	bArmature *arm = (bArmature *)ob->data;
	IDNodeBuilder arm_builder(context, &arm->id);
	
	deg_build_animdata(arm_builder, &arm->id);

	BLI_assert(ob->pose != NULL);

	/* speed optimization for animation lookups */
	BKE_pose_channels_hash_make(ob->pose);
	if (ob->pose->flag & POSE_CONSTRAINTS_NEED_UPDATE_FLAGS) {
		BKE_pose_update_constraint_flags(ob->pose);
	}

	ComponentBuilder pose_builder(context, DEPSNODE_TYPE_EVAL_POSE);

	/* TODO(sergey): This is an inverted relation, matches old depsgraph
	 * behavior and need to be investigated if it still need to be inverted.
	 */
	pose_builder.add_id_dependency(DEPSREL_TYPE_TRANSFORM, "Proxy",
	                               &ob->proxy_from->id, DEPSNODE_TYPE_EVAL_POSE);

	pose_builder.define_operation(DEPSOP_TYPE_INIT,
	                              function_bind(BKE_pose_eval_proxy_copy, _1, ob),
	                              DEG_OPCODE_POSE_INIT);

	for (bPoseChannel *pchan = (bPoseChannel *)ob->pose->chanbase.first;
	     pchan != NULL;
	     pchan = pchan->next)
	{
		ComponentBuilder bone_builder(context, DEPSNODE_TYPE_BONE, pchan->name);
		
		bone_builder.define_operation(DEPSOP_TYPE_INIT,
		                              NULL,
		                              DEG_OPCODE_BONE_LOCAL);

		bone_builder.define_operation(DEPSOP_TYPE_EXEC,
		                              NULL,
		                              DEG_OPCODE_BONE_READY);

		bone_builder.define_operation(DEPSOP_TYPE_POST,
		                              NULL,
		                              DEG_OPCODE_BONE_DONE);
	}

	pose_builder.define_operation(DEPSOP_TYPE_POST,
	                              NULL,
	                              DEG_OPCODE_POSE_DONE);
}

/* Shapekeys */
void deg_build_shapekeys(DepsgraphBuilder &context, Key *key)
{
	IDNodeBuilder builder(context, &key->id);
	ComponentBuilder geom_builder(builder, DEPSNODE_TYPE_GEOMETRY);
	
	deg_build_animdata(builder, &key->id);

	geom_builder.define_operation(DEPSOP_TYPE_EXEC,
	                              NULL,
	                              DEG_OPCODE_PLACEHOLDER, "Shapekey Eval");
}

void deg_build_obdata(DepsgraphBuilder &context, Scene *scene, Object *ob)
{
	ID *obdata = (ID *)ob->data;
	if (context.has_id(obdata)) {
		return;
	}
	
	IDNodeBuilder builder(context, obdata);
	ComponentBuilder geom_builder(builder, DEPSNODE_TYPE_GEOMETRY);
	ComponentBuilder param_builder(builder, DEPSNODE_TYPE_PARAMETERS);

	deg_build_animdata(builder, obdata);

	/* nodes for result of obdata's evaluation, and geometry evaluation on object */
	switch (ob->type) {
		case OB_MESH:
		{
			//Mesh *me = (Mesh *)ob->data;

			/* evaluation operations */
			geom_builder.define_operation(DEPSOP_TYPE_INIT,
			                              function_bind(BKE_mesh_eval_geometry, _1, (Mesh *)obdata),
			                              DEG_OPCODE_PLACEHOLDER, "Geometry Eval");
			break;
		}

		case OB_MBALL:
		{
			Object *mom = BKE_mball_basis_find(scene, ob);

			/* motherball - mom depends on children! */
			if (mom == ob) {
				/* metaball evaluation operations */
				/* NOTE: only the motherball gets evaluated! */
				geom_builder.define_operation(DEPSOP_TYPE_INIT,
				                              function_bind(BKE_mball_eval_geometry, _1, (MetaBall *)obdata),
				                              DEG_OPCODE_PLACEHOLDER, "Geometry Eval");
			}
			break;
		}

		case OB_CURVE:
		case OB_FONT:
		{
			/* curve evaluation operations */
			/* - calculate curve geometry (including path) */
			geom_builder.define_operation(DEPSOP_TYPE_INIT,
			                              function_bind(BKE_curve_eval_geometry, _1, (Curve *)obdata),
			                              DEG_OPCODE_PLACEHOLDER, "Geometry Eval");

			/* - calculate curve path - this is used by constraints, etc. */
			geom_builder.define_operation(DEPSOP_TYPE_EXEC,
			                              function_bind(BKE_curve_eval_path, _1, (Curve *)obdata),
			                              DEG_OPCODE_GEOMETRY_PATH, "Path");
			break;
		}

		case OB_SURF: /* Nurbs Surface */
		{
			/* nurbs evaluation operations */
			geom_builder.define_operation(DEPSOP_TYPE_INIT,
			                              function_bind(BKE_curve_eval_geometry, _1, (Curve *)obdata),
			                              DEG_OPCODE_PLACEHOLDER, "Geometry Eval");
			break;
		}

		case OB_LATTICE: /* Lattice */
		{
			/* lattice evaluation operations */
			geom_builder.define_operation(DEPSOP_TYPE_INIT,
			                              function_bind(BKE_lattice_eval_geometry, _1, (Lattice *)obdata),
			                              DEG_OPCODE_PLACEHOLDER, "Geometry Eval");
			break;
		}
	}

	geom_builder.define_operation(DEPSOP_TYPE_POST,
	                              NULL,
	                              DEG_OPCODE_PLACEHOLDER, "Eval Done");

	/* Parameters for driver sources. */
	param_builder.define_operation(DEPSOP_TYPE_EXEC,
	                               NULL,
	                               DEG_OPCODE_PLACEHOLDER, "Parameters Eval");
}

/* ObData Geometry Evaluation */
// XXX: what happens if the datablock is shared!
void deg_build_object_geom(IDNodeBuilder &context, Scene *scene, Object *ob)
{
	ComponentBuilder builder(context, DEPSNODE_TYPE_GEOMETRY);

	/* Temporary uber-update node, which does everything.
	 * It is for the being we're porting old dependencies into the new system.
	 * We'll get rid of this node as soon as all the granular update functions
	 * are filled in.
	 *
	 * TODO(sergey): Get rid of this node.
	 */
	builder.define_operation(DEPSOP_TYPE_POST,
	                         function_bind(BKE_object_eval_uber_data, _1, scene, ob),
	                         DEG_OPCODE_GEOMETRY_UBEREVAL);

	builder.define_operation(DEPSOP_TYPE_INIT,
	                         NULL,
	                         DEG_OPCODE_PLACEHOLDER, "Eval Init");

	// TODO: "Done" operation

	/* Modifiers */
	if (ob->modifiers.first) {
		ModifierData *md;

		for (md = (ModifierData *)ob->modifiers.first; md; md = md->next) {
			builder.define_operation(DEPSOP_TYPE_EXEC,
			                         function_bind(BKE_object_eval_modifier, _1, scene, ob, md),
			                         DEG_OPCODE_GEOMETRY_MODIFIER, md->name);
		}
	}

	/* geometry collision */
	if (ELEM(ob->type, OB_MESH, OB_CURVE, OB_LATTICE)) {
		// add geometry collider relations
	}
	
	/* TODO(sergey): Only for until we support granular
	 * update of curves.
	 */
	if (ob->type == OB_FONT) {
		Curve *curve = (Curve *)ob->data;
		if (curve->textoncurve) {
			context.set_need_curve_path();
		}
	}
}

/* Cameras */
void deg_build_camera(DepsgraphBuilder &context, Object *ob)
{
	/* TODO: Link scene-camera links in somehow... */
	Camera *cam = (Camera *)ob->data;
	if (context.has_id(&cam->id)) {
		return;
	}
	
	IDNodeBuilder cam_builder(context, &cam->id);

	deg_build_animdata(cam_builder, &cam->id);

	ComponentBuilder param_builder(cam_builder, DEPSNODE_TYPE_PARAMETERS);
	param_builder.define_operation(DEPSOP_TYPE_EXEC,
	                               NULL,
	                               DEG_OPCODE_PLACEHOLDER, "Parameters Eval");

	if (cam->dof_ob != NULL) {
		/* TODO(sergey): For now parametrs are on object level. */
		IDNodeBuilder ob_builder(context, &ob->id);
		ComponentBuilder ob_param_builder(ob_builder, DEPSNODE_TYPE_PARAMETERS);
		
		ob_param_builder.define_operation(DEPSOP_TYPE_EXEC,
		                                  NULL,
		                                  DEG_OPCODE_PLACEHOLDER, "Camera DOF");
	}
}

/* Lamps */
void deg_build_lamp(DepsgraphBuilder &context, Object *ob)
{
	Lamp *la = (Lamp *)ob->data;
	if (context.has_id(&la->id)) {
		return;
	}
	
	IDNodeBuilder builder(context, &la->id);
	ComponentBuilder param_builder(builder, DEPSNODE_TYPE_PARAMETERS);

	deg_build_animdata(builder, &la->id);

	/* TODO(sergey): Is it really how we're supposed to work with drivers? */
	param_builder.define_operation(DEPSOP_TYPE_EXEC,
	                               NULL,
	                               DEG_OPCODE_PLACEHOLDER, "Parameters Eval");

	/* lamp's nodetree */
	if (la->nodetree) {
		deg_build_nodetree(context, &ob->id, la->nodetree);
	}

	/* textures */
	deg_build_texture_stack(context, &ob->id, la->mtex);
}

void deg_build_nodetree(DepsgraphBuilder &context, ID *owner, bNodeTree *ntree)
{
	if (!ntree)
		return;

	IDNodeBuilder builder(context, &ntree->id);
	ComponentBuilder param_builder(builder, DEPSNODE_TYPE_PARAMETERS);

	deg_build_animdata(builder, &ntree->id);

	/* Parameters for drivers. */
	param_builder.define_operation(DEPSOP_TYPE_POST,
	                               NULL,
	                               DEG_OPCODE_PLACEHOLDER, "Parameters Eval");

	/* nodetree's nodes... */
	for (bNode *bnode = (bNode *)ntree->nodes.first; bnode; bnode = bnode->next) {
		if (bnode->id) {
			if (GS(bnode->id->name) == ID_MA) {
				deg_build_material(context, owner, (Material *)bnode->id);
			}
			else if (bnode->type == ID_TE) {
				deg_build_texture(context, owner, (Tex *)bnode->id);
			}
			else if (bnode->type == NODE_GROUP) {
				bNodeTree *group_ntree = (bNodeTree *)bnode->id;
				if ((group_ntree->id.tag & LIB_TAG_DOIT) == 0) {
					deg_build_nodetree(context, owner, group_ntree);
				}
			}
		}
	}

	// TODO: link from nodetree to owner_component?
}

/* Recursively build graph for material */
void deg_build_material(DepsgraphBuilder &context, ID *owner, Material *ma)
{
	if (context.has_id(&ma->id)) {
		return;
	}
	
	IDNodeBuilder builder(context, &ma->id);
	ComponentBuilder shading_builder(builder, DEPSNODE_TYPE_SHADING);

	shading_builder.define_operation(DEPSOP_TYPE_EXEC,
	                                 NULL,
	                                 DEG_OPCODE_PLACEHOLDER, "Material Update");

	/* material animation */
	deg_build_animdata(builder, &ma->id);

	/* textures */
	deg_build_texture_stack(context, owner, ma->mtex);

	/* material's nodetree */
	deg_build_nodetree(context, owner, ma->nodetree);
}

/* Texture-stack attached to some shading datablock */
void deg_build_texture_stack(DepsgraphBuilder &context, ID *owner, MTex **texture_stack)
{
	int i;

	/* for now assume that all texture-stacks have same number of max items */
	for (i = 0; i < MAX_MTEX; i++) {
		MTex *mtex = texture_stack[i];
		if (mtex && mtex->tex)
			deg_build_texture(context, owner, mtex->tex);
	}
}

/* Recursively build graph for texture */
void deg_build_texture(DepsgraphBuilder &context, ID *owner, Tex *tex)
{
	if (context.has_id(&tex->id)) {
		return;
	}
	
	IDNodeBuilder builder(context, &tex->id);
	
	/* texture itself */
	deg_build_animdata(builder, &tex->id);
	
	/* texture's nodetree */
	deg_build_nodetree(context, owner, tex->nodetree);
}

void deg_build_compositor(DepsgraphBuilder &context, Scene *scene)
{
	/* For now, just a plain wrapper? */
	// TODO: create compositing component?
	// XXX: component type undefined!
	//graph->get_node(&scene->id, NULL, DEPSNODE_TYPE_COMPOSITING, NULL);

	/* for now, nodetrees are just parameters; compositing occurs in internals of renderer... */
	IDNodeBuilder builder(context, &scene->id);
	ComponentBuilder param_builder(builder, DEPSNODE_TYPE_PARAMETERS);
	UNUSED_VARS(param_builder);
	
	deg_build_nodetree(context, &scene->id, scene->nodetree);
}

void deg_build_gpencil(DepsgraphBuilder &context, bGPdata *gpd)
{
	if (context.has_id(&gpd->id)) {
		return;
	}
	
	IDNodeBuilder builder(context, &gpd->id);

	/* The main reason Grease Pencil is included here is because the animation (and drivers)
	 * need to be hosted somewhere...
	 */
	deg_build_animdata(builder, &gpd->id);
}

}  // namespace DEG

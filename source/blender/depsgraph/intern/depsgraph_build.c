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
 *
 * Methods for constructing depsgraph
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
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
#include "BKE_fcurve.h"
#include "BKE_group.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_rigidbody.h"
#include "BKE_sound.h"
#include "BKE_texture.h"
#include "BKE_tracking.h"
#include "BKE_world.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "depsgraph_types.h"
#include "depsgraph_eval.h"
#include "despgraph_intern.h"


/* ************************************************* */
/* AnimData */

/* Build graph node(s) for Driver
 * < id: ID-Block that driver is attached to
 * < fcu: Driver-FCurve
 */
static DepsNode *deg_build_driver_rel(Depsgraph *graph, ID *id, FCurve *fcu)
{
	ChannelDriver *driver = fcu->driver;
	DriverVar *dvar;
	char name_buf[DEG_MAX_ID_NAME];
	
	OperationDepsNode *driver_node = NULL;
	DepsNode *affected_node = NULL;
	
	
	/* create data node for this driver ..................................... */
	BLI_snprintf(name_buf, DEG_MAX_ID_NAME, "Driver @ %p", driver);
	
	driver_node = DEG_add_operation(graph, id, NULL, DEPSNODE_TYPE_OP_DRIVER,
	                                DEPSOP_TYPE_EXEC, BKE_animsys_eval_driver,
	                                driver_name_buf);
	
	/* RNA pointer to driver, to provide as context for execution */
	RNA_pointer_create(id, &RNA_FCurve, fcu, &driver_node->ptr);
	
	/* tag "scripted expression" drivers as needing Python (due to GIL issues, etc.) */
	if (driver->type == DRIVER_TYPE_PYTHON) {
		driver_node->flag |= DEPSOP_FLAG_USES_PYTHON;
	}
	
	/* create dependency between driver and data affected by it */
	// XXX: this should return a parameter context for dealing with this...
	affected_node = DEG_get_node_from_rna_path(graph, id, fcu->rna_path);
	if (affected_node) {
		/* make data dependent on driver */
		DEG_add_new_relation(graph, driver_node, affected_node, DEPSREL_TYPE_DRIVER, 
		                     "[Driver -> Data] DepsRel");
		
		/* ensure that affected prop's update callbacks will be triggered once done */
		// TODO: implement this once the functionality to add these links exists in RNA
		// XXX: the data itself could also set this, if it were to be truly initialised later?
	}
	
	/* loop over variables to get the target relationships */
	for (dvar = driver->variables.first; dvar; dvar = dvar->next) {
		/* only used targets */
		DRIVER_TARGETS_USED_LOOPER(dvar) 
		{
			if (dtar->id) {
				DepsNode *target_node = NULL;
				
				/* special handling for directly-named bones... */
				if ((dtar->flag & DTAR_FLAG_STRUCT_REF) && (dtar->pchan_name[0])) {
					Object *ob = (Object *)dtar->ob;
					bPoseChannel *pchan = BKE_pose_channel_find_name(ob->pose, dtar->pchan_name);
					
					/* get node associated with bone */
					target_node = DEG_get_node(graph, dtar->id, pchan->name, DEPSNODE_TYPE_BONE, NULL);
				}
				else {
					/* resolve path to get node... */
					target_node = DEG_get_node_from_rna_path(graph, dtar->id, dtar->rna_path);
				}
				
				/* make driver dependent on this node */
				DEG_add_new_relation(graph, target_node, driver_node, DEPSREL_TYPE_DRIVER_TARGET,
				                     "[Target -> Driver] DepsRel");
			}
		}
		DRIVER_TARGETS_LOOPER_END
	}
	
	/* return driver node created */
	return driver_node;
}

/* Build graph nodes for AnimData block 
 * < scene_node: Scene that ID-block this lives on belongs to
 * < id: ID-Block which hosts the AnimData
 */
static void deg_build_animdata_graph(Depsgraph *graph, Scene *scene, ID *id)
{
	AnimData *adt = BKE_animdata_from_id(id);
	DepsNode *adt_node = NULL;
	FCurve *fcu;
	
	if (adt == NULL)
		return;
	
	/* animation */
	if (adt->action || adt->nla_tracks.first) {
		DepsNode *time_src;
		
		/* create "animation" data node for this block */
		adt_node = DEG_get_node(graph, id, NULL, DEPSNODE_ANIMATION, "Animation");
		
		/* wire up dependency to time source */
		// NOTE: this assumes that timesource was already added as one of first steps!
		time_src = DEG_find_node(graph, NULL, NULL, DEPSNODE_TYPE_TIMESOURCE, NULL);
		DEG_add_new_relation(graph, time_src, adt_node, DEPSREL_TYPE_TIME, 
		                     "[TimeSrc -> Animation] DepsRel");
		                     
		// XXX: Hook up specific update callbacks for special properties which may need it...
	}
	
	/* drivers */
	for (fcu = adt->drivers.first; fcu; fcu = fcu->next) {
		/* create driver */
		DepsNode *driver_node = deg_build_driver_rel(graph, id, fcu);
		
		/* hook up update callback associated with F-Curve */
		// ...
		
		/* prevent driver from occurring before own animation... */
		// NOTE: probably not strictly needed (anim before parameters anyway)...
		if (adt_node) {
			DEG_add_new_relation(graph, adt_node, driver_node, DEPSREL_TYPE_OPERATION, 
			                     "[AnimData Before Drivers] DepsRel");
		}
	}
}


/* ************************************************* */
/* Rigs */

/* Constraints - Objects or Bones 
 * < container: (ComponentDepsNode) component that constraint nodes will operate within
 *               Typically this will either be Transform or Bone components.
 */
static void deg_build_constraints_graph(Depsgraph *graph, Scene *scene, 
                                        Object *ob, bPoseChannel *pchan,
                                        ListBase *constraints, 
                                        DepsNode *container)
{
	OperationDepsNode *constraintStackNode;
	eDepsNode_Type stackNodeType;
	char *subdata_name;
	
	bConstraint *con;
	
	
	/* == Constraints Graph Notes ==
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
	
	/* create node for constraint stack */
	if (pchan) {
		stackNodeType = DEPSNODE_TYPE_OP_BONE;
		subdata_name = pchan->name;
	}
	else {
		stackNodeType = DEPSNODE_TYPE_OP_TRANSFORM;
		subdata_name = NULL;
	}
	
	constraintStackNode = DEG_add_operation(graph, &ob->id, subdata_name, stackNodeType,
	                                       DEPSOP_TYPE_EXEC, BKE_constraints_evaluate,
	                                        "Constraint Stack");
	
	/* add dependencies for each constraint in turn */
	for (con = constraints->first; con; con = con->next) {
		bConstraintTypeInfo *cti = BKE_constraint_get_typeinfo(con);
		ListBase targets = {NULL, NULL};
		bConstraintTarget *ct;
		
		/* invalid constraint type... */
		if (cti == NULL)
			continue;
		
		/* special case for camera tracking -- it doesn't use targets to define relations */
		// TODO: we can now represent dependencies in a much richer manner, so review how this is done...
		if (ELEM3(cti->type, CONSTRAINT_TYPE_FOLLOWTRACK, CONSTRAINT_TYPE_CAMERASOLVER, CONSTRAINT_TYPE_OBJECTSOLVER)) {
			DepsNode *node2, *scene_node;
			bool depends_on_camera = false;
			
			if (cti->type == CONSTRAINT_TYPE_FOLLOWTRACK) {
				bFollowTrackConstraint *data = (bFollowTrackConstraint *)con->data;

				if (((data->clip) || (data->flag & FOLLOWTRACK_ACTIVECLIP)) && data->track[0])
					depends_on_camera = true;
				
				if (data->depth_ob) {
					// DAG_RL_DATA_OB | DAG_RL_OB_OB
					node2 = DEG_get_node(graph, (ID *)data->depth_ob, NULL, DEPSNODE_TYPE_TRANSFORM, NULL);
					DEG_add_new_relation(node2, constraintStackNode, DEPSREL_TYPE_TRANSFORM, cti->name);
				}
			}
			else if (cti->type == CONSTRAINT_TYPE_OBJECTSOLVER) {
				depends_on_camera = true;
			}

			if (depends_on_camera && scene->camera) {
				// DAG_RL_DATA_OB | DAG_RL_OB_OB
				node2 = DEG_get_node(graph, (ID *)scene->camera, NULL, DEPSNODE_TYPE_TRANSFORM, NULL);
				DEG_add_new_relation(node2, constraintStackNode, DEPSREL_TYPE_TRANSFORM, cti->name);
			}
			
			/* tracker <-> constraints */
			// FIXME: actually motionclip dependency on results of motionclip block here...
			//dag_add_relation(dag, scenenode, node, DAG_RL_SCENE, "Scene Relation");
		}
		else if (cti->get_constraint_targets) {
			cti->get_constraint_targets(con, &targets);
			
			for (ct = targets.first; ct; ct = ct->next) {
				if (ct->tar) {
					DepsNode *node2;
					
					if (ELEM(con->type, CONSTRAINT_TYPE_KINEMATIC, CONSTRAINT_TYPE_SPLINEIK)) {
						/* ignore IK constraints - these are handled separately (on pose level) */
					}
					else if (ELEM(con->type, CONSTRAINT_TYPE_FOLLOWPATH, CONSTRAINT_TYPE_CLAMPTO)) {
						/* these constraints require path geometry data... */
						node2 = DEG_get_node(graph, (ID *)ct->tar, NULL, DEPSNODE_TYPE_GEOMETRY, "Path");
						DEG_add_new_relation(node2, constraintStackNode, DEPSREL_TYPE_GEOMETRY_EVAL, cti->name); // XXX: type = geom_transform
					}
					else if ((ct->tar->type == OB_ARMATURE) && (ct->subtarget[0])) {
						/* bone */
						node2 = DEG_get_node(graph, (ID *)ct->tar, ct->subtarget[0], DEPSNODE_TYPE_BONE, NULL);
						DEG_add_new_relation(node2, constraintStackNode, DEPSREL_TYPE_TRANSFORM, cti->name);
					}
					else if (ELEM(ct->tar->type, OB_MESH, OB_LATTICE) && (ct->subtarget[0])) {
						/* vertex group */
						/* NOTE: for now, we don't need to represent vertex groups separately... */
						node2 = DEG_get_node(graph, (ID *)ct->tar, NULL, DEPSNODE_TYPE_GEOMETRY, NULL);
						DEG_add_new_relation(node2, constraintStackNode, DEPSREL_TYPE_GEOMETRY_EVAL, cti->name);
						
						if (ct->tar->type == OB-MESH) {
							//node2->customdata_mask |= CD_MASK_MDEFORMVERT;
						}
					}
					else {
						/* standard object relation */
						// TODO: loc vs rot vs scale?
						node2 = DEG_get_node(graph, (ID *)ct->tar, NULL, DEPSNODE_TYPE_TRANSFORM, NULL);
						DEG_add_new_relation(node2, constraintStackNode, DEPSREL_TYPE_TRANSFORM, cti->name);
					}
				}
			}
			
			if (cti->flush_constraint_targets)
				cti->flush_constraint_targets(con, &targets, 1);
		}
	}
}

/* ------------------------------------------ */

/* IK Solver Eval Steps */
static void deg_build_ik_pose_graph(Depsgraph *graph, Scene *scene, 
                                    Object *ob, bPoseChannel *pchan,
                                    bConstraint *con)
{
	bKinematicConstraint *data = (bKinematicConstraint *)con->data;
	bPoseChannel *rootchan = pchan;
	bPoseChannel *parchan;
	size_t segcount = 0;
	
	OperationDepsNode *solver_op;
	DepsNode *owner_node;
	
	/* component for bone holding the constraint */
	owner_node = DEG_get_node(graph, &ob->id, pchan->name, DEPSNODE_TYPE_BONE, NULL);
	
	/* operation node for evaluating/running IK Solver */
	solver_op = DEG_add_operation(graph, &ob->id, NULL, DEPSNODE_TYPE_OP_POSE,
	                              DEPSOP_TYPE_SIM, BKE_pose_iktree_evaluate, 
	                              "IK Solver");
	
	/* attach owner to IK Solver too 
	 * - assume that owner is always part of chain 
	 * - see notes on direction of rel below...
	 */
	DEG_add_new_relation(owner_node, solver_op, DEPSREL_TYPE_TRANSFORM, "IK Solver Owner");
	
	
	/* exclude tip from chain? */
	if ((data->flag & CONSTRAINT_IK_TIP) == 0)
		parchan = pchan->parent;
	else
		parchan = pchan;
	
	/* Walk to the chain's root */
	while (parchan) {
		/* Make IK-solver dependent on this bone's result,
		 * since it can only run after the standard results 
		 * of the bone are know. Validate links step on the 
		 * bone will ensure that users of this bone only
		 * grab the result with IK solver results...
		 */
		DepsNode *parchan_node = DEG_get_node(graph, &ob->id, parchan->name, DEPSNODE_TYPE_BONE, NULL);
		DEG_add_new_relation(parchan_node, solver_op, DEPSREL_TYPE_TRANSFORM, "IK Solver Update");
		
		/* continue up chain, until we reach target number of items... */
		segcount++;
		if ((segcount == data->rootbone) || (segcount > 255)) break;  /* 255 is weak */
		
		rootchan = parchan;
		parchan  = parchan->parent;
	}
	
	/* store the "root bone" of this chain in the solver, so it knows where to start */
	RNA_pointer_create(&ob->id, &RNA_PoseBone, rootchan, &solver_op->ptr);
}

/* Spline IK Eval Steps */
static void deg_build_splineik_pose_graph(Depsgraph *graph, Scene *scene,
                                          Object *ob, bPoseChannel *pchan_owner,
                                          bConstraint *con)
{
	bSplineIKConstraint *data = (bSplineIKConstraint *)con->data;
	bPoseChannel *parchan, *rootchan = pchan;
	size_t segcount = 0;
	
	OperationDepsNode *solver_op;
	DepsNode *owner_node, *curve_node;
	
	/* component for bone holding the constraint */
	owner_node = DEG_get_node(graph, &ob->id, pchan->name, DEPSNODE_TYPE_BONE, NULL);
	
	/* component for spline-path geometry that this uses */
	// XXX: target may not exist!
	curve_node = DEG_get_node(graph, data->tar, NULL, DEPSNODE_TYPE_GEOMETRY, "Path");
	
	/* --------------- */
	
	/* operation node for evaluating/running IK Solver */
	solver_op = DEG_add_operation(graph, &ob->id, NULL, DEPSNODE_TYPE_OP_POSE,
	                              DEPSOP_TYPE_SIM, BKE_pose_splineik_evaluate, 
	                              "Spline IK Solver");
	// XXX: what sort of ID-data is needed?
	
	/* attach owner to IK Solver too 
	 * - assume that owner is always part of chain 
	 * - see notes on direction of rel below...
	 */
	DEG_add_new_relation(owner_node, solver_op, DEPSREL_TYPE_TRANSFORM, "Spline IK Solver Owner");
	
	/* attach path dependency to solver */
	DEG_add_new_relation(curve_node, solver_op, DEPSREL_TYPE_GEOMETRY_EVAL, "[Curve.Path -> Spline IK] DepsRel");
	
	/* --------------- */
	
	/* Walk to the chain's root */
	for (parchan = pchan->parent;
	     parchan;
	     rootchan = parchan,  parchan = parchan->parent)
	{
		/* Make Spline IK solver dependent on this bone's result,
		 * since it can only run after the standard results 
		 * of the bone are know. Validate links step on the 
		 * bone will ensure that users of this bone only
		 * grab the result with IK solver results...
		 */
		DepsNode *parchan_node = DEG_get_node(graph, &ob->id, parchan->name, DEPSNODE_TYPE_BONE, NULL);
		DEG_add_new_relation(parchan_node, solver_op, DEPSREL_TYPE_TRANSFORM, "Spline IK Solver Update");
		
		/* continue up chain, until we reach target number of items... */
		segcount++;
		if ((segcount == data->chainlen) || (segcount > 255)) break;  /* 255 is weak */
	}
	
	/* store the "root bone" of this chain in the solver, so it knows where to start */
	RNA_pointer_create(&ob->id, &RNA_PoseBone, rootchan, &solver_op->ptr);
}

/* ------------------------------------------ */

/* Pose/Armature Bones Graph */
static void deg_build_rig_graph(Depsgraph *graph, Scene *scene, Object *ob)
{
	bArmature *arm = (bArmature *)ob->data;
	bPoseChannel *pchan;
	
	PoseComponentDepsNode *pose_node;
	
	/* == Pose Rig Graph ==
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
	
	/* pose eval context 
	 * NOTE: init/cleanup steps for this are handled as part of the node's code
	 */
	pose_node = (PoseComponentDepsNode *)DEG_get_node(graph, &ob->id, NULL, DEPSNODE_TYPE_EVAL_POSE, NULL);
	
	/* bones */
	for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		BoneComponentDepsNode *bone_node;
		OperationDepsNode *bone_op;
		
		/* component for hosting bone operations */
		bone_node = (BoneComponentDepsNode *)DEG_get_node(graph, &ob->id, pchan->name, DEPSNODE_TYPE_BONE, NULL);
		bone_node->pchan = pchan;
		
		/* node for bone eval */
		bone_op = DEG_add_operation(graph, &ob->id, pchan->name, DEPSNODE_TYPE_OP_BONE, 
		                              DEPSOP_TYPE_EXEC, BKE_pose_eval_bone,
		                              "Bone Transforms");
		
		/* bone parent */
		if (pchan->parent) {
			DepsNode *par_bone = DEG_get_node(graph, id, pchan->parent->name, DEPSNODE_TYPE_BONE, NULL);
			DEG_add_new_relation(par_bone, bone_node, DEPSREL_TYPE_TRANSFORM, "[Parent Bone -> Child Bone]");
		}
		
		/* constraints */
		if (pchan->constraints.first) {
			deg_build_constraints_graph(graph, scene, ob, 
			                            pchan, &pchan->constraints, 
			                            bone_node);
		}
	}
	
	
	/* IK Solvers...
	 * - These require separate processing steps are pose-level
	 *   to be executed between chains of bones (i.e. once the
	 *   base transforms of a bunch of bones is done)
	 *
	 * Unsolved Issues:
	 * - Care is needed to ensure that multi-headed trees work out the same as in ik-tree building
	 * - Animated chain-lengths are a problem...
	 */
	for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		bConstraint *con;
		
		for (con = pchan->constraints.first; con; con = con->next) {
			switch (con->type) {
				case CONSTRAINT_TYPE_KINEMATIC:
					deg_build_ik_pose_graph(graph, scene, ob, pchan, con);
					break;
					
				case CONSTRAINT_TYPE_SPLINEIK:
					deg_build_splineik_pose_graph(graph, scene, ob, pchan, con);
					break;
					
				default:
					break;
			}
		}
	}
	
	
	/* Armature-Data */
	// TODO: bone names?
	// TODO: selection status?
	if (arm->adt) {
		/* animation and/or drivers linking posebones to base-armature used to define them 
		 * NOTE: AnimData here is really used to control animated deform properties, 
		 *       which ideally should be able to be unique across different instances.
		 *       Eventually, we need some type of proxy/isolation mechanism inbetween here
		 *       to ensure that we can use same rig multiple times in same scene...
		 */
		// TODO: we need a bit of an exception here to redirect drivers to posebones?
		deg_build_animdata_graph(graph, scene, &arm->id);
	}
}

/* ************************************************* */
/* Shading */
// XXX: how to prevent duplication-problems?

/* forward decl. */
static void deg_build_material_graph(Depsgraph *graph, Scene *scene, DepsNode *owner_component, Material *ma);
static void deg_build_texture_graph(Depsgraph *graph, Scene *scene, DepsNode *owner_component, Tex *tex);


/* Recursively build graph for node-tree */
static void deg_build_nodetree_graph(Depsgraph *graph, Scene *scene, DepsNode *owner_component, bNodeTree *ntree)
{
	bNode *n;
	
	/* nodetree itself */
	if (ntree->adt) {
		deg_build_animdata_graph(graph, scene, &ntree->id);
	}
	
	/* nodetree's nodes... */
	for (n = ntree->nodes.first; n; n = n->next) {
		if (n->id) {
			if (GS(n->id->name) == ID_MA) {
				deg_build_material_graph(graph, scene, owner_component, (Material *)n->id);
			}
			else if (n->type == ID_TEX) {
				deg_build_texture_graph(graph, scene, owner_component, (Texture *)n->id);
			}
			else if (n->type == NODE_GROUP) {
				deg_build_nodetree_graph(graph, scene, owner_component, (bNodeTree *)n->id);
			}
		}
	}
	
	// TODO: link from nodetree to owner_component?
}

/* Recursively build graph for texture */
static void deg_build_texture_graph(Depsgraph *graph, Scene *scene, DepsNode *owner_component, Tex *tex)
{
	/* Prevent infinite recursion by checking (and tagging the texture) as having been visited 
	 * already. This assumes tex->id.flag & LIB_DOIT isn't set by anything else
	 * in the meantime... [#32017]
	 */
	if (tex->id.flag & LIB_DOIT)
		return;
	
	tex->id.flag |= LIB_DOIT;
	
	/* texture itself */
	if (tex->adt) {
		deg_build_animdata_graph(graph, scene, &tex->id);
	}
	
	/* texture's nodetree */
	if (tex->nodetree) {
		deg_build_nodetree_graph(graph, scene, owner_component, tex->nodetree);
	}

	tex->id.flag &= ~LIB_DOIT;
}

/* Texture-stack attached to some shading datablock */
static void deg_build_texture_stack_graph(Depsgraph *graph, Scene *scene, DepsNode *owner_component, MTex **texture_stack)
{
	int i;
	
	/* for now assume that all texture-stacks have same number of max items */
	for (i = 0; i < MAX_MTEX; i++) {
		MTex *mtex = texture_stack[i];
		deg_build_texture_graph(graph, scene, owner_component, mtex->tex);
	}
}

/* Recursively build graph for material */
static void deg_build_material_graph(Depsgraph *graph, Scene *scene, DepsNode *owner_component, Material *ma)
{
	/* Prevent infinite recursion by checking (and tagging the material) as having been visited 
	 * already. This assumes ma->id.flag & LIB_DOIT isn't set by anything else
	 * in the meantime... [#32017]
	 */
	if (ma->id.flag & LIB_DOIT)
		return;
	
	ma->id.flag |= LIB_DOIT;
	
	/* material itself */
	if (ma->adt) {
		deg_build_animdata_graph(graph, scene, &ma->id);
	}
	
	/* textures */
	deg_build_texture_stack_graph(graph, scene, owner_component, ma->mtex);
	
	/* material's nodetree */
	if (ma->nodetree) {
		deg_build_nodetree_graph(graph, scene, owner_component, ma->nodetree);
	}

	ma->id.flag &= ~LIB_DOIT;
}

/* Recursively build graph for world */
static void deg_build_world_graph(Depsgraph *graph, Scene *scene, World *wo)
{
	/* Prevent infinite recursion by checking (and tagging the world) as having been visited 
	 * already. This assumes wo->id.flag & LIB_DOIT isn't set by anything else
	 * in the meantime... [#32017]
	 */
	if (wo->id.flag & LIB_DOIT)
		return;
	
	wo->id.flag |= LIB_DOIT;
	
	/* world itself */
	if (wo->adt) {
		deg_build_animdata_graph(graph, scene, &wo->id);
	}
	
	/* TODO: other settings? */
	
	/* textures */
	deg_build_texture_stack_graph(graph, scene, NULL /* world shading/params? */, wo->mtex);
	
	/* world's nodetree */
	if (wo->nodetree) {
		deg_build_nodetree_graph(graph, scene, owner_component, wo->nodetree);
	}

	wo->id.flag &= ~LIB_DOIT;
}

/* Compositing-related nodes */
static void deg_build_compo_graph(Depsgraph *graph, Scene *scene)
{
	/* For now, just a plain wrapper? */
	if (scene->nodetree) {
		// TODO: create compositing component?
		// XXX: component type undefined!
		//deg_build_nodetree_graph(graph, scene, DEPSNODE_TYPE_COMPOSITING, scene->nodetree);
		
		/* for now, nodetrees are just parameters; compositing occurs in internals of renderer... */
		deg_build_nodetree_graph(graph, scene, DEPSNODE_TYPE_PARAMETERS, scene->nodetree);
	}
}

/* ************************************************* */
/* Physics */

/* Physics Systems */
static void deg_build_particles_graph(Depsgraph *graph, Scene *scene, Object *ob)
{
	ParticleSystem *psys;
	DepsNode *psys_comp;
	
	/* STRUCTURE:
	 * 1) ID.EVAL_PARTICLES Component (ob)
	 *   2) ParticleSystem Eval Operation / ParticleSettings Component (part)
	 *   -  For now, just do a single blackbox operation per particle system...
	 *     3) ...
	 */
	
	/* component for all particle systems */
	psys_comp = DEG_add_new_node(graph, &ob->id, NULL, DEPSNODE_TYPE_EVAL_PARTICLES, NULL);
	
	/* particle systems */
	for (psys = ob->particlesystem.first; psys; psys = psys->next) {
		ParticleSettings *part = psys->part;
		DepsNode *psys_op;
		
		/* this particle system */
		psys_op = DEG_add_operation(graph, &ob->id, part->name, DEPSNODE_TYPE_OP_PARTICLE, 
		                            DEPSOP_TYPE_EXEC, BKE_particle_system_eval, 
		                            "PSys Eval");
		
		
	}
	
	/* pointcache */
	// TODO...
}

/* ------------------------------------------------ */

/* Rigidbody Simulation - Scene Level */
static void deg_build_rigidbody_graph(Depsgraph *graph, Scene *scene)
{
	RigidBodyWorld *rbw = scene->rigidbody_world;
	OperationDepsNode *init_node;
	OperationDepsNode *sim_node; // XXX: what happens if we need to split into several groups?
	
	/* == Rigidbody Simulation Nodes == 
	 * There are 3 nodes related to Rigidbody Simulation:
	 * 1) "Initialise/Rebuild World" - this is called sparingly, only when the simulation
	 *    needs to be rebuilt (mainly after file reload, or moving back to start frame)
	 * 2) "Do Simulation" - perform a simulation step - interleaved between the evaluation
	 *    steps for clusters of objects (i.e. between those affected and/or not affected by
	 *    the sim for instance)
	 *
	 * 3) "Pull Results" - grab the specific transforms applied for a specific object -
	 *    performed as part of object's transform-stack building
	 */
	
	/* create nodes ------------------------------------------------------------------------ */
	/* init/rebuild operation */
	init_node = DEG_add_operation(graph, &scene->id, NULL, DEPSNODE_TYPE_OP_RIGIDBODY,
	                              DEPSOP_TYPE_INIT, BKE_rigidbody_rebuild_world,
	                              "Rigidbody World Rebuild");
	
	/* do-sim operation */
	sim_node = DEG_add_operation(graph, &scene->id, NULL, DEPSNODE_TYPE_OP_RIGIDBODY,
	                             DEPSOP_TYPE_EXEC, BKE_rigidbody_do_simulation,
	                             "Rigidbody World Do Simulation");
	
	
	/* rel between the two sim-nodes */
	DEG_add_new_relation(&init_node->nd, &sim_node->nd, DEPSREL_TYPE_OPERATION, "Rigidbody [Init -> SimStep]");
	
	/* set up dependencies between these operations and other builtin nodes --------------- */	
	
	/* time dependency */
	{
		DepsNode *time_src = DEG_get_node(graph, NULL, NULL, DEPSNODE_TYPE_TIMESOURCE, NULL);
		
		/* init node is only occasional (i.e. on certain frame values only), 
		 * but we must still include this link 
		 */
		DEG_add_new_relation(time_src, &init_node->nd, DEPSREL_TYPE_TIME, "TimeSrc -> Rigidbody Reset/Rebuild (Optional)"
		
		/* simulation step must always be performed */
		DEG_add_new_relation(time_src, &sim_node->nd, DEPSREL_TYPE_TIME, "TimeSrc -> Rigidbody Sim Step");
	}
	
	/* objects - simulation participants */
	// XXX: todo...
	
	/* constraints */
	// XXX: todo...
}

/* ************************************************* */
/* Geometry */

/* Shapekeys */
static void deg_build_shapekeys_graph(Depsgraph *graph, Scene *scene, Object *ob, Key *key)
{
	DepsNode *key_node, *obdata_node;
	
	/* create node for shapekeys block */
	// XXX: assume geometry - that's where shapekeys get evaluated anyways...
	key_node = DEG_get_node(graph, &key->id, NULL, DEPSNODE_TYPE_GEOMETRY, NULL);
	
	/* 1) attach to geometry */
	// XXX: aren't shapekeys now done as a pseudo-modifier on object?
	obdata_node = DEG_get_node(graph, (ID *)ob->data, NULL, DEPSNODE_TYPE_GEOMETRY, NULL);
	DEG_add_new_relation(graph, key_node, obdata_node, DEPSREL_TYPE_GEOMETRY_EVAL, "Shapekeys");
	
	/* 2) attach drivers, etc. */
	if (key->adt) {
		deg_build_animdata_graph(graph, scene, &key->id);
	}
}

/* ObData Geometry Evaluation */
static void deg_build_obdata_geom_graph(Depsgraph *graph, Scene *scene, Object *ob)
{
	DepsNode *ob_geom, *obdata_geom;
	DepsNode *node2;
	
	ID *ob_id     = (ID *)ob;
	ID *obdata_id = (ID *)ob->data;
	Key *key;
	
	/* get nodes for result of obdata's evaluation, and geometry evaluation on object */
	geom_node = DEG_get_node(graph, ob_id, NULL, DEPSNODE_TYPE_GEOMETRY, "Ob Geometry Component");
	obdata_geom = DEG_get_node(graph, obdata_id, NULL, DEPSNODE_TYPE_GEOMETRY, "ObData Geometry Component");
	
	/* link components to each other */
	DEG_add_new_relation(obdata_geom, geom_node, DEPSREL_TYPE_DATABLOCK, "Object Geometry Base Data");
	
	
	/* type-specific node/links */
	switch (ob->type) {
		case OB_MESH:
		{
			Mesh *me = (Mesh *)ob->data;
			
			/* evaluation operations */
			// XXX: wrapper around makeDerivedMesh() - which gets BMesh, etc. data...
		}
		break;
		
		case OB_MBALL: 
		{
			Object *mom = BKE_mball_basis_find(scene, ob);
			
			/* motherball - mom depends on children! */
			if (mom != ob) {
				node2 = DEG_get_node(graph, &mom->id, NULL, DEPSNODE_TYPE_GEOMETRY, "Meta-Motherball");
				DEG_add_new_relation(graph, geom_node, node2, DEPSREL_TYPE_GEOMETRY_EVAL, "Metaball Motherball");
			}
			
			/* metaball evaluation operations */
			// BKE_displist_make_mball
		}
		break;
		
		case OB_CURVE:
		case OB_FONT:
		{
			Curve *cu = ob->data;
			
			/* curve's dependencies */
			// XXX: these needs geom data, but where is geom stored?
			if (cu->bevobj) {
				node2 = DEG_get_node(graph, (ID *)cu->bevobj, NULL, DEPSNODE_TYPE_GEOMETRY, NULL);
				DEG_add_new_relation(graph, node2, geom_node, DEPSREL_TYPE_GEOMETRY_EVAL, "Curve Bevel");
			}
			if (cu->taperobj) {
				node2 = DEG_get_node(graph, (ID *)cu->tapeobj, NULL, DEPSNODE_TYPE_GEOMETRY, NULL);
				DEG_add_new_relation(graph, node2, geom_node, DEPSREL_TYPE_GEOMETRY_EVAL, "Curve Taper");
			}
			if (ob->type == OB_FONT) {
				if (cu->textoncurve) {
					node2 = DEG_get_node(graph, (ID *)cu->textoncurve, NULL, DEPSNODE_TYPE_GEOMETRY, NULL);
					DEG_add_new_relation(graph, node2, geom_node, DEPSREL_TYPE_GEOMETRY_EVAL, "Text on Curve");
				}
			}
			
			/* curve evaluation operations */
			// BKE_displist_make_curveTypes
		}
		break;
		
		case OB_SURF: /* Nurbs Surface */
		{
			/* nurbs evaluation operations */
			// BKE_displist_make_curveTypes
		}
		break;
		
		case OB_LATTICE: /* Lattice */
		{
			/* lattice evaluation operations */
			// BKE_lattice_modifiers_calc
		}
		break;
	}
	
	/* ShapeKeys */
	key = BKE_key_from_object(ob);
	if (key) {
		deg_build_shapekeys_graph(graph, scene, ob, key);
	}
	
	/* Modifiers */
	if (ob->modifiers.first) {
		ModifierData *md;
		
		for (md = ob->modifiers.first; md; md = md->next) {
			ModifierTypeInfo *mti = modifierType_getInfo(md->type);
			
			if (mti->updateDepgraph) {
				mti->updateDepgraph(md, graph, scene, ob);
			}
		}
	}
	
	/* materials */
	if (ob->totcol) {
		int a;
		
		for (a = 1; a <= ob->totcol; a++) {
			Material *ma = give_current_material(ob, a);
			
			if (ma) {
				deg_build_material_graph(graph, scene, geom_node, ma);
			}
		}
	}
	
	/* geometry collision */
	if (ELEM3(ob->type, OB_MESH, OB_CURVE, OB_LATTICE)) {
		// add geometry collider relations
	}
}

/* ************************************************* */
/* Assorted Object Data */

/* Cameras */
// TODO: Link scene-camera links in somehow...
static void deg_build_camera_graph(Depsgraph *graph, Scene *scene, Object *ob)
{
	Camera *cam = (Camera *)ob->data;
	DepsNode *obdata_node, *node2;
	
	/* node for obdata */
	obdata_node = DEG_get_node(graph, obdata_id, NULL, DEPSNODE_TYPE_PARAMETERS, "Camera Parameters");
	
	/* DOF */
	if (cam->dof_ob) {
		node2 = DEG_get_node(dag, (ID *)cam->dof_ob, NULL, DEPSNODE_TYPE_TRANSFORM, "Camera DOF Transform");
		DEG_add_new_relation(graph, node2, obdata_node, DEPSREL_TYPE_TRANSFORM, "Camera DOF");
	}
}

/* Lamps */
static void deg_build_lamp_graph(Depsgraph *graph, Scene *scene, Object *ob)
{
	Lamp *la = (Lamp *)ob->data;
	DepsNode *obdata_node;
	
	/* Prevent infinite recursion by checking (and tagging the lamp) as having been visited 
	 * already. This assumes la->id.flag & LIB_DOIT isn't set by anything else
	 * in the meantime... [#32017]
	 */
	if (la->id.flag & LIB_DOIT)
		return;
	
	la->id.flag |= LIB_DOIT;
	
	/* node for obdata */
	obdata_node = DEG_get_node(graph, obdata_id, NULL, DEPSNODE_TYPE_PARAMETERS, "Lamp Parameters");
	
	/* lamp's nodetree */
	if (la->nodetree) {
		deg_build_shader_nodetree_graph(graph, obdata_node, la->nodetree);
	}
	
	/* textures */
	deg_build_texture_stack_graph(graph, scene, obdata_node, la->mtex);
	
	la->id.flag &= ~LIB_DOIT;
}

/* ************************************************* */
/* Objects */

/* object parent relationships */
static void deg_build_object_parents(Depsgraph *graph, Object *ob)
{
	ID *parent_data_id = (ID *)ob->parent->data;
	ID *parent_id = (ID *)ob->parent;
	
	DepsNode *ob_node, parent_node = NULL;
	
	/* parenting affects the transform-stack of an object 
	 * NOTE: attach incoming links to the transform component, 
	 *       which will redirect these to whatever its first 
	 *       operation is in due course...
	 */
	ob_node = DEG_get_node(graph, &ob->id, NULL, DEPSNODE_TYPE_TRANSFORM, "Ob Transform");
	
	DEG_add_operation(graph, &ob->id, NULL, DEPSNODE_TYPE_OP_TRANSFORM, 
	                  DEPSOP_TYPE_EXEC, BKE_object_eval_parent,
	                  "BKE_object_eval_parent");
	
	/* type-specific links */
	switch (ob->partype) {
		case PARSKEL:  /* Armature Deform (Virtual Modifier) */
		{
			parent_node = DEG_get_node(graph, parent_id, NULL, DEPSNODE_TYPE_TRANSFORM, "Par Armature Transform");
			DEG_add_new_relation(graph, parent_node, ob_node, DEPSREL_TYPE_STANDARD, "Armature Deform Parent");
		}
		break;
			
		case PARVERT1: /* Vertex Parent */
		case PARVERT3:
		{
			parent_node = DEG_get_node(graph, parent_id, NULL, DEPSNODE_TYPE_GEOMETRY, "Vertex Parent Geometry Source");
			DEG_add_new_relation(graph, parent_node, ob_node, DEPSREL_TYPE_GEOMETRY_EVAL, "Vertex Parent");
			
			//parent_node->customdata_mask |= CD_MASK_ORIGINDEX;
		}
		break;
			
		case PARBONE: /* Bone Parent */
		{
			parent_node = DEG_get_node(graph, &ob->id, ob->parsubstr, DEPSNODE_TYPE_BONE, NULL);
			DEG_add_new_relation(graph, parent_node, ob_node, DEPSREL_TYPE_TRANSFORM, "Bone Parent");
		}
		break;
			
		default:
		{
			if (ob->parent->type == OB_LATTICE) {
				/* Lattice Deform Parent - Virtual Modifier */
				parent_node = DEG_get_node(graph, parent_id, NULL, DEPSNODE_TYPE_TRANSFORM, "Par Lattice Transform");
				DEG_add_new_relation(graph, parent_node, ob_node, DEPSREL_TYPE_STANDARD, "Lattice Deform Parent");
			}
			else if (ob->parent->type == OB_CURVE) {
				Curve *cu = ob->parent->data;
				
				if (cu->flag & CU_PATH) {
					/* Follow Path */
					parent_node = DEG_get_node(graph, parent_id, NULL, DEPSNODE_TYPE_GEOMETRY, "Curve Path");
					DEG_add_new_relation(graph, parent_node, ob_node, DEPSREL_TYPE_TRANSFORM, "Curve Follow Parent");
					// XXX: link to geometry or object? both are needed?
					// XXX: link to timesource too?
				}
				else {
					/* Standard Parent */
					parent_node = DEG_get_node(graph, parent_id, NULL, DEPSNODE_TYPE_TRANSFORM, "Parent Transform");
					DEG_add_new_relation(graph, parent_node, ob_node, DEPSREL_TYPE_TRANSFORM, "Curve Parent");
				}
			}
			else {
				/* Standard Parent */
				parent_node = DEG_get_node(graph, parent_id, NULL, DEPSNODE_TYPE_TRANSFORM, "Parent Transform");
				DEG_add_new_relation(graph, parent_node, ob_node, DEPSREL_TYPE_TRANSFORM, "Parent");
			}
		}
		break;
	}
	
	/* exception case: parent is duplivert */
	if ((ob->type == OB_MBALL) && (ob->parent->transflag & OB_DUPLIVERTS)) {
		//dag_add_relation(dag, node2, node, DAG_RL_DATA_DATA | DAG_RL_OB_OB, "Duplivert");
	}
}


/* build depsgraph nodes + links for object */
static DepsNode *deg_build_object_graph(Depsgraph *graph, Scene *scene, Object *ob)
{
	DepsNode *ob_node, *params_node, *trans_node;
	
	/* create node for object itself */
	ob_node = DEG_get_node(graph, &ob->id, NULL, DEPSNODE_TYPE_ID_REF, ob->id.name);
	
	/* standard components */
	params_node = DEG_get_node(graph, &ob->id, NULL, DEPSNODE_TYPE_PARAMETERS, NULL);
	trans_node = DEG_get_node(graph, &ob->id, NULL, DEPSNODE_TYPE_TRANSFORM, NULL);
	
	/* object parent */
	if (ob->parent) {
		deg_build_object_parents(graph, ob_node, ob);
	}
	
	/* object constraints */
	if (ob->constraints.first) {
		deg_build_constraints_graph(graph, scene, ob, NULL, &ob->constraints, trans_node);
	}
	
	/* object data */
	if (ob->data) {
		AnimData *data_adt;
		
		/* ob data animation */
		data_adt = BKE_animdata_from_id(obdata_id);
		if (data_adt) {
			deg_build_animdata_graph(graph, scene, obdata_id);
		}
		
		/* type-specific data... */
		switch (ob->type) {
			case OB_MESH:     /* Geometry */
			case OB_CURVE:
			case OB_FONT:
			case OB_SURF:
			case OB_MBALL:
			case OB_LATTICE:
			{
				deg_build_obdata_geom_graph(graph, scene, ob);
			}
			break;
			
			
			case OB_ARMATURE: /* Pose */
				deg_build_rig_graph(graph, scene, ob);
				break;
			
			
			case OB_LAMP:   /* Lamp */
				deg_build_lamp_graph(graph, scene, ob);
				break;
				
			case OB_CAMERA: /* Camera */
				deg_build_camera_graph(graph, scene, ob);
				break;
		}
	}
	
	/* particle systems */
	if (ob->particlesystem.first) {
		deg_build_particles_graph(graph, scene, ob);
	}
	
	/* AnimData */
	if (ob->adt) {
		deg_build_animdata_graph(graph, scene, &ob->id);
	}
	
	/* return object node... */
	return ob_node;
}


/* ************************************************* */
/* Scene */

/* build depsgraph for specified scene - this is called recursively for sets... */
static DepsNode *deg_build_scene_graph(Depsgraph *graph, Main *bmain, Scene *scene)
{
	DepsNode *scene_node;
	DepsNode *time_src;
	Group *group;
	Base *base;
	
	/* init own node */
	scene_node = DEG_get_node(graph, &scene->id, NULL, DEPSNODE_TYPE_ID_REF, scene->id.name);
	
	/* timesource */
	time_src = DEG_get_node(graph, &scene->id, NULL, DEPSNODE_TYPE_TIMESOURCE, "Scene Timesource");
	
	/* sound system */
	// XXX: this is mainly on frame change...
	
	
	/* build subgraph for set, and link this in... */
	// XXX: depending on how this goes, that scene itself could probably store its
	//      own little partial depsgraph?
	if (scene->set) {
		DepsNode *set_node = deg_build_scene_graph(graph, bmain, scene->set);
		// TODO: link set to scene, especially our timesource...
	}
	
	/* scene objects */
	for (base = scene->base.first; base; base = base->next) {
		Object *ob = base->object;
		
		/* object itself */
		deg_build_object_graph(graph, scene, ob);
		
		/* object that this is a proxy for */
		// XXX: the way that proxies work needs to be completely reviewed!
		if (ob->proxy) {
			deg_build_object_graph(graph, scene, ob->proxy);
		}
		
		/* handled in next loop... 
		 * NOTE: in most cases, setting dupli-group means that we may want
		 *       to instance existing data and/or reuse it with very few
		 *       modifications...
		 */
		if (ob->dup_group) {
			ob->dup_group->id.flag |= LIB_DOIT;
		}
	}
	
	/* tagged groups */
	for (group = bmain->group.first; group; group = group->id.next) {
		if (group->id.flag & LIB_DOIT) {
			DepsNode *group_node;
			
			/* add group as a subgraph... */
			group_node = DEG_graph_build_group_subgraph(graph, bmain, group);
			
			group->id.flag &= ~LIB_DOIT;
		}
	}
	
	/* rigidbody */
	if (scene->rigidbody_world) {
		deg_build_rigidbody_graph(graph, scene);
	}
	
	/* scene's animation and drivers */
	if (scene->adt) {
		deg_build_animdata_graph(graph, scene, &scene->id);
	}
	
	/* world */
	if (scene->world) {
		deg_build_world_graph(graph, scene, scene->world);
	}
	
	/* compo nodes */
	if (scene->nodetree) {
		deg_build_compo_graph(graph, scene);
	}
	
	/* sequencer */
	// XXX...
	
	/* return node */
	return scene_node;
}

/* ************************************************* */
/* Depsgraph Building Entrypoints */

/* Build depsgraph for the given group, and dump results in given graph container 
 * This is usually used for building subgraphs for groups to use...
 */
void DEG_graph_build_from_group(Depsgraph *graph, Main *bmain, Group *group)
{
	GroupObject *go;
	
	/* add group objects */
	for (go = group->gobject.first; go; go = go->next) {
		Object *ob = go->ob;
		
		/* Each "group object" is effectively a separate instance of the underlying
		 * object data. When the group is evaluated, the transform results and/or 
		 * some other attributes end up getting overridden by the group
		 */
	}
}

/* Build subgraph for group */
DepsNode *DEG_graph_build_group_subgraph(Depsgraph *graph_main, Main *bmain, Group *group)
{
	Depsgraph *graph;
	SubgraphDepsNode *subgraph_node;
	
	/* sanity checks */
	if (ELEM3(NULL, graph_main, bmain, group))
		return NULL;
	
	/* create new subgraph's data */
	graph = DEG_graph_new();
	DEG_graph_build_from_group(graph, bmain, group);
	
	/* create a node for representing subgraph */
	subgraph_node = (SubgraphDepsNode *)DEG_get_node(graph_main, &group->id, NULL,
	                                                 DEPSNODE_TYPE_SUBGRAPH, 
	                                                 group->id.name);
	                                                     
	subgraph_node->graph = graph;
	
	/* make a copy of the data this node will need? */
	// XXX: do we do this now, or later?
	// TODO: need API function which queries graph's ID's hash, and duplicates those blocks thoroughly with all outside links removed...
	
	/* return new subgraph node */
	return (DepsNode *)subgraph_node;
}

/* -------------------------------------------------- */

/* Build depsgraph for the given scene, and dump results in given graph container */
// XXX: assume that this is called from outside, given the current scene as the "main" scene 
void DEG_graph_build_from_scene(Depsgraph *graph, Main *bmain, Scene *scene)
{
	DepsNode *scene_node;
	
	/* clear "LIB_DOIT" flag from all materials, etc. 
	 * to prevent infinite recursion problems later [#32017] 
	 */
	tag_main_idcode(bmain, ID_MA, FALSE);
	tag_main_idcode(bmain, ID_LA, FALSE);
	tag_main_idcode(bmain, ID_WO, FALSE);
	tag_main_idcode(bmain, ID_TEX, FALSE);
	
	/* build graph for scene (and set) */
	scene_node = deg_build_scene_graph(graph, main, scene);
	
	/* hook this up to a "root" node as entrypoint to graph... */
	graph->root_node = DEG_get_node(graph, NULL, NULL, DEPSNODE_TYPE_ROOT, "Root (Scene)");
	
	DEG_add_new_relation(graph, graph->root_node, scene_node, 
	                     DEPSREL_TYPE_ROOT_TO_ACTIVE, "Root to Active Scene");
	                     
	
	/* ensure that all implicit constraints between nodes are satisfied */
	DEG_graph_validate_links(graph);
	
	/* sort nodes to determine evaluation order (in most cases) */
	DEG_graph_sort(graph);
}

/* ************************************************* */


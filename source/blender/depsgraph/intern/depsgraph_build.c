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
 * Methods for constructing depsgraph
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_constraint_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_constraint.h"
#include "BKE_depsgraph.h"
#include "BKE_key.h"
#include "BKE_material.h"
#include "BKE_object.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "depsgraph_types.h"
#include "depsgraph_eval.h"


/* ************************************************* */

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
	
	DepsNode *affected_node = NULL;
	DepsNode *driver_node = NULL;
	
	
	/* create data node for this driver */
	driver_node = DEG_get_node(graph, DEPSNODE_TYPE_DATA, id, &RNA_Driver);
	// TODO: bind execution operations...
	
	/* create dependency between data affected by driver and driver */
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
					target_node = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, 
					                           dtar->id, &RNA_PoseBone, pchan);
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
		adt_node = DEG_get_node(graph, DEPSNODE_TYPE_DATA, id, &RNA_AnimData, adt);
		// TODO: bind execution operations...
		
		/* make all other nodes on data depend on this... */
		// XXX: ???
		
		/* wire up dependencies to other AnimData nodes */
		// XXX: this step may have to be done later...
		
		/* wire up dependency to time source */
		// NOTE: this assumes that timesource was already added as one of first steps!
		time_src = DEG_find_node(graph, DEPSNODE_TYPE_TIMESOURCE, &scene->id, NULL, NULL);
		DEG_add_new_relation(graph, time_src, adt_node, DEPSREL_TYPE_TIME, 
		                     "[TimeSrc -> Animation] DepsRel");
	}
	
	/* drivers */
	for (fcu = adt->drivers.first; fcu; fcu = fcu->next) {
		/* create driver */
		DepsNode *driver_node = deg_build_driver_rel(graph, id, fcu);
		
		/* prevent driver from occurring before own animation... */
		if (adt_node) {
			DEG_add_new_relation(graph, adt_node, driver_node, DEPSREL_TYPE_OPERATION, 
			                     "[AnimData Before Drivers] DepsRel");
		}
	}
}


/* ************************************************* */
/* Rigs (i.e. Armature Bones) */

/* ************************************************* */
/* Geometry */

/* Shapekeys */
static void deg_build_shapekeys_graph(Depsgraph *graph, Scene *scene, Object *ob, Key *key)
{
	DepsNode *key_node, *obdata_node;
	
	/* create node for shapekeys block */
	key_node = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, &key->id, NULL, NULL);
	
	/* 1) attach to geometry */
	obdata_node = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, (ID *)ob->data, NULL, NULL);
	DEG_add_new_relation(graph, key_node, obdata_node, DEPSREL_TYPE_GEOMETRY_EVAL, "Shapekeys");
	
	/* 2) attach drivers, etc. */
	if (key->adt) {
		deg_build_animdata_graph(graph, scene, &key->id);
	}
}

/* ************************************************* */
/* Objects */

/* object parent relationships */
static void deg_build_object_parents(Depsgraph *graph, DepsNode *ob_node, Object *ob)
{
	ID *parent_data_id = (ID *)ob->parent->data;
	ID *parent_id = (ID *)ob->parent;
	
	DepsNode *parent_node = NULL;
	
	/* type-specific links */
	// TODO: attach execution hooks...
	switch (ob->partype) {
		case PARSKEL:  /* Armature Deform (Virtual Modifier) */
		{
			// DAG_RL_DATA_DATA | DAG_RL_OB_OB
			parent_node = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, parent_id, NULL, NULL);
			DEG_add_new_relationship(graph, parent_node, ob_node, DEPSREL_TYPE_STANDARD, "Armature Deform Parent");
		}
		break;
			
		case PARVERT1: /* Vertex Parent */
		case PARVERT3:
		{
			// DAG_RL_DATA_OB | DAG_RL_OB_OB
			parent_node = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, parent_data_id, NULL, NULL);
			DEG_add_new_relationship(graph, parent_node, ob_node, DEPSREL_TYPE_GEOMETRY_EVAL, "Vertex Parent");
			
			//parent_node->customdata_mask |= CD_MASK_ORIGINDEX;
		}
		break;
			
		case PARBONE: /* Bone Parent */
		{
			bPoseChannel *pchan = BKE_pose_channel_from_name(ob->parent->pose, ob->parsubstr);
			
			parent_node = DEG_get_node(graph, DEPSNODE_TYPE_DATA, parent_id, &RNA_PoseBone, pchan);
			DEG_add_new_relationship(graph, parent_node, ob_node, DEPSREL_TYPE_TRANSFORM, "Bone Parent");
		}
		break;
			
		default:
			if (ob->parent->type == OB_LATTICE) {
				/* Lattice Deform Parent - Virtual Modifier */
				// DAG_RL_DATA_DATA | DAG_RL_OB_OB
				parent_node = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, parent_id, NULL, NULL);
				DEG_add_new_relationship(graph, parent_node, ob_node, DEPSREL_TYPE_STANDARD, "Lattice Deform Parent");
			}
			else if (ob->parent->type == OB_CURVE) {
				Curve *cu = ob->parent->data;
				
				if (cu->flag & CU_PATH) {
					/* Follow Path */
					parent_node = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, parent_data_id, NULL, NULL);
					DEG_add_new_relationship(graph, parent_node, ob_node, DEPSREL_TYPE_TRANSFORM, "Curve Follow Parent");
					// XXX: link to geometry or object? both are needed?
					// XXX: link to timesource too?
				}
				else {
					/* Standard Parent */
					parent_node = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, parent_id, NULL, NULL);
					DEG_add_new_relationship(graph, parent_node, ob_node, DEPSREL_TYPE_TRANSFORM, "Curve Parent");
				}
			}
			else {
				/* Standard Parent */
				parent_node = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, parent_id, NULL, NULL);
				DEG_add_new_relationship(graph, parent_node, ob_node, DEPSREL_TYPE_TRANSFORM, "Parent");
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
	DepsNode *ob_node, *obdata_node = NULL;
	Key *key;
	
	/* create node for object itself */
	ob_node = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, ob->id.name);
	
	/* object parent */
	if (ob->parent) {
		deg_build_object_parents(graph, ob_node, ob);
	}
	
	/* ShapeKeys */
	key = BKE_key_from_object(ob);
	if (key) {
		deg_build_shapekeys_graph(graph, scene, ob, key);
	}
	
	/* object data */
	if (ob->data) {
		AnimData *data_adt = BKE_animdata_from_id((ID *)ob->data);
		DepsNode *node2;
		
		switch (ob->type) {
			case OB_ARMATURE:
			{
				bArmature *arm = (bArmature *)ob->data;
				...
			}
			break;
			
			case OB_MESH:
			{
				Mesh *me = (Mesh *)ob->data;
				...
			}
			break;
			
			case OB_MBALL: 
			{
				Object *mom = BKE_mball_basis_find(scene, ob); // XXX: scene
				
				/* node for obdata */
				// XXX...
				obdata_node = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, obdata_id, NULL, NULL);
				
				/* motherball - mom depends on children! */
				// XXX: these needs geom data, but where is geom stored?
				if (mom != ob) {
					node2 = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, (ID *)mom->data, NULL, NULL);
					DEG_add_new_relation(graph, obdata_node, node2, DEPSREL_TYPE_GEOMETRY_EVAL, "Metaball Motherball");
				}
			}
			break;
			
			case OB_CURVE:
			case OB_FONT:
			{
				Curve *cu = ob->data;
				
				/* node for obdata */
				obdata_node = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, obdata_id, NULL, NULL);
				
				/* curve's dependencies */
				// XXX: these needs geom data, but where is geom stored?
				if (cu->bevobj) {
					node2 = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, (ID *)cu->bevobj->data, NULL, NULL);
					DEG_add_new_relation(graph, node2, obdata_node, DEPSREL_TYPE_GEOMETRY_EVAL, "Curve Bevel");
				}
				if (cu->taperobj) {
					node2 = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, (ID *)cu->taperobj->data, NULL, NULL);
					DEG_add_new_relation(graph, node2, obdata_node, DEPSREL_TYPE_GEOMETRY_EVAL, "Curve Taper");
				}
				if (ob->type == OB_FONT) {
					if (cu->textoncurve) {
						node2 = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, (ID *)cu->textoncurve->data, NULL, NULL);
						DEG_add_new_relation(graph, node2, obdata_node, DEPSREL_TYPE_GEOMETRY_EVAL, "Text on Curve");
					}
				}
			}
			break;
			
			case OB_CAMERA:
			{
				Camera *cam = (Camera *)ob->data;
				
				/* node for obdata */
				obdata_node = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, obdata_id, NULL, NULL);
				
				/* DOF */
				if (cam->dof_ob) {
					// XXX: to be precise, we want the "transforms" component only...
					node2 = DEG_get_node(dag, DEPSNODE_TYPE_OUTER_ID, (ID *)cam->dof_ob, NULL, NULL);
					DEG_add_new_relation(graph, node2, node, DEPSREL_TYPE_TRANSFORM, "Camera DOF");
				}
			}
			break;
		}
		
		/* ob data animation */
		if (data_adt) {
			deg_build_animdata_graph(graph, scene, (ID *)ob->data);
		}
	}
	
	/* object constraints */
	if (ob->constraints.first) {
		bConstraint *con;
		
		for (con = ob->constraints.first; con; con = con->next) {
			...
		}
	}
	
	/* modifiers */
	if (ob->modifiers.first) {
		ModifierData *md;
		
		for (md = ob->modifiers.first; md; md = md->next) {
			ModifierTypeInfo *mti = modifierType_getInfo(md->type);
			
			if (mti->updateDepgraph) {
				mti->updateDepgraph(md, graph, scene, ob, ob_node);
			}
		}
	}
	
	/* materials */
	if (ob->totcol) {
		int a;
		
		for (a = 1; a <= ob->totcol; a++) {
			Material *ma = give_current_material(ob, a);
			...
		}
	}
	
	/* particle systems */
	if (ob->particlesystem.first) {
		dag_build_particles_graph(graph, scene, ob);
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
static DepsNode *deg_build_scene_graph(Depsgraph *graph, Scene *scene)
{
	DepsNode *scene_node;
	Base *base;
	
	/* init own node */
	scene_node = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, "Scene");
	
	// TODO: timesource?
	
	/* build subgraph for set, and link this in... */
	// XXX: depending on how this goes, that scene itself could probably store its
	//      own little partial depsgraph?
	if (scene->set) {
		DepsNode *set_node = deg_build_scene_graph(graph, scene->set);
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
	}
	
	/* scene's animation and drivers */
	if (scene->adt) {
		deg_build_animdata_graph(graph, scene_node, &scene->id);
	}
	
	/* world */
	if (scene->world) {
	
	}
	
	/* compo nodes */
	
	/* sequencer */
	// XXX...
	
	/* return node */
	return scene_node;
}

/* ************************************************* */
/* Depsgraph Building Entrypoints */

/* Build depsgraph for the given scene, and dump results in given graph container */
// XXX: assume that this is called from outside, given the current scene as the "main" scene 
void DEG_graph_build_from_scene(Depsgraph *graph, Scene *scene)
{
	DepsNode *scene_node;
	
	/* build graph for scene (and set) */
	scene_node = deg_build_scene_graph(graph, scene);
	
	/* hook this up to a "root" node as entrypoint to graph... */
	graph->root_node = DEG_get_node(graph, DEPSNODE_TYPE_ROOT, "Root (Scene)");
	
	DEG_add_new_relation(graph, graph->root_node, scene_node, 
	                     DEPSREL_TYPE_ROOT_TO_ACTIVE, "Root to Active Scene");
}

/* ************************************************* */


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
	driver_node = DEG_get_node(graph, id, DEPSNODE_TYPE_OP_DRIVER, NULL);
	// XXX: attach metadata regarding this exact driver
	
	/* create dependency between data affected by driver and driver */
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
					// XXX...
					target_node = DEG_get_node(graph, dtar->id, DEPSNODE_TYPE_OP_BONE, pchan->name);
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
		adt_node = DEG_get_node(graph, id, DEPSNODE_ANIMATION, "Animation");
		
		/* wire up dependency to time source */
		// NOTE: this assumes that timesource was already added as one of first steps!
		time_src = DEG_find_node(graph, NULL, DEPSNODE_TYPE_TIMESOURCE, NULL);
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
/* Rigs (i.e. Armature Bones) */

/* ************************************************* */
/* Geometry */

/* Shapekeys */
static void deg_build_shapekeys_graph(Depsgraph *graph, Scene *scene, Object *ob, Key *key)
{
	DepsNode *key_node, *obdata_node;
	
	/* create node for shapekeys block */
	// XXX: assume geometry - that's where shapekeys get evaluated anyways...
	key_node = DEG_get_node(graph, &key->id, DEPSNODE_TYPE_GEOMETRY, NULL);
	
	/* 1) attach to geometry */
	// XXX: aren't shapekeys now done as a pseudo-modifier on object?
	obdata_node = DEG_get_node(graph, (ID *)ob->data, DEPSNODE_TYPE_GEOMETRY, NULL);
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
	geom_node = DEG_get_node(graph, ob_id, DEPSNODE_TYPE_GEOMETRY, "Ob Geometry Component");
	obdata_geom = DEG_get_node(graph, obdata_id, DEPSNODE_TYPE_GEOMETRY, "ObData Geometry Component");
	
	/* link components to each other */
	DEG_add_new_relation(obdata_geom, geom_node, DEPSREL_TYPE_DATABLOCK, "Object Geometry Base Data");
	
	
	/* type-specific node/links */
	switch (ob->type) {
		case OB_MESH:
		{
			Mesh *me = (Mesh *)ob->data;
			...
		}
		break;
		
		case OB_MBALL: 
		{
			Object *mom = BKE_mball_basis_find(scene, ob);
			
			/* motherball - mom depends on children! */
			if (mom != ob) {
				node2 = DEG_get_node(graph, &mom->id, DEPSNODE_TYPE_GEOMETRY, "Meta-Motherball");
				DEG_add_new_relation(graph, geom_node, node2, DEPSREL_TYPE_GEOMETRY_EVAL, "Metaball Motherball");
			}
			
			/* metaball evaluation operations */
			// xxx...
		}
		break;
		
		case OB_CURVE:
		case OB_FONT:
		{
			Curve *cu = ob->data;
			
			/* curve's dependencies */
			// XXX: these needs geom data, but where is geom stored?
			if (cu->bevobj) {
				node2 = DEG_get_node(graph, (ID *)cu->bevobj, DEPSNODE_TYPE_GEOMETRY, NULL);
				DEG_add_new_relation(graph, node2, geom_node, DEPSREL_TYPE_GEOMETRY_EVAL, "Curve Bevel");
			}
			if (cu->taperobj) {
				node2 = DEG_get_node(graph, (ID *)cu->tapeobj, DEPSNODE_TYPE_GEOMETRY, NULL);
				DEG_add_new_relation(graph, node2, geom_node, DEPSREL_TYPE_GEOMETRY_EVAL, "Curve Taper");
			}
			if (ob->type == OB_FONT) {
				if (cu->textoncurve) {
					node2 = DEG_get_node(graph, (ID *)cu->textoncurve, DEPSNODE_TYPE_GEOMETRY, NULL);
					DEG_add_new_relation(graph, node2, geom_node, DEPSREL_TYPE_GEOMETRY_EVAL, "Text on Curve");
				}
			}
			
			/* curve evaluation operations */
			// xxx...
		}
		break;
		
		case OB_SURF: /* Nurbs Surface */
		{
			...
		}
		break;
		
		case OB_LATTICE: /* Lattice */
		{
			...
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
}

/* ************************************************* */
/* Objects */

/* object parent relationships */
static void deg_build_object_parents(Depsgraph *graph, Object *ob)
{
	ID *parent_data_id = (ID *)ob->parent->data;
	ID *parent_id = (ID *)ob->parent;
	
	DepsNode *ob_node, parent_node = NULL;
	
	/* parenting affects the transform-stack of an object */
	ob_node = DEG_get_node(graph, &ob->id, DEPSNODE_TYPE_TRANSFORM, "Ob Transform");
	DEG_add_operation(graph, &ob->id, DEPSNODE_TYPE_OP_TRANSFORM, 
	                  DEPSOP_TYPE_EXEC, BKE_object_eval_parent);
	
	/* type-specific links */
	switch (ob->partype) {
		case PARSKEL:  /* Armature Deform (Virtual Modifier) */
		{
			parent_node = DEG_get_node(graph, parent_id, DEPSNODE_TYPE_TRANSFORM, "Par Armature Transform");
			DEG_add_new_relation(graph, parent_node, ob_node, DEPSREL_TYPE_STANDARD, "Armature Deform Parent");
		}
		break;
			
		case PARVERT1: /* Vertex Parent */
		case PARVERT3:
		{
			parent_node = DEG_get_node(graph, parent_id, DEPSNODE_TYPE_GEOMETRY, "Vertex Parent Geometry Source");
			DEG_add_new_relation(graph, parent_node, ob_node, DEPSREL_TYPE_GEOMETRY_EVAL, "Vertex Parent");
			
			//parent_node->customdata_mask |= CD_MASK_ORIGINDEX;
		}
		break;
			
		case PARBONE: /* Bone Parent */
		{
			bPoseChannel *pchan = BKE_pose_channel_from_name(ob->parent->pose, ob->parsubstr);
			
			parent_node = DEG_get_node(graph, &ob->id, DEPSNODE_TYPE_
			DEG_add_new_relation(graph, parent_node, ob_node, DEPSREL_TYPE_TRANSFORM, "Bone Parent");
		}
		break;
			
		default:
		{
			if (ob->parent->type == OB_LATTICE) {
				/* Lattice Deform Parent - Virtual Modifier */
				parent_node = DEG_get_node(graph, parent_id, DEPSNODE_TYPE_TRANSFORM, "Par Lattice Transform");
				DEG_add_new_relation(graph, parent_node, ob_node, DEPSREL_TYPE_STANDARD, "Lattice Deform Parent");
			}
			else if (ob->parent->type == OB_CURVE) {
				Curve *cu = ob->parent->data;
				
				if (cu->flag & CU_PATH) {
					/* Follow Path */
					parent_node = DEG_get_node(graph, parent_id, DEPSNODE_TYPE_GEOMETRY, "Curve Path");
					DEG_add_new_relation(graph, parent_node, ob_node, DEPSREL_TYPE_TRANSFORM, "Curve Follow Parent");
					// XXX: link to geometry or object? both are needed?
					// XXX: link to timesource too?
				}
				else {
					/* Standard Parent */
					parent_node = DEG_get_node(graph, parent_id, DEPSNODE_TYPE_TRANSFORM, "Parent Transform");
					DEG_add_new_relation(graph, parent_node, ob_node, DEPSREL_TYPE_TRANSFORM, "Curve Parent");
				}
			}
			else {
				/* Standard Parent */
				parent_node = DEG_get_node(graph, parent_id, DEPSNODE_TYPE_TRANSFORM, "Parent Transform");
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
	DepsNode *ob_node;
	
	/* create node for object itself */
	ob_node = DEG_get_node(graph, &ob->id, DEPSNODE_TYPE_ID_REF, ob->id.name);
	
	/* object parent */
	if (ob->parent) {
		deg_build_object_parents(graph, ob_node, ob);
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
			{
				bArmature *arm = (bArmature *)ob->data;
				...
			}
			break;
			
			case OB_CAMERA: /* Camera */
			{
				Camera *cam = (Camera *)ob->data;
				DepsNode *obdata_node, *node2;
				
				/* node for obdata */
				obdata_node = DEG_get_node(graph, obdata_id, DEPSNODE_TYPE_PARAMETERS, "Camera Parameters");
				
				/* DOF */
				if (cam->dof_ob) {
					node2 = DEG_get_node(dag, (ID *)cam->dof_ob, DEPSNODE_TYPE_TRANSFORM, "Camera DOF Transform");
					DEG_add_new_relation(graph, node2, obdata_node, DEPSREL_TYPE_TRANSFORM, "Camera DOF");
				}
			}
			break;
		}
	}
	
	/* object constraints */
	if (ob->constraints.first) {
		bConstraint *con;
		
		for (con = ob->constraints.first; con; con = con->next) {
			...
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
	DepsNode *time_src;
	Base *base;
	
	/* init own node */
	scene_node = DEG_get_node(graph, &scene->id, DEPSNODE_TYPE_ID_REF, scene->id.name);
	
	/* timesource */
	time_src = DEG_get_node(graph, &scene->id, DEPSNODE_TYPE_TIMESOURCE, "Scene Timesource");
	
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
	                     
	
	/* ensure that all implicit constraints between nodes are satisfied */
	DEG_graph_validate_links(graph);
	
	/* sort nodes to determine evaluation order (in most cases) */
	DEG_graph_sort(graph);
}

/* ************************************************* */


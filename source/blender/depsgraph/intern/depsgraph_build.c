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

#include "BKE_animsys.h"
#include "BKE_constraint.h"
#include "BKE_depsgraph.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "depsgraph_types.h"
#include "depsgraph_eval.h"


/* ************************************************* */

/* ************************************************* */
/* AnimData */

static void deg_build_animdata_graph(Depsgraph *graph, DepsNode *scene_node, ID *id)
{
	DepsNode *id_node = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, id, NULL, NULL);
	AnimData *adt = BKE_animdata_from_id(id);
	FCurve *fcu;
	
	BLI_assert(adt != NULL);
	
	/* animation */
	if (adt->action || adt->nla_tracks.first) {
		/* create "animation" data node for this block */
		
		/* attach AnimData node to ID block it affects */
		
		/* wire up dependencies to other AnimData nodes + */
		// XXX: this step may have to be done later...
	}
	
	/* drivers */
	for (fcu = adt->drivers.first; fcu; fcu = fcu->next) {
		// 
	}
}


/* ************************************************* */
/* Rigs (i.e. Armature Bones) */

/* ************************************************* */
/* Objects */

static DepsNode *deg_build_object_graph(Depsgraph *graph, DepsNode *scene_node, Object *ob)
{
	DepsNode *ob_node;
	
	/* create node for object itself */
	ob_node = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, ob->id.name);
	
	/* object parent */
	if (ob->parent) {
		
	}
	
	/* object data */
	if (ob->data) {
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
			
			case OB_CURVE:
			case OB_FONT:
			{
				Curve *cu = (Curve *)ob->data;
				...
			}
			break;
		}
		
		/* ob data animation */
		
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
	
	/* AnimData */
	if (ob->adt) {
		deg_build_animdata_graph(graph, scene_node, &ob->id);
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
		deg_build_object_graph(graph, scene_node, ob);
		
		/* object that this is a proxy for */
		// XXX: the way that proxies work needs to be completely reviewed!
		if (ob->proxy) {
			deg_build_object_graph(graph, scene_node, ob->proxy);
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
	                     DEG_ROOT_TO_ACTIVE, "Root to Active Scene");
}

/* ************************************************* */


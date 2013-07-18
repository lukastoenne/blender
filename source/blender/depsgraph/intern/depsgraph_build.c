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

#include "BKE_constraint.h"
#include "BKE_depsgraph.h"


#include "depsgraph_types.h"
#include "depsgraph_eval.h"


/* ************************************************* */

/* ************************************************* */
/* Objects */

static DepsNode *deg_build_object_graph(Depsgraph *graph, DepsNode *scene_node, Object *ob)
{
	DepsNode *ob_node;
	bConstraint *con;
	ModifierData *md;
	
	/* create node for object itself */
	// XXX: directly using this name may be dangerous - if object gets freed we could end up having crashes
	/* XXX: we have a problem here - other nodes we depend on may not exist yet (likewise, others that depend 
	 * on us may have been done before us). So, we need to make it possible to let nodes hook up with whatever
	 * they need, if/when they need it. But, this means that we need to be a bit more careful about how
	 * this all works out... */
	ob_node = DEG_add_node(graph, DEPSNODE_TYPE_OUTER_ID, ob->id.name);
	
	/* AnimData */
	if (ob->adt) {
		deg_build_animdata_graph(graph, scene_node, &ob->id);
	}
	
	/* object data */
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
		{
			Curve *cu = (Curve *)ob->data;
			...
		}
		break;
	}
	
		/* object constraints */
	for (con = ob->constraints.first; con; con = con->next) {
		...
	}
	
	/* modifiers */
	for (md = ob->modifiers.first; md; md = md->next) {
		...
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
	
	/* init own node */
	scene_node = DEG_add_node(graph, DEPSNODE_TYPE_OUTER_ID, "Scene");
	
	/* build subgraph for set, and link this in... */
	// XXX: depending on how this goes, that scene itself could probably store its
	//      own little partial depsgraph?
	if (scene->set) {
		DepsNode *set_node = deg_build_scene_graph(graph, scene->set);
		// TODO: link set to scene, especially our timesource...
	}
	
	/* scene objects */
	
	/* scene's animation and drivers */
	
	/* world */
	
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
	graph->root_node = DEG_add_node(graph, DEPSNODE_TYPE_ROOT, "Root (Scene)");
	
	DEG_add_relation(graph, graph->root_node, scene_node, 
	                 DEG_ROOT_TO_ACTIVE, "Root to Active Scene");
}

/* ************************************************* */


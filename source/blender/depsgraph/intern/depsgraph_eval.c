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
 * Evaluation engine entrypoints for Depsgraph Engine
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_constraint.h"
#include "BKE_DerivedMesh.h"
#include "BKE_depsgraph.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_scene.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "depsgraph_types.h"
#include "depsgraph_eval.h"
#include "depsgraph_intern.h"

/* *************************************************** */
/* Evaluation Internals */

/* Perform evaluation of a node */
// NOTE: this is called by the scheduler on a worker thread
static void deg_exec_node(Depsgraph *graph, DepsNode *node)
{
	/* get context and dispatch */
	if (node->class == DEPSNODE_CLASS_OPERATION) {
		OperationDepsNode *op  = (OperationDepsNode *)node;
		ComponentDepsNode *com = (ComponentDepsNode *)op->nd.owner; 
		void *context = NULL, *item = NULL;
		
		/* get context */
		// TODO: who initialises this? "Init" operations aren't able to initialise it!!!
		BLI_assert(com != NULL);
		context = com->context;
		
		/* get "item" */
		// XXX: not everything will use this - some may want something else!
		item = &op->ptr;
		
		/* take note of current time */
		op->start_time = PIL_check_seconds_timer();
		
		/* perform operation */
		op->evaluate(context, item);
		
		/* note how long this took */
		op->last_time = PIL_check_seconds_timer() - op->start_time;
	}
	/* NOTE: "generic" nodes cannot be executed, but will still end up calling this */
}

/* *************************************************** */
/* Evaluation Entrypoints */

/* Evaluate all nodes tagged for updating 
 * ! This is usually done as part of main loop, but may also be 
 *   called from frame-change update
 */
void DEG_evaluate_on_refresh(Depsgraph *graph)
{
	/* generate base evaluation context, upon which all the others are derived... */
	// TODO: this needs both main and scene access...
	
	/* from the root node, start queuing up nodes to evaluate */
	// ... start scheduler, etc.
	
	// ...
	
	/* clear any uncleared tags - just in case */
	DEG_graph_clear_tags(graph);
}

/* Frame-change happened for root scene that graph belongs to */
void DEG_evaluate_on_framechange(Depsgraph *graph, double ctime)
{
	TimeSourceDepsNode *tsrc;
	
	/* update time on primary timesource */
	tsrc = (TimeSourceDepsNode *)DEG_find_node(graph, NULL, NULL, DEPSNODE_TYPE_TIMESOURCE, NULL);
	tsrc->cfra = ctime;
	
	DEG_node_tag_update(graph, &tsrc->nd);
	
	/* recursively push updates out to all nodes dependent on this, 
	 * until all affected are tagged and/or scheduled up for eval
	 */
	DEG_graph_flush_updates(graph);
	
	/* perform recalculation updates */
	DEG_evaluate_on_refresh(graph);
}

/* *************************************************** */
/* Evaluation Context Management */

/* Initialise evaluation context for given node 
 * < context_type: (eDEG_OperationContext_UserType) type of evaluation contexts to create
 */
static void deg_node_evaluation_context_init(ComponentDepsNode *comp, short context_type)
{
	DepsNodeTypeInfo *nti = DEG_node_get_typeinfo((DepsNode *)comp);
	
	/* check if the requested evaluation context exists already */
	if (comp->contexts[context_type] == NULL) {
		/* doesn't exist, so create new evaluation context here */
		DEG_OperationsContext *ctx = NULL;
		
		// fill in type-specific stuff...
	}
	else {
		// TODO: validate existing data, as some parts may no longer exist 
	}
}

/* Initialise evaluation contexts for all nodes
 * < context_type: (eDEG_OperationContext_UserType) type of evaluation contexts to create
 */
void DEG_evaluation_context_init(Depsgraph *graph, short context_type)
{
	GHashIterator idHashIter;
	
	/* initialise master context first... */
	// ...
	
	/* loop over components, initialising their contexts */
	GHASH_ITER(idHashIter, graph->id_hash) {
		IDDepsNode *id_ref = BLI_ghashIterator_getValue(&idHashIter);
		GHashIterator compHashIter;
		
		/* loop over components */
		GHASH_ITER(compHashIter, id_ref->component_hash) {
			/* initialise evaluation context */
			ComponentDepsNode *comp = BLI_ghashIterator_getValue(&compHashIter); 
			deg_node_evaluation_context_init(comp, context_type);
		}
	}
}

/* --------------------------------------------------- */

/* Free evaluation contexts for node */
static void deg_node_evaluation_contexts_free(ComponentDepsNode *comp)
{
	DepsNodeTypeInfo *nti = DEG_get_node_typeinfo((DepsNode *)comp);
	
	/* free each context in turn */
	
}

/* Free evaluation contexts for all nodes */
void DEG_evaluation_contexts_free(Depsgraph *graph)
{
	GHashIterator idHashIter;
	
	/* free contexts for components first */
	GHASH_ITER(idHashIter, graph->id_hash) {
		IDDepsNode *id_ref = BLI_ghashIterator_getValue(&idHashIter);
		GHashIterator compHashIter;
		
		GHASH_ITER(compHashIter, id_ref->component_hash) {
			/* free evaluation context */
			ComponentDepsNode *comp = BLI_ghashIterator_getValue(&compHashIter);
			deg_node_evaluation_contexts_free(comp);
		}
	}
}

/* *************************************************** */

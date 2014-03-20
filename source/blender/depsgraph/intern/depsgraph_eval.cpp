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

extern "C" {
#include "BLI_blenlib.h"
#include "BLI_string.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_constraint.h"
#include "BKE_DerivedMesh.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_types.h"
} /* extern "C" */

#include "depsgraph_types.h"
#include "depsgraph_eval.h"
#include "depsgraph_queue.h"
#include "depsgraph_intern.h"

/* *************************************************** */
/* Multi-Threaded Evaluation Internals */

/* Internal - Lock shared between depsgraph internals for various critical activities */
// XXX: need to review the access modifiers here, as other files within depsgraph may need to access
static SpinLock threaded_update_lock;

/* Initialise threading lock - called during application startup */
void DEG_threaded_init(void)
{
	BLI_spin_init(&threaded_update_lock);
}

/* Free threading lock - called during application shutdown */
void DEG_threaded_exit(void)
{
	BLI_spin_end(&threaded_update_lock);
}

/* *************************************************** */
/* Evaluation Internals */

/* Perform evaluation of a node 
 * < graph: Dependency Graph that operations belong to
 * < node: operation node to evaluate
 * < context_type: the context/purpose that the node is being evaluated for
 */
// NOTE: this is called by the scheduler on a worker thread
static void deg_exec_node(Depsgraph *graph, DepsNode *node, eEvaluationContextType context_type)
{
	/* get context and dispatch */
	if (node->tclass == DEPSNODE_CLASS_OPERATION) {
		OperationDepsNode *op  = (OperationDepsNode *)node;
		ComponentDepsNode *com = (ComponentDepsNode *)op->owner; 
		void *context = NULL, *item = NULL;
		
		/* get context */
		// TODO: who initialises this? "Init" operations aren't able to initialise it!!!
		BLI_assert(com != NULL);
		context = com->contexts[context_type];
		
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
void DEG_evaluate_on_refresh(Depsgraph *graph, eEvaluationContextType context_type)
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
void DEG_evaluate_on_framechange(Depsgraph *graph, eEvaluationContextType context_type, double ctime)
{
	TimeSourceDepsNode *tsrc;
	
	/* update time on primary timesource */
	tsrc = (TimeSourceDepsNode *)DEG_find_node(graph, NULL, NULL, DEPSNODE_TYPE_TIMESOURCE, NULL);
	tsrc->cfra = ctime;
	
	DEG_node_tag_update(graph, tsrc);
	
	/* recursively push updates out to all nodes dependent on this, 
	 * until all affected are tagged and/or scheduled up for eval
	 */
	DEG_graph_flush_updates(graph);
	
	/* perform recalculation updates */
	DEG_evaluate_on_refresh(graph, context_type);
}

/* *************************************************** */
/* Evaluation Context Management */

/* Initialise evaluation context for given node */
static void deg_node_evaluation_context_init(ComponentDepsNode *comp, eEvaluationContextType context_type)
{
	DepsNodeFactory *nti = DEG_node_get_factory((DepsNode *)comp);
	
	/* check if the requested evaluation context exists already */
	if (comp->contexts[context_type] == NULL) {
		/* doesn't exist, so create new evaluation context here */
		bool valid = comp->eval_context_init(context_type);
		if (!valid) {
			/* initialise using standard techniques */
			comp->contexts[context_type] = MEM_callocN(sizeof(DEG_OperationsContext), "Evaluation Context");
			// TODO: init from master context somehow...
		}
	}
	else {
		// TODO: validate existing data, as some parts may no longer exist 
	}
}

/* Initialise evaluation contexts for all nodes */
void DEG_evaluation_context_init(Depsgraph *graph, eEvaluationContextType context_type)
{
	/* initialise master context first... */
	// ...
	
	/* loop over components, initialising their contexts */
	for (Depsgraph::IDNodeMap::const_iterator it = graph->id_hash.begin(); it != graph->id_hash.end(); ++it) {
		IDDepsNode *id_ref = it->second;
		
		/* loop over components */
		for (IDDepsNode::ComponentMap::iterator it = id_ref->components.begin(); it != id_ref->components.end(); ++it) {
			ComponentDepsNode *comp = it->second;
			/* initialise evaluation context */
			// TODO: we probably need to pass the master context down so that it can be handled properly
			deg_node_evaluation_context_init(comp, context_type);
		}
	}
}

/* --------------------------------------------------- */

/* Free evaluation contexts for node */
static void deg_node_evaluation_contexts_free(ComponentDepsNode *comp)
{
	size_t i;
	
	/* free each context in turn */
	for (i = 0; i < DEG_MAX_EVALUATION_CONTEXTS; i++) {
		if (comp->contexts[i]) {
			comp->eval_context_free((eEvaluationContextType)i);
			
			MEM_freeN(comp->contexts[i]);
			comp->contexts[i] = NULL;
		}
	}
}

/* Free evaluation contexts for all nodes */
void DEG_evaluation_contexts_free(Depsgraph *graph)
{
	/* free contexts for components first */
	for (Depsgraph::IDNodeMap::const_iterator it = graph->id_hash.begin(); it != graph->id_hash.end(); ++it) {
		IDDepsNode *id_ref = it->second;
		
		for (IDDepsNode::ComponentMap::iterator it = id_ref->components.begin(); it != id_ref->components.end(); ++it) {
			ComponentDepsNode *comp = it->second;
			/* free evaluation context */
			deg_node_evaluation_contexts_free(comp);
		}
	}
}

/* *************************************************** */

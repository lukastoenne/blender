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
 * Public API for Depsgraph
 */

#ifndef __DEG_DEPSGRAPH_H__
#define __DEG_DEPSGRAPH_H__

/* Dependency Graph
 *
 * The dependency graph tracks relations between various pieces of data in
 * a Blender file, but mainly just those which make up scene data. It is used
 * to determine the set of operations need to ensure that all data has been
 * correctly evaluated in response to changes, based on dependencies and visibility
 * of affected data.
 *
 *
 * Evaluation Engine
 *
 * The evaluation takes the operation-nodes the Depsgraph has tagged for updating, 
 * and schedules them up for being evaluated/executed such that the all dependency
 * relationship constraints are satisfied. 
 */

/* ************************************************* */
/* Forward-defined typedefs for core types
 * - These are used in all depsgraph code and by all callers of Depsgraph API...
 */

/* Dependency Graph */
typedef struct Depsgraph Depsgraph;

/* ------------------------------------------------ */

struct Main;
struct Scene;

struct PointerRNA;
struct PropertyRNA;

#ifdef __cplusplus
extern "C" {
#endif

/* ************************************************ */
/* Depsgraph API */

/* CRUD ------------------------------------------- */

// Get main depsgraph instance from context!

/* Create new Depsgraph instance */
// TODO: what args are needed here? What's the building-graph entrypoint?
Depsgraph *DEG_graph_new(void);

/* Free Depsgraph itself and all its data */
void DEG_graph_free(Depsgraph *graph);


/* Node Types Registry ---------------------------- */

/* Register all node types */
void DEG_register_node_types(void);

/* Free node type registry on exit */
void DEG_free_node_types(void);

/* Update Tagging -------------------------------- */

/* Tag node(s) associated with states such as time and visibility */
// XXX: what are these for?
void DEG_scene_update_flags(Depsgraph *graph, const bool do_time);
void DEG_on_visible_update(Depsgraph *graph, const bool do_time);

/* Tag node(s) associated with changed data for later updates */
void DEG_id_tag_update(Depsgraph *graph, const struct ID *id);
void DEG_data_tag_update(Depsgraph *graph, const struct PointerRNA *ptr);
void DEG_property_tag_update(Depsgraph *graph, const struct PointerRNA *ptr, const struct PropertyRNA *prop);

/* Update Flushing ------------------------------- */

/* Flush updates */
void DEG_graph_flush_updates(Depsgraph *graph);

/* Clear all update tags 
 * - For aborted updates, or after successful evaluation 
 */
void DEG_graph_clear_tags(Depsgraph *graph);

/* ************************************************ */
/* Evaluation Engine API */

/* Role of evaluation context
 * Describes what each context is to be used for evaluating
 */
typedef enum eEvaluationContextType {
	/* All Contexts - Not a proper role, but is used when passing args to functions */
	DEG_ALL_EVALUATION_CONTEXTS     = -1,
	
	/* Viewport Display */
	DEG_EVALUATION_CONTEXT_VIEWPORT = 0,
	/* Render Engine DB Conversion */
	DEG_EVALUATION_CONTEXT_RENDER   = 1,
	/* Background baking operation */
	DEG_EVALUATION_CONTEXT_BAKE     = 2,
	
	/* Maximum number of support evaluation contexts (Always as last) */
	DEG_MAX_EVALUATION_CONTEXTS
} eEvaluationContextType;


/* Intialise evaluation context 
 * < context_type: type of evaluation context to initialise
 */
void DEG_evaluation_context_init(Depsgraph *graph, eEvaluationContextType context_type);

/* Free evaluation context */
void DEG_evaluation_contexts_free(Depsgraph *graph);

/* ----------------------------------------------- */

/* Frame changed recalculation entrypoint 
 * < context_type: context to perform evaluation for
 * < ctime: (frame) new frame to evaluate values on
 */
void DEG_evaluate_on_framechange(Depsgraph *graph, eEvaluationContextType context_type, double ctime);

/* Data changed recalculation entrypoint 
 * < context_type: context to perform evaluation for
 */
void DEG_evaluate_on_refresh(Depsgraph *graph, eEvaluationContextType context_type);

/* ----------------------------------------------- */

/* Initialise threading lock - called during application startup */
void DEG_threaded_init(void);

/* Free threading lock - called during application shutdown */
void DEG_threaded_exit(void);

/* ************************************************ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __DEG_DEPSGRAPH_H__

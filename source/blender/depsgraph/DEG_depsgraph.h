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
 */

/** \file blender/depsgraph/DEG_depsgraph.h
 *  \ingroup depsgraph
 *
 * Public API for Depsgraph
 *
 * Dependency Graph
 * ================
 *
 * The dependency graph tracks relations between various pieces of data in
 * a Blender file, but mainly just those which make up scene data. It is used
 * to determine the set of operations need to ensure that all data has been
 * correctly evaluated in response to changes, based on dependencies and visibility
 * of affected data.
 *
 *
 * Evaluation Engine
 * =================
 *
 * The evaluation takes the operation-nodes the Depsgraph has tagged for updating, 
 * and schedules them up for being evaluated/executed such that the all dependency
 * relationship constraints are satisfied. 
 */

/* ************************************************* */
/* Forward-defined typedefs for core types
 * - These are used in all depsgraph code and by all callers of Depsgraph API...
 */

#ifndef __DEG_DEPSGRAPH_H__
#define __DEG_DEPSGRAPH_H__

/* Dependency Graph */
typedef struct Depsgraph Depsgraph;

/* ------------------------------------------------ */

struct EvaluationContext;
struct Main;

struct PointerRNA;
struct PropertyRNA;

#ifdef __cplusplus
extern "C" {
#endif

bool DEG_depsgraph_use_legacy(void);
void DEG_depsgraph_switch_to_legacy(void);
void DEG_depsgraph_switch_to_new(void);

/* ************************************************ */
/* Depsgraph API */

/* CRUD ------------------------------------------- */

// Get main depsgraph instance from context!

/* Create new Depsgraph instance */
// TODO: what args are needed here? What's the building-graph entry point?
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
void DEG_scene_update_flags(Depsgraph *graph, const bool do_time);

/* Update dependency graph when visible scenes/layers changes. */
void DEG_graph_on_visible_update(struct Main *bmain, struct Scene *scene);

/* Update all dependency graphs when visible scenes/layers changes. */
void DEG_on_visible_update(struct Main *bmain, const bool do_time);

/* Tag node(s) associated with changed data for later updates */
void DEG_graph_id_tag_update(struct Main *bmain,
                             Depsgraph *graph,
                             struct ID *id);
void DEG_graph_data_tag_update(Depsgraph *graph, const struct PointerRNA *ptr);
void DEG_graph_property_tag_update(Depsgraph *graph, const struct PointerRNA *ptr, const struct PropertyRNA *prop);

/* Tag given ID for an update in all the dependency graphs. */
void DEG_id_tag_update(struct ID *id, short flag);
void DEG_id_tag_update_ex(struct Main *bmain,
                          struct ID *id,
                          short flag);

/* Tag given ID type for update.
 *
 * Used by all sort of render engines to quickly check if
 * IDs of a given type need to be checked for update.
 */
void DEG_id_type_tag(struct Main *bmain, short idtype);

void DEG_ids_clear_recalc(struct Main *bmain);

/* Update Flushing ------------------------------- */

/* Flush updates */
void DEG_graph_flush_updates(struct Main *bmain, Depsgraph *graph);

/* Flush updates for all IDs */
void DEG_ids_flush_tagged(struct Main *bmain);

/* Check if something was changed in the database and inform
 * editors about this.
 */
void DEG_ids_check_recalc(struct Main *bmain,
                          struct Scene *scene,
                          bool time);

/* Clear all update tags
 * - For aborted updates, or after successful evaluation
 */
void DEG_graph_clear_tags(Depsgraph *graph);

/* ************************************************ */
/* Evaluation Engine API */

/* Evaluation Context ---------------------------- */

/* Create new evaluation context. */
struct EvaluationContext *DEG_evaluation_context_new(int mode);

/* Initialize evaluation context.
 * Used by the areas which currently overrides the context or doesn't have
 * access to a proper one.
 */
void DEG_evaluation_context_init(struct EvaluationContext *eval_ctx, int mode);

/* Free evaluation context. */
void DEG_evaluation_context_free(struct EvaluationContext *eval_ctx);

/* Graph Evaluation  ----------------------------- */

/* Frame changed recalculation entry point
 * < context_type: context to perform evaluation for
 * < ctime: (frame) new frame to evaluate values on
 */
void DEG_evaluate_on_framechange(struct EvaluationContext *eval_ctx,
                                 struct Main *bmain,
                                 Depsgraph *graph,
                                 float ctime,
                                 const int layer);

/* Data changed recalculation entry point.
 * < context_type: context to perform evaluation for
 * < layers: visible layers bitmask to update the graph for
 */
void DEG_evaluate_on_refresh_ex(struct EvaluationContext *eval_ctx,
                                Depsgraph *graph,
                                const int layers);

/* Data changed recalculation entry point.
 * < context_type: context to perform evaluation for
 */
void DEG_evaluate_on_refresh(struct EvaluationContext *eval_ctx,
                             Depsgraph *graph,
                             struct Scene *scene);

bool DEG_needs_eval(Depsgraph *graph);

/* Editors Integration  -------------------------- */

/* Mechanism to allow editors to be informed of depsgraph updates,
 * to do their own updates based on changes.
 */

typedef void (*DEG_EditorUpdateIDCb)(struct Main *bmain, struct ID *id);
typedef void (*DEG_EditorUpdateSceneCb)(struct Main *bmain,
                                        struct Scene *scene,
                                        int updated);
typedef void (*DEG_EditorUpdateScenePreCb)(struct Main *bmain,
                                           struct Scene *scene,
                                           bool time);

/* Set callbacks which are being called when depsgraph changes. */
void DEG_editors_set_update_cb(DEG_EditorUpdateIDCb id_func,
                               DEG_EditorUpdateSceneCb scene_func,
                               DEG_EditorUpdateScenePreCb scene_pre_func);

void DEG_editors_update_pre(struct Main *bmain, struct Scene *scene, bool time);


/* Types of Nodes */
typedef enum eDepsNode_Type {
	DEPSNODE_TYPE_UNDEFINED        = -1,       /* fallback type for invalid return value */

	DEPSNODE_TYPE_OPERATION        = 0,        /* Inner Node (Operation) */

	/* Generic Types */
	DEPSNODE_TYPE_ROOT             = 1,        /* "Current Scene" - basically whatever kicks off the evaluation process */
	DEPSNODE_TYPE_TIMESOURCE       = 2,        /* Time-Source */

	DEPSNODE_TYPE_ID_REF           = 3,        /* ID-Block reference - used as landmarks/collection point for components, but not usually part of main graph */
	DEPSNODE_TYPE_SUBGRAPH         = 4,        /* Isolated sub-graph - used for keeping instanced data separate from instances using them */

	/* Outer Types */
	DEPSNODE_TYPE_PARAMETERS       = 11,       /* Parameters Component - Default when nothing else fits (i.e. just SDNA property setting) */
	DEPSNODE_TYPE_PROXY            = 12,       /* Generic "Proxy-Inherit" Component */   // XXX: Also for instancing of subgraphs?
	DEPSNODE_TYPE_ANIMATION        = 13,       /* Animation Component */                 // XXX: merge in with parameters?
	DEPSNODE_TYPE_TRANSFORM        = 14,       /* Transform Component (Parenting/Constraints) */
	DEPSNODE_TYPE_GEOMETRY         = 15,       /* Geometry Component (DerivedMesh/Displist) */
	DEPSNODE_TYPE_SEQUENCER        = 16,       /* Sequencer Component (Scene Only) */

	/* Evaluation-Related Outer Types (with Subdata) */
	DEPSNODE_TYPE_EVAL_POSE        = 21,       /* Pose Component - Owner/Container of Bones Eval */
	DEPSNODE_TYPE_BONE             = 22,       /* Bone Component - Child/Subcomponent of Pose */

	DEPSNODE_TYPE_EVAL_PARTICLES   = 23,       /* Particle Systems Component */
	DEPSNODE_TYPE_SHADING          = 24,       /* Material Shading Component */
} eDepsNode_Type;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  /* __DEG_DEPSGRAPH_H__ */

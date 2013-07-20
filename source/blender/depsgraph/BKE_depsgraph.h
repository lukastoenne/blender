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

#ifndef __BKE_DEPSGRAPH_H__
#define __BKE_DEPSGRAPH_H__

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

/* Base type for all Node types in Depsgraph */
typedef struct DepsNode DepsNode;

/* Relationship/link (A -> B) type in Depsgraph between Nodes */
typedef struct DepsRelation DepsRelation;

/* ************************************************ */
/* Depsgraph API */

/* CRUD ------------------------------------------- */

// Get main depsgraph instance from context!

/* Create new Depsgraph instance */
// TODO: what args are needed here? What's the building-graph entrypoint?
Depsgraph *DEG_graph_new(void);

/* Free Depsgraph and all its data */
void DEG_free(Depsgraph *graph);

/* Register all node types */
void DEG_register_node_types(void);

/* Update Tagging -------------------------------- */

/* Tag node(s) associated with states such as time and visibility */
void DEG_scene_update_flags(Depsgraph *graph, const bool do_time);
void DEG_on_visible_update(Depsgraph *graph, const bool do_time);

/* Tag node(s) associated with changed data for later updates */
void DEG_id_tag_update(Depsgraph *graph, const ID *id);
void DEG_data_tag_update(Depsgraph *graph, const PointerRNA *ptr);
void DEG_property_tag_update(Depsgraph *graph, const PointerRNA *ptr, const PropertyRNA *prop);

/* Flush updates */
void DEG_scene_flush_update(Depsgraph *graph);

/* ************************************************ */
/* Evaluation Engine API */

/* Frame changed recalculation entrypoint */
void DEG_evaluate_on_framechange(Depsgraph *graph, double ctime);

/* Data changed recalculation entrypoint */
void DEG_evaluate_on_refresh(Depsgraph *graph);

/* ************************************************ */

#endif // __BKE_DEPSGRAPH_H__

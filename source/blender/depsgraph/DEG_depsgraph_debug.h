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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Lukas Toenne
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Public API for Querying and Filtering Depsgraph
 */

#ifndef __DEG_DEPSGRAPH_DEBUG_H__
#define __DEG_DEPSGRAPH_DEBUG_H__

struct Depsgraph;
struct DepsNode;
struct DepsRelation;

#ifdef __cplusplus
extern "C" {
#endif

/* ************************************************ */
/* Graphviz Debugging */

void DEG_debug_graphviz(const struct Depsgraph *graph, FILE *stream);

typedef void (*DEG_DebugBuildCb_NodeAdded)(void *userdata, const struct DepsNode *node);
typedef void (*DEG_DebugBuildCb_RelationAdded)(void *userdata, const struct DepsRelation *rel);

void DEG_debug_build_init(void *userdata, DEG_DebugBuildCb_NodeAdded node_added_cb, DEG_DebugBuildCb_RelationAdded rel_added_cb);
void DEG_debug_build_end(void);

/* ************************************************ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __DEG_DEPSGRAPH_DEBUG_H__

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
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Public API for Depsgraph
 */

#ifndef __DEG_DEPSGRAPH_BUILD_H__
#define __DEG_DEPSGRAPH_BUILD_H__

/* ************************************************* */

/* Dependency Graph */
struct Depsgraph;

/* ------------------------------------------------ */

struct Main;
struct Scene;

struct PointerRNA;
struct PropertyRNA;

#ifdef __cplusplus
extern "C" {
#endif

/* Graph Building -------------------------------- */

#if 0 /* XXX unused, old API functions */
/* Rebuild dependency graph only for a given scene */
void DEG_scene_relations_rebuild(Depsgraph *graph, struct Main *bmain, struct Scene *scene);

/* Create dependency graph if it was cleared or didn't exist yet */
void DEG_scene_relations_update(struct Main *bmain, struct Scene *scene);
#endif

/* Build depsgraph for the given scene, and dump results in given graph container */
void DEG_graph_build_from_scene(Depsgraph *graph, struct Main *bmain, struct Scene *scene);

/* ************************************************ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __DEG_DEPSGRAPH_BUILD_H__

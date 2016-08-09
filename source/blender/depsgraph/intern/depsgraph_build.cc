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
 * Contributor(s): Based on original depsgraph.c code - Blender Foundation (2005-2013)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/depsgraph_build.cc
 *  \ingroup depsgraph
 *
 * Methods for constructing depsgraph.
 */

#include "MEM_guardedalloc.h"

extern "C" {
#include "DNA_cachefile_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "BKE_main.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_debug.h"
#include "DEG_depsgraph_build.h"

} /* extern "C" */

#include "builder/deg_builder.h"
#include "builder/deg_builder_cycle.h"
#include "builder/deg_builder_nodes.h"
#include "builder/deg_builder_relations.h"
#include "builder/deg_builder_transitive.h"

#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"

#include "intern/depsgraph_types.h"
#include "intern/depsgraph_intern.h"

#include "util/deg_util_foreach.h"

/* ****************** */
/* External Build API */

void DEG_add_scene_relation(DepsNodeHandle *handle,
                            struct Scene *scene,
                            eDepsComponent component,
                            const char *description)
{
	if (handle->add_scene_relation)
		handle->add_scene_relation(handle, scene, component, description);
}

void DEG_add_object_relation(struct DepsNodeHandle *handle,
                             struct Object *ob,
                             eDepsComponent component,
                             const char *description)
{
	if (handle->add_object_relation)
		handle->add_object_relation(handle, ob, component, description);
}

void DEG_add_bone_relation(struct DepsNodeHandle *handle,
                           struct Object *ob,
                           const char *bone_name,
                           eDepsComponent component,
                           const char *description)
{
	if (handle->add_bone_relation)
		handle->add_bone_relation(handle, ob, bone_name, component, description);
}

void DEG_add_texture_relation(struct DepsNodeHandle *handle,
                              struct Tex *tex,
                              eDepsComponent component,
                              const char *description)
{
	if (handle->add_texture_relation)
		handle->add_texture_relation(handle, tex, component, description);
}

void DEG_add_nodetree_relation(struct DepsNodeHandle *handle,
                               struct bNodeTree *ntree,
                               eDepsComponent component,
                               const char *description)
{
	if (handle->add_nodetree_relation)
		handle->add_nodetree_relation(handle, ntree, component, description);
}

void DEG_add_image_relation(struct DepsNodeHandle *handle,
                            struct Image *ima,
                            eDepsComponent component,
                            const char *description)
{
	if (handle->add_image_relation)
		handle->add_image_relation(handle, ima, component, description);
}

void DEG_add_object_cache_relation(struct DepsNodeHandle *handle,
                                   struct CacheFile *cache_file,
                                   eDepsComponent component,
                                   const char *description)
{
	if (handle->add_cache_relation)
		handle->add_cache_relation(handle, cache_file, component, description);
}

void DEG_add_special_eval_flag(Depsgraph *graph, ID *id, short flag)
{
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
	if (graph == NULL) {
		BLI_assert(!"Graph should always be valid");
		return;
	}
	DEG::IDDepsNode *id_node = deg_graph->find_id_node(id);
	if (id_node == NULL) {
		BLI_assert(!"ID should always be valid");
		return;
	}
	id_node->eval_flags |= flag;
}

/* ******************** */
/* Graph Building API's */

/* Build depsgraph for the given scene, and dump results in given
 * graph container.
 */
/* XXX: assume that this is called from outside, given the current scene as
 * the "main" scene.
 */
void DEG_graph_build_from_scene(Depsgraph *graph, Main *bmain, Scene *scene)
{
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);

	/* 1) Generate all the nodes in the graph first */
	DEG::DepsgraphNodeBuilder node_builder(bmain, deg_graph);
	/* create root node for scene first
	 * - this way it should be the first in the graph,
	 *   reflecting its role as the entrypoint
	 */
	node_builder.add_root_node();
	node_builder.build_scene(bmain, scene);

	/* 2) Hook up relationships between operations - to determine evaluation
	 *    order.
	 */
	DEG::DepsgraphRelationBuilder relation_builder(deg_graph);
	/* Hook scene up to the root node as entrypoint to graph. */
	/* XXX what does this relation actually mean?
	 * it doesnt add any operations anyway and is not clear what part of the
	 * scene is to be connected.
	 */
#if 0
	relation_builder.add_relation(RootKey(),
	                              IDKey(scene),
	                              DEPSREL_TYPE_ROOT_TO_ACTIVE,
	                              "Root to Active Scene");
#endif
	relation_builder.build_scene(bmain, scene);

	/* Detect and solve cycles. */
	DEG::deg_graph_detect_cycles(deg_graph);

	/* 3) Simplify the graph by removing redundant relations (to optimize
	 *    traversal later). */
	/* TODO: it would be useful to have an option to disable this in cases where
	 *       it is causing trouble.
	 */
	if (G.debug_value == 799) {
		DEG::deg_graph_transitive_reduction(deg_graph);
	}

	/* 4) Flush visibility layer and re-schedule nodes for update. */
	DEG::deg_graph_build_finalize(deg_graph);

#if 0
	if (!DEG_debug_consistency_check(deg_graph)) {
		printf("Consistency validation failed, ABORTING!\n");
		abort();
	}
#endif
}

/* Tag graph relations for update. */
void DEG_graph_tag_relations_update(Depsgraph *graph)
{
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
	deg_graph->need_update = true;
}

/* Tag all relations for update. */
void DEG_relations_tag_update(Main *bmain)
{
	for (Scene *scene = (Scene *)bmain->scene.first;
	     scene != NULL;
	     scene = (Scene *)scene->id.next)
	{
		if (scene->depsgraph != NULL) {
			DEG_graph_tag_relations_update(scene->depsgraph);
		}
	}
}

/* Create new graph if didn't exist yet,
 * or update relations if graph was tagged for update.
 */
void DEG_scene_relations_update(Main *bmain, Scene *scene)
{
	if (scene->depsgraph == NULL) {
		/* Rebuild graph from scratch and exit. */
		scene->depsgraph = DEG_graph_new();
		DEG_graph_build_from_scene(scene->depsgraph, bmain, scene);
		return;
	}

	DEG::Depsgraph *graph = reinterpret_cast<DEG::Depsgraph *>(scene->depsgraph);
	if (!graph->need_update) {
		/* Graph is up to date, nothing to do. */
		return;
	}

	/* Clear all previous nodes and operations. */
	graph->clear_all_nodes();
	graph->operations.clear();
	BLI_gset_clear(graph->entry_tags, NULL);

	/* Build new nodes and relations. */
	DEG_graph_build_from_scene(reinterpret_cast< ::Depsgraph * >(graph),
	                           bmain,
	                           scene);

	graph->need_update = false;
}

/* Rebuild dependency graph only for a given scene. */
void DEG_scene_relations_rebuild(Main *bmain, Scene *scene)
{
	if (scene->depsgraph != NULL) {
		DEG_graph_tag_relations_update(scene->depsgraph);
	}
	DEG_scene_relations_update(bmain, scene);
}

void DEG_scene_graph_free(Scene *scene)
{
	if (scene->depsgraph) {
		DEG_graph_free(scene->depsgraph);
		scene->depsgraph = NULL;
	}
}

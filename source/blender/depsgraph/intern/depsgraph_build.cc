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
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "BKE_depsgraph.h"
#include "BKE_library.h"
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

static DEG::eDepsNode_Type deg_build_scene_component_type(
        eDepsSceneComponentType component)
{
	switch (component) {
		case DEG_SCENE_COMP_PARAMETERS:     return DEG::DEPSNODE_TYPE_PARAMETERS;
		case DEG_SCENE_COMP_ANIMATION:      return DEG::DEPSNODE_TYPE_ANIMATION;
		case DEG_SCENE_COMP_SEQUENCER:      return DEG::DEPSNODE_TYPE_SEQUENCER;
	}
	return DEG::DEPSNODE_TYPE_UNDEFINED;
}

static DEG::eDepsNode_Type deg_build_object_component_type(
        eDepsObjectComponentType component)
{
	switch (component) {
		case DEG_OB_COMP_PARAMETERS:        return DEG::DEPSNODE_TYPE_PARAMETERS;
		case DEG_OB_COMP_PROXY:             return DEG::DEPSNODE_TYPE_PROXY;
		case DEG_OB_COMP_ANIMATION:         return DEG::DEPSNODE_TYPE_ANIMATION;
		case DEG_OB_COMP_TRANSFORM:         return DEG::DEPSNODE_TYPE_TRANSFORM;
		case DEG_OB_COMP_GEOMETRY:          return DEG::DEPSNODE_TYPE_GEOMETRY;
		case DEG_OB_COMP_EVAL_POSE:         return DEG::DEPSNODE_TYPE_EVAL_POSE;
		case DEG_OB_COMP_BONE:              return DEG::DEPSNODE_TYPE_BONE;
		case DEG_OB_COMP_EVAL_PARTICLES:    return DEG::DEPSNODE_TYPE_EVAL_PARTICLES;
		case DEG_OB_COMP_SHADING:           return DEG::DEPSNODE_TYPE_SHADING;
	}
	return DEG::DEPSNODE_TYPE_UNDEFINED;
}

static DEG::DepsNodeHandle *get_handle(DepsNodeHandle *handle)
{
	return reinterpret_cast<DEG::DepsNodeHandle *>(handle);
}

void DEG_add_scene_relation(DepsNodeHandle *handle,
                            Scene *scene,
                            eDepsSceneComponentType component,
                            const char *description)
{
	DEG::DepsNodeHandle *deg_handle = get_handle(handle);
	DEG::eDepsNode_Type type = deg_build_scene_component_type(component);
#ifdef DEG_OLD_BUILDERS
	DEG::ComponentKey comp_key(&scene->id, type);
	deg_handle->builder->add_node_handle_relation(comp_key,
	                                              deg_handle,
	                                              DEG::DEPSREL_TYPE_GEOMETRY_EVAL,
	                                              description);
#else
	deg_handle->builder->add_id_dependency(DEG::DEPSREL_TYPE_STANDARD,
	                                       description,
	                                       &scene->id,
	                                       type);
#endif
}

void DEG_add_object_relation(DepsNodeHandle *handle,
                             Object *ob,
                             eDepsObjectComponentType component,
                             const char *description)
{
	DEG::DepsNodeHandle *deg_handle = get_handle(handle);
	DEG::eDepsNode_Type type = deg_build_object_component_type(component);
#ifdef DEG_OLD_BUILDERS
	DEG::ComponentKey comp_key(&ob->id, type);
	deg_handle->builder->add_node_handle_relation(comp_key,
	                                              deg_handle,
	                                              DEG::DEPSREL_TYPE_GEOMETRY_EVAL,
	                                              description);
#else
	deg_handle->builder->add_id_dependency(DEG::DEPSREL_TYPE_STANDARD,
	                                       description,
	                                       &ob->id,
	                                       type);
#endif
}
void DEG_add_bone_relation(DepsNodeHandle *handle,
                           Object *ob,
                           const char *bone_name,
                           eDepsObjectComponentType component,
                           const char *description)
{
	DEG::DepsNodeHandle *deg_handle = get_handle(handle);
	DEG::eDepsNode_Type type = deg_build_object_component_type(component);
#ifdef DEG_OLD_BUILDERS
	DEG::ComponentKey comp_key(&ob->id, type, bone_name);
	/* XXX: "Geometry Eval" might not always be true, but this only gets called
	 * from modifier building now.
	 */
	deg_handle->builder->add_node_handle_relation(comp_key,
	                                              deg_handle,
	                                              DEG::DEPSREL_TYPE_GEOMETRY_EVAL,
	                                              description);
#else
	deg_handle->builder->add_id_dependency(DEG::DEPSREL_TYPE_STANDARD,
	                                       description,
	                                       &ob->id,
	                                       type,
	                                       bone_name);
#endif
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

namespace DEG {

DepsgraphBuilder::DepsgraphBuilder(Depsgraph *graph) :
    m_graph(graph)
{
}

DepsgraphBuilder::~DepsgraphBuilder()
{}

bool DepsgraphBuilder::has_id(ID *id) const
{
	IDDepsNode *idnode = m_graph->find_id_node(id);
	/* XXX We consider ID nodes finished only when they are tagged as well.
	 * Currently this is redundant, because nodes are tagged "done"
	 * as soon as they are created ...
	 */
	return (idnode && idnode->id->tag & LIB_TAG_DOIT);
}

void DepsgraphBuilder::add_id(ID *id)
{
	m_graph->add_id_node(id);
}

void DepsgraphBuilder::add_time_source()
{
	/* root-node */
	RootDepsNode *root_node = m_graph->root_node;
	if (root_node) {
		root_node->add_time_source("Time Source");
	}
}

void DepsgraphBuilder::add_subgraph(Depsgraph *subgraph, ID *id)
{
	/* create a node for representing subgraph */
	SubgraphDepsNode *subgraph_node = m_graph->add_subgraph_node(id);
	subgraph_node->graph = subgraph;

	/* make a copy of the data this node will need? */
	// XXX: do we do this now, or later?
	// TODO: need API function which queries graph's ID's hash, and duplicates those blocks thoroughly with all outside links removed...
}

/* ------------------------------------------------------------------------- */

IDNodeBuilder::IDNodeBuilder(Depsgraph *graph, ID *id) :
    m_graph(graph),
    m_idnode(NULL)
{
	m_idnode = m_graph->add_id_node(id);
}

IDNodeBuilder::IDNodeBuilder(const DepsgraphBuilder &context, ID *id) :
    m_graph(context.graph())
{
	m_idnode = m_graph->add_id_node(id);
}

IDNodeBuilder::IDNodeBuilder(const IDNodeBuilder &other, ID *id) :
    m_graph(other.m_graph)
{
	m_idnode = m_graph->add_id_node(id);
}

IDNodeBuilder::~IDNodeBuilder()
{}

void IDNodeBuilder::add_time_source()
{
#if 0 /* XXX TODO */
	/* depends on what this is... */
	switch (GS(m_id->name)) {
		case ID_SCE: /* Scene - Usually sequencer strip causing time remapping... */
		{
			// TODO...
		}
			break;
			
		case ID_GR: /* Group */
		{
			// TODO...
		}
			break;
			
			// XXX: time source...
			
		default:     /* Unhandled */
			printf("%s(): Unhandled ID - %s \n", __func__, m_id->name);
			break;
	}
#endif
}

void IDNodeBuilder::set_layers(int layers)
{
	m_idnode->layers = layers;
}

void IDNodeBuilder::set_need_curve_path()
{
	m_idnode->eval_flags |= DAG_EVAL_NEED_CURVE_PATH;
}

/* ------------------------------------------------------------------------- */

static void free_dependency(void *val)
{
	Dependency *dep = reinterpret_cast<Dependency*>(val);
	delete dep;
}

static unsigned int dependency_hash(const void *ptr)
{
	const Dependency *dep = reinterpret_cast<const Dependency*>(ptr);
	return BLI_ghashutil_ptrhash(dep->id) ^ BLI_ghashutil_inthash(dep->component) ^ BLI_ghashutil_strhash(dep->component_name.c_str());
}

static bool dependency_cmp(const void *a, const void *b)
{
	const Dependency *dep_a = reinterpret_cast<const Dependency*>(a);
	const Dependency *dep_b = reinterpret_cast<const Dependency*>(b);
	return (dep_a->id != dep_b->id) ||
	        (dep_a->component != dep_b->component) ||
	        dep_a->component_name != dep_b->component_name;
}

ComponentBuilder::ComponentBuilder(Depsgraph *graph, IDDepsNode *idnode,
                                   eDepsNode_Type component, const string &name) :
    m_graph(graph),
    m_component(NULL)
{
	m_component = idnode->add_component(component, name);
}

ComponentBuilder::ComponentBuilder(const IDNodeBuilder &context,
                                   eDepsNode_Type component, const string &name) :
    m_graph(context.graph()),
    m_component(NULL)
{
	m_dependencies = BLI_ghash_new(dependency_hash, dependency_cmp, "ComponentBuilder dependencies");
	
	m_component = context.idnode()->add_component(component, name);
}

ComponentBuilder::~ComponentBuilder()
{
	BLI_ghash_free(m_dependencies, NULL, free_dependency);
}

bool ComponentBuilder::has_operation(eDepsOperation_Code opcode,
                                     const string &name)
{
	return m_component->has_operation(opcode, name);
}

Operation ComponentBuilder::define_operation(eDepsOperation_Type optype,
                                             DepsEvalOperationCb op,
                                             eDepsOperation_Code opcode,
                                             const string &description)
{
	OperationDepsNode *opnode = m_component->has_operation(opcode, description);
	if (opnode == NULL) {
		opnode = m_component->add_operation(optype, op, opcode, description);
		m_graph->operations.push_back(opnode);
	}
	else {
		fprintf(stderr, "add_operation: Operation already exists - %s has %s at %p\n",
		        m_component->identifier().c_str(),
		        opnode->identifier().c_str(),
		        opnode);
		BLI_assert(!"Should not happen!");
	}
	
	return opnode;
}

void ComponentBuilder::add_relation(eDepsRelation_Type type, const string &description,
                                    Operation from, Operation to)
{
	if (from && to) {
		m_graph->add_new_relation(from, to, type, description.c_str());
	}
	else {
		DEG_DEBUG_PRINTF("add_relation(%p = %s, %p = %s, %d, %s) Failed\n",
		                 from, (from) ? from->identifier().c_str() : "<None>",
		                 to,   (to)   ? to->identifier().c_str() : "<None>",
		                 type, description.c_str());
	}
}

void ComponentBuilder::add_dependency(eDepsRelation_Type type, const string &description,
                                      eDepsNode_Type component, const string &component_name)
{
	ID *id = m_component->owner->id;
	/* does nothing if same dependency exists already */
	Dependency *dep = new Dependency(type, description, id, component, component_name);
	BLI_ghash_insert(m_dependencies, dep, dep);
}

void ComponentBuilder::add_id_dependency(eDepsRelation_Type type, const string &description,
                                         ID *id, eDepsNode_Type component, const string &component_name)
{
	Dependency *dep = new Dependency(type, description, id, component, component_name);
	BLI_ghash_insert(m_dependencies, dep, dep);
}

void ComponentBuilder::set_entry_operation(const Operation &op)
{
	m_component->entry_operation = op;
}

void ComponentBuilder::set_exit_operation(const Operation &op)
{
	m_component->exit_operation = op;
}

void ComponentBuilder::set_operation_uses_python(const Operation &op)
{
	op->flag |= DEPSOP_FLAG_USES_PYTHON;
}

}  // namespace DEG

/* ********************** */
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

#ifdef DEG_OLD_BUILDERS
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
#else
	/* LIB_TAG_DOIT is used to indicate whether node for given ID was already
	 * created or not. This flag is being set in add_id_node(), so functions
	 * shouldn't bother with setting it, they only might query this flag when
	 * needed.
	 */
	BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);
	
	/* create root node for scene first
	 * - this way it should be the first in the graph,
	 *   reflecting its role as the entrypoint
	 */
	deg_graph->add_root_node();
	
	/* add ID nodes and components, define operations and declare dependencies */
	DEG::DepsgraphBuilder builder(deg_graph);
	deg_build_scene(builder, scene);
	
	/* TODO lower ID/component dependencies to operation relations */
#endif

	/* Detect and solve cycles. */
	DEG::deg_graph_detect_cycles(deg_graph);

	/* Simplify the graph by removing redundant relations (to optimize traversal later). */
	/* TODO: it would be useful to have an option to disable this in cases where
	 *       it is causing trouble.
	 */
	if (G.debug_value == 799) {
		DEG::deg_graph_transitive_reduction(deg_graph);
	}

	/* Flush visibility layer and re-schedule nodes for update. */
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

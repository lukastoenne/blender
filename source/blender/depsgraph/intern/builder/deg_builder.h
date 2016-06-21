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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Sergey Sharybin
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/builder/deg_builder.h
 *  \ingroup depsgraph
 */

#pragma once

#include "intern/depsgraph_types.h"

struct Base;
struct bGPdata;
struct ListBase;
struct GHash;
struct ID;
struct FCurve;
struct Group;
struct Key;
struct Main;
struct Material;
struct MTex;
struct bNodeTree;
struct Object;
struct bPoseChannel;
struct bConstraint;
struct Scene;
struct Tex;
struct World;
struct ParticleSystem;
struct PropertyRNA;

namespace DEG {

struct Depsgraph;
struct DepsNode;
struct DepsNodeHandle;
struct RootDepsNode;
struct SubgraphDepsNode;
struct IDDepsNode;
struct TimeSourceDepsNode;
struct ComponentDepsNode;
struct OperationDepsNode;
struct RootPChanMap;

/* Get unique identifier for FCurves and Drivers */
string deg_fcurve_id_name(const FCurve *fcu);

void deg_graph_build_finalize(struct Depsgraph *graph);

/* ------------------------------------------------------------------------- */

typedef OperationDepsNode* Operation;

/** Component key, used to cache dependencies during building.
 */
struct Dependency {
	Dependency(eDepsRelation_Type type, const string &description,
	           ID *id, eDepsNode_Type component, const string &component_name) :
	    id(id), component(component), component_name(component_name),
	    type(type), description(description)
	{}
	
	bool operator == (const Dependency &other) const
	{
		return (id == other.id && component == other.component && component_name == other.component_name);
	}
	
	ID *id;
	eDepsNode_Type component;
	string component_name;
	
	eDepsRelation_Type type;
	const string &description;
};

/* ------------------------------------------------------------------------- */

class DepsgraphBuilder {
	Depsgraph *m_graph;
	
public:
	DepsgraphBuilder(Depsgraph *graph);
	~DepsgraphBuilder();
	
	Depsgraph *graph() const { return m_graph; }
	
	/* True if nodes for the ID block have already been built */
	bool has_id(ID *id) const;
	
	void add_id(ID *id);
	void add_time_source();
	void add_subgraph(Depsgraph *subgraph, ID *id);
};

/* ------------------------------------------------------------------------- */

class IDNodeBuilder {
	Depsgraph *m_graph;
	IDDepsNode *m_idnode;
	
public:
	IDNodeBuilder(Depsgraph *graph, ID *id);
	IDNodeBuilder(const DepsgraphBuilder &context, ID *id);
	IDNodeBuilder(const IDNodeBuilder &other, ID *id);
	~IDNodeBuilder();
	
	Depsgraph *graph() const { return m_graph; }
	IDDepsNode *idnode() const { return m_idnode; }
	
	void add_time_source();
	
	/* XXX hackish utility functions */
	void set_layers(int layers);
	void set_need_curve_path();
};

/* ------------------------------------------------------------------------- */

/**
 * Utility class for defining operations belonging to a specific component.
 */
class ComponentBuilder {
	Depsgraph *m_graph;
	ComponentDepsNode *m_component;
	
	typedef struct GHash *DependencySet;
	
	DependencySet m_dependencies;
	
public:
	ComponentBuilder(Depsgraph *graph, IDDepsNode *idnode,
	                 eDepsNode_Type component, const string &name = "");
	ComponentBuilder(const IDNodeBuilder &context,
	                 eDepsNode_Type component, const string &name = "");
	~ComponentBuilder();
	
	/* True if an operation for this opcode and name has already been defined */
	bool has_operation(eDepsOperation_Code opcode,
	                   const string &name = "");
	
	/* Define an operation to execute when the component is updated. */
	Operation define_operation(eDepsOperation_Type optype,
	                           DepsEvalOperationCb op,
	                           eDepsOperation_Code opcode,
	                           const string &description = "");
	
	/* Add an internal dependency between operations of this component. */
	void add_relation(eDepsRelation_Type type, const char *description,
	                  Operation from, Operation to);
	
	const DependencySet &dependencies() const { return m_dependencies; }
	
	/* Add a dependency on another component of the same ID block. */
	void add_dependency(eDepsRelation_Type type, const string &description,
	                    eDepsNode_Type component, const string &component_name = "");
	/* Add a dependency on a component of another ID block. */
	void add_id_dependency(eDepsRelation_Type type, const string &description,
	                       ID *id, eDepsNode_Type component, const string &component_name = "");
	
	/* XXX hackish utility functions */
	void set_entry_operation(const Operation &op);
	void set_exit_operation(const Operation &op);
	void set_operation_uses_python(const Operation &op);
};

void deg_build_scene(DepsgraphBuilder &context, Scene *scene);
void deg_build_subgraph(DepsgraphBuilder &context, Group *group);
void deg_build_group(DepsgraphBuilder &context, Scene *scene, Base *base, Group *group);
void deg_build_object(DepsgraphBuilder &context, Scene *scene, Base *base, Object *ob);
void deg_build_object_transform(IDNodeBuilder &context, Scene *scene, Object *ob);
Operation deg_build_object_constraints(IDNodeBuilder &context, Scene *scene, Object *ob);
Operation deg_build_pose_constraints(IDNodeBuilder &context, Object *ob, bPoseChannel *pchan);
void deg_build_rigidbody(IDNodeBuilder &context, Scene *scene);
void deg_build_particles(IDNodeBuilder &context, Scene *scene, Object *ob);
void deg_build_animdata(IDNodeBuilder &context, ID *id);
void deg_build_driver(IDNodeBuilder &context, ID *id, FCurve *fcurve);
void deg_build_ik_pose(ComponentBuilder &pose_builder, Scene *scene, Object *ob,
                       bPoseChannel *pchan, bConstraint *con);
void deg_build_splineik_pose(ComponentBuilder &pose_builder, Scene *scene, Object *ob,
                             bPoseChannel *pchan, bConstraint *con);
void deg_build_rig(IDNodeBuilder &context, Scene *scene, Object *ob);
void deg_build_proxy_rig(IDNodeBuilder &context, Object *ob);
void deg_build_shapekeys(DepsgraphBuilder &context, Key *key);
void deg_build_obdata(DepsgraphBuilder &context, Scene *scene, Object *ob);
void deg_build_object_geom(IDNodeBuilder &context, Scene *scene, Object *ob);
void deg_build_camera(DepsgraphBuilder &context, Object *ob);
void deg_build_lamp(DepsgraphBuilder &context, Object *ob);
void deg_build_nodetree(DepsgraphBuilder &context, ID *owner, bNodeTree *ntree);
void deg_build_material(DepsgraphBuilder &context, ID *owner, Material *ma);
void deg_build_texture(DepsgraphBuilder &context, ID *owner, Tex *tex);
void deg_build_texture_stack(DepsgraphBuilder &context, ID *owner, MTex **texture_stack);
void deg_build_world(DepsgraphBuilder &context, World *world);
void deg_build_compositor(DepsgraphBuilder &context, Scene *scene);
void deg_build_gpencil(DepsgraphBuilder &context, bGPdata *gpd);

struct DepsNodeHandle
{
	DepsNodeHandle(ComponentBuilder &builder) :
	    builder(&builder)
	{
	}

	ComponentBuilder *builder;
};

}  // namespace DEG

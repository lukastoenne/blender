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
 * Datatypes for internal use in the Depsgraph
 * 
 * All of these datatypes are only really used within the "core" depsgraph.
 * In particular, node types declared here form the structure of operations
 * in the graph.
 */

#ifndef __DEPSNODE_OPERATION_H__
#define __DEPSNODE_OPERATION_H__

#include "MEM_guardedalloc.h"

extern "C" {
#include "RNA_access.h"
}

#include "depsnode.h"

#include "depsgraph_util_map.h"
#include "depsgraph_util_set.h"

struct ID;

struct Depsgraph;
struct DepsgraphCopyContext;

/* Atomic Operation - Base type for all operations */
struct OperationDepsNode : public DepsNode {
	void remove_from_graph(Depsgraph *graph);
	
	ComponentDepsNode *owner;     /* component that contains the operation */
	
	DepsEvalOperationCb evaluate; /* callback for operation */
	
	PointerRNA ptr;               /* item that operation is to be performed on (optional) */
	
	double start_time;            /* (secs) last timestamp (in seconds) when operation was started */
	double last_time;             /* (seconds) time in seconds that last evaluation took */
	
	short optype;                 /* (eDepsOperation_Type) stage of evaluation */
	short flag;                   /* (eDepsOperation_Flag) extra settings affecting evaluation */
	
protected:
	void add_to_component_node(Depsgraph *graph, const ID *id, eDepsNode_Type component_type);
};

/* Extra flags affecting operations */
typedef enum eDepsOperation_Flag {
	DEPSOP_FLAG_USES_PYTHON   = (1 << 0),  /* Operation is evaluated using CPython; has GIL and security implications... */      
} eDepsOperation_Flag;

struct ParametersOperationDepsNode : public OperationDepsNode {
	void add_to_graph(Depsgraph *graph, const ID *id);
	
	DEG_DEPSNODE_DECLARE;
};

struct AnimationOperationDepsNode : public OperationDepsNode {
	void add_to_graph(Depsgraph *graph, const ID *id);
	
	DEG_DEPSNODE_DECLARE;
};

struct ProxyOperationDepsNode : public OperationDepsNode {
	void add_to_graph(Depsgraph *graph, const ID *id);
	
	DEG_DEPSNODE_DECLARE;
};

struct TransformOperationDepsNode : public OperationDepsNode {
	void add_to_graph(Depsgraph *graph, const ID *id);
	
	DEG_DEPSNODE_DECLARE;
};

struct GeometryOperationDepsNode : public OperationDepsNode {
	void add_to_graph(Depsgraph *graph, const ID *id);
	
	DEG_DEPSNODE_DECLARE;
};

struct SequencerOperationDepsNode : public OperationDepsNode {
	void add_to_graph(Depsgraph *graph, const ID *id);
	
	DEG_DEPSNODE_DECLARE;
};

struct UpdateOperationDepsNode : public OperationDepsNode {
	void add_to_graph(Depsgraph *graph, const ID *id);
	
	DEG_DEPSNODE_DECLARE;
};

struct DriverOperationDepsNode : public OperationDepsNode {
	void add_to_graph(Depsgraph *graph, const ID *id);
	
	DEG_DEPSNODE_DECLARE;
};

struct PoseOperationDepsNode : public OperationDepsNode {
	void add_to_graph(Depsgraph *graph, const ID *id);
	
	DEG_DEPSNODE_DECLARE;
};

struct BoneOperationDepsNode : public OperationDepsNode {
	void init(const ID *id, const string &subdata);
	void add_to_graph(Depsgraph *graph, const ID *id);
	
	DEG_DEPSNODE_DECLARE;
};

struct ParticlesOperationDepsNode : public OperationDepsNode {
	void add_to_graph(Depsgraph *graph, const ID *id);
	void remove_from_graph(Depsgraph *graph);
	
	DEG_DEPSNODE_DECLARE;
};

struct RigidBodyOperationDepsNode : public OperationDepsNode {
	void add_to_graph(Depsgraph *graph, const ID *id);
	
	DEG_DEPSNODE_DECLARE;
};

void DEG_register_operation_depsnodes();

#endif // __DEPSNODE_OPERATION_H__

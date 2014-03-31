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

#ifndef __DEPSNODE_H__
#define __DEPSNODE_H__

#include "MEM_guardedalloc.h"

#include "depsgraph_types.h"

#include "depsgraph_util_map.h"
#include "depsgraph_util_set.h"
#include "depsgraph_util_string.h"

struct ID;
struct Scene;

struct Depsgraph;
struct DepsRelation;
struct DepsgraphCopyContext;

/* ************************************* */
/* Base-Defines for Nodes in Depsgraph */

/* "Colors" for use in depsgraph topology algorithms */
typedef enum eDepsNode_Color {
	DEPSNODE_WHITE = 0,
	DEPSNODE_GRAY  = 1,
	DEPSNODE_BLACK = 2
} eDepsNode_Color;

/* Flags for Depsgraph Nodes */
typedef enum eDepsNode_Flag {
	/* node needs to be updated */
	DEPSNODE_FLAG_NEEDS_UPDATE       = (1 << 0),
	
	/* node was directly modified, causing need for update */
	/* XXX: intention is to make it easier to tell when we just need to take subgraphs */
	DEPSNODE_FLAG_DIRECTLY_MODIFIED  = (1 << 1),
	
	/* node was visited/handled already in traversal... */
	DEPSNODE_FLAG_TEMP_TAG           = (1 << 2),
} eDepsNode_Flag;

/* All nodes in Depsgraph are descended from this */
struct DepsNode {
	/* Helper class for static typeinfo in subclasses */
	struct TypeInfo {
		TypeInfo(eDepsNode_Type type, const string &tname);
		
		eDepsNode_Type type;
		eDepsNode_Class tclass;
		string tname;
	};
	
	typedef unordered_set<DepsRelation *> Relations;
	
	string name;                /* identifier - mainly for debugging purposes... */
	
	Relations inlinks;          /* nodes which this one depends on */
	Relations outlinks;         /* nodes which depend on this one */
	
	eDepsNode_Type type;        /* structural type of node */
	eDepsNode_Class tclass;     /* type of data/behaviour represented by node... */
	
	eDepsNode_Color color;      /* stuff for tagging nodes (for algorithmic purposes) */
	short flag;                 /* (eDepsNode_Flag) dirty/visited tags */
	
	uint32_t num_links_pending; /* how many inlinks are we still waiting on before we can be evaluated... */
	int lasttime;               /* for keeping track of whether node has been evaluated yet, without performing full purge of flags first */

public:
	DepsNode();
	virtual ~DepsNode();
	
	virtual void init(const ID *id, const string &subdata) {}
	virtual void copy(DepsgraphCopyContext *dcc, const DepsNode *src) {}
	
	/* Add node to graph - Will add additional inbetween nodes as needed
	 * < (id): ID-Block that node is associated with (if applicable)
	 */
	virtual void add_to_graph(Depsgraph *graph, const ID *id) = 0;
	/* Remove node from graph - Only use when node is to be replaced... */
	virtual void remove_from_graph(Depsgraph *graph) = 0;
	
	/* Recursively ensure that all implicit/builtin link rules have been applied */
	/* i.e. init()/cleanup() callbacks as last items for components + component ordering rules obeyed */
	virtual void validate_links(Depsgraph *graph) {}
	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("DEG:DepsNode")
#endif
};

/* Macros for common static typeinfo */
#define DEG_DEPSNODE_DECLARE \
	static const DepsNode::TypeInfo typeinfo
#define DEG_DEPSNODE_DEFINE(NodeType, type_, tname_) \
	const DepsNode::TypeInfo NodeType::typeinfo = DepsNode::TypeInfo(type_, tname_)


/* Generic Nodes ======================= */

struct ComponentDepsNode;

/* Time Source Node */
struct TimeSourceDepsNode : public DepsNode {
	void add_to_graph(Depsgraph *graph, const ID *id);
	void remove_from_graph(Depsgraph *graph);
	
	// XXX: how do we keep track of the chain of time sources for propagation of delays?
	
	double cfra;                    /* new "current time" */
	double offset;                  /* time-offset relative to the "official" time source that this one has */
	
	DEG_DEPSNODE_DECLARE;
};

/* Root Node */
struct RootDepsNode : public DepsNode {
	void add_to_graph(Depsgraph *graph, const ID *id);
	void remove_from_graph(Depsgraph *graph);
	
	struct Scene *scene;             /* scene that this corresponds to */
	TimeSourceDepsNode *time_source; /* entrypoint node for time-changed */
	
	DEG_DEPSNODE_DECLARE;
};

/* ID-Block Reference */
struct IDDepsNode : public DepsNode {
	typedef unordered_map<eDepsNode_Type, ComponentDepsNode *, hash<int> > ComponentMap;
	
	void init(const ID *id, const string &subdata);
	void copy(DepsgraphCopyContext *dcc, const IDDepsNode *src);
	~IDDepsNode();
	
	void add_to_graph(Depsgraph *graph, const ID *id);
	void remove_from_graph(Depsgraph *graph);
	
	void validate_links(Depsgraph *graph);
	
	ComponentDepsNode *find_component(eDepsNode_Type type) const;
	
	struct ID *id;                  /* ID Block referenced */
	ComponentMap components;        /* hash to make it faster to look up components */
	
	DEG_DEPSNODE_DECLARE;
};

/* Subgraph Reference */
struct SubgraphDepsNode : public DepsNode {
	void init(const ID *id, const string &subdata);
	void copy(DepsgraphCopyContext *dcc, const SubgraphDepsNode *src);
	~SubgraphDepsNode();
	
	void add_to_graph(Depsgraph *graph, const ID *id);
	void remove_from_graph(Depsgraph *graph);
	
	void validate_links(Depsgraph *graph);
	
	Depsgraph *graph;        /* instanced graph */
	struct ID *root_id;      /* ID-block at root of subgraph (if applicable) */
	
	size_t num_users;        /* number of nodes which use/reference this subgraph - if just 1, it may be possible to merge into main */
	int flag;                /* (eSubgraphRef_Flag) assorted settings for subgraph node */
	
	DEG_DEPSNODE_DECLARE;
};

/* Flags for subgraph node */
typedef enum eSubgraphRef_Flag {
	SUBGRAPH_FLAG_SHARED      = (1 << 0),   /* subgraph referenced is shared with another reference, so shouldn't free on exit */
	SUBGRAPH_FLAG_FIRSTREF    = (1 << 1),   /* node is first reference to subgraph, so it can be freed when we are removed */
} eSubgraphRef_Flag;

/* ************************************* */

void DEG_register_base_depsnodes();

#endif // __DEPSNODE_H__

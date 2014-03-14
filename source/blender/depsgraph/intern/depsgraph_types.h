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

#ifndef __DEPSGRAPH_TYPES_H__
#define __DEPSGRAPH_TYPES_H__

struct Scene;
struct bPoseChannel;

/* Maximum length of identifier names used in Depsgraph */
#define DEG_MAX_ID_NAME     128

/* ************************************* */
/* Relationships Between Nodes */

/* B depends on A (A -> B) */
struct DepsRelation {
	DepsRelation *next, *prev;
	
	/* the nodes in the relationship (since this is shared between the nodes) */
	DepsNode *from;               /* A */
	DepsNode *to;                 /* B */
	
	/* relationship attributes */
	char name[DEG_MAX_ID_NAME];   /* label for debugging */
	
	int type;                     /* (eDepsRelation_Type) */
	int flag;                     /* (eDepsRelation_Flag) */
};


/* Types of relationships between nodes 
 *
 * This is used to provide additional hints to use when filtering
 * the graph, so that we can go without doing more extensive
 * data-level checks...
 */
typedef enum eDepsRelation_Type {
	/* reationship type unknown/irrelevant */
	DEPSREL_TYPE_STANDARD = 0,
	
	/* root -> active scene or entity (screen, image, etc.) */
	DEPSREL_TYPE_ROOT_TO_ACTIVE,
	
	/* general datablock dependency */
	DEPSREL_TYPE_DATABLOCK,
	
	/* time dependency */
	DEPSREL_TYPE_TIME,
	
	/* component depends on results of another */
	DEPSREL_TYPE_COMPONENT_ORDER,
	
	/* relationship is just used to enforce ordering of operations
	 * (e.g. "init()" callback done before "exec() and "cleanup()")
	 */
	DEPSREL_TYPE_OPERATION,
	
	/* relationship results from a property driver affecting property */
	DEPSREL_TYPE_DRIVER,
	
	/* relationship is something driver depends on */
	DEPSREL_TYPE_DRIVER_TARGET,
	
	/* relationship is used for transform stack 
	 * (e.g. parenting, user transforms, constraints)
	 */
	DEPSREL_TYPE_TRANSFORM,
	
	/* relationship is used for geometry evaluation 
	 * (e.g. metaball "motherball" or modifiers)
	 */
	DEPSREL_TYPE_GEOMETRY_EVAL,
	
	/* relationship is used to trigger a post-change validity updates */
	DEPSREL_TYPE_UPDATE,
	
	/* relationship is used to trigger editor/screen updates */
	DEPSREL_TYPE_UPDATE_UI,
} eDepsRelation_Type;


/* Settings/Tags on Relationship */
typedef enum eDepsRelation_Flag {
	/* "touched" tag is used when filtering, to know which to collect */
	DEPSREL_FLAG_TEMP_TAG   = (1 << 0),
	
	/* "cyclic" link - when detecting cycles, this relationship was the one
	 * which triggers a cyclic relationship to exist in the graph
	 */
	DEPSREL_FLAG_CYCLIC     = (1 << 1),
} eDepsRelation_Flag;

/* ************************************* */
/* Base-Defines for Nodes in Depsgraph */

/* All nodes in Depsgraph are descended from this */
struct DepsNode {
	DepsNode *next, *prev;		/* linked-list of siblings (from same parent node) */
	DepsNode *owner;            /* mainly for inner-nodes to see which outer/data node they came from */
	
	char name[DEG_MAX_ID_NAME]; /* identifier - mainly for debugging purposes... */
	
	ListBase inlinks;           /* (LinkData : DepsRelation) nodes which this one depends on */
	ListBase outlinks;          /* (LinkData : DepsRelation) nodes which depend on this one */
	
	short type;                 /* (eDepsNode_Type) structural type of node */
	short class;                /* (eDepsNode_Class) type of data/behaviour represented by node... */
	
	short  color;               /* (eDepsNode_Color) stuff for tagging nodes (for algorithmic purposes) */
	short  flag;                /* (eDepsNode_Flag) dirty/visited tags */
	
	uint32_t num_links_pending; /* how many inlinks are we still waiting on before we can be evaluated... */
	int lasttime;               /* for keeping track of whether node has been evaluated yet, without performing full purge of flags first */
};

/* Metatype of Nodes - The general "level" in the graph structure the node serves */
typedef enum eDepsNode_Class {
	DEPSNODE_CLASS_GENERIC         = 0,        /* Types generally unassociated with user-visible entities, but needed for graph functioning */
	
	DEPSNODE_CLASS_COMPONENT       = 1,        /* [Outer Node] An "aspect" of evaluating/updating an ID-Block, requiring certain types of evaluation behaviours */    
	DEPSNODE_CLASS_OPERATION       = 2,        /* [Inner Node] A glorified function-pointer/callback for scheduling up evaluation operations for components, subject to relationship requirements */
} eDepsNode_Class;

/* Types of Nodes */
typedef enum eDepsNode_Type {
	/* Generic Types */
	DEPSNODE_TYPE_ROOT             = 0,        /* "Current Scene" - basically whatever kicks off the evaluation process */
	DEPSNODE_TYPE_TIMESOURCE       = 1,        /* Time-Source */
	
	DEPSNODE_TYPE_ID_REF           = 2,        /* ID-Block reference - used as landmarks/collection point for components, but not usually part of main graph */
	DEPSNODE_TYPE_SUBGRAPH         = 3,        /* Isolated sub-graph - used for keeping instanced data separate from instances using them */
	
	/* Outer Types */
	DEPSNODE_TYPE_PARAMETERS       = 10,       /* Parameters Component - Default when nothing else fits (i.e. just SDNA property setting) */
	DEPSNODE_TYPE_PROXY            = 11,       /* Generic "Proxy-Inherit" Component */   // XXX: Also for instancing of subgraphs?
	DEPSNODE_TYPE_ANIMATION        = 12,       /* Animation Component */                 // XXX: merge in with parameters?
	DEPSNODE_TYPE_TRANSFORM        = 13,       /* Transform Component (Parenting/Constraints) */
	DEPSNODE_TYPE_GEOMETRY         = 14,       /* Geometry Component (DerivedMesh/Displist) */
	DEPSNODE_TYPE_SEQUENCER        = 15,       /* Sequencer Component (Scene Only) */
	
	/* Evaluation-Related Outer Types (with Subdata) */
	DEPSNODE_TYPE_EVAL_POSE        = 20,       /* Pose Component - Owner/Container of Bones Eval */
	DEPSNODE_TYPE_BONE             = 21,       /* Bone Component - Child/Subcomponent of Pose */
	
	DEPSNODE_TYPE_EVAL_PARTICLES   = 22,       /* Particle Systems Component */
	
	
	/* Inner Types */
	DEPSNODE_TYPE_OP_PARAMETER     = 100,      /* Parameter Evaluation Operation */
	DEPSNODE_TYPE_OP_PROXY         = 101,      /* Proxy Evaluation Operation */
	DEPSNODE_TYPE_OP_ANIMATION     = 102,      /* Animation Evaluation Operation */
	DEPSNODE_TYPE_OP_TRANSFORM     = 103,      /* Transform Evaluation Operation (incl. constraints, parenting, anim-to-matrix) */
	DEPSNODE_TYPE_OP_GEOMETRY      = 104,      /* Geometry Evaluation Operation (incl. modifiers) */
	DEPSNODE_TYPE_OP_SEQUENCER     = 105,      /* Sequencer Evaluation Operation */
	
	DEPSNODE_TYPE_OP_UPDATE        = 110,      /* Property Update Evaluation Operation [Parameter] */
	DEPSNODE_TYPE_OP_DRIVER        = 112,      /* Driver Evaluation Operation [Parameter] */
	
	DEPSNODE_TYPE_OP_POSE          = 115,      /* Pose Evaluation (incl. setup/cleanup IK trees, IK Solvers) [Pose] */
	DEPSNODE_TYPE_OP_BONE          = 116,      /* Bone Evaluation [Bone] */
	
	DEPSNODE_TYPE_OP_PARTICLE      = 120,      /* Particles Evaluation [Particle] */
	DEPSNODE_TYPE_OP_RIGIDBODY     = 121,      /* Rigidbody Sim (Step) Evaluation */
} eDepsNode_Type;


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

/* ************************************* */
/* Node Types - to be customised during graph building */

/* Generic Nodes ======================= */

/* Time Source Node */
/* Super(DepsNode) */
typedef struct TimeSourceDepsNode {
	DepsNode nd;                   /* standard header */
	
	// XXX: how do we keep track of the chain of time sources for propagation of delays?
	
	double cfra;                    /* new "current time" */
	double offset;                  /* time-offset relative to the "official" time source that this one has */
} TimeSourceDepsNode;

/* Root Node */
/* Super(DepsNode) */
typedef struct RootDepsNode {
	DepsNode nd;                     /* standard header */
	
	struct Scene *scene;             /* scene that this corresponds to */
	TimeSourceDepsNode *time_source; /* entrypoint node for time-changed */
} RootDepsNode;



/* ID-Block Reference */
/* Super(DepsNode) */
typedef struct IDDepsNode {
	DepsNode nd;             /* standard header */
	
	ID *id;                  /* ID Block referenced */
	GHash *component_hash;   /* <eDepsNode_Type, ComponentDepsNode*> hash to make it faster to look up components */
} IDDepsNode;


/* Subgraph Reference */
/* Super(DepsNode) */
typedef struct SubgraphDepsNode {
	DepsNode nd;             /* standard header */
	
	Depsgraph *graph;        /* instanced graph */
	ID *root_id;             /* ID-block at root of subgraph (if applicable) */
	
	size_t num_users;        /* number of nodes which use/reference this subgraph - if just 1, it may be possible to merge into main */
	int flag;                /* (eSubgraphRef_Flag) assorted settings for subgraph node */
} SubgraphDepsNode;

/* Flags for subgraph node */
typedef enum eSubgraphRef_Flag {
	SUBGRAPH_FLAG_SHARED      = (1 << 0),   /* subgraph referenced is shared with another reference, so shouldn't free on exit */
	SUBGRAPH_FLAG_FIRSTREF    = (1 << 1),   /* node is first reference to subgraph, so it can be freed when we are removed */
} eSubgraphRef_Flag;

/* Outer Nodes ========================= */

/* ID Component - Base type for all components */
/* Super(DepsNode) */
typedef struct ComponentDepsNode {
	DepsNode nd;             /* standard header */
	
	ListBase ops;            /* ([OperationDepsNode]) inner nodes for this component */
	GHash *op_hash;          /* <String, OperationDepsNode> quicker lookups for inner nodes attached here by name/identifier */
	
	/* (DEG_OperationsContext) array of evaluation contexts to be passed to evaluation functions for this component. 
	 *                         Only the requested context will be used during any particular evaluation
	 */
	void *contexts[DEG_MAX_EVALUATION_CONTEXTS];
	
	// XXX: a poll() callback to check if component's first node can be started?
} ComponentDepsNode;

/* ---------------------------------------- */

/* Pose Evaluation - Sub-data needed */
/* Super(ComponentDepsNode) */
typedef struct PoseComponentDepsNode {
	/* ComponentDepsNode */
	DepsNode nd;             /* standard header */
	
	ListBase ops;            /* ([OperationDepsNode]) inner nodes for this component */
	GHash *op_hash;          /* <String, OperationDepsNode> quicker lookups for inner nodes attached here by name/identifier (pose-level) */
	
	void *contexts[DEG_MAX_EVALUATION_CONTEXTS];      /* (DEG_OperationsContext) */
	
	/* PoseComponentDepsNode */
	GHash *bone_hash;        /* <String, BoneComponentDepsNode> hash for quickly finding bone components */
} PoseComponentDepsNode;

/* Bone Component */
/* Super(ComponentDepsNode) */
typedef struct BoneComponentDepsNode {
	/* ComponentDepsNode */
	DepsNode nd;                    /* standard header */
	
	ListBase ops;                   /* ([OperationDepsNode]) inner nodes for this component */
	GHash *op_hash;                 /* <String, OperationDepsNode> quicker lookups for inner nodes attached here by name/identifier (bone-level) */
	
	void *contexts[DEG_MAX_EVALUATION_CONTEXTS];      /* (DEG_OperationsContext) */
	
	/* BoneComponentDepsNode */
	struct bPoseChannel *pchan;     /* the bone that this component represents */
} BoneComponentDepsNode;

/* Inner Nodes ========================= */

/* Evaluation Operation for atomic operation 
 * < context: (ComponentEvalContext) context containing data necessary for performing this operation
 *            Results can generally be written to the context directly...
 * < (item): (ComponentDepsNode/PointerRNA) the specific entity involved, where applicable
 */
// XXX: move this to another header that can be exposed?
typedef void (*DepsEvalOperationCb)(void *context, void *item);

/* Atomic Operation - Base type for all operations */
/* Super(DepsNode) */
typedef struct OperationDepsNode {
	DepsNode nd;                  /* standard header */
	
	DepsEvalOperationCb evaluate; /* callback for operation */
	
	PointerRNA ptr;               /* item that operation is to be performed on (optional) */
	
	double start_time;            /* (secs) last timestamp (in seconds) when operation was started */
	double last_time;             /* (seconds) time in seconds that last evaluation took */
	
	short optype;                 /* (eDepsOperation_Type) stage of evaluation */
	short flag;                   /* (eDepsOperation_Flag) extra settings affecting evaluation */
} OperationDepsNode;

/* Type of operation */
typedef enum eDepsOperation_Type {
	/* Primary operation types */
	DEPSOP_TYPE_INIT    = 0, /* initialise evaluation data */
	DEPSOP_TYPE_EXEC    = 1, /* standard evaluation step */
	DEPSOP_TYPE_POST    = 2, /* cleanup evaluation data + flush results */
	
	/* Additional operation types */
	DEPSOP_TYPE_OUT     = 3, /* indicator for outputting a temporary result that other components can use */ // XXX?
	DEPSOP_TYPE_SIM     = 4, /* indicator for things like IK Solvers and Rigidbody Sim steps which modify final results of separate entities at once */
	DEPSOP_TYPE_REBUILD = 5, /* rebuild internal evaluation data - used for Rigidbody Reset and Armature Rebuild-On-Load */
} eDepsOperation_Type;

/* Extra flags affecting operations */
typedef enum eDepsOperation_Flag {
	DEPSOP_FLAG_USES_PYTHON   = (1 << 0),  /* Operation is evaluated using CPython; has GIL and security implications... */      
} eDepsOperation_Flag;

/* ************************************* */
/* Depsgraph */

/* Dependency Graph object */
struct Depsgraph {
	/* Core Graph Functionality ........... */
	GHash *id_hash;          /* <ID : IDDepsNode> mapping from ID blocks to nodes representing these blocks (for quick lookups) */
	DepsNode *root_node;     /* "root" node - the one where all evaluation enters from */
	
	ListBase subgraphs;      /* (SubgraphDepsNode) subgraphs referenced in tree... */
	
	/* Quick-Access Temp Data ............. */
	ListBase entry_tags;     /* (LinkData : DepsNode) nodes which have been tagged as "directly modified" */
	size_t tagged_count;     /* number of nodes that have been tagged for updates/refresh - used for completion cross-checking */     
	
	/* Convenience Data ................... */
	ListBase all_opnodes;    /* (LinkData : DepsNode) all operation nodes, sorted in order of single-thread traversal order */
	size_t num_nodes;        /* number of operation nodes in all_opnodes list */
	
	// XXX: additional stuff like eval contexts, mempools for allocating nodes from, etc.
};

/* ************************************* */

#endif // __DEPSGRAPH_TYPES_H__

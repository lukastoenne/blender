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
 * Implementation of Querying and Filtering API's
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_utildefines.h"

#include "DNA_action_types.h"
#include "DNA_ID.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "RNA_access.h"
#include "RNA_types.h"
} /* extern "C" */

#include "depsgraph_types.h"
#include "depsgraph_intern.h"
#include "depsgraph_queue.h"

#include "stubs.h" // XXX: THIS MUST BE REMOVED WHEN THE DEPSGRAPH REFACTOR IS DONE

/* ************************************************ */
/* Low-Level Graph Traversal */

/* Prepare for graph traversal, by tagging nodes, etc. */
void DEG_graph_traverse_begin(Depsgraph *graph)
{
	/* go over all nodes, initialising the valence counts */
	// XXX: this will end up being O(|V|), which is bad when we're just updating a few nodes... 
}

/* Perform a traversal of graph from given starting node (in execution order) */
// TODO: additional flags for controlling the process?
void DEG_graph_traverse_from_node(Depsgraph *graph, DepsNode *start_node,
                                  DEG_FilterPredicate filter, void *filter_data,
                                  DEG_NodeOperation op, void *operation_data)
{
	DepsgraphQueue *q;
	
	/* sanity checks */
	if (ELEM3(NULL, graph, start_node, op))
		return;
	
	/* add node as starting node to be evaluated, with value of 0 */
	q = DEG_queue_new();
	
	start_node->num_links_pending = 0;
	DEG_queue_push(q, start_node, 0.0f);
	
	/* while we still have nodes in the queue, grab and work on next one */
	do {
		/* grab item at front of queue */
		// XXX: in practice, we may need to wait until one becomes available...
		DepsNode *node = (DepsNode *)DEG_queue_pop(q);
		
		/* perform operation on node */
		op(graph, node, operation_data);
		
		/* schedule up operations which depend on this */
		DEPSNODE_RELATIONS_ITER_BEGIN(node->outlinks, rel)
		{
			/* ensure that relationship is not tagged for ignoring (i.e. cyclic, etc.) */
			// TODO: cyclic refs should probably all get clustered towards the end, so that we can just stop on the first one
			if ((rel->flag & DEPSREL_FLAG_CYCLIC) == 0) {
				DepsNode *child_node = rel->to;
				
				/* only visit node if the filtering function agrees */
				if ((filter == NULL) || filter(graph, child_node, filter_data)) {			
					/* schedule up node... */
					child_node->num_links_pending--;
					DEG_queue_push(q, child_node, (float)child_node->num_links_pending);
				}
			}
		}
		DEPSNODE_RELATIONS_ITER_END;
	} while (DEG_queue_is_empty(q) == false);
	
	/* cleanup */
	DEG_queue_free(q);
}

/* ************************************************ */
/* Filtering API - Basically, making a copy of the existing graph */

/* Create filtering context */
// TODO: allow passing in a number of criteria?
DepsgraphCopyContext *DEG_filter_init()
{
	DepsgraphCopyContext *dcc = (DepsgraphCopyContext *)MEM_callocN(sizeof(DepsgraphCopyContext), "DepsgraphCopyContext");
	
	/* init hashes for easy lookups */
	dcc->nodes_hash = BLI_ghash_ptr_new("Depsgraph Filter NodeHash");
	dcc->rels_hash = BLI_ghash_ptr_new("Depsgraph Filter Relationship Hash"); // XXX?
	
	/* store filtering criteria? */
	// xxx...
	
	return dcc;
}

/* Cleanup filtering context */
void DEG_filter_cleanup(DepsgraphCopyContext *dcc)
{
	/* sanity check */
	if (dcc == NULL)
		return;
		
	/* free hashes - contents are weren't copied, so are ok... */
	BLI_ghash_free(dcc->nodes_hash, NULL, NULL);
	BLI_ghash_free(dcc->rels_hash, NULL, NULL);
	
	/* clear filtering criteria */
	// ...
	
	/* free dcc itself */
	MEM_freeN(dcc);
}

/* -------------------------------------------------- */

/* Create a copy of provided node */
// FIXME: the handling of sub-nodes and links will need to be subject to filtering options...
// XXX: perhaps this really shouldn't be exposed, as it will just be a sub-step of the evaluation process?
DepsNode *DEG_copy_node(DepsgraphCopyContext *dcc, const DepsNode *src)
{
	/* sanity check */
	if (src == NULL)
		return NULL;
	
	DepsNodeFactory *factory = DEG_get_node_factory(src->type);
	BLI_assert(factory != NULL);
	DepsNode *dst = factory->copy_node(dcc, src);
	
	/* add this node-pair to the hash... */
	BLI_ghash_insert(dcc->nodes_hash, (DepsNode *)src, dst);
	
	/* now, fix up any links in standard "node header" (i.e. DepsNode struct, that all 
	 * all others are derived from) that are now corrupt 
	 */
	{
		/* not assigned to graph... */
		dst->next = dst->prev = NULL;
		dst->owner = NULL;
		
		/* relationships to other nodes... */
		// FIXME: how to handle links? We may only have partial set of all nodes still?
		// XXX: the exact details of how to handle this are really part of the querying API...
		
		// XXX: BUT, for copying subgraphs, we'll need to define an API for doing this stuff anyways
		// (i.e. for resolving and patching over links that exist within subtree...)
		dst->inlinks.clear();
		dst->outlinks.clear();
		
		/* clear traversal data */
		dst->num_links_pending = 0;
		dst->lasttime = 0;
	}
	
	/* fix links */
	// XXX...
	
	/* return copied node */
	return dst;
}

/* Make a copy of a relationship */
DepsRelation *DEG_copy_relation(const DepsRelation *src)
{
	DepsRelation *dst = (DepsRelation *)MEM_dupallocN(src);
	
	/* clear out old pointers which no-longer apply */
	dst->next = dst->prev = NULL;
	
	/* return copy */
	return dst;
}

/* ************************************************ */
/* Low-Level Querying API */
/* NOTE: These querying operations are generally only
 *       used internally within the Depsgraph module
 *       and shouldn't really be exposed to the outside
 *       world.
 */

/* Find Matching Node ------------------------------ */
/* For situations where only a single matching node is expected 
 * (i.e. mainly for use when constructing graph)
 */

/* helper for finding inner nodes by their names */
static DepsNode *deg_find_inner_node(Depsgraph *graph, const ID *id, const char subdata[MAX_NAME],
                                     eDepsNode_Type component_type, eDepsNode_Type type, 
                                     const char name[DEG_MAX_ID_NAME])
{
	ComponentDepsNode *component = (ComponentDepsNode *)DEG_find_node(graph, id, subdata, component_type, NULL);
	
	if (component) {
		/* lookup node with matching name... */
		DepsNode *node = component->find_operation(name);
		
		if (node) {
			/* make sure type matches too... just in case */
			BLI_assert(node->type == type);
			return node;
		}
	}
	
	/* no match... */
	return NULL;
}

/* helper for finding bone component nodes by their names */
static DepsNode *deg_find_bone_node(Depsgraph *graph, const ID *id, const char subdata[MAX_NAME],
                                    eDepsNode_Type type, const char name[DEG_MAX_ID_NAME])
{
	PoseComponentDepsNode *pose_comp;
	
	pose_comp = (PoseComponentDepsNode *)DEG_find_node(graph, id, NULL, DEPSNODE_TYPE_EVAL_POSE, NULL);
	if (pose_comp)  {
		/* lookup bone component with matching name */
		BoneComponentDepsNode *bone_node = pose_comp->find_bone_component(subdata);
		
		if (type == DEPSNODE_TYPE_BONE) {
			/* bone component is what we want */
			return (DepsNode *)bone_node;
		}
		else if (type == DEPSNODE_TYPE_OP_BONE) {
			/* now lookup relevant operation node */
			return bone_node->find_operation(name);
		}
	}
	
	/* no match */
	return NULL;
}

/* Find matching node */
DepsNode *DEG_find_node(Depsgraph *graph, const ID *id, const char subdata[MAX_NAME],
                        eDepsNode_Type type, const char name[DEG_MAX_ID_NAME])
{
	DepsNode *result = NULL;
	
	/* each class of types requires a different search strategy... */
	switch (type) {
		/* "Generic" Types -------------------------- */
		case DEPSNODE_TYPE_ROOT:   /* NOTE: this case shouldn't need to exist, but just in case... */
			result = graph->root_node;
			break;
			
		case DEPSNODE_TYPE_TIMESOURCE: /* Time Source */
		{
			/* search for one attached to a particular ID? */
			if (id) {
				/* check if it was added as a component 
				 * (as may be done for subgraphs needing timeoffset) 
				 */
				// XXX: review this
				IDDepsNode *id_node = graph->find_id_node(id);
				
				if (id_node) {
					result = id_node->find_component(type);
				}
			}
			else {
				/* use "official" timesource */
				RootDepsNode *root_node = (RootDepsNode *)graph->root_node;
				result = (DepsNode *)root_node->time_source;
			}
		}
			break;
		
		case DEPSNODE_TYPE_ID_REF: /* ID Block Index/Reference */
		{
			/* lookup relevant ID using nodehash */
			result = graph->find_id_node(id);
		}	
			break;
			
		/* "Outer" Nodes ---------------------------- */
		
		case DEPSNODE_TYPE_PARAMETERS: /* Components... */
		case DEPSNODE_TYPE_PROXY:
		case DEPSNODE_TYPE_ANIMATION:
		case DEPSNODE_TYPE_TRANSFORM:
		case DEPSNODE_TYPE_GEOMETRY:
		case DEPSNODE_TYPE_SEQUENCER:
		case DEPSNODE_TYPE_EVAL_POSE:
		case DEPSNODE_TYPE_EVAL_PARTICLES:
		{
			/* Each ID-Node knows the set of components that are associated with it */
			IDDepsNode *id_node = graph->find_id_node(id);
			
			if (id_node) {
				result = id_node->find_component(type);
			}
		}
			break;
			
		case DEPSNODE_TYPE_BONE:       /* Bone Component */
		{
			/* this will find the bone component */
			result = deg_find_bone_node(graph, id, subdata, type, name);
		}
			break;
		
		/* "Inner" Nodes ---------------------------- */
		
		case DEPSNODE_TYPE_OP_PARAMETER:  /* Parameter Related Ops */
			result = deg_find_inner_node(graph, id, subdata, DEPSNODE_TYPE_PARAMETERS, type, name);
			break;
		case DEPSNODE_TYPE_OP_PROXY:      /* Proxy Ops */
			result = deg_find_inner_node(graph, id, subdata, DEPSNODE_TYPE_PROXY, type, name);
			break;
		case DEPSNODE_TYPE_OP_TRANSFORM:  /* Transform Ops */
			result = deg_find_inner_node(graph, id, subdata, DEPSNODE_TYPE_TRANSFORM, type, name);
			break;
		case DEPSNODE_TYPE_OP_ANIMATION:  /* Animation Ops */
			result = deg_find_inner_node(graph, id, subdata, DEPSNODE_TYPE_ANIMATION, type, name);
			break;
		case DEPSNODE_TYPE_OP_GEOMETRY:   /* Geometry Ops */
			result = deg_find_inner_node(graph, id, subdata, DEPSNODE_TYPE_GEOMETRY, type, name);
			break;
		case DEPSNODE_TYPE_OP_SEQUENCER:  /* Sequencer Ops */
			result = deg_find_inner_node(graph, id, subdata, DEPSNODE_TYPE_SEQUENCER, type, name);
			break;
			
		case DEPSNODE_TYPE_OP_UPDATE:     /* Updates */
			result = deg_find_inner_node(graph, id, subdata, DEPSNODE_TYPE_PARAMETERS, type, name);
			break;
		case DEPSNODE_TYPE_OP_DRIVER:     /* Drivers */
			result = deg_find_inner_node(graph, id, subdata, DEPSNODE_TYPE_PARAMETERS, type, name);
			break;
			
		case DEPSNODE_TYPE_OP_POSE:       /* Pose Eval (Non-Bone Operations) */
			result = deg_find_inner_node(graph, id, subdata, DEPSNODE_TYPE_EVAL_POSE, type, name);
			break;
		case DEPSNODE_TYPE_OP_BONE:       /* Bone */
			result = deg_find_bone_node(graph, id, subdata, type, name);
			break;
			
		case DEPSNODE_TYPE_OP_PARTICLE:  /* Particle System/Step */
			result = deg_find_inner_node(graph, id, subdata, DEPSNODE_TYPE_EVAL_PARTICLES, type, name);
			break;
			
		case DEPSNODE_TYPE_OP_RIGIDBODY: /* Rigidbody Sim */
			result = deg_find_inner_node(graph, id, subdata, DEPSNODE_TYPE_TRANSFORM, type, name); // XXX: needs review
			break;
		
		default:
			/* Unhandled... */
			printf("%s(): Unknown node type %d\n", __func__, type);
			break;
	}
	
	return result;
}

/* Query Conditions from RNA ----------------------- */

/* Determine node-querying criteria for finding a suitable node,
 * given a RNA Pointer (and optionally, a property too)
 */
void DEG_find_node_criteria_from_pointer(const PointerRNA *ptr, const PropertyRNA *prop,
                                         ID **id, char subdata[MAX_NAME],
                                         eDepsNode_Type *type, char name[DEG_MAX_ID_NAME])
{
	/* set default values for returns */
	*id       = (ID *)ptr->id.data;              /* for obvious reasons... */
	*subdata  = '\0';                      /* default to no subdata (e.g. bone) name lookup in most cases */
	*type     = DEPSNODE_TYPE_PARAMETERS;  /* all unknown data effectively falls under "parameter evaluation" */
	name[0]   = '\0';                      /* default to no name to lookup in most cases */
	
	/* handling of commonly known scenarios... */
	if (ptr->type == &RNA_PoseBone) {
		bPoseChannel *pchan = (bPoseChannel *)ptr->data;
		
		/* bone - generally, we just want the bone component... */
		*type = DEPSNODE_TYPE_BONE;
		BLI_strncpy(subdata, pchan->name, MAX_NAME);
	}
	else if (ptr->type == &RNA_Object) {
		Object *ob = (Object *)ptr->data;
		
		/* transforms props? */
		// ...
	}
	else if (RNA_struct_is_a(ptr->type, &RNA_Sequence)) {
		Sequence *seq = (Sequence *)ptr->data;
		
		/* sequencer strip */
		*type = DEPSNODE_TYPE_SEQUENCER;
		BLI_strncpy(subdata, seq->name, MAX_NAME); // xxx?
	}
}

/* Convenience wrapper to find node given just pointer + property */
DepsNode *DEG_find_node_from_pointer(Depsgraph *graph, const PointerRNA *ptr, const PropertyRNA *prop)
{
	ID *id;
	eDepsNode_Type type;
	char subdata[MAX_NAME];
	char name[DEG_MAX_ID_NAME];
	
	/* get querying conditions */
	DEG_find_node_criteria_from_pointer(ptr, prop, &id, subdata, &type, name);
	
	/* use standard node finding code... */
	return DEG_find_node(graph, id, subdata, type, name);
}

/* ************************************************ */
/* Querying API */


/* ************************************************ */
/* Specialized Debugging */

#define NL "\r\n"

static const char *deg_debug_graphviz_fontname = "helvetica";
static const int deg_debug_max_colors = 12;
static const char *deg_debug_colors_dark[] = {"#6e8997","#144f77","#76945b","#216a1d",
                                              "#a76665","#971112","#a87f49","#a9540",
                                              "#86768e","#462866","#a9a965","#753b1a"};
static const char *deg_debug_colors[] = {"#a6cee3","#1f78b4","#b2df8a","#33a02c",
                                         "#fb9a99","#e31a1c","#fdbf6f","#ff7f00",
                                         "#cab2d6","#6a3d9a","#ffff99","#b15928"};
static const char *deg_debug_colors_light[] = {"#8dd3c7","#ffffb3","#bebada","#fb8072",
                                               "#80b1d3","#fdb462","#b3de69","#fccde5",
                                               "#d9d9d9","#bc80bd","#ccebc5","#ffed6f"};
static const int deg_debug_node_type_color_map[][2] = {
    {DEPSNODE_TYPE_ROOT,         0},
    {DEPSNODE_TYPE_TIMESOURCE,   1},
    {DEPSNODE_TYPE_ID_REF,       2},
    {DEPSNODE_TYPE_SUBGRAPH,     3},
    
    /* Outer Types */
    {DEPSNODE_TYPE_PARAMETERS,   4},
    {DEPSNODE_TYPE_PROXY,        5},
    {DEPSNODE_TYPE_ANIMATION,    6},
    {DEPSNODE_TYPE_TRANSFORM,    7},
    {DEPSNODE_TYPE_GEOMETRY,     8},
    {DEPSNODE_TYPE_SEQUENCER,    9},
    {-1,                         0}
};

static int deg_debug_node_type_color_index(eDepsNode_Type type)
{
	const int (*pair)[2];
	for (pair = deg_debug_node_type_color_map; (*pair)[0] >= 0; ++pair)
		if ((*pair)[0] == type)
			return (*pair)[1];
	return -1;
}

static const int deg_debug_relation_type_color_map[][2] = {
    {DEPSREL_TYPE_STANDARD,         0},
    {DEPSREL_TYPE_ROOT_TO_ACTIVE,   1},
    {DEPSREL_TYPE_DATABLOCK,        2},
    {DEPSREL_TYPE_TIME,             3},
    {DEPSREL_TYPE_COMPONENT_ORDER,  4},
    {DEPSREL_TYPE_OPERATION,        5},
    {DEPSREL_TYPE_DRIVER,           6},
    {DEPSREL_TYPE_DRIVER_TARGET,    7},
    {DEPSREL_TYPE_TRANSFORM,        8},
    {DEPSREL_TYPE_GEOMETRY_EVAL,    9},
    {DEPSREL_TYPE_UPDATE,           10},
    {DEPSREL_TYPE_UPDATE_UI,        11},
    {-1,                            0}
};

static void deg_debug_graphviz_legend_color(FILE *f, const char *name, const char *color)
{
	fprintf(f, "<TR>");
	fprintf(f, "<TD>%s</TD>", name);
	fprintf(f, "<TD BGCOLOR=\"%s\"></TD>", color);
	fprintf(f, "</TR>" NL);
}

#if 0
static void deg_debug_graphviz_legend_line(FILE *f, const char *name, const char *color, const char *style)
{
	/* XXX TODO */
	fprintf(f, "" NL);
}

static void deg_debug_graphviz_legend_cluster(FILE *f, const char *name, const char *color, const char *style)
{
	fprintf(f, "<TR>");
	fprintf(f, "<TD>%s</TD>", name);
	fprintf(f, "<TD CELLPADDING=\"4\"><TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\">");
	fprintf(f, "<TR><TD BGCOLOR=\"%s\"></TD></TR>", color);
	fprintf(f, "</TABLE></TD>");
	fprintf(f, "</TR>" NL);
}
#endif

static void deg_debug_graphviz_legend(FILE *f)
{
	const int (*pair)[2];
	
	fprintf(f, "{" NL);
	fprintf(f, "rank = sink;" NL);
	fprintf(f, "Legend [shape=none, margin=0, label=<" NL);
	fprintf(f, "  <TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">" NL);
	fprintf(f, "<TR><TD COLSPAN=\"2\"><B>Legend</B></TD></TR>" NL);
	
	for (pair = deg_debug_node_type_color_map; (*pair)[0] >= 0; ++pair) {
		DepsNodeFactory *nti = DEG_get_node_factory((eDepsNode_Type)(*pair)[0]);
		deg_debug_graphviz_legend_color(f, nti->tname(), deg_debug_colors_light[(*pair)[1] % deg_debug_max_colors]);
	}
	
	fprintf(f, "</TABLE>" NL);
	fprintf(f, ">" NL);
	fprintf(f, ",fontname=\"%s\"", deg_debug_graphviz_fontname);
	fprintf(f, "];" NL);
	fprintf(f, "}" NL);
}

static int deg_debug_relation_type_color_index(eDepsRelation_Type type)
{
	const int (*pair)[2];
	for (pair = deg_debug_relation_type_color_map; (*pair)[0] >= 0; ++pair)
		if ((*pair)[0] == type)
			return (*pair)[1];
	return -1;
}

static void deg_debug_graphviz_node_type_color(FILE *f, const char *attr, eDepsNode_Type type)
{
	const char *defaultcolor = "gainsboro";
	int color = deg_debug_node_type_color_index(type);
	
	fprintf(f, "%s=", attr);
	if (color < 0)
		fprintf(f, "%s", defaultcolor);
	else
		fprintf(f, "\"%s\"", deg_debug_colors_light[color % deg_debug_max_colors]);
}

static void deg_debug_graphviz_relation_type_color(FILE *f, const char *attr, eDepsRelation_Type type)
{
	const char *defaultcolor = "black";
#if 0 /* disabled for now, edge colors are hardly distinguishable */
	int color = deg_debug_relation_type_color_index(type);
	
	fprintf(f, "%s=", attr);
	if (color < 0)
		fprintf(f, "%s", defaultcolor);
	else
		fprintf(f, "\"%s\"", deg_debug_colors_dark[color % deg_debug_max_colors]);
#else
	fprintf(f, "%s=%s", attr, defaultcolor);
#endif
}

static void deg_debug_graphviz_node_single(FILE *f, const void *p, const char *name, const char *style, eDepsNode_Type type)
{
	const char *shape = "box";
	
	fprintf(f, "// %s\n", name);
	fprintf(f, "\"node_%p\"", p);
	fprintf(f, "[");
//	fprintf(f, "label=<<B>%s</B>>", name);
	fprintf(f, "label=<%s>", name);
	fprintf(f, ",fontname=\"%s\"", deg_debug_graphviz_fontname);
	fprintf(f, ",shape=%s", shape);
	fprintf(f, ",style=%s", style);
	deg_debug_graphviz_node_type_color(f, ",fillcolor", type);
	fprintf(f, "];" NL);
	
	fprintf(f, NL);
}

static void deg_debug_graphviz_node_cluster_begin(FILE *f, const void *p, const char *name, const char *style, eDepsNode_Type type)
{
	fprintf(f, "// %s\n", name);
	fprintf(f, "subgraph \"cluster_%p\" {" NL, p);
//	fprintf(f, "label=<<B>%s</B>>;" NL, name);
	fprintf(f, "label=<%s>;" NL, name);
	fprintf(f, "fontname=\"%s\";" NL, deg_debug_graphviz_fontname);
	fprintf(f, "style=%s;" NL, style);
	deg_debug_graphviz_node_type_color(f, "fillcolor", type); fprintf(f, ";" NL);
	
	/* dummy node, so we can add edges between clusters */
	fprintf(f, "\"node_%p\"", p);
	fprintf(f, "[");
	fprintf(f, "shape=%s", "point");
	fprintf(f, ",style=%s", "invis");
	fprintf(f, "];" NL);
	
	fprintf(f, NL);
}

static void deg_debug_graphviz_node_cluster_end(FILE *f)
{
	fprintf(f, "}" NL);
	
	fprintf(f, NL);
}

static void deg_debug_graphviz_graph_nodes(FILE *f, const Depsgraph *graph);
static void deg_debug_graphviz_graph_relations(FILE *f, const Depsgraph *graph);

static void deg_debug_graphviz_node(FILE *f, const DepsNode *node)
{
	const char *style;
	switch (node->tclass) {
		case DEPSNODE_CLASS_GENERIC:
			style = "\"filled\"";
			break;
		case DEPSNODE_CLASS_COMPONENT:
			style = "\"filled\"";
			break;
		case DEPSNODE_CLASS_OPERATION:
			style = "\"filled,rounded\"";
			break;
	}
	
	switch (node->type) {
		case DEPSNODE_TYPE_ID_REF: {
			const IDDepsNode *id_node = (const IDDepsNode *)node;
			if (id_node->components.empty()) {
				deg_debug_graphviz_node_single(f, node, node->name, style, node->type);
			}
			else {
				deg_debug_graphviz_node_cluster_begin(f, node, node->name, style, node->type);
				for (IDDepsNode::ComponentMap::const_iterator it = id_node->components.begin(); it != id_node->components.end(); ++it) {
					const ComponentDepsNode *comp = it->second;
					deg_debug_graphviz_node(f, comp);
				}
				deg_debug_graphviz_node_cluster_end(f);
			}
			break;
		}
		
		case DEPSNODE_TYPE_SUBGRAPH: {
			SubgraphDepsNode *sub_node = (SubgraphDepsNode *)node;
			if (sub_node->graph) {
				deg_debug_graphviz_node_cluster_begin(f, node, node->name, style, node->type);
				deg_debug_graphviz_graph_nodes(f, sub_node->graph);
				deg_debug_graphviz_node_cluster_end(f);
			}
			else {
				deg_debug_graphviz_node_single(f, node, node->name, style, node->type);
			}
			break;
		}
		
		case DEPSNODE_TYPE_PARAMETERS:
		case DEPSNODE_TYPE_ANIMATION:
		case DEPSNODE_TYPE_TRANSFORM:
		case DEPSNODE_TYPE_PROXY:
		case DEPSNODE_TYPE_GEOMETRY:
		case DEPSNODE_TYPE_SEQUENCER: {
			ComponentDepsNode *comp_node = (ComponentDepsNode *)node;
			if (!comp_node->operations.empty()) {
				deg_debug_graphviz_node_cluster_begin(f, node, node->name, style, node->type);
				for (ComponentDepsNode::OperationMap::const_iterator it = comp_node->operations.begin(); it != comp_node->operations.end(); ++it) {
					const DepsNode *op_node = it->second;
					deg_debug_graphviz_node(f, op_node);
				}
				deg_debug_graphviz_node_cluster_end(f);
			}
			else {
				deg_debug_graphviz_node_single(f, node, node->name, style, node->type);
			}
			break;
		}
		
		case DEPSNODE_TYPE_EVAL_POSE: {
			PoseComponentDepsNode *pose_node = (PoseComponentDepsNode *)node;
			if (!pose_node->bone_hash.empty()) {
				deg_debug_graphviz_node_cluster_begin(f, node, node->name, style, node->type);
				for (PoseComponentDepsNode::BoneComponentMap::const_iterator it = pose_node->bone_hash.begin(); it != pose_node->bone_hash.end(); ++it) {
					const DepsNode *bone_comp = it->second;
					deg_debug_graphviz_node(f, bone_comp);
				}
				deg_debug_graphviz_node_cluster_end(f);
			}
			else {
				deg_debug_graphviz_node_single(f, node, node->name, style, node->type);
			}
			break;
		}
		
		default: {
			deg_debug_graphviz_node_single(f, node, node->name, style, node->type);
		}
	}
}

static bool deg_debug_graphviz_is_cluster(const DepsNode *node)
{
	switch (node->type) {
		case DEPSNODE_TYPE_ID_REF: {
			const IDDepsNode *id_node = (const IDDepsNode *)node;
			return !id_node->components.empty();
		}
		
		case DEPSNODE_TYPE_SUBGRAPH: {
			SubgraphDepsNode *sub_node = (SubgraphDepsNode *)node;
			return sub_node->graph != NULL;
		}
		
		case DEPSNODE_TYPE_PARAMETERS:
		case DEPSNODE_TYPE_ANIMATION:
		case DEPSNODE_TYPE_TRANSFORM:
		case DEPSNODE_TYPE_PROXY:
		case DEPSNODE_TYPE_GEOMETRY:
		case DEPSNODE_TYPE_SEQUENCER: {
			ComponentDepsNode *comp_node = (ComponentDepsNode *)node;
			return !comp_node->operations.empty();
		}
		
		case DEPSNODE_TYPE_EVAL_POSE: {
			PoseComponentDepsNode *pose_node = (PoseComponentDepsNode *)node;
			return !pose_node->bone_hash.empty();
		}
		
		default:
			return false;
	}
}

static void deg_debug_graphviz_node_relations(FILE *f, const DepsNode *node)
{
	DEPSNODE_RELATIONS_ITER_BEGIN(node->inlinks, rel)
	{
		const DepsNode *tail = rel->to;
		const DepsNode *head = rel->from;
		
		fprintf(f, "// %s -> %s\n", tail->name, head->name);
		fprintf(f, "\"node_%p\"", tail); /* same as node */
		fprintf(f, " -> ");
		fprintf(f, "\"node_%p\"", head);

		fprintf(f, "[");
		fprintf(f, "label=<%s>", rel->name);
		fprintf(f, ",fontname=\"%s\"", deg_debug_graphviz_fontname);
		deg_debug_graphviz_relation_type_color(f, ",color", rel->type);
		
		if (deg_debug_graphviz_is_cluster(tail))
			fprintf(f, ",ltail=\"cluster_%p\"", tail);
		if (deg_debug_graphviz_is_cluster(head))
			fprintf(f, ",lhead=\"cluster_%p\"", head);
		
		fprintf(f, "];" NL);
		fprintf(f, NL);
	}
	DEPSNODE_RELATIONS_ITER_END;

	switch (node->type) {
		case DEPSNODE_TYPE_ID_REF: {
			const IDDepsNode *id_node = (const IDDepsNode *)node;
			for (IDDepsNode::ComponentMap::const_iterator it = id_node->components.begin(); it != id_node->components.end(); ++it) {
				const ComponentDepsNode *comp = it->second;
				deg_debug_graphviz_node_relations(f, comp);
			}
			break;
		}
		
		case DEPSNODE_TYPE_SUBGRAPH: {
			SubgraphDepsNode *sub_node = (SubgraphDepsNode *)node;
			if (sub_node->graph) {
				deg_debug_graphviz_graph_relations(f, sub_node->graph);
			}
			break;
		}
		
		default:
			break;
	}
}

static void deg_debug_graphviz_graph_nodes(FILE *f, const Depsgraph *graph)
{
	if (graph->root_node)
		deg_debug_graphviz_node(f, graph->root_node);
	for (Depsgraph::IDNodeMap::const_iterator it = graph->id_hash.begin(); it != graph->id_hash.end(); ++it) {
		DepsNode *node = it->second;
		deg_debug_graphviz_node(f, node);
	}
}

static void deg_debug_graphviz_graph_relations(FILE *f, const Depsgraph *graph)
{
	if (graph->root_node)
		deg_debug_graphviz_node_relations(f, graph->root_node);
	for (Depsgraph::IDNodeMap::const_iterator it = graph->id_hash.begin(); it != graph->id_hash.end(); ++it) {
		DepsNode *node = it->second;
		deg_debug_graphviz_node_relations(f, node);
	}
}

void DEG_debug_graphviz(const Depsgraph *graph, FILE *f)
{
#if 0 /* generate shaded color set */
	static char colors[][3] = {{0xa6, 0xce, 0xe3},{0x1f, 0x78, 0xb4},{0xb2, 0xdf, 0x8a},{0x33, 0xa0, 0x2c},
	                           {0xfb, 0x9a, 0x99},{0xe3, 0x1a, 0x1c},{0xfd, 0xbf, 0x6f},{0xff, 0x7f, 0x00},
	                           {0xca, 0xb2, 0xd6},{0x6a, 0x3d, 0x9a},{0xff, 0xff, 0x99},{0xb1, 0x59, 0x28}};
	int i;
	const float factor = 0.666f;
	for (i=0; i < 12; ++i)
		printf("\"#%x%x%x\"\n", (char)(colors[i][0] * factor), (char)(colors[i][1] * factor), (char)(colors[i][2] * factor));
#endif
	
	if (!graph)
		return;
	
	fprintf(f, "digraph depgraph {" NL);
	fprintf(f, "graph [compound=true];" NL);
	
	deg_debug_graphviz_graph_nodes(f, graph);
	deg_debug_graphviz_graph_relations(f, graph);
	
	deg_debug_graphviz_legend(f);
	
	fprintf(f, "}" NL);
}

#undef NL

#ifndef NDEBUG
#define DEG_DEBUG_BUILD
#endif

#ifdef DEG_DEBUG_BUILD

static void *deg_debug_build_userdata;
DEG_DebugBuildCb_NodeAdded deg_debug_build_node_added_cb;
DEG_DebugBuildCb_RelationAdded deg_debug_build_rel_added_cb;

void DEG_debug_build_init(void *userdata, DEG_DebugBuildCb_NodeAdded node_added_cb, DEG_DebugBuildCb_RelationAdded rel_added_cb)
{
	deg_debug_build_userdata = userdata;
	deg_debug_build_node_added_cb = node_added_cb;
	deg_debug_build_rel_added_cb = rel_added_cb;
}

void DEG_debug_build_node_added(const DepsNode *node)
{
	if (deg_debug_build_node_added_cb) {
		deg_debug_build_node_added_cb(deg_debug_build_userdata, node);
	}
}

void DEG_debug_build_relation_added(const DepsRelation *rel)
{
	if (deg_debug_build_rel_added_cb) {
		deg_debug_build_rel_added_cb(deg_debug_build_userdata, rel);
	}
}

void DEG_debug_build_end(void)
{
	deg_debug_build_userdata = NULL;
	deg_debug_build_node_added_cb = NULL;
	deg_debug_build_rel_added_cb = NULL;
}

#else /* DEG_DEBUG_BUILD */

void DEG_debug_build_init(void *userdata, DEG_DebugBuildCb_NodeAdded node_added_cb, DEG_DebugBuildCb_RelationAdded rel_added_cb) {}
void DEG_debug_build_node_added(const DepsNode *node) {}
void DEG_debug_build_relation_added(const DepsRelation *rel) {}
void DEG_debug_build_end(void) {}

#endif /* DEG_DEBUG_BUILD */

/* ************************************************ */

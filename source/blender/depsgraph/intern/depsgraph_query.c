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

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_utildefines.h"

#include "DNA_action_types.h"
#include "DNA_ID.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BKE_depsgraph.h"
#include "BKE_depsgraph_query.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "depsgraph_types.h"
#include "depsgraph_intern.h"

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
	
	BLI_assert(start_node->valency == 0); // XXX: otherwise, it may never start!
	DEG_queue_push(q, start_node);
	
	/* while we still have nodes in the queue, grab and work on next one */
	do {
		/* grab item at front of queue */
		// XXX: in practice, we may need to wait until one becomes available...
		DepsNode *node = DEG_queue_pop(q);
		
		/* perform operation on node */
		op(graph, node, operation_data);
		
		/* schedule up operations which depend on this */
		DEPSNODE_RELATIONS_ITER_BEGIN(node->outlinks.first, rel)
		{
			DepsNode *new_node = rel->to;
			
			// XXX: ensure that relationship is not tagged for ignoring (i.e. cyclic, etc.)
			// XXX: ensure that relationsip ends up pointing where we expect it to...
			
			/* schedule up node... */
			DEG_queue_push(q, new_node);
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
	DepsgraphCopyContext *dcc = MEM_callocN(sizeof(DepsgraphCopyContext), "DepsgraphCopyContext");
	
	/* init hashes for easy lookups */
	dcc->nodes_hash = BLI_ghash_ptr_new("Despgraph Filter NodeHash");
	dcc->rels_hash = BLI_ghash_ptr_new("Despgraph Filter Relationship Hash"); // XXX?
	
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
	const DepsNodeTypeInfo *nti = DEG_node_get_typeinfo(src);
	DepsNode *dst;
	
	/* sanity check */
	if (src == NULL)
		return NULL;
	
	/* allocate new node, and brute-force copy over all "basic" data */
	// XXX: need to review the name here, as we can't have exact duplicates...
	dst = DEG_create_node(src->type);
	memcpy(dst, src, nti->size);
	
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
		dst->inlinks.first = dst->inlinks.last = NULL;
		dst->outlinks.first = dst->outlinks.last = NULL;
		
		/* clear traversal data */
		dst->valency = 0;
		dst->lasttime = 0;
	}
	
	/* fix up type-specific data (and/or subtree...) */
	if (nti->copy_data) {
		nti->copy_data(dcc, dst, src);
	}
	
	/* fix links */
	// XXX...
	
	/* return copied node */
	return dst;
}

/* Make a copy of a relationship */
DepsRelation *DEG_copy_relation(const DepsRelation *src)
{
	DepsRelation *dst = MEM_dupallocN(src);
	
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
		DepsNode *node = BLI_ghash_lookup(component->op_hash, name);
		
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
		BoneComponentDepsNode *bone_node = BLI_ghash_lookup(pose_comp->bone_hash, subdata);
		
		if (type == DEPSNODE_TYPE_BONE) {
			/* bone component is what we want */
			return (DepsNode *)bone_node;
		}
		else if (type == DEPSNODE_TYPE_OP_BONE) {
			/* now lookup relevant operation node */
			return BLI_ghash_lookup(bone_node->op_hash, name);
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
				IDDepsNode *id_node = BLI_ghash_lookup(graph->id_hash, id);
				
				if (id_node) {
					result = BLI_ghash_lookup(id_node->component_hash,
					                          SET_INT_IN_POINTER(type));
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
			result = BLI_ghash_lookup(graph->id_hash, id);
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
			IDDepsNode *id_node = BLI_ghash_lookup(graph->id_hash, id);
			
			if (id_node) {
				result = BLI_ghash_lookup(id_node->component_hash, SET_INT_IN_POINTER(type));
			}
		}
			break;
			
		case DEPSNODE_TYPE_BONE:       /* Bone Component */
		{
			/* this will find the bone component */
			result = deg_find_bone_node(graph, id, name, type, NULL);
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
	*id       = ptr->id.data;              /* for obvious reasons... */
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

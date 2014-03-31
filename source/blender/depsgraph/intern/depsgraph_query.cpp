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

#include "depsnode.h"
#include "depsnode_component.h"
#include "depsnode_operation.h"
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
static DepsNode *deg_find_inner_node(Depsgraph *graph, const ID *id, const string &subdata,
                                     eDepsNode_Type component_type, eDepsNode_Type type, 
                                     const string &name)
{
	ComponentDepsNode *component = (ComponentDepsNode *)graph->find_node(id, subdata, component_type, "");
	
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
static DepsNode *deg_find_bone_node(Depsgraph *graph, const ID *id, const string &subdata,
                                    eDepsNode_Type type, const string &name)
{
	PoseComponentDepsNode *pose_comp;
	
	pose_comp = (PoseComponentDepsNode *)graph->find_node(id, NULL, DEPSNODE_TYPE_EVAL_POSE, "");
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
DepsNode *Depsgraph::find_node(const ID *id, const string &subdata, eDepsNode_Type type, const string &name)
{
	DepsNode *result = NULL;
	
	/* each class of types requires a different search strategy... */
	switch (type) {
		/* "Generic" Types -------------------------- */
		case DEPSNODE_TYPE_ROOT:   /* NOTE: this case shouldn't need to exist, but just in case... */
			result = this->root_node;
			break;
			
		case DEPSNODE_TYPE_TIMESOURCE: /* Time Source */
		{
			/* search for one attached to a particular ID? */
			if (id) {
				/* check if it was added as a component 
				 * (as may be done for subgraphs needing timeoffset) 
				 */
				// XXX: review this
				IDDepsNode *id_node = this->find_id_node(id);
				
				if (id_node) {
					result = id_node->find_component(type);
				}
			}
			else {
				/* use "official" timesource */
				RootDepsNode *root_node = (RootDepsNode *)this->root_node;
				result = (DepsNode *)root_node->time_source;
			}
		}
			break;
		
		case DEPSNODE_TYPE_ID_REF: /* ID Block Index/Reference */
		{
			/* lookup relevant ID using nodehash */
			result = this->find_id_node(id);
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
			IDDepsNode *id_node = this->find_id_node(id);
			
			if (id_node) {
				result = id_node->find_component(type);
			}
		}
			break;
			
		case DEPSNODE_TYPE_BONE:       /* Bone Component */
		{
			/* this will find the bone component */
			result = deg_find_bone_node(this, id, subdata, type, name);
		}
			break;
		
		/* "Inner" Nodes ---------------------------- */
		
		case DEPSNODE_TYPE_OP_PARAMETER:  /* Parameter Related Ops */
			result = deg_find_inner_node(this, id, subdata, DEPSNODE_TYPE_PARAMETERS, type, name);
			break;
		case DEPSNODE_TYPE_OP_PROXY:      /* Proxy Ops */
			result = deg_find_inner_node(this, id, subdata, DEPSNODE_TYPE_PROXY, type, name);
			break;
		case DEPSNODE_TYPE_OP_TRANSFORM:  /* Transform Ops */
			result = deg_find_inner_node(this, id, subdata, DEPSNODE_TYPE_TRANSFORM, type, name);
			break;
		case DEPSNODE_TYPE_OP_ANIMATION:  /* Animation Ops */
			result = deg_find_inner_node(this, id, subdata, DEPSNODE_TYPE_ANIMATION, type, name);
			break;
		case DEPSNODE_TYPE_OP_GEOMETRY:   /* Geometry Ops */
			result = deg_find_inner_node(this, id, subdata, DEPSNODE_TYPE_GEOMETRY, type, name);
			break;
		case DEPSNODE_TYPE_OP_SEQUENCER:  /* Sequencer Ops */
			result = deg_find_inner_node(this, id, subdata, DEPSNODE_TYPE_SEQUENCER, type, name);
			break;
			
		case DEPSNODE_TYPE_OP_UPDATE:     /* Updates */
			result = deg_find_inner_node(this, id, subdata, DEPSNODE_TYPE_PARAMETERS, type, name);
			break;
		case DEPSNODE_TYPE_OP_DRIVER:     /* Drivers */
			result = deg_find_inner_node(this, id, subdata, DEPSNODE_TYPE_PARAMETERS, type, name);
			break;
			
		case DEPSNODE_TYPE_OP_POSE:       /* Pose Eval (Non-Bone Operations) */
			result = deg_find_inner_node(this, id, subdata, DEPSNODE_TYPE_EVAL_POSE, type, name);
			break;
		case DEPSNODE_TYPE_OP_BONE:       /* Bone */
			result = deg_find_bone_node(this, id, subdata, type, name);
			break;
			
		case DEPSNODE_TYPE_OP_PARTICLE:  /* Particle System/Step */
			result = deg_find_inner_node(this, id, subdata, DEPSNODE_TYPE_EVAL_PARTICLES, type, name);
			break;
			
		case DEPSNODE_TYPE_OP_RIGIDBODY: /* Rigidbody Sim */
			result = deg_find_inner_node(this, id, subdata, DEPSNODE_TYPE_TRANSFORM, type, name); // XXX: needs review
			break;
		
		default:
			/* Unhandled... */
			printf("%s(): Unknown node type %d\n", __func__, type);
			break;
	}
	
	return result;
}

/* ************************************************ */
/* Querying API */


/* ************************************************ */

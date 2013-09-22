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
 * Implementation of special queue type for use in Depsgraph traversals
 */

#include <stdio.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_heap.h"
#include "BLI_ghash.h"
#include "BLI_utildefines.h"

#include "depsgraph_types.h"
#include "depsgraph_queue.h"

/* ********************************************************* */
// TODO: move these type defines to a separate header file

/* Dependency Graph Traversal Queue 
 *
 * There are two parts to this:
 * a) "Pending" Nodes - This part contains the set of nodes
 *    which are related to those which have been visited
 *    previously, but are not yet ready to actually be visited.
 * b) "Scheduled" Nodes - These are the nodes whose ancestors
 *    have all been evaluated already, which means that any
 *    or all of them can be picked (in practically in order) to
 *    be visited immediately.
 *
 * Internally, the queue makes sure that each node in the graph
 * only gets added to the queue once. This is because there can
 * be multiple inlinks to each node given the way that the relations
 * work. 
 */

/* Depsgraph Queue Type */
typedef struct DepsgraphQueue {
	/* Pending */
	Heap *pending_heap;         /* (valence:int, DepsNode*) */
	GHash *pending_hash;        /* (DepsNode* : HeapNode*>) */
	
	/* Ready to be visited - fifo */
	Heap *ready_heap;           /* (idx:int, DepsNode*) */
	
	/* Size/Order counts */
	size_t idx;                 /* total number of nodes which are/have been ready so far (including those already visited) */
	size_t tot;                 /* total number of nodes which have passed through queue; mainly for debug */
} DepsgraphQueue;

/* ********************************************************* */
/* Depsgraph Queue implementation */

/* Data Management ----------------------------------------- */

DepsgraphQueue *DEG_queue_new(void)
{
	DepsgraphQueue *q = MEM_callocN(sizeof(DepsgraphQueue), "DEG_queue_new()");
	
	/* init data structures for use here */
	q->pending_heap = BLI_heap_new();
	q->pending_hash = BLI_ghash_ptr_new("DEG Queue Pending Hash");
	
	q->ready_heap   = BLI_heap_new();
	
	/* init settings */
	q->idx = 0;
	q->tot = 0;
	
	/* return queue */
	return q;
}

void DEG_queue_free(DepsgraphQueue *q)
{
	/* free data structures */
	BLI_assert(BLI_heap_size(q->pending_heap) == 0);
	BLI_assert(BLI_heap_size(q->ready_heap) == 0);
	BLI_assert(BLI_ghash_size(q->pending_hash) == 0);
	
	BLI_heap_free(q->pending_heap, NULL);
	BLI_heap_free(q->ready_heap, NULL);
	BLI_ghash_free(q->pending_hash, NULL, NULL);
	
	/* free queue itself */
	MEM_freeN(q);
}

/* Statistics --------------------------------------------- */

/* Get the number of nodes which are we should visit, but are not able to yet */
size_t DEG_queue_num_pending(DepsgraphQueue *q)
{
	return BLI_heap_size(q->pending_heap);
}

/* Get the number of nodes which are now ready to be visited */
size_t DEG_queue_num_ready(DepsgraphQueue *q)
{
	return BLI_heap_size(q->ready_heap);
}

/* Check if queue has any items in it (still passing through) */
bool DEG_queue_is_empty(DepsgraphQueue *q)
{
	return ((DEG_queue_num_pending(q) == 0) && ((DEG_queue_num_ready(q) == 0));
}

/* Queue Operations --------------------------------------- */

/* Add DepsNode to the queue 
 * - Each node is only added once to the queue(s)
 * - Valence counts get adjusted here, since this call is used
 *   when the in-node has been visited, clearing the way for
 *   its dependencies (i.e. the ones we're adding now)
 */
void DEG_queue_push(DepsgraphQueue *q, DepsNode *dnode)
{
	HeapNode *hnode = NULL;
	int cost;
	
	/* adjust valence count of node */
	// TODO: this should probably be done as an atomic op?
	dnode->valency--;
	cost = dnode->valency;
	
	/* Shortcut: Directly add to ready if node isn't waiting on anything now... */
	if (cost == 0) {
		/* node is now ready to be visited - schedule it up for such */
		if (BLI_ghash_has_key(q->pending_hash, dnode)) {
			/* remove from pending queue - we're moving it to the scheduling queue */
			hnode = BLI_ghash_lookup(q->pending_hash, dnode);
			BLI_heap_remove(q->pending_heap, hnode);
			
			BLI_ghash_remove(q->pending_hash, dnode, NULL, NULL);
		}
		
		/* schedule up node using latest count (of ready nodes) */
		BLI_heap_insert(q->ready_hash, (float)q->idx, dnode);
		q->idx++;
	}
	else {
		/* node is still waiting on some other ancestors, 
		 * so add it to the pending heap in the meantime...
		 */
		// XXX: is this even necessary now?
		if (BLI_ghash_has_key(q->pending_hash, dnode)) {
			/* just update cost on pending node */
			hnode = BLI_ghash_lookup(q->pending_hash, dnode);
			BLI_heap_node_value_set(q->pending_heap, hnode, (float)cost);
		}
		else {
			/* add new node to pending queue */
			hnode = BLI_heap_insert(q->pending_heap, (float)cost, dnode);
		}
	}
}

/* Grab a "ready" node from the queue */
void *DEG_queue_pop(DepsgraphQueue *q)
{
	/* only grab "ready" nodes */
	return BLI_heap_popmin(q->ready_heap);
}

/* ********************************************************* */


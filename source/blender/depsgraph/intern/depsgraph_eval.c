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
 * Evaluation engine entrypoints for Depsgraph Engine
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_constraint.h"
#include "BKE_DerivedMesh.h"
#include "BKE_depsgraph.h"
#include "BKE_object.h"
#include "BKE_scene.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "depsgraph_types.h"
#include "depsgraph_eval.h"
#include "depsgraph_intern.h"

/* *************************************************** */

/* *************************************************** */
/* Evaluation Entrypoints */

/* Evaluate all nodes tagged for updating 
 * ! This is usually done as part of main loop, but may also be 
 *   called from frame-change update
 */
void DEG_evaluate_on_refresh(Depsgraph *graph)
{
	/* from the root node, start queuing up nodes to evaluate */
	// ... start scheduler, etc.
}

/* Frame-change happened for root scene that graph belongs to */
void DEG_evaluate_on_framechange(Depsgraph *graph, double ctime)
{
	TimeSourceDepsNode *tsrc;
	
	/* update time on primary timesource */
	tsrc = (TimeSourceDepsNode *)DEG_find_node(graph, NULL, DEPSNODE_TYPE_TIMESOURCE, NULL);
	
	tsrc->cfra = ctime;
	tsrc->flag |= (DEPSNODE_FLAG_DIRECTLY_MODIFIED); // XXX: have API calls for touching these, which store in a queue for flushing?
	
	/* recursively push updates out to all nodes dependent on this, 
	 * until all affected are tagged and/or scheduled up for eval
	 */
	DEG_graph_flush_updates(graph);
	
	/* perform recalculation updates */
	DEG_evaluate_on_refresh(graph);
}

/* *************************************************** */

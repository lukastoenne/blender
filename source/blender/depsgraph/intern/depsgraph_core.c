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
 * Core routines for how the Depsgraph works
 */
 
#include <stdio.h>
#include <stdlib.h>

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_depsgraph.h"

#include "depsgraph_types.h"
#include "depsgraph_intern.h"

/* ************************************************** */
/* Low-Level Dependency Graph Traversal / Sorting / Validity + Integrity */

/* ************************************************** */
/* Node Management */

/* Find matching node */
DepsNode *DEG_find_node(Depsgraph *graph, eDepsNode_Type type, ID *id, StructRNA *srna, void *data)
{
	DepsNode *result = NULL;
	
	return result;
}

/* Add a new outer node */
DepsNode *DEG_add_node(Depsgraph *graph, eDepsNode_Type type, ID *id, StructRNA *srna, void *data)
{
	DepsNode *result = NULL;
	
	/* depends on type of node... */
	
	/* return the newly created node matching the description */
	return result;
}

/* Get a matching node, creating one if need be */
DepsNode *DEG_get_node(Depsgraph *graph, eDepsNode_Type type, ID *id, StructRNA *srna, void *data)
{
	DepsNode *node;
	
	/* firstly try to get an existing node... */
	node = DEG_find_node(graph, type, id, srna, data);
	if (node == NULL) {
		/* nothing exists, so create one instead! */
		node = DEG_add_node(graph, type, id, srna, data);
	}
	
	/* return the node - it must exist now... */
	return node;
}

/* ************************************************** */
/* Public API */


/* ************************************************** */
/* Evaluation */


/* ************************************************** */

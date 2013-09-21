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

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "depsgraph_queue.h"

/* ********************************************************* */
// TODO: move these type defines to a separate header file

/* Dependency Graph Traversal Queue 
 *
 * This actually consists of two queues - a "pending" queue
 * and a "ready" queue. Only nodes in the "ready" queue can
 * be retrieved and visited.
 */
typedef struct DepsgraphQueue {
	
} DepsgraphQueue;

/* ********************************************************* */
/* Depsgraph Queue implementation */

/* ********************************************************* */


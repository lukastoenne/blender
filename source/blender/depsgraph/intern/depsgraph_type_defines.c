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
 * Defines and code for core node types
 */
 
#include <stdio.h>
#include <stdlib.h>

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_depsgraph.h"

#include "depsgraph_types.h"
#include "depsgraph_intern.h"

/* ******************************************************** */


/* ******************************************************** */
/* External API */

/* Global type registry */
// XXX: hash or array?

/* Registration ------------------------------------------- */

/* Register node type */
void DEG_register_node_typeinfo(DepsNodeTypeInfo *typeinfo)
{
	// TODO: 
}


/* Register all node types */
void DEG_register_node_types(void)
{
	DEG_register_node_typeinfo(DNTI_OUTER_ID);
	DEG_register_node_typeinfo(DNTI_OUTER_GROUP);
	// ...
}

/* Getters ------------------------------------------------- */

/* Get typeinfo for specified type */
DepsNodeTypeInfo *DEG_get_typeinfo(eDepsNode_Type type)
{
	DepsNodeTypeInfo *nti = NULL;
	
	// TODO: look up typeinfo associated with this type...
	return nti;
}

/* Get typeinfo for provided node */
DepsNodeTypeInfo *DEG_node_get_typeinfo(DepsNode *node)
{
	DepsNodeTypeInfo *nti = NULL;
	
	if (node) {
		nti = DEG_get_typeinfo(node->type);
	}
	return nti;
}

/* ******************************************************** */

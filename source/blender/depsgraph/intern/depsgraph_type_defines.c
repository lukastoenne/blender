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
#include <string.h>

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_utildefines.h"

#include "BKE_depsgraph.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "depsgraph_types.h"
#include "depsgraph_intern.h"

/* ******************************************************** */
/* Outer Nodes */

/* ID Node ================================================ */

/* Ensure that outer node gets copied correctly */
static void dnti_outer_node__copy_data(DepsNode *dst_node, const DepsNode *src_node)
{
	const OuterIdDepsNodeTemplate *src = (const OuterIdDepsNodeTemplate *)src_node;
	OuterIdDepsNodeTemplate *dst       = (OuterIdDepsNodeTemplate *)dst_node;
	
	/* copy subdata nodes */
	if (src->subdata.first) {
		DepsNode *itA;
		
		/* 1) make valid copies of each node */
		for (itA = src->subdata.first; itA; itA = itA->next) {
			DepsNode *node = DEG_copy_node(itA);
			BLI_addtail(&dst->subdata, node);
		}
		
		/* 2) hook them up correctly again */
		// ...?
	}
	
	/* copy inner nodes */
	// XXX: perhaps we only need generic logic for this which can be copied/linked around?
	{
		DepsNode *itA;
		
		/* 1) make valid copies of each node */
		for (itA = src->nodes.first; itA; itA = itA->next) {
			DepsNode *node = DEG_copy_node(itA);
			BLI_addtail(&dst->nodes, node);
		}
		
		/* 2) hook them up correctly again */
		// ...?
	}
}

/* Add 'id' node to graph */
static void dnti_outer_id__add_to_graph(Depsgraph *graph, DepsNode *node, ID *id)
{
	/* add to toplevel node and graph */
	BLI_ghash_insert(graph->nodehash, id, node);
	BLI_addtail(&graph->nodes, node);
}

/* Remove 'id' node from graph - to be replaced with a group perhaps */
static void dnti_outer_id__remove_from_graph(Depsgraph *graph, DepsNode *node)
{
	/* remove toplevel node and hash entry
	 * NOTE: these will be replaced with new versions later
	 *       and the other links can be redirected non-destructively
	 */
	BLI_ghash_remove(graph->nodehash, id, NULL, NULL);
	BLI_remlink(&graph->nodes, node);
}

/* ID Node Type Info */
static DepsNodeTypeInfo DNTI_OUTER_ID = {
	/* type */               DEPSNODE_TYPE_OUTER_ID,
	/* size */               sizeof(IDDepsNode),
	/* name */               "ID Node",
	
	/* init_data() */        NULL,
	/* free_data() */        NULL,
	/* copy_data() */        dnti_outer_node__copy_data,
	
	/* add_to_graph() */     dnti_outer_id__add_to_graph,
	/* remove_from_graph()*/ dnti_outer_id__remove_from_graph,
	
	/* match_outer() */      NULL, // XXX...
	
	/* build_subgraph() */   NULL
};

/* Group Node ============================================= */

/* Ensure that 'group' node's subgraph gets copied correctly */
static void dnti_outer_group__copy_data(DepsNode *dst_node, const DepsNode *src_node)
{
	const GroupDepsNode *src = (const GroupDepsNode *)src_node;
	GroupDepsNode *dst       = (GroupDepsNode *)dst_node;
	
	/* perform outer-node copying first */
	dnti_outer_node__copy_data(dst_nod, src_node);
	
	/* copy headliner section - these are just LinkData's with ptrs to ID's */
	BLI_duplicatelist(&dst->id_blocks, &src->id_blocks);
}

/* Add 'group' node to graph */
static void dnti_outer_group__add_to_graph(Depsgraph *graph, DepsNode *node, ID *UNUSED(dummy))
{
	GroupDepsNode *group = (GroupDepsNode *)node;
	LinkData *ld;
	
	/* add node to toplevel */
	BLI_addtail(&graph->nodes, node);
	
	/* add all ID links that node has */
	// XXX: probably there won't actually be any, unless we hijack the adding process...
	for (ld = group->id_blocks.first; ld; ld = ld->next) {
		ID *id = (ID *)ld->data;
		BLI_ghash_insert(graph->nodehash, id, node);
	}
}

/* Group Node Type Info */
static DepsNodeTypeInfo DNTI_OUTER_GROUP = {
	/* type */               DEPSNODE_TYPE_OUTER_GROUP,
	/* size */               sizeof(GroupDepsNode),
	/* name */               "ID Group Node",
	
	/* init_data() */        NULL,
	/* free_data() */        NULL,
	/* copy_data() */        dnti_outer_group__copy_data,
	
	/* add_to_graph() */     dnti_outer_group__add_to_graph,
	/* remove_from_graph()*/ dnti_outer_group__remove_from_graph,
	
	/* match_outer() */      NULL, // XXX...
	
	/* build_subgraph() */   NULL
};

/* Data Node ============================================== */

/* Add 'data' node to graph */
static void dnti_data__add_to_graph(Depsgraph *graph, DepsNode *node, ID *id)
{
	DepsNode *id_node;
	
	/* find parent for this node */
	id_node = DEG_find_node(graph, DEPSNODE_TYPE_OUTER_ID, id, NULL, NULL);
	BLI_assert(id_node != NULL);
	
	/* attach to owner */
	node->owner = id_node;
	
	if (id_node->type == DEPSNODE_TYPE_OUTER_ID) {
		IDDepsNode *id_data = (IDDepsNode *)id_node;
		
		/* ID Node - data node is "subdata" here... */
		BLI_addtail(&id_data->subdata, node);
	}
	else {
		GroupDepsNode *grp_data = (GroupDepsNode *)id_node;
		
		/* Group Node */
		// XXX: for quicker checks, it may be nice to be able to have "ID + data" subdata node hash?
		BLI_addtail(&grp_data->subdaa, node);
	}
}


/* Data Node Type Info */
static DepsNodeTypeInfo DNTI_DATA = {
	/* type */               DEPSNODE_TYPE_DATA,
	/* size */               sizeof(DataDepsNode),
	/* name */               "ID Group Node",
	
	/* init_data() */        NULL,
	/* free_data() */        NULL,
	/* copy_data() */        NULL,
	
	/* add_to_graph() */     dnti_data__add_to_graph,
	/* remove_from_graph()*/ dnti_data__remove_from_graph,
	
	/* match_outer() */      NULL, // XXX...
	
	/* build_subgraph() */   NULL
};

/* ******************************************************** */
/* Inner Nodes */

/* ******************************************************** */
/* Internal API */

/* Make a group from the two given outer nodes */
DepsNode *DEG_group_cyclic_node_pair(Depsgraph *graph, DepsNode *node1, DepsNode *node2)
{
	// TODO...
	return NULL;
}

/* ******************************************************** */
/* External API */

/* Global type registry */

/* NOTE: For now, this is a hashtable not array, since the core node types
 * currently do not have contiguous ID values. Using a hash here gives us
 * more flexibility, albeit using more memory and also sacrificing a little
 * speed. Later on, when things stabilise we may turn this back to an array
 * since there are only just a few node types that an array would cope fine...
 */
static GHash *_depsnode_typeinfo_registry = NULL;

/* Registration ------------------------------------------- */

/* Register node type */
void DEG_register_node_typeinfo(DepsNodeTypeInfo *typeinfo)
{
	if (typeinfo) {
		eDepsNode_Type type = typeinfo->type;
		BLI_ghash_insert(_depsnode_typeinfo_registry, SET_INT_IN_POINTER(type), typeinfo);
	}
}


/* Register all node types */
void DEG_register_node_types(void)
{
	/* initialise registry */
	_depsnode_typeinfo_registry = BLI_ghash_int_new(__func__);
	
	/* register node types */
	DEG_register_node_typeinfo(DNTI_OUTER_ID);
	DEG_register_node_typeinfo(DNTI_OUTER_GROUP);
	DEG_register_node_typeinfo(DNTI_OUTER_OP);
	
	DEG_register_node_typeinfo(DNTI_DATA);
	
	// ...
}

/* Free registry on exit */
void DEG_free_node_types(void)
{
	BLI_ghash_free(_depsnode_typeinfo_registry, NULL, NULL);
}

/* Getters ------------------------------------------------- */

/* Get typeinfo for specified type */
DepsNodeTypeInfo *DEG_get_node_typeinfo(eDepsNode_Type type)
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
		nti = DEG_get_node_typeinfo(node->type);
	}
	return nti;
}

/* ******************************************************** */

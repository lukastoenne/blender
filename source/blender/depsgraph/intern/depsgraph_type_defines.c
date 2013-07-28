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
/* Generic Nodes */

/* ID Node ================================================ */

/* Initialise 'id' node - from pointer data given */
static void dnti_id_ref__init_data(DepsNode *node, ID *id)
{
	IDDepsNode *id_node = (IDDepsNode *)node;
	
	/* store ID-pointer */
	BLI_assert(id != NULL);
	id_node->id = id;
	
	/* init components hash - eDepsNode_Type : ComponentDepsNode */
	id_node->component_hash = BLI_ghash_int_new("IDDepsNode Component Hash");
	
	/* create components 
	 * NOTE: we do this in advance regardless of whether there will actually be anything inside them...
	 *       This will probably be wasteful, but at least helps us catch valid things...
	 */
	deg_idnode_components_init(id_node);
}

/* Helper for freeing ID nodes - Used by component hash to free data... */
static void dnti_id_ref__hash_free_component(void *component_p)
{
	DEG_free_node((DepsNode *)component_p);
}

/* Free 'id' node */
static void dnti_id_ref__free_data(DepsNode *node)
{
	IDDepsNode *id_node = (IDDepsNode *)node;
	
	/* free components (and recursively, their data) while we empty the hash */
	BLI_ghash_free(id_node->component_hash, NULL, dnti_id_ref__hash_free_component);
}

/* Copy 'id' node */
static void dnti_id_ref__copy_data(DepsNode *dst, const DepsNode *src)
{
	const IDDepsNode *src_node = (const IDDepsNode *)src;
	IDDepsNode *dst_node       = (IDDepsNode *)dst;
	
	// XXX: duplicate hash...
}

/* Add 'id' node to graph */
static void dnti_id_ref__add_to_graph(Depsgraph *graph, DepsNode *node, ID *id)
{
	/* add to hash so that it can be found */
	BLI_ghash_insert(graph->id_hash, id, node);
}

/* Remove 'id' node from graph */
static void dnti_id_ref__remove_from_graph(Depsgraph *graph, DepsNode *node)
{
	/* remove toplevel node and hash entry, but don't free... */
	BLI_ghash_remove(graph->nodehash, id, NULL, NULL);
}

/* ID Node Type Info */
static DepsNodeTypeInfo DNTI_ID_REF = {
	/* type */               DEPSNODE_TYPE_ID_REF,
	/* size */               sizeof(IDDepsNode),
	/* name */               "ID Node",
	
	/* init_data() */        dnti_id_ref__init_data,
	/* free_data() */        dnti_id_ref__free_data,
	/* copy_data() */        dnti_id_ref__copy_data,
	
	/* add_to_graph() */     dnti_id_ref__add_to_graph,
	/* remove_from_graph()*/ dnti_id_ref__remove_from_graph,
};

/* ******************************************************** */
/* Outer Nodes */

/* ******************************************************** */
/* Inner Nodes */

/* AtomicOperationNode =================================== */

/* Partially initialise operation node 
 * - Just the pointer; Operation needs to be done through API in a different way
 */
/* Initialise 'data' node - from pointer data given */
static void dnti_data__init_data(DepsNode *node, ID *id)
{
	
}

/* Add 'operation' node to graph */
static void dnti_atomic_op__add_to_graph(Depsgraph *graph, DepsNode *node, ID *id)
{
	AtomicOperationDepsNode *aon = (AtomicOperationDepsNode *)node;
	DepsNode *owner_node;
	
	/* find potential owner */
	// XXX: need to review this! Maybe only toplevel outer nodes are allowed to hold "all" nodes
	//      and the inner data nodes only hold references to ones they belong to them...
	if (RNA_struct_is_ID(aon->ptr.type)) {
		/* ID */
		owner_node = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, id, NULL, NULL);
	}
	else {
		StructRNA *type = aon->ptr.type;
		void *data = aon->ptr.data;
		
		/* find relevant data node */
		// XXX: but it could also be operation?
		owner_node = DEG_get_node(graph, DEPSNODE_TYPE_DATA, id, type, data);
	}
	
	/* attach to owner */
	aon->owner = owner_node;
	if (owner_node) {
		OuterDepsNodeTemplate *outer = (OuterDepsNodeTemplate *)owner_node;
		BLI_addtail(&outer->nodes, aon);
	}
}

/* Remove 'operation' node from graph */
static void dnti_atomic_op__remove_from_graph(Depsgraph *graph, DepsNode *node)
{
	AtomicOperationDepsNode *aon = (AtomicOperationDepsNode *)node;
	
	/* detach from owner */
	if (node->owner) {
		if (DEPSNODE_IS_OUTER_NODE(node->owner)) {
			OuterDepsNodeTemplate *outer = (OuterDepsNodeTemplate *)node->owner;
			BLI_remlink(&outer->nodes, node);
		}
	}
	
	// detach relationships?
}

/* Atomic Operation Node Type Info */
static DepsNodeTypeInfo DNTI_ATOMIC_OP = {
	/* type */               DEPSNODE_TYPE_INNER_ATOM,
	/* size */               sizeof(AtomicOperationDepsNode),
	/* name */               "Atomic Operation",
	
	/* init_data() */        dnti_atomic_op__init_data,
	/* free_data() */        NULL,
	/* copy_data() */        NULL,
	
	/* add_to_graph() */     dnti_atomic_op__add_to_graph,
	/* remove_from_graph()*/ dnti_atomic_op__remove_from_graph,
};


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
	_depsnode_typeinfo_registry = BLI_ghash_int_new("Depsgraph Node Type Registry");
	
	/* register node types */
	/* GENERIC */
	DEG_register_node_typeinfo(DNTI_ROOT);
	DEG_register_node_typeinfo(DNTI_TIMESOURCE);
	DEG_register_node_typeinfo(DNTI_ID_REF);
	
	/* OUTER */
	DEG_register_node_typeinfo(DNTI_PARAMETERS);
	DEG_register_node_typeinfo(DNTI_PROXY);
	DEG_register_node_typeinfo(DNTI_ANIMATION);
	DEG_register_node_typeinfo(DNTI_TRANSFORM);
	DEG_register_node_typeinfo(DNTI_GEOMETRY);
	
	DEG_register_node_typeinfo(DNTI_EVAL_POSE);
	DEG_register_node_typeinfo(DNTI_EVAL_PARTICLES);
	
	/* INNER */
	DEG_register_node_typeinfo(DNTI_OP_PARAMETER);
	DEG_register_node_typeinfo(DNTI_OP_PROXY);
	DEG_register_node_typeinfo(DNTI_OP_ANIMATION);
	DEG_register_node_typeinfo(DNTI_OP_TRANSFORM);
	DEG_register_node_typeinfo(DNTI_OP_GEOMETRY);
	
	DEG_register_node_typeinfo(DNTI_OP_UPDATE);
	DEG_register_node_typeinfo(DNTI_OP_DRIVER);
	
	DEG_register_node_typeinfo(DNTI_OP_BONE);
	DEG_register_node_typeinfo(DNTI_OP_PARTICLE);
	DEG_register_node_typeinfo(DNTI_OP_RIGIDBODY);
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
	/* look up type - at worst, it doesn't exist in table yet, and we fail */
	return BLI_ghash_lookup(_depsnode_typeinfo_registry, type);
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

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

/* General "Outer" Node Stuff (ID/Group Nodes) ============ */


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

/* Outer node data freeing */
static void dnti_outer_node__free_data(DepsNode *node)
{
	OuterIdDepsNodeTemplate *outer = (OuterIdDepsNodeTemplate *)node;
	DepsNode *itA, *next;
	
	/* free data nodes */
	for (itA = outer->subdata.first; itA; itA = next) {
		next = itA->next;
		
		DEG_free_node(itA);
		BLI_freelinkN(&outer->subdata, itA);
	}
	
	/* free inner nodes */
	for (itA = outer->nodes.first; itA; itA = next) {
		next = itA->next;
		
		DEG_free_node(itA);
		BLI_freelinkN(&outer->nodes, itA);
	}
}

/* ID Node ================================================ */

/* Initialise 'id' node - from pointer data given */
static void dnti_outer_id__init_data(DepsNode *node, ID *id, StructRNA *UNUSED(srna), void *UNUSED(data))
{
	IDDepsNode *idnode = (IDDepsNode *)node;
	
	/* store ID-pointer */
	idnode->id = id;
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
	
	/* init_data() */        dnti_outer_id__init_data,
	/* free_data() */        dnti_outer_node__free_data,
	/* copy_data() */        dnti_outer_node__copy_data,
	
	/* add_to_graph() */     dnti_outer_id__add_to_graph,
	/* remove_from_graph()*/ dnti_outer_id__remove_from_graph,
	
	/* match_outer() */      NULL, // XXX...
	
	/* build_subgraph() */   NULL
};

/* Group Node ============================================= */

/* Free node data */
static void dnti_outer_group__free_data(DepsNode *node)
{
	GroupDepsNode *group = (GroupDepsNode *)node;
	
	/* outer-node freeing */
	dnti_outer_node__free_data(node);
	
	/* free id-refs */
	BLI_freelistN(&group->id_blocks);
}

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
	for (ld = group->id_blocks.first; ld; ld = ld->next) {
		ID *id = (ID *)ld->data;
		BLI_ghash_insert(graph->nodehash, id, node);
	}
}

/* Remove group node from graph - either when it is being merged, or when freeing the graph */
static void dnti_outer_group__remove_from_graph(Depsgraph *graph, DepsNode *node)
{
	GroupDepsNode *group = (GroupDepsNode *)node;
	LinkData *ld;
	
	/* remove toplevel node */
	BLI_remlink(&graph->nodes, node);
	
	/* remove ID links 
	 * NOTE: this list should be empty if we've already transfered all data successfully
	 *       so this shouldn't cause any problems there
	 */
	for (ld = group->id_blocks.first; ld; ld = ld->next) {
		BLI_ghash_remove(graph->nodehash, ld->data, NULL, NULL);
	}
}

/* Group Node Type Info */
static DepsNodeTypeInfo DNTI_OUTER_GROUP = {
	/* type */               DEPSNODE_TYPE_OUTER_GROUP,
	/* size */               sizeof(GroupDepsNode),
	/* name */               "ID Group Node",
	
	/* init_data() */        NULL, /* Not Needed - see DEG_group_cyclic_node_pair() */
	/* free_data() */        dnti_outer_group__free_data,
	/* copy_data() */        dnti_outer_group__copy_data,
	
	/* add_to_graph() */     dnti_outer_group__add_to_graph,
	/* remove_from_graph()*/ dnti_outer_group__remove_from_graph,
	
	/* match_outer() */      NULL, // XXX...
	
	/* build_subgraph() */   NULL
};

/* Data Node ============================================== */

/* Initialise 'data' node - from pointer data given */
static void dnti_data__init_data(DepsNode *node, ID *id, StructRNA *srna, void *data)
{
	DataDepsNode *ddn = (DataDepsNode *)node;
	
	/* create RNA pointer */
	RNA_pointer_create(id, srna, data, &ddn->ptr);
}

/* Add 'data' node to graph */
static void dnti_data__add_to_graph(Depsgraph *graph, DepsNode *node, ID *id)
{
	OuterIdDepsNodeTemplate *owner;
	DepsNode *id_node;
	
	/* get parent for this node 
	 * NOTE: this will add a new parent if it doesn't exist yet...
	 */
	id_node = DEG_get_node(graph, DEPSNODE_TYPE_OUTER_ID, id, NULL, NULL);
	BLI_assert(id_node != NULL);
	
	/* attach to owner */
	// XXX: perhaps subdata should also have a hash instead, just for quicker seeking?
	node->owner = id_node;
	
	owner = (OuterIdDepsNodeTemplate *)id_node;
	BLI_addtail(&owner->subdata, node);
}

/* Remove 'data' node from graph */
static void dnti_data__remove_from_graph(Depsgraph *graph, DepsNode *node)
{
	/* remove from owner */
	if (node->owner) {
		if (ELEM(node->owner->type, DEPSNODE_TYPE_OUTER_ID, DEPSNODE_TYPE_OUTER_GROUP)) {
			OuterIdDepsNodeTemplate *owner = (OuterIdDepsNodeTemplate *)node->owner;
			BLI_remlink(&owner->subdata, node);
		}
	}
	
	// XXX: what to do with relationships?
}

/* Check if matching data pointer has been found */
static bool dnti_data__match_outer(DepsNode *node, ID *id, StructRNA *srna, void *data)
{
	DataDepsNode *ddn = (DataDepsNode *)node;
	PointerRNA *ptr = &ddn->ptr;
	
	/* just check RNA pointer element-by-element */
	/* XXX: what if we only have a partial query?
	 *      Well, in that case, this may not really
	 *       be what we're really after anyway...
	 */
	return ((ddn->id == id) && (ddn->type == snra) && (ddn->data == data));
}

/* Data Node Type Info */
static DepsNodeTypeInfo DNTI_DATA = {
	/* type */               DEPSNODE_TYPE_DATA,
	/* size */               sizeof(DataDepsNode),
	/* name */               "Data Node",
	
	/* init_data() */        dnti_data__init_data,
	/* free_data() */        NULL,
	/* copy_data() */        NULL,
	
	/* add_to_graph() */     dnti_data__add_to_graph,
	/* remove_from_graph()*/ dnti_data__remove_from_graph,
	
	/* match_outer() */      dnti_data__match_outer,
	
	/* build_subgraph() */   NULL
};

/* ******************************************************** */
/* Inner Nodes */

/* AtomicOperationNode =================================== */

/* Partially initialise operation node 
 * - Just the pointer; Operation needs to be done through API in a different way
 */
/* Initialise 'data' node - from pointer data given */
static void dnti_data__init_data(DepsNode *node, ID *id, StructRNA *srna, void *data)
{
	AtomicOperationDepsNode *aon = (AtomicOperationDepsNode *)node;
	
	/* create RNA pointer */
	RNA_pointer_create(id, srna, data, &aon->ptr);
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
	
	/* match_outer() */      dnti_atomic_op__match_outer,
	
	/* build_subgraph() */   NULL
};

/* ******************************************************** */
/* Internal API */

/* Groups ================================================== */

/* Add ID dependency to a group 
 * ! If graph is not NULL, nodehash will be updated to point to group...
 */
static void deg_group_add_id_ref(Depsgraph *graph, GroupDepsNode *group, ID *id)
{
	/* add ID reference to group's container */
	BLI_addtail(&group->id_blocks, BLI_genericNodeN(id));
	
	/* make nodehash point to group (for lookups of id), 
	 * but only if caller expects us to do so (graph != NULL) 
	 */
	if (graph) {
		BLI_ghash_insert(graph->nodehash, id, group);
	}
}

/* Transfer links from ID/Group node over to Group 
 * > group: the group where data should be sent
 * < src: the ID/Group node where data is coming from
 */
static void deg_group_add_nodedata(Depsgraph *graph, GroupDepsNode *group, OuterIdDepsNodeTemplate *src)
{
	DepsNode *node;
	
	/* redirect relationships from src to group - all links hold */
	DEPSNODE_RELATIONS_ITER_BEGIN(src->nd.inlinks.first, rel)
	{	
		if (rel->to == src)
			rel->to = group;
	}
	DEPSNODE_RELATIONS_ITER_END;
	
	DEPSNODE_RELATIONS_ITER_BEGIN(src->nd.outlinks.first, rel)
	{
		if (rel->from == src)
			rel->from = group;
	}
	DEPSNODE_RELATIONS_ITER_END;
	
	/* redirect owner values to point to group... */
	for (node = src->subdata.first; node; node = node->next) {
		/* BLI_assert(node->owner == src); */
		node->owner = group;
	}
	
	for (node = src->nodes.first; node; node = node->next) {
		/* some may point to subdata node, which will just be transfered straight across... */
		if (node->owner == src)
			node->owner = group;
	}
	
	/* move the lists over directly */
	BLI_movelisttolist(&group->nd.inlinks, &src->nd.inlinks);
	BLI_movelisttolist(&group->nd.outlinks, &src->nd.outlinks);
	
	BLI_movelisttolist(&group->subdata, &src->subdata);
	BLI_movelisttolist(&group->nodes, &src->nodes);
}

/* Make a group from the two given outer nodes */
DepsNode *DEG_group_cyclic_node_pair(Depsgraph *graph, DepsNode *node1, DepsNode *node2)
{
	const eDepsNode_Type t1 = node1->type;
	const eDepsNode_Type t2 = node2->type;
	
	DepsNode *result = NULL;
	
	/* check node types to see what scenario we're dealing with... */
	if ((t1 == DEPSNODE_TYPE_OUTER_ID) && (t2 == DEPSNODE_TYPE_OUTER_ID)) {
		/* create new group, and add both to it */
		IDDepsNode *id1 = (IDDepsNode *)node1;
		IDDepsNode *id2 = (IDDepsNode *)node2;
		GroupDepsNode *group;
		
		/* create group... */
		result = DEG_create_node(DEPSNODE_TYPE_OUTER_GROUP);
		group = (GroupDepsNode *)result;
		
		/* transfer node data */
		deg_group_add_nodedata(graph, group, (OuterIdDepsNodeTemplate *)id1);
		deg_group_add_nodedata(graph, group, (OuterIdDepsNodeTemplate *)id2);
		
		/* remove old ID nodes from graph */
		DEG_remove_node(graph, node1);
		DEG_remove_node(graph, node2);
		
		/* re-add these ID's as part of headliner section of group 
		 * (NOTE: no need to flush to nodehash, as group isn't part of graph yet)
		 */
		deg_group_add_id_ref(NULL, group, id1->id);
		deg_group_add_id_ref(NULL, group, id2->id);
		
		/* free old ID-nodes */
		DEG_free_node(node1);
		DEG_free_node(node2);
		
		MEM_freeN(node1);
		MEM_freeN(node2);
		
		/* add group to graph */
		DEG_add_node(graph, result, NULL);
	}
	else if ((t1 == DEPSNODE_TYPE_OUTER_GROUP) && (t2 == DEPSNODE_TYPE_OUTER_GROUP)) {
		/* merge the groups - node1 becomes base */
		GroupDepsNode *g1 = (GroupDepsNode *)node1;
		GroupDepsNode *g2 = (GroupDepsNode *)node2;
		LinkData *ld;
		
		/* redirect node-hash + ID-link references 
		 * NOTE: perform this inline, since we're just shifting/replacing links not making new ones
		 */
		for (ld = g2->id_blocks.first; ld; ld = ld->next) {
			BLI_ghash_remove(graph->nodehash, ld->data, NULL, NULL);
			BLI_ghash_insert(graph->nodehash, ld->data, g1);
		}
		BLI_movelisttolist(&g1->id_blocks, &g2->id_blocks);
		
		/* copy over node2's data */
		deg_group_add_nodedata(graph, group, (OuterIdDepsNodeTemplate *)node2);
		
		/* remove and free node2 */
		DEG_remove_node(node2);
		DEG_free_node(node2);
		MEM_freeN(node2);
		
		/* node 1 is now the combined group */
		result = node1;
	}
	else {
		/* add ID to whatever one is a group */
		GroupDepsNode *group;
		OuterIdDepsNodeTemplate *idnode;
		
		if (t1 == DEPSNODE_TYPE_OUTER_GROUP) {
			result = node1;
			
			group = (GroupDepsNode *)node1;
			idnode = (OuterIdDepsNodeTemplate *)node2;
		}
		else {
			result = node2;
			
			group = (GroupDepsNode *)node2;
			idnode = (OuterIdDepsNodeTemplate *)node1;
		}
		
		/* add ID's data to this group */
		deg_group_add_nodedata(graph, group, idnode);
		
		/* remove old ID node from the graph, and assign that ref to group instead */
		DEG_remove_node(graph, idnode);
		deg_group_add_id_ref(graph, group, idnode->id);
		
		/* free old ID node */
		DEG_free_node(idnode);
		MEM_freeN(idnode);
	}
	
	/* return merged group */
	return result;
}

/* Atomic Operations ====================================== */

// XXX: todo...

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
	
	DEG_register_node_typeinfo(DNTI_ATOMIC_OP);
	
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

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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenjit/intern/codegen.cpp
 *  \ingroup bjit
 */

#include <map>
#include <string>
#include <vector>

extern "C" {
#include "BLI_utildefines.h"

#include "DNA_node_types.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"

#include "BKE_effect.h"
#include "BKE_node.h"
}

#include "BJIT_nodes.h"
#include "bjit_intern.h"

namespace bjit {

using namespace llvm;

struct NodeType;


struct NodeSocket {
	NodeSocket(const std::string &name) :
	    name(name)
	{}
	~NodeSocket()
	{}
	
	std::string name;
};

struct NodeType {
	typedef std::vector<NodeSocket> SocketList;
	
	NodeType(const std::string &name) :
	    name(name)
	{}
	~NodeType()
	{}
	
	const NodeSocket *find_input(int index) const { return &inputs[index]; }
	const NodeSocket *find_output(int index) const { return &outputs[index]; }
	
	const NodeSocket *find_input(const std::string &name) const
	{
		for (SocketList::const_iterator it = inputs.begin(); it != inputs.end(); ++it) {
			const NodeSocket &socket = *it;
			if (socket.name == name)
				return &socket;
		}
		return NULL;
	}
	
	const NodeSocket *find_output(const std::string &name) const
	{
		for (SocketList::const_iterator it = outputs.begin(); it != outputs.end(); ++it) {
			const NodeSocket &socket = *it;
			if (socket.name == name)
				return &socket;
		}
		return NULL;
	}
	
	/* stub implementation in case socket is passed directly */
	const NodeSocket *find_input(const NodeSocket *socket) const { return socket; }
	const NodeSocket *find_output(const NodeSocket *socket) const { return socket; }
	
	std::string name;
	SocketList inputs;
	SocketList outputs;
};

struct NodeInstance {
	struct InputInstance {
		NodeInstance *link_node;
		const NodeSocket *link_socket;
		Value *value;
	};
	
	struct OutputInstance {
		Value *value;
	};
	
	typedef std::map<std::string, InputInstance> InputMap;
	typedef std::pair<std::string, InputInstance> InputPair;
	typedef std::map<std::string, OutputInstance> OutputMap;
	typedef std::pair<std::string, OutputInstance> OutputPair;
	
	NodeInstance(const NodeType *type, const std::string &name) :
	    type(type), name(name)
	{}

	~NodeInstance()
	{}
	
	bool set_input_value(const std::string &name, Value *value)
	{
		InputInstance &input = inputs[name];
		if (input.value)
			return false;
		input.value = value;
		return true;
	}
	
	bool set_input_link(const std::string &name, NodeInstance *from_node, const NodeSocket *from_socket)
	{
		InputInstance &input = inputs[name];
		if (input.link_node || input.link_socket)
			return false;
		
		input.link_node = from_node;
		input.link_socket = from_socket;
		return true;
	}
	
	const NodeType *type;
	std::string name;
	InputMap inputs;
	OutputMap outputs;
	CallInst *call_inst;
};

struct NodeTree {
	typedef std::map<std::string, NodeType> NodeTypeMap;
	typedef std::pair<std::string, NodeType> NodeTypeMapPair;
	typedef std::map<std::string, NodeInstance> NodeInstanceMap;
	typedef std::pair<std::string, NodeInstance> NodeInstanceMapPair;
	
	static NodeTypeMap node_types;
	
	static const NodeType *find_node_type(const std::string &name)
	{
		NodeTypeMap::const_iterator it = node_types.find(name);
		return (it != node_types.end())? &it->second : NULL;
	}
	
	NodeTree()
	{}
	~NodeTree()
	{}
	
	NodeInstance *get_node(const std::string &name)
	{
		NodeInstanceMap::iterator it = nodes.find(name);
		return (it != nodes.end())? &it->second : NULL;
	}
	
	NodeInstance *add_node(const std::string &type, const std::string &name)
	{
		const NodeType *nodetype = find_node_type(type);
		std::pair<NodeInstanceMap::iterator, bool> result =
		        nodes.insert(NodeInstanceMapPair(name, NodeInstance(nodetype, name)));
		
		return (result.second)? &result.first->second : NULL;
	}
	
	template <typename FromT, typename ToT>
	bool add_link(NodeInstance *from_node, FromT from,
	              NodeInstance *to_node, ToT to)
	{
		if (!to_node || !from_node)
			return false;
		
		const NodeSocket *from_socket = from_node->type->find_output(from);
		const NodeSocket *to_socket = to_node->type->find_input(to);
		if (!from_socket || !to_socket)
			return false;
		
		to_node->set_input_link(to_socket->name, from_node, from_socket);
		
		return true;
	}
	
	template <typename FromT, typename ToT>
	bool add_link(const std::string &from_node, FromT from,
	              const std::string &to_node, ToT to)
	{
		return add_link(get_node(from_node), from, get_node(to_node), to);
	}
	
	NodeInstanceMap nodes;
};

/* ========================================================================= */

template <typename T>
struct NodeTreeBuilder;

/* ------------------------------------------------------------------------- */
/* bNodeTree */

template <>
struct NodeTreeBuilder<bNodeTree> {
	NodeTree build(bNodeTree *btree)
	{
		NodeTree tree;
		
		for (bNode *bnode = (bNode*)btree->nodes.first; bnode; bnode = bnode->next) {
			BLI_assert(bnode->typeinfo != NULL);
			if (!nodeIsRegistered(bnode))
				continue;
			
			const char *type = bnode->typeinfo->idname;
			/*NodeInstance *node =*/ tree.add_node(type, bnode->name);
		}
		
		for (bNodeLink *blink = (bNodeLink *)btree->links.first; blink; blink = blink->next) {
			if (!(blink->flag & NODE_LINK_VALID))
				continue;
			
			tree.add_link(blink->fromnode->name, blink->fromsock->name,
			              blink->tonode->name, blink->tosock->name);
		}
		
		return tree;
	}
};

/* ------------------------------------------------------------------------- */
/* Effectors */

static std::string get_effector_prefix(short forcefield)
{
	switch (forcefield) {
		case PFIELD_FORCE:
			return "force";
		case PFIELD_WIND:
			return "wind";
			
		case PFIELD_NULL:
		case PFIELD_VORTEX:
		case PFIELD_MAGNET:
		case PFIELD_GUIDE:
		case PFIELD_TEXTURE:
		case PFIELD_HARMONIC:
		case PFIELD_CHARGE:
		case PFIELD_LENNARDJ:
		case PFIELD_BOID:
		case PFIELD_TURBULENCE:
		case PFIELD_DRAG:
		case PFIELD_SMOKEFLOW:
			return "";
		
		default: {
			/* unknown type, should not happen */
			BLI_assert(false);
			return "";
		}
	}
	return "";
}

template <>
struct NodeTreeBuilder<EffectorContext> {
	NodeTree build(EffectorContext *effctx)
	{
		NodeTree tree;
		NodeInstance *node_prev = NULL;
		const NodeSocket *socket_prev = NULL;
		
		for (EffectorCache *eff = (EffectorCache *)effctx->effectors.first; eff; eff = eff->next) {
			if (!eff->ob || !eff->pd)
				continue;
			
			std::string prefix = get_effector_prefix(eff->pd->forcefield);
			if (prefix.empty()) {
				/* undefined force type */
				continue;
			}
			
			std::string nodetype = "effector_" + prefix + "_eval";
			std::string nodename = eff->ob->id.name;
			NodeInstance *node = tree.add_node(nodetype, nodename);
			const NodeSocket *socket = node->type->find_output(0);
			
			if (node_prev && socket_prev) {
				std::string combinename = "combine_" + node_prev->name + "_" + node->name;
				NodeInstance *combine = tree.add_node("effector_result_combine", combinename);
				
				tree.add_link(node_prev, socket_prev,
				              combine, combine->type->find_input(0));
				tree.add_link(node, socket,
				              combine, combine->type->find_input(1));
				
				node_prev = combine;
				socket_prev = combine->type->find_output(0);
			}
			else {
				node_prev = node;
				socket_prev = socket;
			}
		}
		
		return tree;
	}
};


template <typename T>
struct ListBaseIterator {
	ListBaseIterator() :
	    link(NULL)
	{}

	ListBaseIterator(const ListBase &lb) :
	    link((Link *)lb.first)
	{}
	
	ListBaseIterator& operator++ (void)
	{
		link = link->next;
		return *this;
	}
	
	T* operator* (void)
	{
		return (T *)link;
	}
	
private:
	Link *link;
};

} /* namespace bjit */

/* ------------------------------------------------------------------------- */

using namespace bjit;

void *BJIT_build_nodetree_function(bNodeTree *ntree)
{
	NodeTreeBuilder<bNodeTree> builder;
	
	NodeTree tree = builder.build(ntree);
	
	return NULL;
}

void BJIT_free_nodetree_function(void *func)
{
	
}

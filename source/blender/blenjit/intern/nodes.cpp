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
	typedef std::map<std::string, NodeSocket> SocketMap;
	typedef std::pair<std::string, NodeSocket> SocketMapPair;
	
	NodeType(const std::string &name) :
	    name(name)
	{}
	~NodeType()
	{}
	
	const NodeSocket *find_input(const std::string &name) const
	{
		SocketMap::const_iterator it = inputs.find(name);
		return (it != inputs.end())? &it->second : NULL;
	}
	
	const NodeSocket *find_output(const std::string &name) const
	{
		SocketMap::const_iterator it = outputs.find(name);
		return (it != outputs.end())? &it->second : NULL;
	}
	
	std::string name;
	SocketMap inputs;
	SocketMap outputs;
};

struct NodeInstance {
	typedef NodeType::SocketMap SocketTypeMap;
	typedef NodeType::SocketMapPair SocketTypePair;
	
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
	
	bool add_link(const std::string &from_node, const std::string &from_socket,
	              const std::string &to_node, const std::string &to_socket)
	{
		NodeInstance *tonode = get_node(to_node);
		NodeInstance *fromnode = get_node(from_node);
		const NodeSocket *fromsocket = fromnode->type->find_output(from_socket);
		
		if (!tonode || !fromnode || !fromsocket)
			return false;
		
		tonode->set_input_link(to_socket, fromnode, fromsocket);
		
		return true;
	}
	
	NodeInstanceMap nodes;
};

/* ------------------------------------------------------------------------- */

template <typename T>
struct NodeTreeBuilder;

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

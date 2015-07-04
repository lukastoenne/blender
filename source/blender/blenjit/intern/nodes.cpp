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

#include "bjit_intern.h"

namespace bjit {

using namespace llvm;

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
	
	std::string name;
	SocketMap inputs;
	SocketMap outputs;
};

struct NodeInstance {
	typedef std::map<std::string, Value *> SocketMap;
	typedef std::pair<std::string, Value *> SocketMapPair;
	typedef NodeType::SocketMap SocketTypeMap;
	typedef NodeType::SocketMapPair SocketTypeMapPair;
	
	NodeInstance(const NodeType *type, const std::string &name) :
	    type(type), name(name)
	{}

	~NodeInstance()
	{}
	
	const NodeType *type;
	std::string name;
	SocketMap inputs;
	SocketMap outputs;
	CallInst *call_inst;
};

struct NodeLink {
	NodeLink(NodeInstance *from_node, NodeSocket *from_socket, NodeInstance *to_node, NodeSocket *to_socket) :
	    from_node(from_node), from_socket(from_socket), to_node(to_node), to_socket(to_socket)
	{}
	~NodeLink()
	{}
	
	NodeInstance *from_node;
	NodeSocket *from_socket;
	NodeInstance *to_node;
	NodeSocket *to_socket;
};

template <typename Traits>
struct NodeTree {
	typedef std::map<std::string, NodeType> NodeTypeMap;
	typedef std::pair<std::string, NodeType> NodeTypeMapPair;
	typedef std::map<std::string, NodeInstance> NodeInstanceMap;
	typedef std::pair<std::string, NodeInstance> NodeInstanceMapPair;
	
	NodeTree()
	{}
	~NodeTree()
	{}
	
	static NodeType *find_node_type(const std::string &name)
	{
		NodeTypeMap::const_iterator it = node_types.find(name);
		return (it != node_types.end())? &it->second : NULL;
	}
	
	NodeInstance *get_node(const std::string &name)
	{
		NodeInstanceMap::iterator it = nodes.find(name);
		return (it != nodes.end())? &it->second : NULL;
	}

	NodeInstance *add_node(const std::string &type, const std::string &name)
	{
		NodeType *nodetype = find_node_type(type);
		std::pair<NodeInstanceMap::iterator, bool> result =
		        nodes.insert(NodeInstanceMapPair(name, NodeInstance(nodetype, name)));
		
		return (result.second)? &result.first->second : NULL;
	}
	
	static NodeTypeMap node_types;
	NodeInstanceMap nodes;
};

/* ------------------------------------------------------------------------- */

template <typename T>
struct NodeTreeBuilder;


//template <>
//struct NodeTreeBuilder<bNodeTree>;

} /* namespace bjit */

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

/** \file blender/blenjit/intern/nodegraph.cpp
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

#include "bjit_nodegraph.h"
#include "BJIT_nodes.h"

namespace bjit {

using namespace llvm;

NodeSocket::NodeSocket(const std::string &name) :
    name(name)
{
}

NodeSocket::~NodeSocket()
{
}

/* ------------------------------------------------------------------------- */

NodeType::NodeType(const std::string &name) :
    name(name)
{
}

NodeType::~NodeType()
{
}

const NodeSocket *NodeType::find_input(int index) const
{
	return &inputs[index];
}

const NodeSocket *NodeType::find_output(int index) const
{
	return &outputs[index];
}

const NodeSocket *NodeType::find_input(const std::string &name) const
{
	for (SocketList::const_iterator it = inputs.begin(); it != inputs.end(); ++it) {
		const NodeSocket &socket = *it;
		if (socket.name == name)
			return &socket;
	}
	return NULL;
}

const NodeSocket *NodeType::find_output(const std::string &name) const
{
	for (SocketList::const_iterator it = outputs.begin(); it != outputs.end(); ++it) {
		const NodeSocket &socket = *it;
		if (socket.name == name)
			return &socket;
	}
	return NULL;
}

/* stub implementation in case socket is passed directly */
const NodeSocket *NodeType::find_input(const NodeSocket *socket) const
{
	return socket;
}

const NodeSocket *NodeType::find_output(const NodeSocket *socket) const
{
	return socket;
}

/* ------------------------------------------------------------------------- */

NodeInstance::NodeInstance(const NodeType *type, const std::string &name) :
    type(type), name(name)
{
}

NodeInstance::~NodeInstance()
{
}

bool NodeInstance::set_input_value(const std::string &name, Value *value)
{
	InputInstance &input = inputs[name];
	if (input.value)
		return false;
	input.value = value;
	return true;
}

bool NodeInstance::set_input_link(const std::string &name, NodeInstance *from_node, const NodeSocket *from_socket)
{
	InputInstance &input = inputs[name];
	if (input.link_node || input.link_socket)
		return false;
	
	input.link_node = from_node;
	input.link_socket = from_socket;
	return true;
}

/* ------------------------------------------------------------------------- */

const NodeType *NodeGraph::find_node_type(const std::string &name)
{
	NodeTypeMap::const_iterator it = node_types.find(name);
	return (it != node_types.end())? &it->second : NULL;
}

NodeGraph::NodeGraph()
{
}

NodeGraph::~NodeGraph()
{
}

NodeInstance *NodeGraph::get_node(const std::string &name)
{
	NodeInstanceMap::iterator it = nodes.find(name);
	return (it != nodes.end())? &it->second : NULL;
}

NodeInstance *NodeGraph::add_node(const std::string &type, const std::string &name)
{
	const NodeType *nodetype = find_node_type(type);
	std::pair<NodeInstanceMap::iterator, bool> result =
	        nodes.insert(NodeInstanceMapPair(name, NodeInstance(nodetype, name)));
	
	return (result.second)? &result.first->second : NULL;
}

/* ========================================================================= */

std::string get_effector_prefix(short forcefield)
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

} /* namespace bjit */

/* ------------------------------------------------------------------------- */

using namespace bjit;

void *BJIT_build_nodetree_function(bNodeTree *ntree)
{
	NodeGraphBuilder<bNodeTree> builder;
	
	NodeGraph graph = builder.build(ntree);
	
	return NULL;
}

void BJIT_free_nodetree_function(void *func)
{
	
}

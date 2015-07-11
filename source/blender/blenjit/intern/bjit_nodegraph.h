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

/** \file blender/blenjit/intern/bjit_nodegraph.h
 *  \ingroup bjit
 */

#ifndef __BJIT_NODEGRAPH_H__
#define __BJIT_NODEGRAPH_H__

#include <string>
#include <iostream>
#include <map>
#include <vector>
#include <type_traits>

extern "C" {
#include "BLI_utildefines.h"

#include "DNA_node_types.h"

#include "BKE_node.h"
}

#include "bjit_llvm.h"
#include "bjit_types.h"
#include "bjit_intern.h"

namespace bjit {

using namespace llvm;

typedef enum {
	BJIT_TYPE_FLOAT,
	BJIT_TYPE_INT,
	BJIT_TYPE_VEC3,
	
	BJIT_NUMTYPES
} SocketTypeID;

Type *bjit_get_socket_llvm_type(SocketTypeID type, LLVMContext &context);

template <typename T> Constant *bjit_get_socket_llvm_constant(SocketTypeID type, T value, LLVMContext &context);

/* ------------------------------------------------------------------------- */

template <SocketTypeID type>
struct SocketTypeImpl;

template <> struct SocketTypeImpl<BJIT_TYPE_FLOAT> {
	typedef FP type;
	typedef float extern_type;
	
	static Constant *create_constant(float value, LLVMContext &context)
	{
		return ConstantFP::get(context, APFloat(value));
	}
	
	template <typename T>
	static Constant *create_constant(T /*value*/, LLVMContext &/*context*/)
	{
		return NULL;
	}
};

template <> struct SocketTypeImpl<BJIT_TYPE_INT> {
	typedef typename types::i<32> type;
	typedef int32_t extern_type;
	
	static Constant *create_constant(int value, LLVMContext &context)
	{
		return ConstantInt::get(context, APInt(32, value));
	}
	
	template <typename T>
	static Constant *create_constant(T /*value*/, LLVMContext &/*context*/)
	{
		return NULL;
	}
};

template <> struct SocketTypeImpl<BJIT_TYPE_VEC3> {
	typedef vec3_t type;
	typedef float extern_type[3];
	
	static Constant *create_constant(const float *value, LLVMContext &context)
	{
		return ConstantDataArray::get(context, ArrayRef<float>(value, 3));
	}
	
	template <typename T>
	static Constant *create_constant(T /*value*/, LLVMContext &/*context*/)
	{
		return NULL;
	}
};

/* ========================================================================= */
/* inline functions */

namespace internal {
} /* namespace internal */

template <typename T>
Constant *bjit_get_socket_llvm_constant(SocketTypeID type, T value, LLVMContext &context)
{
	switch (type) {
		case BJIT_TYPE_FLOAT:
			return SocketTypeImpl<BJIT_TYPE_FLOAT>::create_constant(value, context);
		case BJIT_TYPE_INT:
			return SocketTypeImpl<BJIT_TYPE_INT>::create_constant(value, context);
		case BJIT_TYPE_VEC3:
			return SocketTypeImpl<BJIT_TYPE_VEC3>::create_constant(value, context);
		default:
			return NULL;
	}
	return NULL;
}

/* ========================================================================= */

struct NodeType;

struct NodeSocket {
	NodeSocket(const std::string &name, SocketTypeID type, Constant *default_value);
	~NodeSocket();
	
	std::string name;
	SocketTypeID type;
	llvm::Constant *default_value;
};

struct NodeType {
	typedef std::vector<NodeSocket> SocketList;
	
	NodeType(const std::string &name);
	~NodeType();
	
	const NodeSocket *find_input(int index) const;
	const NodeSocket *find_output(int index) const;
	const NodeSocket *find_input(const std::string &name) const;
	const NodeSocket *find_output(const std::string &name) const;
	/* stub implementation in case socket is passed directly */
	const NodeSocket *find_input(const NodeSocket *socket) const;
	const NodeSocket *find_output(const NodeSocket *socket) const;
	
	const NodeSocket *add_input(const std::string &name, SocketTypeID type, Constant *default_value);
	const NodeSocket *add_output(const std::string &name, SocketTypeID type, Constant *default_value);
	
	template <typename T>
	const NodeSocket *add_input(const std::string &name, SocketTypeID type, const T &default_value, LLVMContext &context)
	{
		return add_input(name, type, bjit_get_socket_llvm_constant(type, default_value, context));
	}
	
	template <typename T>
	const NodeSocket *add_output(const std::string &name, SocketTypeID type, const T &default_value, LLVMContext &context)
	{
		return add_output(name, type, bjit_get_socket_llvm_constant(type, default_value, context));
	}
	
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
	
	NodeInstance(const NodeType *type, const std::string &name);
	~NodeInstance();
	
	NodeInstance *find_input_link_node(const std::string &name) const;
	NodeInstance *find_input_link_node(int index) const;
	const NodeSocket *find_input_link_socket(const std::string &name) const;
	const NodeSocket *find_input_link_socket(int index) const;
	Value *find_input_value(const std::string &name) const;
	Value *find_input_value(int index) const;
	Value *find_output_value(const std::string &name) const;
	Value *find_output_value(int index) const;
	
	bool set_input_value(const std::string &name, Value *value);
	bool set_input_link(const std::string &name, NodeInstance *from_node, const NodeSocket *from_socket);
	bool set_output_value(const std::string &name, Value *value);
	
	const NodeType *type;
	std::string name;
	InputMap inputs;
	OutputMap outputs;
	CallInst *call_inst;
};

struct NodeGraph {
	struct Input {
		Input(const std::string &name, SocketTypeID type) : name(name), type(type)
		{}
		std::string name;
		SocketTypeID type;
	};
	struct Output {
		Output(const std::string &name, SocketTypeID type, Constant *default_value) :
		    name(name), type(type), default_value(default_value), link_node(NULL), link_socket(NULL)
		{}
		std::string name;
		SocketTypeID type;
		Constant *default_value;
		
		NodeInstance *link_node;
		const NodeSocket *link_socket;
	};
	typedef std::vector<Input> InputList;
	typedef std::vector<Output> OutputList;
	
	typedef std::map<std::string, NodeType> NodeTypeMap;
	typedef std::pair<std::string, NodeType> NodeTypeMapPair;
	typedef std::map<std::string, NodeInstance> NodeInstanceMap;
	typedef std::pair<std::string, NodeInstance> NodeInstanceMapPair;
	
	
	static NodeTypeMap node_types;
	
	static const NodeType *find_node_type(const std::string &name);
	static NodeType *add_node_type(const std::string &name);
	static void remove_node_type(const std::string &name);
	
	NodeGraph();
	~NodeGraph();
	
	NodeInstance *get_node(const std::string &name);
	NodeInstance *add_node(const std::string &type, const std::string &name);
	
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
	
	const Input *get_input(int index) const;
	const Output *get_output(int index) const;
	const Input *get_input(const std::string &name) const;
	const Output *get_output(const std::string &name) const;
	const Input *add_input(const std::string &name, SocketTypeID type);
	const Output *add_output(const std::string &name, SocketTypeID type, Constant *default_value);
	void set_output_link(const std::string &name, NodeInstance *link_node, const std::string &link_socket);
	
	template <typename T>
	const Output *add_output(const std::string &name, SocketTypeID type, const T &default_value, LLVMContext &context)
	{
		return add_output(name, type, bjit_get_socket_llvm_constant(type, default_value, context));
	}
	
	void dump(std::ostream &stream = std::cout);
	
	NodeInstanceMap nodes;
	InputList inputs;
	OutputList outputs;
};

/* ========================================================================= */

template <typename type>
struct NodeGraphBuilder;

/* ------------------------------------------------------------------------- */
/* bNodeTree */

template <>
struct NodeGraphBuilder<bNodeTree> {
	NodeGraph build(bNodeTree *btree)
	{
		NodeGraph tree;
		
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

} /* namespace bjit */

#endif

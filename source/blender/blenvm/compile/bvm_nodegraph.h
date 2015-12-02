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

/** \file blender/blenvm/intern/bvm_nodegraph.h
 *  \ingroup bvm
 */

#ifndef __BVM_NODEGRAPH_H__
#define __BVM_NODEGRAPH_H__

#include <stdlib.h>
#include <string>
#include <iostream>
#include <map>
#include <vector>

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_utildefines.h"
#if 0
#include "DNA_node_types.h"
#include "BKE_node.h"
#endif
}

#include "bvm_opcode.h"
#include "bvm_util_string.h"
#include "bvm_util_typedesc.h"

namespace bvm {

struct NodeGraph;
struct NodeType;
struct NodeInstance;

struct NodeSocket {
	NodeSocket(const string &name,
	           const TypeDesc &typedesc,
	           Value *default_value,
	           BVMValueType value_type);
	~NodeSocket();
	
	string name;
	TypeDesc typedesc;
	Value *default_value;
	BVMValueType value_type;
};

struct NodeType {
	typedef std::vector<NodeSocket> SocketList;
	
	NodeType(const string &name, bool is_kernel_node, bool is_pass_node = false);
	~NodeType();
	
	const string &name() const { return m_name; }
	bool is_kernel_node() const { return m_is_kernel_node; }
	bool is_pass_node() const { return m_is_pass_node; }
	
	int num_inputs() const { return inputs.size(); }
	int num_outputs() const { return outputs.size(); }
	
	const NodeSocket *find_input(int index) const;
	const NodeSocket *find_output(int index) const;
	const NodeSocket *find_input(const string &name) const;
	const NodeSocket *find_output(const string &name) const;
	/* stub implementation in case socket is passed directly */
	const NodeSocket *find_input(const NodeSocket *socket) const;
	const NodeSocket *find_output(const NodeSocket *socket) const;
	
//	bool verify_argument_socket(NodeSocket &socket, Type *type, int index,
//	                            Module *module, LLVMContext &context, raw_ostream &err);
//	bool verify_arguments(Module *module, LLVMContext &context, raw_ostream &err);
	
	const NodeSocket *add_input(const string &name,
	                            BVMType type,
	                            Value *default_value,
	                            BVMValueType value_type = VALUE_VARIABLE);
	const NodeSocket *add_output(const string &name,
	                             BVMType type,
	                             Value *default_value);
	
	template <typename T>
	const NodeSocket *add_input(const string &name,
	                            BVMType type,
	                            T default_value,
	                            BVMValueType value_type = VALUE_VARIABLE)
	{
		Value *c = Value::create(type, default_value);
		BLI_assert(c != NULL);
		return add_input(name, type, c, value_type);
	}
	
	template <typename T>
	const NodeSocket *add_output(const string &name,
	                             BVMType type,
	                             T default_value)
	{
		Value *c = Value::create(type, default_value);
		BLI_assert(c != NULL);
		return add_output(name, type, c);
	}
	
private:
	string m_name;
	SocketList inputs;
	SocketList outputs;
	bool m_is_pass_node; /* pass nodes are skipped */
	bool m_is_kernel_node; /* only kernel nodes can have function inputs */

	MEM_CXX_CLASS_ALLOC_FUNCS("BVM:NodeType")
};

struct ConstSocketPair {
	ConstSocketPair() :
	    node(NULL), socket("")
	{}
	ConstSocketPair(const NodeInstance *node, const string &socket) :
	    node(node), socket(socket)
	{}
	
	bool operator < (const ConstSocketPair &other) const
	{
		if (node < other.node)
			return true;
		else if (node > other.node)
			return false;
		else
			return socket < other.socket;
	}
	
	operator bool() const
	{
		return node != NULL && !socket.empty();
	}
	
	const NodeInstance *node;
	string socket;
};

struct SocketPair {
	SocketPair() :
	    node(NULL), socket("")
	{}
	SocketPair(NodeInstance *node, const string &socket) :
	    node(node), socket(socket)
	{}
	
	operator ConstSocketPair() const
	{
		return ConstSocketPair(node, socket);
	}
	
	bool operator < (const SocketPair &other) const
	{
		if (node < other.node)
			return true;
		else if (node > other.node)
			return false;
		else
			return socket < other.socket;
	}
	
	operator bool() const
	{
		return node != NULL && !socket.empty();
	}
	
	NodeInstance *node;
	string socket;
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
	
	typedef std::map<string, InputInstance> InputMap;
	typedef std::pair<string, InputInstance> InputPair;
	typedef std::map<string, OutputInstance> OutputMap;
	typedef std::pair<string, OutputInstance> OutputPair;
	
	NodeInstance(const NodeType *type, const string &name);
	~NodeInstance();
	
	SocketPair input(const string &name);
	SocketPair input(int index);
	ConstSocketPair input(const string &name) const;
	ConstSocketPair input(int index) const;
	SocketPair output(const string &name);
	SocketPair output(int index);
	ConstSocketPair output(const string &name) const;
	ConstSocketPair output(int index) const;
	
	int num_inputs() const { return type->num_inputs(); }
	int num_outputs() const { return type->num_outputs(); }
	
	NodeInstance *find_input_link_node(const string &name) const;
	NodeInstance *find_input_link_node(int index) const;
	SocketPair link(const string &name) const;
	SocketPair link(int index) const;
	const NodeSocket *find_input_link_socket(const string &name) const;
	const NodeSocket *find_input_link_socket(int index) const;
	Value *find_input_value(const string &name) const;
	Value *find_input_value(int index) const;
	Value *find_output_value(const string &name) const;
	Value *find_output_value(int index) const;
	
	bool set_input_value(const string &name, Value *value);
	bool set_input_link(const string &name, NodeInstance *from_node, const NodeSocket *from_socket);
	bool set_output_value(const string &name, Value *value);
	
	template <typename T>
	bool set_input_value(const string &name, const T &value)
	{
		const NodeSocket *socket = type->find_input(name);
		return socket ? set_input_value(name, Value::create(socket->typedesc, value)) : false;
	}
	
	template <typename T>
	bool set_output_value(const string &name, const T &value)
	{
		const NodeSocket *socket = type->find_output(name);
		return socket ? set_output_value(name, Value::create(socket->typedesc, value)) : false;
	}
	
	bool has_input_link(const string &name) const;
	bool has_input_link(int index) const;
	bool has_input_value(const string &name) const;
	bool has_input_value(int index) const;
	bool is_input_constant(const string &name) const;
	bool is_input_constant(int index) const;
	bool is_input_function(const string &name) const;
	bool is_input_function(int index) const;
	bool has_output_value(const string &name) const;
	bool has_output_value(int index) const;
	
	const NodeType *type;
	string name;
	InputMap inputs;
	OutputMap outputs;

	MEM_CXX_CLASS_ALLOC_FUNCS("BVM:NodeInstance")
};

struct NodeGraph {
	struct Input {
		Input(const string &name, const SocketPair &key) : name(name), key(key) {}
		string name;
		SocketPair key;
	};
	struct Output {
		Output(const string &name, const SocketPair &key) : name(name), key(key) {}
		string name;
		SocketPair key;
	};
	typedef std::vector<Input> InputList;
	typedef std::vector<Output> OutputList;
	
	typedef std::map<string, NodeType> NodeTypeMap;
	typedef std::pair<string, NodeType> NodeTypeMapPair;
	typedef std::map<string, NodeInstance*> NodeInstanceMap;
	typedef std::pair<string, NodeInstance*> NodeInstanceMapPair;
	
	static NodeTypeMap node_types;
	
	static const NodeType *find_node_type(const string &name);
	static NodeType *add_function_node_type(const string &name);
	static NodeType *add_kernel_node_type(const string &name);
	static NodeType *add_pass_node_type(const string &name);
	static void remove_node_type(const string &name);
	
	NodeGraph();
	~NodeGraph();
	
	NodeInstance *get_node(const string &name);
	NodeInstance *add_node(const string &type, const string &name = "");
	
	const Input *get_input(int index) const;
	const Input *get_input(const string &name) const;
	const Output *get_output(int index) const;
	const Output *get_output(const string &name) const;
	
	const Input *add_input(const string &name, BVMType type);
	const Output *add_output(const string &name, BVMType type, Value *default_value);
	
	template <typename T>
	const Output *add_output(const string &name, BVMType type, const T &default_value)
	{
		return add_output(name, type, Value::create(type, default_value));
	}
	
	void finalize();
	
	void dump(std::ostream &stream = std::cout);
	void dump_graphviz(FILE *f, const string &label);
	
protected:
	static NodeType *add_node_type(const string &name, bool is_kernel_node, bool is_pass_node);
	
	SocketPair add_proxy(const TypeDesc &typedesc, Value *default_value = NULL);
	SocketPair add_value_node(Value *value);
	
	void remove_all_nodes();
	SocketPair find_root(const SocketPair &key);
	void skip_pass_nodes();
	void remove_unused_nodes();
	
public:
	NodeInstanceMap nodes;
	InputList inputs;
	OutputList outputs;

	MEM_CXX_CLASS_ALLOC_FUNCS("BVM:NodeGraph")
};

OpCode get_opcode_from_node_type(const string &node);
void nodes_init();
void nodes_free();

} /* namespace bvm */

#endif

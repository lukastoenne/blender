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

struct NodeInput {
	NodeInput(const string &name,
	           const TypeDesc &typedesc,
	           Value *default_value,
	           BVMInputValueType value_type);
	~NodeInput();
	
	string name;
	TypeDesc typedesc;
	Value *default_value;
	BVMInputValueType value_type;
};

struct NodeOutput {
	NodeOutput(const string &name,
	           const TypeDesc &typedesc,
	           BVMOutputValueType value_type);
	~NodeOutput();
	
	string name;
	TypeDesc typedesc;
	BVMOutputValueType value_type;
};

struct NodeType {
	typedef std::vector<NodeInput> InputList;
	typedef std::vector<NodeOutput> OutputList;
	
	NodeType(const string &name, bool is_kernel_node, bool is_pass_node = false);
	~NodeType();
	
	const string &name() const { return m_name; }
	bool is_kernel_node() const { return m_is_kernel_node; }
	bool is_pass_node() const { return m_is_pass_node; }
	
	int num_inputs() const { return m_inputs.size(); }
	int num_outputs() const { return m_outputs.size(); }
	
	const NodeInput *find_input(int index) const;
	const NodeOutput *find_output(int index) const;
	const NodeInput *find_input(const string &name) const;
	const NodeOutput *find_output(const string &name) const;
	/* stub implementation in case socket is passed directly */
	const NodeInput *find_input(const NodeInput *socket) const;
	const NodeOutput *find_output(const NodeOutput *socket) const;
	
//	bool verify_argument_socket(NodeSocket &socket, Type *type, int index,
//	                            Module *module, LLVMContext &context, raw_ostream &err);
//	bool verify_arguments(Module *module, LLVMContext &context, raw_ostream &err);
	
	const NodeInput *add_input(const string &name,
	                           const TypeDesc &typedesc,
	                           Value *default_value,
	                           BVMInputValueType value_type = INPUT_VARIABLE);

	const NodeOutput *add_output(const string &name,
	                             const TypeDesc &typedesc,
	                             BVMOutputValueType value_type = OUTPUT_VARIABLE);
	
	template <typename T>
	const NodeInput *add_input(const string &name,
	                           const TypeDesc &typedesc,
	                           T default_value,
	                           BVMInputValueType value_type = INPUT_VARIABLE)
	{
		Value *c = Value::create(typedesc, default_value);
		BLI_assert(c != NULL);
		return add_input(name, typedesc, c, value_type);
	}
	
private:
	string m_name;
	InputList m_inputs;
	OutputList m_outputs;
	bool m_is_pass_node; /* pass nodes are skipped */
	bool m_is_kernel_node; /* only kernel nodes can have function inputs */

	MEM_CXX_CLASS_ALLOC_FUNCS("BVM:NodeType")
};

struct ConstOutputKey {
	ConstOutputKey();
	ConstOutputKey(const NodeInstance *node, const string &socket);
	ConstOutputKey(const NodeInstance *node, const NodeOutput *socket);
	
	bool operator < (const ConstOutputKey &other) const;
	operator bool() const;
	
	const NodeInstance *node;
	const NodeOutput *socket;
};

struct OutputKey {
	OutputKey();
	OutputKey(NodeInstance *node, const string &socket);
	OutputKey(NodeInstance *node, const NodeOutput *socket);
	
	operator ConstOutputKey() const;
	bool operator < (const OutputKey &other) const;
	operator bool() const;
	
	NodeInstance *node;
	const NodeOutput *socket;
};

struct ConstInputKey {
	ConstInputKey();
	ConstInputKey(const NodeInstance *node, const string &socket);
	ConstInputKey(const NodeInstance *node, const NodeInput *socket);
	
	bool operator < (const ConstInputKey &other) const;
	operator bool() const;
	
	ConstOutputKey link() const;
	const Value *value() const;
	bool is_constant() const;
	bool is_expression() const;
	
	const NodeInstance *node;
	const NodeInput *socket;
};

struct InputKey {
	InputKey();
	InputKey(NodeInstance *node, const string &socket);
	InputKey(NodeInstance *node, const NodeInput *socket);
	
	operator ConstInputKey() const;
	bool operator < (const InputKey &other) const;
	operator bool() const;
	
	OutputKey link() const;
	const Value *value() const;
	void value_set(Value *value) const;
	bool is_constant() const;
	bool is_expression() const;
	
	NodeInstance *node;
	const NodeInput *socket;
};

struct NodeInstance {
	struct InputInstance {
		InputInstance() :
		    value(NULL)
		{}
		
		OutputKey link;
		Value *value;
	};
	
	typedef std::map<string, InputInstance> InputMap;
	typedef std::pair<string, InputInstance> InputPair;
	
	NodeInstance(const NodeType *type, const string &name);
	~NodeInstance();
	
	InputKey input(const string &name);
	InputKey input(int index);
	ConstInputKey input(const string &name) const;
	ConstInputKey input(int index) const;
	OutputKey output(const string &name);
	OutputKey output(int index);
	ConstOutputKey output(const string &name) const;
	ConstOutputKey output(int index) const;
	
	int num_inputs() const { return type->num_inputs(); }
	int num_outputs() const { return type->num_outputs(); }
	
	OutputKey link(const string &name) const;
	OutputKey link(int index) const;
	bool link_set(const string &name, const OutputKey &from);
	
	const Value *input_value(const string &name) const;
	const Value *input_value(int index) const;
	bool input_value_set(const string &name, Value *value);
	template <typename T>
	bool input_value_set(const string &name, const T &value)
	{
		const NodeInput *socket = type->find_input(name);
		return socket ? input_value_set(name, Value::create(socket->typedesc, value)) : false;
	}
	
	const NodeType *type;
	string name;
	InputMap inputs;

	MEM_CXX_CLASS_ALLOC_FUNCS("BVM:NodeInstance")
};

struct NodeGraph {
	struct Input {
		Input(const string &name, const TypeDesc &typedesc, const OutputKey &key) :
		    name(name), typedesc(typedesc), key(key) {}
		string name;
		TypeDesc typedesc;
		OutputKey key;
	};
	struct Output {
		Output(const string &name, const TypeDesc &typedesc, const OutputKey &key) :
		    name(name), typedesc(typedesc), key(key) {}
		string name;
		TypeDesc typedesc;
		OutputKey key;
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
	void set_output_socket(int index, const OutputKey &key);
	void set_output_socket(const string &name, const OutputKey &key);
	
	const Input *add_input(const string &name, const TypeDesc &typedesc);
	const Output *add_output(const string &name, const TypeDesc &typedesc, Value *default_value);
	
	template <typename T>
	const Output *add_output(const string &name, const TypeDesc &typedesc, const T &default_value)
	{
		return add_output(name, typedesc, Value::create(typedesc, default_value));
	}
	
	void finalize();
	
protected:
	static NodeType *add_node_type(const string &name, bool is_kernel_node, bool is_pass_node);
	
	NodeInstance *add_proxy(const TypeDesc &typedesc, Value *default_value = NULL);
	OutputKey add_value_node(Value *value);
	OutputKey add_argument_node(const TypeDesc &typedesc);
	
	void remove_all_nodes();
	OutputKey find_root(const OutputKey &key);
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

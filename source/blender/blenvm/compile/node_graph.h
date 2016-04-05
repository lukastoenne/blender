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
#include <set>
#include <list>

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_utildefines.h"
#if 0
#include "DNA_node_types.h"
#include "BKE_node.h"
#endif
}

#include "node_value.h"

#include "util_opcode.h"
#include "util_string.h"

namespace blenvm {

struct NodeGraph;
struct NodeType;
struct NodeInstance;
struct NodeBlock;

struct NodeInput {
	NodeInput(const string &name,
	           const TypeDesc &typedesc,
	           NodeValue *default_value,
	           BVMInputValueType value_type);
	~NodeInput();
	
	string name;
	TypeDesc typedesc;
	NodeValue *default_value;
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
	                           const string &type,
	                           NodeValue *default_value,
	                           BVMInputValueType value_type = INPUT_EXPRESSION);

	const NodeOutput *add_output(const string &name,
	                             const string &type,
	                             BVMOutputValueType value_type = OUTPUT_EXPRESSION);
	
	template <typename T>
	const NodeInput *add_input(const string &name,
	                           const string &type,
	                           T default_value,
	                           BVMInputValueType value_type = INPUT_EXPRESSION);
	
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
	
	BVMOutputValueType value_type() const;
	
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
	
	BVMOutputValueType value_type() const;
	
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
	const NodeValue *value() const;
	BVMInputValueType value_type() const;
	
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
	void link_set(const OutputKey &from) const;
	const NodeValue *value() const;
	void value_set(NodeValue *value) const;
	BVMInputValueType value_type() const;
	
	NodeInstance *node;
	const NodeInput *socket;
};

typedef std::set<ConstInputKey> InputSet;
typedef std::set<ConstOutputKey> OutputSet;
typedef std::map<string, OutputKey> VariableMap;

struct NodeInstance {
	struct InputInstance {
		InputInstance() :
		    value(NULL)
		{}
		
		OutputKey link;
		NodeValue *value;
	};
	
	typedef std::map<string, InputInstance> InputMap;
	typedef std::pair<string, InputInstance> InputPair;
	
	NodeInstance(const NodeType *type, const string &name);
	NodeInstance(const NodeInstance *other, const string &name);
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
	
	const NodeValue *input_value(const string &name) const;
	const NodeValue *input_value(int index) const;
	bool input_value_set(const string &name, NodeValue *value);
	template <typename T>
	bool input_value_set(const string &name, const T &value)
	{
		const NodeInput *socket = type->find_input(name);
		return socket ? input_value_set(name, NodeValue::create(socket->typedesc, value)) : false;
	}
	
	const NodeType *type;
	string name;
	InputMap inputs;
	int index; /* ordering index */
	const NodeBlock *block;

	MEM_CXX_CLASS_ALLOC_FUNCS("BVM:NodeInstance")
};

typedef std::set<NodeInstance*> NodeSet;
typedef std::map<const NodeInstance*, NodeInstance*> NodeMap;
typedef std::map<ConstOutputKey, OutputKey> OutputMap;

struct NodeIndexCmp {
	bool operator () (const NodeInstance *a, const NodeInstance *b) const
	{
		return a->index < b->index;
	}
};

typedef std::set<const NodeInstance *, NodeIndexCmp> OrderedNodeSet;

struct NodeBlock {
	typedef std::map<string, ConstOutputKey> ArgumentMap;
	
	NodeBlock(const string &name, NodeBlock *parent = NULL);
	
	const string &name() const { return m_name; }
	NodeBlock *parent() const { return m_parent; }
	void parent_set(NodeBlock *parent) { m_parent = parent; }
	NodeSet &nodes() { return m_nodes; }
	const NodeSet &nodes() const { return m_nodes; }
	ConstOutputKey local_arg(const string &name) const;
	void local_arg_set(const string &name, const ConstOutputKey &arg);
	
	void prune(const NodeSet &used_nodes);
	
private:
	string m_name;
	NodeBlock *m_parent;
	NodeSet m_nodes;
	ArgumentMap m_local_args; // XXX REMOVE
	
	MEM_CXX_CLASS_ALLOC_FUNCS("BVM:NodeBlock")
};

typedef std::set<NodeBlock *> NodeBlockSet;

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
	
	typedef std::map<string, TypeDesc> TypeDefMap;
	typedef std::pair<string, TypeDesc> TypeDefPair;
	typedef std::map<string, NodeType> NodeTypeMap;
	typedef std::pair<string, NodeType> NodeTypeMapPair;
	
	typedef std::map<string, NodeInstance*> NodeInstanceMap;
	typedef std::pair<string, NodeInstance*> NodeInstanceMapPair;
	typedef std::list<NodeBlock> NodeBlockList;
	
	static TypeDefMap typedefs;
	static NodeTypeMap node_types;
	
	static const TypeDesc &find_typedef(const string &name);
	static bool has_typedef(const string &name);
	static TypeDesc *add_typedef(const string &name, BVMType base_type, BVMBufferType buffer_type=BVM_BUFFER_SINGLE);
	static TypeDesc *add_typedef_struct(const string &name);
	static void remove_typedef(const string &name);
	
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
	
	const Input *add_input(const string &name, const string &type);
	const Output *add_output(const string &name, const string &type, NodeValue *default_value);
	
	template <typename T>
	const Output *add_output(const string &name, const string &type, const T &default_value)
	{
		return add_output(name, type, NodeValue::create(find_typedef(type), default_value));
	}
	
	void finalize();
	
	/* first block is main */
	const NodeBlock &main_block() const { return blocks.front(); }
	
protected:
	static NodeType *add_node_type(const string &name, bool is_kernel_node, bool is_pass_node);
	
	NodeInstance *add_proxy(const TypeDesc &typedesc, NodeValue *default_value = NULL);
	OutputKey add_value_node(NodeValue *value);
	OutputKey add_argument_node(const TypeDesc &typedesc);
	
	void remove_all_nodes();
	
	void insert_node(NodeInstance *node);
	NodeInstance *copy_node(const NodeInstance *node);
	NodeInstance *copy_node(const NodeInstance *node, NodeMap &node_map);
	
	/* optimizations */
	
	void remap_outputs(const OutputMap &replacements);
	
	void ensure_valid_expression_inputs();
	
	OutputKey find_root(const OutputKey &key);
	
	void skip_pass_nodes();
	
	NodeInstance *inline_node(NodeInstance *old_node, const VariableMap &vars);
	void inline_function_calls();
	
	void remove_unused_nodes();
	
	bool add_block_node(NodeBlock &block, const OutputSet &local_vars,
	                    NodeInstance *node, NodeSet &visited);
	void blockify_nodes();
	
	void sort_nodes();
	
public:
	NodeBlockList blocks;
	NodeInstanceMap nodes;
	InputList inputs;
	OutputList outputs;

	MEM_CXX_CLASS_ALLOC_FUNCS("BVM:NodeGraph")
};

OpCode get_opcode_from_node_type(const string &node);
void nodes_init();
void nodes_free();

/* ========================================================================= */
/* inline functions */

template <typename T>
const NodeInput *NodeType::add_input(const string &name,
                                     const string &type,
                                     T default_value,
                                     BVMInputValueType value_type)
{
	NodeValue *c = NodeValue::create(NodeGraph::find_typedef(type), default_value);
	BLI_assert(c != NULL);
	return add_input(name, type, c, value_type);
}

} /* namespace blenvm */

#endif

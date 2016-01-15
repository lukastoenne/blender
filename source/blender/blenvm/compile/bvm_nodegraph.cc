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

/** \file blender/blenvm/intern/bvm_nodegraph.cc
 *  \ingroup bvm
 */

#include <map>
#include <set>
#include <vector>
#include <cassert>
#include <cstdio>
#include <sstream>
#include <algorithm>

#include "bvm_nodegraph.h"
#include "bvm_opcode.h"

#include "bvm_util_math.h"

namespace bvm {

NodeInput::NodeInput(const string &name,
                     const TypeDesc &typedesc,
                     Value *default_value,
                     BVMInputValueType value_type) :
    name(name),
    typedesc(typedesc),
    default_value(default_value),
    value_type(value_type)
{
}

NodeInput::~NodeInput()
{
}

NodeOutput::NodeOutput(const string &name,
                       const TypeDesc &typedesc,
                       BVMOutputValueType value_type) :
    name(name),
    typedesc(typedesc),
    value_type(value_type)
{
}

NodeOutput::~NodeOutput()
{
}

/* ------------------------------------------------------------------------- */

NodeType::NodeType(const string &name, bool is_kernel_node, bool is_pass_node) :
    m_name(name),
    m_is_pass_node(is_pass_node),
    m_is_kernel_node(is_kernel_node)
{
}

NodeType::~NodeType()
{
	/* input values are owned by the node type */
	for (InputList::const_iterator it = m_inputs.begin(); it != m_inputs.end(); ++it) {
		const NodeInput &input = *it;
		if (input.default_value)
			delete input.default_value;
	}
}

const NodeInput *NodeType::find_input(int index) const
{
	BLI_assert(index >= 0 && index < m_inputs.size());
	return &m_inputs[index];
}

const NodeOutput *NodeType::find_output(int index) const
{
	BLI_assert(index >= 0 && index < m_outputs.size());
	return &m_outputs[index];
}

const NodeInput *NodeType::find_input(const string &name) const
{
	for (InputList::const_iterator it = m_inputs.begin(); it != m_inputs.end(); ++it) {
		const NodeInput &socket = *it;
		if (socket.name == name)
			return &socket;
	}
	return NULL;
}

const NodeOutput *NodeType::find_output(const string &name) const
{
	for (OutputList::const_iterator it = m_outputs.begin(); it != m_outputs.end(); ++it) {
		const NodeOutput &socket = *it;
		if (socket.name == name)
			return &socket;
	}
	return NULL;
}

/* stub implementation in case socket is passed directly */
const NodeInput *NodeType::find_input(const NodeInput *socket) const
{
	return socket;
}

const NodeOutput *NodeType::find_output(const NodeOutput *socket) const
{
	return socket;
}

#if 0
bool NodeType::verify_argument_socket(NodeSocket &socket, Type *type, int /*index*/,
                                      Module *module, LLVMContext &context, raw_ostream &err)
{
	BVMType tid = bjit_find_llvm_typeid(type, context, module);
	if (tid == BJIT_TYPE_UNKNOWN) {
		err << "Unknown output type: ";
		type->print(err);
		err << "\n";
		return false;
	}
	
	if (tid != socket.type) {
		err << "Output type %d mismatch: ";
		err << "argument type '" << bjit_get_socket_type_name(tid) << "' expected, ";
		err << "got '" << bjit_get_socket_type_name(socket.type) << "'\n";
		return false;
	}
	
	return true;
}

bool NodeType::verify_arguments(Module *module, LLVMContext &context, raw_ostream &err)
{
	Function *fn = bjit_find_external_function(module, name);
	if (!fn)
		return false;
	
	int num_args = fn->arg_size();
	int num_inputs = num_args;
	int num_outputs = 1;
	Type *rstruct = NULL;
	
	Function::arg_iterator it = fn->arg_begin();
	if (it != fn->arg_end()) {
		Argument &arg = *it;
		if (arg.hasStructRetAttr()) {
			rstruct = arg.getType();
			num_inputs = num_args - 1;
			num_outputs = rstruct->getStructNumElements();
			
			++it;
		}
	}
	
	/* add return fields as outputs */
	if (num_inputs != inputs.size()) {
		err << "Inputs number " << inputs.size() << " does not match arguments (" << num_inputs <<  ")\n";
		return false;
	}
	if (num_outputs != outputs.size()) {
		err << "Outputs number " << outputs.size() << " does not match return arguments (" << num_outputs <<  ")\n";
		return false;
	}
	
	for (int i = 0; it != fn->arg_end(); ++it, ++i) {
//		Argument &arg = *it;
		
		/* TODO fuzzy name check could help here? */
//		if (arg.getName() != inputs[i].name) ...
		
		Type *type = rstruct->getStructElementType(i);
		if (!verify_argument_socket(inputs[i], type, i, module, context, err))
			return false;
	}
	
	for (int i = 0; i < num_outputs; ++i) {
		Type *type = rstruct->getStructElementType(i);
		if (!verify_argument_socket(outputs[i], type, i, module, context, err))
			return false;
	}
	
	return true;
}
#endif

const NodeInput *NodeType::add_input(const string &name,
                                     const string &type,
                                     Value *default_value,
                                     BVMInputValueType value_type)
{
	BLI_assert(!find_input(name));
	BLI_assert(NodeGraph::has_typedef(type));
	/* function inputs only allowed for kernel nodes */
	BLI_assert(m_is_kernel_node || value_type != INPUT_EXPRESSION);
	m_inputs.push_back(NodeInput(name, NodeGraph::find_typedef(type), default_value, value_type));
	return &m_inputs.back();
}

const NodeOutput *NodeType::add_output(const string &name,
                                       const string &type,
                                       BVMOutputValueType value_type)
{
	BLI_assert(!find_output(name));
	BLI_assert(NodeGraph::has_typedef(type));
	/* local outputs only allowed for kernel nodes */
	BLI_assert(m_is_kernel_node || value_type != OUTPUT_LOCAL);
	m_outputs.push_back(NodeOutput(name, NodeGraph::find_typedef(type), value_type));
	return &m_outputs.back();
}

/* ------------------------------------------------------------------------- */

ConstOutputKey::ConstOutputKey() :
    node(NULL), socket(NULL)
{}

ConstOutputKey::ConstOutputKey(const NodeInstance *node, const string &socket) :
    node(node), socket(node->type->find_output(socket))
{}

ConstOutputKey::ConstOutputKey(const NodeInstance *node, const NodeOutput *socket) :
    node(node), socket(socket)
{}

bool ConstOutputKey::operator < (const ConstOutputKey &other) const
{
	if (node < other.node)
		return true;
	else if (node > other.node)
		return false;
	else
		return socket < other.socket;
}

ConstOutputKey::operator bool() const
{
	return node != NULL && socket != NULL;
}

/*****/

OutputKey::OutputKey() :
    node(NULL), socket(NULL)
{}

OutputKey::OutputKey(NodeInstance *node, const string &socket) :
    node(node), socket(node->type->find_output(socket))
{}

OutputKey::OutputKey(NodeInstance *node, const NodeOutput *socket) :
    node(node), socket(socket)
{}

OutputKey::operator ConstOutputKey() const
{
	return ConstOutputKey(node, socket);
}

bool OutputKey::operator < (const OutputKey &other) const
{
	if (node < other.node)
		return true;
	else if (node > other.node)
		return false;
	else
		return socket < other.socket;
}

OutputKey::operator bool() const
{
	return node != NULL && socket != NULL;
}

/*****/

ConstInputKey::ConstInputKey() :
    node(NULL), socket(NULL)
{}

ConstInputKey::ConstInputKey(const NodeInstance *node, const string &socket) :
    node(node), socket(node->type->find_input(socket))
{}

ConstInputKey::ConstInputKey(const NodeInstance *node, const NodeInput *socket) :
    node(node), socket(socket)
{}

bool ConstInputKey::operator < (const ConstInputKey &other) const
{
	if (node < other.node)
		return true;
	else if (node > other.node)
		return false;
	else
		return socket < other.socket;
}

ConstInputKey::operator bool() const
{
	return node != NULL && socket != NULL;
}

ConstOutputKey ConstInputKey::link() const
{
	if (node)
		return node->link(socket->name);
	else
		return ConstOutputKey();
}

const Value *ConstInputKey::value() const
{
	return node->input_value(socket->name);
}

bool ConstInputKey::is_constant() const
{
	return socket->value_type == INPUT_CONSTANT;
}

bool ConstInputKey::is_expression() const
{
	return socket->value_type == INPUT_EXPRESSION;
}

/*****/

InputKey::InputKey() :
    node(NULL), socket(NULL)
{}

InputKey::InputKey(NodeInstance *node, const string &socket) :
    node(node), socket(node->type->find_input(socket))
{}

InputKey::InputKey(NodeInstance *node, const NodeInput *socket) :
    node(node), socket(socket)
{}

InputKey::operator ConstInputKey() const
{
	return ConstInputKey(node, socket);
}

bool InputKey::operator < (const InputKey &other) const
{
	if (node < other.node)
		return true;
	else if (node > other.node)
		return false;
	else
		return socket < other.socket;
}

InputKey::operator bool() const
{
	return node != NULL && socket != NULL;
}

OutputKey InputKey::link() const
{
	if (node)
		return node->link(socket->name);
	else
		return OutputKey();
}

void InputKey::link_set(const OutputKey &from) const
{
	node->link_set(socket->name, from);
}

const Value *InputKey::value() const
{
	return node->input_value(socket->name);
}

void InputKey::value_set(Value *value) const
{
	node->input_value_set(socket->name, value);
}

bool InputKey::is_constant() const
{
	return socket->value_type == INPUT_CONSTANT;
}

bool InputKey::is_expression() const
{
	return socket->value_type == INPUT_EXPRESSION;
}

/* ------------------------------------------------------------------------- */

NodeInstance::NodeInstance(const NodeType *type, const string &name) :
    type(type), name(name), index(0), block(NULL)
{
}

NodeInstance::NodeInstance(const NodeInstance *other, const string &name) :
    type(other->type), name(name), index(0), block(NULL)
{
	for (InputMap::const_iterator it = other->inputs.begin(); it != other->inputs.end(); ++it) {
		const string &input_name = it->first;
		const InputInstance &other_input = it->second;
		
		InputInstance &input = inputs[input_name];
		if (other_input.value)
			input.value = other_input.value->copy();
		/* note: links have to be copied externally based on a node map */
	}
}

NodeInstance::~NodeInstance()
{
	/* value instances are managed by the node */
	for (InputMap::const_iterator it = inputs.begin(); it != inputs.end(); ++it) {
		const InputInstance &input = it->second;
		if (input.value)
			delete input.value;
	}
}

InputKey NodeInstance::input(const string &name)
{
	assert(type->find_input(name) != NULL);
	return InputKey(this, name);
}

InputKey NodeInstance::input(int index)
{
	assert(type->find_input(index) != NULL);
	return InputKey(this, type->find_input(index)->name);
}

OutputKey NodeInstance::output(const string &name)
{
	assert(type->find_output(name) != NULL);
	return OutputKey(this, name);
}

OutputKey NodeInstance::output(int index)
{
	assert(type->find_output(index) != NULL);
	return OutputKey(this, type->find_output(index)->name);
}

ConstInputKey NodeInstance::input(const string &name) const
{
	assert(type->find_input(name) != NULL);
	return ConstInputKey(this, name);
}

ConstInputKey NodeInstance::input(int index) const
{
	assert(type->find_input(index) != NULL);
	return ConstInputKey(this, type->find_input(index)->name);
}

ConstOutputKey NodeInstance::output(const string &name) const
{
	assert(type->find_output(name) != NULL);
	return ConstOutputKey(this, name);
}

ConstOutputKey NodeInstance::output(int index) const
{
	assert(type->find_output(index) != NULL);
	return ConstOutputKey(this, type->find_output(index)->name);
}

OutputKey NodeInstance::link(const string &name) const
{
	InputMap::const_iterator it = inputs.find(name);
	if (it != inputs.end()) {
		const NodeInstance::InputInstance &input = it->second;
		return input.link;
	}
	else
		return OutputKey();
}

OutputKey NodeInstance::link(int index) const
{
	const NodeInput *socket = type->find_input(index);
	return socket ? link(socket->name) : OutputKey(NULL, "");
}

bool NodeInstance::link_set(const string &name, const OutputKey &from)
{
	const NodeInput *socket = type->find_input(name);
	InputInstance &input = inputs[name];
	
	if (socket->typedesc.assignable(from.socket->typedesc)) {
		input.link = from;
		return true;
	}
	else
		return false;
}

const Value *NodeInstance::input_value(const string &name) const
{
	InputMap::const_iterator it = inputs.find(name);
	if (it != inputs.end()) {
		if (it->second.value)
			return it->second.value;
	}
	return type->find_input(name)->default_value;
}

const Value *NodeInstance::input_value(int index) const
{
	const NodeInput *socket = type->find_input(index);
	return socket ? input_value(socket->name) : NULL;
}

bool NodeInstance::input_value_set(const string &name, Value *value)
{
	InputInstance &input = inputs[name];
	if (input.value)
		return false;
	input.value = value;
	return true;
}

/* ------------------------------------------------------------------------- */

NodeBlock::NodeBlock(const string &name, NodeBlock *parent) :
    m_name(name),
    m_parent(parent)
{}

ConstOutputKey NodeBlock::local_arg(const string &name) const
{
	ArgumentMap::const_iterator it = m_local_args.find(name);
	if (it != m_local_args.end())
		return it->second;
	else
		return ConstOutputKey();
}

void NodeBlock::local_arg_set(const string &name, const ConstOutputKey &arg)
{
	m_local_args[name] = arg;
}

void NodeBlock::insert(NodeInstance *node)
{
	m_nodes.insert(node);
	assert(node->block == NULL);
	node->block = this;
}

void NodeBlock::prune(const NodeSet &used_nodes)
{
	NodeSet used_block_nodes;
	std::set_intersection(used_nodes.begin(), used_nodes.end(),
	                      m_nodes.begin(), m_nodes.end(),
	                      std::inserter(used_block_nodes, used_block_nodes.end()));
	m_nodes = used_block_nodes;
	
	for (ArgumentMap::iterator it = m_local_args.begin(); it != m_local_args.end(); ++it) {
		NodeInstance *node = const_cast<NodeInstance *>(it->second.node);
		if (used_nodes.find(node) == used_nodes.end()) {
			it->second = ConstOutputKey();
		}
	}
}

/* ------------------------------------------------------------------------- */

NodeGraph::TypeDefMap NodeGraph::typedefs;
NodeGraph::NodeTypeMap NodeGraph::node_types;

const TypeDesc &NodeGraph::find_typedef(const string &name)
{
	return typedefs.at(name);
}

bool NodeGraph::has_typedef(const string &name)
{
	return typedefs.find(name) != typedefs.end();
}

TypeDesc *NodeGraph::add_typedef(const string &name, BVMType base_type, BVMBufferType buffer_type)
{
	std::pair<TypeDefMap::iterator, bool> result =
	        typedefs.insert(TypeDefPair(name, TypeDesc(base_type, buffer_type)));
	if (result.second) {
		TypeDesc *typedesc = &result.first->second;
		return typedesc;
	}
	else
		return NULL;
}

TypeDesc *NodeGraph::add_typedef_struct(const string &name)
{
	std::pair<TypeDefMap::iterator, bool> result =
	        typedefs.insert(TypeDefPair(name, TypeDesc(BVM_INT)));
	if (result.second) {
		TypeDesc *typedesc = &result.first->second;
		typedesc->make_structure();
		return typedesc;
	}
	else
		return NULL;
}

void NodeGraph::remove_typedef(const string &name)
{
	TypeDefMap::iterator it = typedefs.find(name);
	if (it != typedefs.end())
		typedefs.erase(it);
}

const NodeType *NodeGraph::find_node_type(const string &name)
{
	NodeTypeMap::const_iterator it = node_types.find(name);
	return (it != node_types.end())? &it->second : NULL;
}

NodeType *NodeGraph::add_node_type(const string &name, bool is_kernel_node, bool is_pass_node)
{
	std::pair<NodeTypeMap::iterator, bool> result =
	        node_types.insert(NodeTypeMapPair(
	                              name,
	                              NodeType(name, is_kernel_node, is_pass_node))
	                          );
	if (result.second) {
		NodeType *type = &result.first->second;
		return type;
	}
	else
		return NULL;
}

NodeType *NodeGraph::add_function_node_type(const string &name)
{
	return add_node_type(name, false, false);
}

NodeType *NodeGraph::add_kernel_node_type(const string &name)
{
	return add_node_type(name, true, false);
}

NodeType *NodeGraph::add_pass_node_type(const string &name)
{
	return add_node_type(name, false, true);
}

void NodeGraph::remove_node_type(const string &name)
{
	NodeTypeMap::iterator it = node_types.find(name);
	if (it != node_types.end())
		node_types.erase(it);
}

NodeGraph::NodeGraph()
{
}

NodeGraph::~NodeGraph()
{
	remove_all_nodes();
}

NodeInstance *NodeGraph::get_node(const string &name)
{
	NodeInstanceMap::iterator it = nodes.find(name);
	return (it != nodes.end())? it->second : NULL;
}

static void make_unique_name(string &name, const NodeGraph::NodeInstanceMap &nodes)
{
	if (nodes.find(name) == nodes.end())
		return;
	else {
		string new_name;
		int suffix = 1;
		do {
			++suffix;
			std::stringstream ss;
			ss << name << suffix;
			new_name = ss.str();
		} while (nodes.find(new_name) != nodes.end());
		
		name = new_name;
	}
}

NodeInstance *NodeGraph::add_node(const string &type, const string &name)
{
	const NodeType *nodetype = find_node_type(type);
	assert(nodetype);
	
	string final = name;
	if (final.empty()) {
		std::stringstream ss;
		ss << nodetype->name();
		final = ss.str();
	}
	make_unique_name(final, nodes);
	
	NodeInstance *node = new NodeInstance(nodetype, final);
	std::pair<NodeInstanceMap::iterator, bool> result =
	        nodes.insert(NodeInstanceMapPair(final, node));
	BLI_assert(result.second);
	
	return result.first->second;
}

const NodeGraph::Input *NodeGraph::get_input(int index) const
{
	BLI_assert(index >= 0 && index < inputs.size());
	return &inputs[index];
}

const NodeGraph::Output *NodeGraph::get_output(int index) const
{
	BLI_assert(index >= 0 && index < outputs.size());
	return &outputs[index];
}

const NodeGraph::Input *NodeGraph::get_input(const string &name) const
{
	for (InputList::const_iterator it = inputs.begin(); it != inputs.end(); ++it) {
		if (it->name == name)
			return &(*it);
	}
	return NULL;
}

const NodeGraph::Output *NodeGraph::get_output(const string &name) const
{
	for (OutputList::const_iterator it = outputs.begin(); it != outputs.end(); ++it) {
		if (it->name == name)
			return &(*it);
	}
	return NULL;
}

void NodeGraph::set_output_socket(int index, const OutputKey &key)
{
	BLI_assert(index >= 0 && index < outputs.size());
	outputs[index].key = key;
}

void NodeGraph::set_output_socket(const string &name, const OutputKey &key)
{
	for (OutputList::iterator it = outputs.begin(); it != outputs.end(); ++it) {
		if (it->name == name)
			it->key = key;
	}
}

const NodeGraph::Input *NodeGraph::add_input(const string &name, const string &type)
{
	BLI_assert(!get_input(name));
	const TypeDesc &typedesc = find_typedef(type);
	inputs.push_back(Input(name, typedesc, add_argument_node(typedesc)));
	return &inputs.back();
}

const NodeGraph::Output *NodeGraph::add_output(const string &name, const string &type, Value *default_value)
{
	BLI_assert(!get_output(name));
	const TypeDesc &typedesc = find_typedef(type);
	outputs.push_back(Output(name, typedesc, add_proxy(typedesc, default_value)->output(0)));
	return &outputs.back();
}

/* ------------------------------------------------------------------------- */

NodeInstance *NodeGraph::add_proxy(const TypeDesc &typedesc, Value *default_value)
{
	NodeInstance *node = NULL;
	switch (typedesc.buffer_type()) {
		case BVM_BUFFER_SINGLE:
			switch (typedesc.base_type()) {
				case BVM_FLOAT: node = add_node("PASS_FLOAT"); break;
				case BVM_FLOAT3: node = add_node("PASS_FLOAT3"); break;
				case BVM_FLOAT4: node = add_node("PASS_FLOAT4"); break;
				case BVM_INT: node = add_node("PASS_INT"); break;
				case BVM_MATRIX44: node = add_node("PASS_MATRIX44"); break;
				case BVM_STRING: node = add_node("PASS_STRING"); break;
				case BVM_RNAPOINTER: node = add_node("PASS_RNAPOINTER"); break;
				case BVM_MESH: node = add_node("PASS_MESH"); break;
				case BVM_DUPLIS: node = add_node("PASS_DUPLIS"); break;
			}
			break;
		case BVM_BUFFER_ARRAY:
			switch (typedesc.base_type()) {
				case BVM_FLOAT: node = add_node("PASS_FLOAT_ARRAY"); break;
				case BVM_FLOAT3: node = add_node("PASS_FLOAT3_ARRAY"); break;
				case BVM_FLOAT4: node = add_node("PASS_FLOAT4_ARRAY"); break;
				case BVM_INT: node = add_node("PASS_INT_ARRAY"); break;
				case BVM_MATRIX44: node = add_node("PASS_MATRIX44_ARRAY"); break;
				case BVM_STRING: node = add_node("PASS_STRING_ARRAY"); break;
				case BVM_RNAPOINTER: node = add_node("PASS_RNAPOINTER_ARRAY"); break;
				case BVM_MESH: node = add_node("PASS_MESH_ARRAY"); break;
				case BVM_DUPLIS: node = add_node("PASS_DUPLIS_ARRAY"); break;
			}
			break;
	}
	if (node && default_value)
		node->input_value_set("value", default_value);
	return node;
}

OutputKey NodeGraph::add_value_node(Value *value)
{
	NodeInstance *node = NULL;
	switch (value->typedesc().base_type()) {
		case BVM_FLOAT: node = add_node("VALUE_FLOAT"); break;
		case BVM_FLOAT3: node = add_node("VALUE_FLOAT3"); break;
		case BVM_FLOAT4: node = add_node("VALUE_FLOAT4"); break;
		case BVM_INT: node = add_node("VALUE_INT"); break;
		case BVM_MATRIX44: node = add_node("VALUE_MATRIX44"); break;
		case BVM_STRING: node = add_node("VALUE_STRING"); break;
		case BVM_RNAPOINTER: node = add_node("VALUE_RNAPOINTER"); break;
		case BVM_MESH: node = add_node("VALUE_MESH"); break;
		case BVM_DUPLIS: node = add_node("VALUE_DUPLIS"); break;
	}
	if (node)
		node->input_value_set("value", value);
	return OutputKey(node, "value");
}

OutputKey NodeGraph::add_argument_node(const TypeDesc &typedesc)
{
	NodeInstance *node = NULL;
	switch (typedesc.base_type()) {
		case BVM_FLOAT: node = add_node("ARG_FLOAT"); break;
		case BVM_FLOAT3: node = add_node("ARG_FLOAT3"); break;
		case BVM_FLOAT4: node = add_node("ARG_FLOAT4"); break;
		case BVM_INT: node = add_node("ARG_INT"); break;
		case BVM_MATRIX44: node = add_node("ARG_MATRIX44"); break;
		case BVM_STRING: node = add_node("ARG_STRING"); break;
		case BVM_RNAPOINTER: node = add_node("ARG_RNAPOINTER"); break;
		case BVM_MESH: node = add_node("ARG_MESH"); break;
		case BVM_DUPLIS: node = add_node("ARG_DUPLIS"); break;
	}
	return OutputKey(node, "value");
}

void NodeGraph::remove_all_nodes()
{
	for (NodeInstanceMap::iterator it = nodes.begin(); it != nodes.end(); ++it) {
		delete it->second;
	}
	nodes.clear();
}

NodeInstance *NodeGraph::copy_node(const NodeInstance *node, NodeMap &node_map)
{
	string name = node->name;
	make_unique_name(name, nodes);
	NodeInstance *cnode = new NodeInstance(node, name);
	std::pair<NodeInstanceMap::iterator, bool> result =
	        nodes.insert(NodeInstanceMapPair(cnode->name, cnode));
	BLI_assert(result.second);
	
	node_map[node] = cnode;
	for (NodeInstance::InputMap::const_iterator it = node->inputs.begin(); it != node->inputs.end(); ++it) {
		const string &input_name = it->first;
		const NodeInstance::InputInstance &input = it->second;
		
		if (input.link) {
			NodeInstance *clink_node;
			if (node_map.find(input.link.node) != node_map.end())
				clink_node = node_map.at(input.link.node);
			else
				clink_node = input.link.node;
			cnode->inputs[input_name].link = OutputKey(clink_node, input.link.socket);
		}
	}
	
	return cnode;
}

/* ------------------------------------------------------------------------- */
/* Optimization */

/* add a value node on unbound inputs */
void NodeGraph::ensure_bound_inputs()
{
	/* copy node pointers to avoid looping over new nodes again */
	NodeSet old_nodes;
	for (NodeInstanceMap::iterator it = nodes.begin(); it != nodes.end(); ++it)
		old_nodes.insert(it->second);
	
	for (NodeSet::iterator it = old_nodes.begin(); it != old_nodes.end(); ++it) {
		NodeInstance *node = *it;
		for (int i = 0; i < node->num_inputs(); ++i) {
			InputKey input = node->input(i);
			if (input.is_constant())
				continue;
			else if (!input.link()) {
				input.link_set(add_value_node(input.value()->copy()));
			}
		}
	}
}

OutputKey NodeGraph::find_root(const OutputKey &key)
{
	OutputKey root = key;
	/* value is used to create a valid root node if necessary */
	const Value *value = NULL;
	while (root.node && root.node->type->is_pass_node()) {
		value = root.node->input_value(0);
		root = root.node->link(0);
	}
	
	/* create a value node as valid root if necessary */
	if (!root.node) {
		assert(value != NULL);
		root = add_value_node(value->copy());
	}
	
	return root;
}

void NodeGraph::skip_pass_nodes()
{
	/* redirect links to skip over 'pass'-type nodes */
	for (NodeInstanceMap::iterator it = nodes.begin(); it != nodes.end(); ++it) {
		NodeInstance *node = it->second;
		
		if (node->type->is_pass_node())
			continue;
		
		for (NodeInstance::InputMap::iterator it_input = node->inputs.begin();
		     it_input != node->inputs.end();
		     ++it_input) {
			NodeInstance::InputInstance &input = it_input->second;
			
			if (input.link) {
				input.link = find_root(input.link);
			}
		}
	}
	
	/* move output references upstream as well */
	for (OutputList::iterator it_output = outputs.begin(); it_output != outputs.end(); ++it_output) {
		Output &output = *it_output;
		assert(output.key);
		output.key = find_root(output.key);
	}
}

void NodeGraph::make_args_local(NodeBlock &block, NodeMap &block_map, NodeSet &block_visited, const NodeInstance *arg_node)
{
	if (!arg_node->type->is_kernel_node())
		return;
	
	for (int i = 0; i < arg_node->num_outputs(); ++i) {
		const NodeOutput *output = arg_node->type->find_output(i);
		
		if (output->value_type == OUTPUT_LOCAL) {
			const Input *graph_input = get_input(output->name);
			assert(graph_input);
			
			if (graph_input->key) {
				block_visited.insert(graph_input->key.node);
				OutputKey local_arg(copy_node(graph_input->key.node, block_map), graph_input->key.socket);
				block.insert(local_arg.node);
				block.local_arg_set(output->name, local_arg);
			}
		}
	}
}

bool NodeGraph::add_block_node(NodeInstance *node, NodeBlock &block, NodeMap &block_map, NodeSet &block_visited)
{
	/* determines if the node is part of the block */
	bool is_block_node = block_map.find(node) != block_map.end();
	
	if (block_visited.find(node) != block_visited.end())
		return is_block_node;
	assert(!is_block_node); /* can't have been mapped yet */
	block_visited.insert(node);
	
	for (int i = 0; i < node->num_inputs(); ++i) {
		InputKey input = node->input(i);
		if (input.is_constant()) {
			if (!block.parent())
				is_block_node |= true;
		}
		else if (input.is_expression()) {
			if (!block.parent())
				is_block_node |= blockify_expression(input, block, block_map, block_visited);
		}
		else {
			OutputKey output = input.link();
			if (output) {
				is_block_node |= add_block_node(output.node, block, block_map, block_visited);
			}
		}
	}
	
	if (is_block_node) {
		NodeInstance *block_node;
		if (block.parent()) {
			block_node = copy_node(node, block_map);
		}
		else {
			block_node = node;
			block_map[node] = node;
		}
		block.insert(block_node);
		return true;
	}
	else
		return false;
}

bool NodeGraph::blockify_expression(const InputKey &input, NodeBlock &block, NodeMap &block_map, NodeSet &block_visited)
{
	OutputKey link_key = input.link();
	if (!link_key)
		return false;
	NodeInstance *link_node = link_key.node;
	
	bool is_block_node = false;
	
	/* generate a local block for the input expression */
	blocks.push_back(NodeBlock(input.node->name + ":" + input.socket->name, &block));
	NodeBlock &expr_block = blocks.back();
	NodeSet expr_visited;
	NodeMap expr_block_map;
	
	make_args_local(expr_block, expr_block_map, expr_visited, input.node);
	
	add_block_node(link_node, expr_block, expr_block_map, expr_visited);
	
	if (expr_block_map.find(link_node) != expr_block_map.end()) {
		/* remap the input link */
		input.link_set(OutputKey(expr_block_map.at(link_node), link_key.socket));
	}
	else {
		/* use the input directly if no expression nodes are generated (no local arg dependencies) */
		is_block_node |= add_block_node(link_node, block, block_map, block_visited);
	}
	
	/* find node inputs in the expression block that use values outside of it,
	 * which means these must be included in the parent block
	 */
	for (NodeSet::iterator it = expr_block.nodes().begin(); it != expr_block.nodes().end(); ++it) {
		NodeInstance *node = *it;
		for (int i = 0; i < node->num_inputs(); ++i) {
			InputKey expr_input = node->input(i);
			NodeInstance *link_node = expr_input.link().node;
			if (link_node && expr_block.nodes().find(link_node) == expr_block.nodes().end())
				is_block_node |= add_block_node(link_node, block, block_map, block_visited);
		}
	}
	
	return is_block_node;
}

void NodeGraph::blockify_nodes()
{
	blocks.push_back(NodeBlock("main", NULL));
	NodeBlock &main = blocks.back();
	NodeSet main_visited;
	NodeMap main_map;
	
	/* input argument nodes must always be included in main,
	 * to provide reliable storage for caller arguments
	 */
	for (InputList::const_iterator it = inputs.begin(); it != inputs.end(); ++it) {
		const Input &input = *it;
		if (input.key) {
			NodeInstance *node = input.key.node;
			main_visited.insert(node);
			
			NodeInstance *block_node = node; /* for main block: same as original node */
			main_map[node] = block_node;
			main.insert(block_node);
		}
	}
	
	for (OutputList::const_iterator it = outputs.begin(); it != outputs.end(); ++it) {
		const Output &output = *it;
		if (output.key)
			add_block_node(output.key.node, main, main_map, main_visited);
	}
}

static void used_nodes_append(NodeInstance *node, NodeSet &used_nodes)
{
	if (used_nodes.find(node) != used_nodes.end())
		return;
	used_nodes.insert(node);
	
	for (NodeInstance::InputMap::iterator it = node->inputs.begin(); it != node->inputs.end(); ++it) {
		NodeInstance::InputInstance &input = it->second;
		if (input.link) {
			used_nodes_append(input.link.node, used_nodes);
		}
	}
}

void NodeGraph::remove_unused_nodes()
{
	NodeSet used_nodes;
	/* all output nodes and their inputs subgraphs are used */
	for (NodeGraph::OutputList::iterator it = outputs.begin(); it != outputs.end(); ++it) {
		Output &output = *it;
		used_nodes_append(output.key.node, used_nodes);
	}
	/* make sure unused inputs don't leave dangling node pointers */
	for (NodeGraph::NodeBlockList::iterator it = blocks.begin(); it != blocks.end(); ++it) {
		NodeBlock &block = *it;
		block.prune(used_nodes);
	}
	for (NodeGraph::InputList::iterator it = inputs.begin(); it != inputs.end(); ++it) {
		Input &input = *it;
		if (used_nodes.find(input.key.node) == used_nodes.end()) {
			input.key = OutputKey();
		}
	}
	
	NodeInstanceMap::iterator it = nodes.begin();
	while (it != nodes.end()) {
		if (used_nodes.find(it->second) == used_nodes.end()) {
			/* it_del is invalidated on erase */
			NodeInstanceMap::iterator it_del = it;
			
			++it;
			
			delete it_del->second;
			nodes.erase(it_del);
		}
		else
			it++;
	}
}

static void assign_node_index(NodeInstance *node, int *next_index)
{
	if (node->index > 0)
		return;
	
	for (int i = 0; i < node->num_inputs(); ++i) {
		NodeInstance *link_node = node->link(i).node;
		if (link_node) {
			assign_node_index(link_node, next_index);
		}
	}
	
	node->index = (*next_index)++;
}

/* assign a global index to each node to allow sorted sets */
void NodeGraph::sort_nodes()
{
	int next_index = 1;
	for (NodeInstanceMap::iterator it = nodes.begin(); it != nodes.end(); ++it) {
		assign_node_index(it->second, &next_index);
	}
}

void NodeGraph::finalize()
{
	ensure_bound_inputs();
	skip_pass_nodes();
	blockify_nodes();
	remove_unused_nodes();
	sort_nodes();
}

/* ------------------------------------------------------------------------- */

static void register_typedefs()
{
	TypeDesc *t;
	
	t = NodeGraph::add_typedef("FLOAT", BVM_FLOAT);
	
	t = NodeGraph::add_typedef("FLOAT3", BVM_FLOAT3);
	
	t = NodeGraph::add_typedef("FLOAT4", BVM_FLOAT4);
	
	t = NodeGraph::add_typedef("INT", BVM_INT);
	
	t = NodeGraph::add_typedef("MATRIX44", BVM_MATRIX44);
	
	t = NodeGraph::add_typedef("STRING", BVM_STRING);
	
	t = NodeGraph::add_typedef("RNAPOINTER", BVM_RNAPOINTER);
	
	t = NodeGraph::add_typedef("MESH", BVM_MESH);
	
	t = NodeGraph::add_typedef("DUPLIS", BVM_DUPLIS);
	
	
	t = NodeGraph::add_typedef("FLOAT_ARRAY", BVM_FLOAT, BVM_BUFFER_ARRAY);
	
	t = NodeGraph::add_typedef("FLOAT3_ARRAY", BVM_FLOAT3, BVM_BUFFER_ARRAY);
	
	t = NodeGraph::add_typedef("FLOAT4_ARRAY", BVM_FLOAT4, BVM_BUFFER_ARRAY);
	
	t = NodeGraph::add_typedef("INT_ARRAY", BVM_INT, BVM_BUFFER_ARRAY);
	
	t = NodeGraph::add_typedef("MATRIX44_ARRAY", BVM_MATRIX44, BVM_BUFFER_ARRAY);
	
	t = NodeGraph::add_typedef("STRING_ARRAY", BVM_STRING, BVM_BUFFER_ARRAY);
	
	t = NodeGraph::add_typedef("RNAPOINTER_ARRAY", BVM_RNAPOINTER, BVM_BUFFER_ARRAY);
	
	t = NodeGraph::add_typedef("MESH_ARRAY", BVM_MESH, BVM_BUFFER_ARRAY);
	
	t = NodeGraph::add_typedef("DUPLIS_ARRAY", BVM_DUPLIS, BVM_BUFFER_ARRAY);
	
	
	
	(void)t;
}

OpCode get_opcode_from_node_type(const string &node)
{
	#define NODETYPE(name) \
	if (node == STRINGIFY(name)) \
		return OP_##name
	
	NODETYPE(VALUE_FLOAT);
	NODETYPE(VALUE_FLOAT3);
	NODETYPE(VALUE_FLOAT4);
	NODETYPE(VALUE_INT);
	NODETYPE(VALUE_MATRIX44);
	NODETYPE(VALUE_STRING);
	NODETYPE(VALUE_RNAPOINTER);
	NODETYPE(VALUE_MESH);
	NODETYPE(VALUE_DUPLIS);
	
	NODETYPE(FLOAT_TO_INT);
	NODETYPE(INT_TO_FLOAT);
	NODETYPE(SET_FLOAT3);
	NODETYPE(GET_ELEM_FLOAT3);
	NODETYPE(SET_FLOAT4);
	NODETYPE(GET_ELEM_FLOAT4);
	NODETYPE(MATRIX44_TO_LOC);
	NODETYPE(MATRIX44_TO_EULER);
	NODETYPE(MATRIX44_TO_AXISANGLE);
	NODETYPE(MATRIX44_TO_SCALE);
	NODETYPE(LOC_TO_MATRIX44);
	NODETYPE(EULER_TO_MATRIX44);
	NODETYPE(AXISANGLE_TO_MATRIX44);
	NODETYPE(SCALE_TO_MATRIX44);
	
	NODETYPE(ADD_FLOAT);
	NODETYPE(SUB_FLOAT);
	NODETYPE(MUL_FLOAT);
	NODETYPE(DIV_FLOAT);
	NODETYPE(SINE);
	NODETYPE(COSINE);
	NODETYPE(TANGENT);
	NODETYPE(ARCSINE);
	NODETYPE(ARCCOSINE);
	NODETYPE(ARCTANGENT);
	NODETYPE(POWER);
	NODETYPE(LOGARITHM);
	NODETYPE(MINIMUM);
	NODETYPE(MAXIMUM);
	NODETYPE(ROUND);
	NODETYPE(LESS_THAN);
	NODETYPE(GREATER_THAN);
	NODETYPE(MODULO);
	NODETYPE(ABSOLUTE);
	NODETYPE(CLAMP);
	NODETYPE(SQRT);
	
	NODETYPE(ADD_FLOAT3);
	NODETYPE(SUB_FLOAT3);
	NODETYPE(MUL_FLOAT3);
	NODETYPE(DIV_FLOAT3);
	NODETYPE(MUL_FLOAT3_FLOAT);
	NODETYPE(DIV_FLOAT3_FLOAT);
	NODETYPE(AVERAGE_FLOAT3);
	NODETYPE(DOT_FLOAT3);
	NODETYPE(CROSS_FLOAT3);
	NODETYPE(NORMALIZE_FLOAT3);
	NODETYPE(LENGTH_FLOAT3);
	
	NODETYPE(ADD_MATRIX44);
	NODETYPE(SUB_MATRIX44);
	NODETYPE(MUL_MATRIX44);
	NODETYPE(MUL_MATRIX44_FLOAT);
	NODETYPE(DIV_MATRIX44_FLOAT);
	NODETYPE(NEGATE_MATRIX44);
	NODETYPE(TRANSPOSE_MATRIX44);
	NODETYPE(INVERT_MATRIX44);
	NODETYPE(ADJOINT_MATRIX44);
	NODETYPE(DETERMINANT_MATRIX44);
	
	NODETYPE(MUL_MATRIX44_FLOAT3);
	NODETYPE(MUL_MATRIX44_FLOAT4);
	
	NODETYPE(MIX_RGB);
	
	NODETYPE(INT_TO_RANDOM);
	NODETYPE(FLOAT_TO_RANDOM);
	
	NODETYPE(TEX_PROC_VORONOI);
	NODETYPE(TEX_PROC_CLOUDS);
	NODETYPE(TEX_PROC_WOOD);
	NODETYPE(TEX_PROC_MUSGRAVE);
	NODETYPE(TEX_PROC_MAGIC);
	NODETYPE(TEX_PROC_STUCCI);
	NODETYPE(TEX_PROC_MARBLE);
	NODETYPE(TEX_PROC_DISTNOISE);
	
	NODETYPE(OBJECT_LOOKUP);
	NODETYPE(OBJECT_TRANSFORM);
	NODETYPE(OBJECT_FINAL_MESH);
	
	NODETYPE(EFFECTOR_TRANSFORM);
	NODETYPE(EFFECTOR_CLOSEST_POINT);
	
	NODETYPE(MESH_LOAD);
	NODETYPE(MESH_COMBINE);
	NODETYPE(MESH_ARRAY);
	NODETYPE(MESH_DISPLACE);
	NODETYPE(MESH_BOOLEAN);
	NODETYPE(MESH_CLOSEST_POINT);
	
	NODETYPE(CURVE_PATH);
	
	NODETYPE(MAKE_DUPLI);
	NODETYPE(DUPLIS_COMBINE);
	
	#undef NODETYPE
	
	return OP_NOOP;
}

static mesh_ptr __empty_mesh__;

static void register_opcode_node_types()
{
	static DupliList *__empty_duplilist__ = new DupliList();
	
	NodeType *nt;
	
	nt = NodeGraph::add_function_node_type("FLOAT_TO_INT");
	nt->add_input("value", "FLOAT", 0.0f);
	nt->add_output("value", "INT");
	
	nt = NodeGraph::add_function_node_type("INT_TO_FLOAT");
	nt->add_input("value", "INT", 0);
	nt->add_output("value", "FLOAT");
	
	nt = NodeGraph::add_pass_node_type("PASS_FLOAT");
	nt->add_input("value", "FLOAT", 0.0f);
	nt->add_output("value", "FLOAT");
	
	nt = NodeGraph::add_pass_node_type("PASS_FLOAT3");
	nt->add_input("value", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", "FLOAT3");
	
	nt = NodeGraph::add_pass_node_type("PASS_FLOAT4");
	nt->add_input("value", "FLOAT4", float4(0.0f, 0.0f, 0.0f, 0.0f));
	nt->add_output("value", "FLOAT4");
	
	nt = NodeGraph::add_pass_node_type("PASS_INT");
	nt->add_input("value", "INT", 0);
	nt->add_output("value", "INT");
	
	nt = NodeGraph::add_pass_node_type("PASS_MATRIX44");
	nt->add_input("value", "MATRIX44", matrix44::identity());
	nt->add_output("value", "MATRIX44");
	
	nt = NodeGraph::add_pass_node_type("PASS_STRING");
	nt->add_input("value", "STRING", "");
	nt->add_output("value", "STRING");
	
	nt = NodeGraph::add_pass_node_type("PASS_RNAPOINTER");
	nt->add_input("value", "RNAPOINTER", PointerRNA_NULL);
	nt->add_output("value", "RNAPOINTER");
	
	nt = NodeGraph::add_pass_node_type("PASS_MESH");
	nt->add_input("value", "MESH", __empty_mesh__);
	nt->add_output("value", "MESH");
	
	nt = NodeGraph::add_pass_node_type("PASS_DUPLIS");
	nt->add_input("value", "DUPLIS", duplis_ptr(__empty_duplilist__));
	nt->add_output("value", "DUPLIS");
	
	nt = NodeGraph::add_pass_node_type("PASS_FLOAT_ARRAY");
	nt->add_input("value", "FLOAT_ARRAY", array<BVM_FLOAT>());
	nt->add_output("value", "FLOAT_ARRAY");
	
	nt = NodeGraph::add_pass_node_type("PASS_FLOAT3_ARRAY");
	nt->add_input("value", "FLOAT3_ARRAY", array<BVM_FLOAT3>());
	nt->add_output("value", "FLOAT3_ARRAY");
	
	nt = NodeGraph::add_pass_node_type("PASS_FLOAT4_ARRAY");
	nt->add_input("value", "FLOAT4_ARRAY", array<BVM_FLOAT4>());
	nt->add_output("value", "FLOAT4_ARRAY");
	
	nt = NodeGraph::add_pass_node_type("PASS_INT_ARRAY");
	nt->add_input("value", "INT_ARRAY", array<BVM_INT>());
	nt->add_output("value", "INT_ARRAY");
	
	nt = NodeGraph::add_pass_node_type("PASS_MATRIX44_ARRAY");
	nt->add_input("value", "MATRIX44_ARRAY", array<BVM_MATRIX44>());
	nt->add_output("value", "MATRIX44_ARRAY");
	
	nt = NodeGraph::add_pass_node_type("PASS_STRING_ARRAY");
	nt->add_input("value", "STRING_ARRAY", array<BVM_STRING>());
	nt->add_output("value", "STRING_ARRAY");
	
	nt = NodeGraph::add_pass_node_type("PASS_RNAPOINTER_ARRAY");
	nt->add_input("value", "RNAPOINTER_ARRAY", array<BVM_RNAPOINTER>());
	nt->add_output("value", "RNAPOINTER_ARRAY");
	
	nt = NodeGraph::add_pass_node_type("PASS_MESH_ARRAY");
	nt->add_input("value", "MESH_ARRAY", array<BVM_MESH>());
	nt->add_output("value", "MESH_ARRAY");
	
	nt = NodeGraph::add_pass_node_type("PASS_DUPLIS_ARRAY");
	nt->add_input("value", "DUPLIS_ARRAY", array<BVM_DUPLIS>());
	nt->add_output("value", "DUPLIS_ARRAY");
	
	nt = NodeGraph::add_function_node_type("ARG_FLOAT");
	nt->add_output("value", "FLOAT");
	
	nt = NodeGraph::add_function_node_type("ARG_FLOAT3");
	nt->add_output("value", "FLOAT3");
	
	nt = NodeGraph::add_function_node_type("ARG_FLOAT4");
	nt->add_output("value", "FLOAT4");
	
	nt = NodeGraph::add_function_node_type("ARG_INT");
	nt->add_output("value", "INT");
	
	nt = NodeGraph::add_function_node_type("ARG_MATRIX44");
	nt->add_output("value", "MATRIX44");
	
	nt = NodeGraph::add_function_node_type("ARG_STRING");
	nt->add_output("value", "STRING");
	
	nt = NodeGraph::add_function_node_type("ARG_RNAPOINTER");
	nt->add_output("value", "RNAPOINTER");
	
	nt = NodeGraph::add_function_node_type("ARG_MESH");
	nt->add_output("value", "MESH");
	
	nt = NodeGraph::add_function_node_type("ARG_DUPLIS");
	nt->add_output("value", "DUPLIS");
	
	nt = NodeGraph::add_function_node_type("VALUE_FLOAT");
	nt->add_input("value", "FLOAT", 0.0f, INPUT_CONSTANT);
	nt->add_output("value", "FLOAT");
	
	nt = NodeGraph::add_function_node_type("VALUE_FLOAT3");
	nt->add_input("value", "FLOAT3", float3(0.0f, 0.0f, 0.0f), INPUT_CONSTANT);
	nt->add_output("value", "FLOAT3");
	
	nt = NodeGraph::add_function_node_type("VALUE_FLOAT4");
	nt->add_input("value", "FLOAT4", float4(0.0f, 0.0f, 0.0f, 0.0f), INPUT_CONSTANT);
	nt->add_output("value", "FLOAT4");
	
	nt = NodeGraph::add_function_node_type("VALUE_INT");
	nt->add_input("value", "INT", 0, INPUT_CONSTANT);
	nt->add_output("value", "INT");
	
	nt = NodeGraph::add_function_node_type("VALUE_MATRIX44");
	nt->add_input("value", "MATRIX44", matrix44::identity(), INPUT_CONSTANT);
	nt->add_output("value", "MATRIX44");
	
	nt = NodeGraph::add_function_node_type("VALUE_STRING");
	nt->add_input("value", "STRING", "", INPUT_CONSTANT);
	nt->add_output("value", "STRING");
	
	nt = NodeGraph::add_function_node_type("VALUE_RNAPOINTER");
	nt->add_input("value", "RNAPOINTER", PointerRNA_NULL, INPUT_CONSTANT);
	nt->add_output("value", "RNAPOINTER");
	
	nt = NodeGraph::add_function_node_type("VALUE_MESH");
	nt->add_input("value", "MESH", __empty_mesh__, INPUT_CONSTANT);
	nt->add_output("value", "MESH");
	
	nt = NodeGraph::add_function_node_type("VALUE_DUPLIS");
	nt->add_input("value", "DUPLIS", duplis_ptr(__empty_duplilist__), INPUT_CONSTANT);
	nt->add_output("value", "DUPLIS");
	
	nt = NodeGraph::add_function_node_type("GET_ELEM_FLOAT3");
	nt->add_input("index", "INT", 0, INPUT_CONSTANT);
	nt->add_input("value", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", "FLOAT");
	
	nt = NodeGraph::add_function_node_type("SET_FLOAT3");
	nt->add_input("value_x", "FLOAT", 0.0f);
	nt->add_input("value_y", "FLOAT", 0.0f);
	nt->add_input("value_z", "FLOAT", 0.0f);
	nt->add_output("value", "FLOAT3");
	
	nt = NodeGraph::add_function_node_type("GET_ELEM_FLOAT4");
	nt->add_input("index", "INT", 0, INPUT_CONSTANT);
	nt->add_input("value", "FLOAT4", float4(0.0f, 0.0f, 0.0f, 0.0f));
	nt->add_output("value", "FLOAT");
	
	nt = NodeGraph::add_function_node_type("SET_FLOAT4");
	nt->add_input("value_x", "FLOAT", 0.0f);
	nt->add_input("value_y", "FLOAT", 0.0f);
	nt->add_input("value_z", "FLOAT", 0.0f);
	nt->add_input("value_w", "FLOAT", 0.0f);
	nt->add_output("value", "FLOAT4");
	
	#define BINARY_MATH_NODE(name) \
	nt = NodeGraph::add_function_node_type(STRINGIFY(name)); \
	nt->add_input("value_a", "FLOAT", 0.0f); \
	nt->add_input("value_b", "FLOAT", 0.0f); \
	nt->add_output("value", "FLOAT")
	
	#define UNARY_MATH_NODE(name) \
	nt = NodeGraph::add_function_node_type(STRINGIFY(name)); \
	nt->add_input("value", "FLOAT", 0.0f); \
	nt->add_output("value", "FLOAT")
	
	BINARY_MATH_NODE(ADD_FLOAT);
	BINARY_MATH_NODE(SUB_FLOAT);
	BINARY_MATH_NODE(MUL_FLOAT);
	BINARY_MATH_NODE(DIV_FLOAT);
	UNARY_MATH_NODE(SINE);
	UNARY_MATH_NODE(COSINE);
	UNARY_MATH_NODE(TANGENT);
	UNARY_MATH_NODE(ARCSINE);
	UNARY_MATH_NODE(ARCCOSINE);
	UNARY_MATH_NODE(ARCTANGENT);
	BINARY_MATH_NODE(POWER);
	BINARY_MATH_NODE(LOGARITHM);
	BINARY_MATH_NODE(MINIMUM);
	BINARY_MATH_NODE(MAXIMUM);
	UNARY_MATH_NODE(ROUND);
	BINARY_MATH_NODE(LESS_THAN);
	BINARY_MATH_NODE(GREATER_THAN);
	BINARY_MATH_NODE(MODULO);
	UNARY_MATH_NODE(ABSOLUTE);
	UNARY_MATH_NODE(CLAMP);
	UNARY_MATH_NODE(SQRT);
	
	#undef BINARY_MATH_NODE
	#undef UNARY_MATH_NODE
	
	nt = NodeGraph::add_function_node_type("ADD_FLOAT3");
	nt->add_input("value_a", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", "FLOAT3");
	
	nt = NodeGraph::add_function_node_type("SUB_FLOAT3");
	nt->add_input("value_a", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", "FLOAT3");
	
	nt = NodeGraph::add_function_node_type("MUL_FLOAT3");
	nt->add_input("value_a", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", "FLOAT3");
	
	nt = NodeGraph::add_function_node_type("DIV_FLOAT3");
	nt->add_input("value_a", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", "FLOAT3");
	
	nt = NodeGraph::add_function_node_type("MUL_FLOAT3_FLOAT");
	nt->add_input("value_a", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", "FLOAT", 0.0f);
	nt->add_output("value", "FLOAT3");
	
	nt = NodeGraph::add_function_node_type("DIV_FLOAT3_FLOAT");
	nt->add_input("value_a", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", "FLOAT", 0.0f);
	nt->add_output("value", "FLOAT3");
	
	nt = NodeGraph::add_function_node_type("AVERAGE_FLOAT3");
	nt->add_input("value_a", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", "FLOAT3");
	
	nt = NodeGraph::add_function_node_type("DOT_FLOAT3");
	nt->add_input("value_a", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", "FLOAT");
	
	nt = NodeGraph::add_function_node_type("CROSS_FLOAT3");
	nt->add_input("value_a", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", "FLOAT3");
	
	nt = NodeGraph::add_function_node_type("NORMALIZE_FLOAT3");
	nt->add_input("value", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_output("vector", "FLOAT3");
	nt->add_output("value", "FLOAT");
	
	nt = NodeGraph::add_function_node_type("LENGTH_FLOAT3");
	nt->add_input("value", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_output("length", "FLOAT");
	
	nt = NodeGraph::add_function_node_type("MIX_RGB");
	nt->add_input("mode", "INT", 0, INPUT_CONSTANT);
	nt->add_input("factor", "FLOAT", 0.0f);
	nt->add_input("color1", "FLOAT4", float4(0.0f, 0.0f, 0.0f, 1.0f));
	nt->add_input("color2", "FLOAT4", float4(0.0f, 0.0f, 0.0f, 1.0f));
	nt->add_output("color", "FLOAT4");
	
	nt = NodeGraph::add_function_node_type("INT_TO_RANDOM");
	nt->add_input("seed", "INT", 0, INPUT_CONSTANT);
	nt->add_input("value", "INT", 0);
	nt->add_output("irandom", "INT");
	nt->add_output("frandom", "FLOAT");
	
	nt = NodeGraph::add_function_node_type("FLOAT_TO_RANDOM");
	nt->add_input("seed", "INT", 0, INPUT_CONSTANT);
	nt->add_input("value", "FLOAT", 0);
	nt->add_output("irandom", "INT");
	nt->add_output("frandom", "FLOAT");
	
	nt = NodeGraph::add_function_node_type("TEX_PROC_VORONOI");
	nt->add_input("distance_metric", "INT", 0, INPUT_CONSTANT);
	nt->add_input("color_type", "INT", 0, INPUT_CONSTANT);
	nt->add_input("minkowski_exponent", "FLOAT", 2.5f);
	nt->add_input("scale", "FLOAT", 1.0f);
	nt->add_input("noise_size", "FLOAT", 1.0f);
	nt->add_input("nabla", "FLOAT", 0.05f);
	nt->add_input("w1", "FLOAT", 1.0f);
	nt->add_input("w2", "FLOAT", 0.0f);
	nt->add_input("w3", "FLOAT", 0.0f);
	nt->add_input("w4", "FLOAT", 0.0f);
	nt->add_input("position", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_output("intensity", "FLOAT");
	nt->add_output("color", "FLOAT4");
	nt->add_output("normal", "FLOAT3");
	
	nt = NodeGraph::add_function_node_type("TEX_PROC_CLOUDS");
	nt->add_input("position", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_input("nabla", "FLOAT", 0.05f);
	nt->add_input("size", "FLOAT", 1.0f);
	nt->add_input("depth", "INT", 2, INPUT_CONSTANT);
	nt->add_input("noise_basis", "INT", 0, INPUT_CONSTANT);
	nt->add_input("noise_hard", "INT", 0, INPUT_CONSTANT);
	nt->add_output("intensity", "FLOAT");
	nt->add_output("color", "FLOAT4");
	nt->add_output("normal", "FLOAT3");

	nt = NodeGraph::add_function_node_type("TEX_PROC_WOOD");
	nt->add_input("position", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_input("nabla", "FLOAT", 0.05f);
	nt->add_input("size", "FLOAT", 1.0f);
	nt->add_input("turbulence", "FLOAT", 1.0f);
	nt->add_input("noise_basis", "INT", 0, INPUT_CONSTANT);
	nt->add_input("noise_basis_2", "INT", 0, INPUT_CONSTANT);
	nt->add_input("noise_hard", "INT", 0, INPUT_CONSTANT);
	nt->add_input("wood_type", "INT", 0, INPUT_CONSTANT);
	nt->add_output("intensity", "FLOAT");
	nt->add_output("normal", "FLOAT3");

	nt = NodeGraph::add_function_node_type("TEX_PROC_MUSGRAVE");
	nt->add_input("position", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_input("nabla", "FLOAT", 0.05f);
	nt->add_input("size", "FLOAT", 1.0f);
	nt->add_input("dimension", "FLOAT", 1.0f);
	nt->add_input("lacunarity", "FLOAT", 1.0f);
	nt->add_input("octaves", "FLOAT", 1.0f);
	nt->add_input("intensity", "FLOAT", 1.0f);
	nt->add_input("offset", "FLOAT", 1.0f);
	nt->add_input("gain", "FLOAT", 1.0f);
	nt->add_input("noise_basis", "INT", 0, INPUT_CONSTANT);
	nt->add_input("musgrave_type", "INT", 0, INPUT_CONSTANT);
	nt->add_output("intensity", "FLOAT");
	nt->add_output("normal", "FLOAT3");

	nt = NodeGraph::add_function_node_type("TEX_PROC_MAGIC");
	nt->add_input("position", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_input("turbulence", "FLOAT", 1.0f);
	nt->add_input("depth", "INT", 2, INPUT_CONSTANT);
	nt->add_output("intensity", "FLOAT");
	nt->add_output("color", "FLOAT4");
	nt->add_output("normal", "FLOAT3");

	nt = NodeGraph::add_function_node_type("TEX_PROC_STUCCI");
	nt->add_input("position", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_input("size", "FLOAT", 1.0f);
	nt->add_input("turbulence", "FLOAT", 1.0f);
	nt->add_input("noise_basis", "INT", 0, INPUT_CONSTANT);
	nt->add_input("noise_hard", "INT", 0, INPUT_CONSTANT);
	nt->add_input("stucci_type", "INT", 0, INPUT_CONSTANT);
	nt->add_output("intensity", "FLOAT");
	nt->add_output("normal", "FLOAT3");

	nt = NodeGraph::add_function_node_type("TEX_PROC_MARBLE");
	nt->add_input("position", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_input("size", "FLOAT", 1.0f);
	nt->add_input("nabla", "FLOAT", 0.05f);
	nt->add_input("turbulence", "FLOAT", 1.0f);
	nt->add_input("depth", "INT", 2, INPUT_CONSTANT);
	nt->add_input("noise_basis", "INT", 0, INPUT_CONSTANT);
	nt->add_input("noise_basis_2", "INT", 0, INPUT_CONSTANT);
	nt->add_input("noise_hard", "INT", 0, INPUT_CONSTANT);
	nt->add_input("marble_type", "INT", 0, INPUT_CONSTANT);
	nt->add_output("intensity", "FLOAT");
	nt->add_output("normal", "FLOAT3");

	nt = NodeGraph::add_function_node_type("TEX_PROC_DISTNOISE");
	nt->add_input("position", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_input("size", "FLOAT", 1.0f);
	nt->add_input("nabla", "FLOAT", 0.05f);
	nt->add_input("dist_amount", "FLOAT", 1.0f);
	nt->add_input("noise_dist", "INT", 0, INPUT_CONSTANT);
	nt->add_input("noise_basis", "INT", 0, INPUT_CONSTANT);
	nt->add_output("intensity", "FLOAT");
	nt->add_output("normal", "FLOAT3");
	
	nt = NodeGraph::add_function_node_type("OBJECT_LOOKUP");
	nt->add_input("key", "INT", 0, INPUT_CONSTANT);
	nt->add_output("object", "RNAPOINTER");
	
	nt = NodeGraph::add_function_node_type("OBJECT_TRANSFORM");
	nt->add_input("object", "RNAPOINTER", PointerRNA_NULL);
	nt->add_output("transform", "MATRIX44");
	
	nt = NodeGraph::add_function_node_type("OBJECT_FINAL_MESH");
	nt->add_input("object", "RNAPOINTER", PointerRNA_NULL);
	nt->add_output("mesh", "MESH");
	
	nt = NodeGraph::add_function_node_type("EFFECTOR_TRANSFORM");
	nt->add_input("object", "INT", 0, INPUT_CONSTANT);
	nt->add_output("transform", "MATRIX44");
	
	nt = NodeGraph::add_function_node_type("EFFECTOR_CLOSEST_POINT");
	nt->add_input("object", "RNAPOINTER", PointerRNA_NULL);
	nt->add_input("vector", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_output("position", "FLOAT3");
	nt->add_output("normal", "FLOAT3");
	nt->add_output("tangent", "FLOAT3");
	
	nt = NodeGraph::add_kernel_node_type("MESH_LOAD");
	nt->add_input("base_mesh", "RNAPOINTER", PointerRNA_NULL);
	nt->add_output("mesh", "MESH");
	
	nt = NodeGraph::add_kernel_node_type("MESH_COMBINE");
	nt->add_input("mesh_a", "MESH", __empty_mesh__);
	nt->add_input("mesh_b", "MESH", __empty_mesh__);
	nt->add_output("mesh_out", "MESH");
	
	nt = NodeGraph::add_kernel_node_type("MESH_ARRAY");
	nt->add_input("mesh_in", "MESH", __empty_mesh__);
	nt->add_input("count", "INT", 1);
	nt->add_input("transform", "MATRIX44", matrix44::identity(), INPUT_EXPRESSION);
	nt->add_output("mesh_out", "MESH");
	nt->add_output("iteration", "INT", OUTPUT_LOCAL);
	
	nt = NodeGraph::add_kernel_node_type("MESH_DISPLACE");
	nt->add_input("mesh_in", "MESH", __empty_mesh__);
	nt->add_input("vector", "FLOAT3", float3(0.0f, 0.0f, 0.0f), INPUT_EXPRESSION);
	nt->add_output("mesh_out", "MESH");
	nt->add_output("element.index", "INT", OUTPUT_LOCAL);
	nt->add_output("element.location", "FLOAT3", OUTPUT_LOCAL);
	
	nt = NodeGraph::add_kernel_node_type("MESH_BOOLEAN");
	nt->add_input("mesh_in", "MESH", __empty_mesh__);
	nt->add_input("object", "RNAPOINTER", PointerRNA_NULL);
	nt->add_input("transform", "MATRIX44", matrix44::identity());
	nt->add_input("inverse_transform", "MATRIX44", matrix44::identity());
	nt->add_input("operation", "INT", -1);
	nt->add_input("separate", "INT", 0);
	nt->add_input("dissolve", "INT", 1);
	nt->add_input("connect_regions", "INT", 1);
	nt->add_input("threshold", "FLOAT", 0.0f);
	nt->add_output("mesh_out", "MESH");
	
	nt = NodeGraph::add_function_node_type("MESH_CLOSEST_POINT");
	nt->add_input("mesh", "MESH", __empty_mesh__);
	nt->add_input("transform", "MATRIX44", matrix44::identity());
	nt->add_input("inverse_transform", "MATRIX44", matrix44::identity());
	nt->add_input("vector", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_output("position", "FLOAT3");
	nt->add_output("normal", "FLOAT3");
	nt->add_output("tangent", "FLOAT3");
	
	nt = NodeGraph::add_function_node_type("CURVE_PATH");
	nt->add_input("object", "RNAPOINTER", PointerRNA_NULL);
	nt->add_input("transform", "MATRIX44", matrix44::identity());
	nt->add_input("inverse_transform", "MATRIX44", matrix44::identity());
	nt->add_input("parameter", "FLOAT", 0.0f);
	nt->add_output("location", "FLOAT3");
	nt->add_output("direction", "FLOAT3");
	nt->add_output("normal", "FLOAT3");
	nt->add_output("rotation", "MATRIX44");
	nt->add_output("radius", "FLOAT");
	nt->add_output("weight", "FLOAT");
	nt->add_output("tilt", "FLOAT");
	
	nt = NodeGraph::add_function_node_type("MAKE_DUPLI");
	nt->add_input("object", "RNAPOINTER", PointerRNA_NULL);
	nt->add_input("transform", "MATRIX44", matrix44::identity());
	nt->add_input("index", "INT", 0);
	nt->add_input("hide", "INT", 0);
	nt->add_input("recursive", "INT", 1);
	nt->add_output("dupli", "DUPLIS");
	
	nt = NodeGraph::add_function_node_type("DUPLIS_COMBINE");
	nt->add_input("duplis_a", "DUPLIS", duplis_ptr(__empty_duplilist__));
	nt->add_input("duplis_b", "DUPLIS", duplis_ptr(__empty_duplilist__));
	nt->add_output("duplis", "DUPLIS");
	
	nt = NodeGraph::add_function_node_type("ADD_MATRIX44");
	nt->add_input("value_a", "MATRIX44", matrix44::identity());
	nt->add_input("value_b", "MATRIX44", matrix44::identity());
	nt->add_output("value", "MATRIX44");
	
	nt = NodeGraph::add_function_node_type("SUB_MATRIX44");
	nt->add_input("value_a", "MATRIX44", matrix44::identity());
	nt->add_input("value_b", "MATRIX44", matrix44::identity());
	nt->add_output("value", "MATRIX44");
	
	nt = NodeGraph::add_function_node_type("MUL_MATRIX44");
	nt->add_input("value_a", "MATRIX44", matrix44::identity());
	nt->add_input("value_b", "MATRIX44", matrix44::identity());
	nt->add_output("value", "MATRIX44");
	
	nt = NodeGraph::add_function_node_type("MUL_MATRIX44_FLOAT");
	nt->add_input("value_a", "MATRIX44", matrix44::identity());
	nt->add_input("value_b", "FLOAT", 0.0f);
	nt->add_output("value", "MATRIX44");
	
	nt = NodeGraph::add_function_node_type("DIV_MATRIX44_FLOAT");
	nt->add_input("value_a", "MATRIX44", matrix44::identity());
	nt->add_input("value_b", "FLOAT", 1.0f);
	nt->add_output("value", "MATRIX44");
	
	nt = NodeGraph::add_function_node_type("NEGATE_MATRIX44");
	nt->add_input("value", "MATRIX44", matrix44::identity());
	nt->add_output("value", "MATRIX44");
	
	nt = NodeGraph::add_function_node_type("TRANSPOSE_MATRIX44");
	nt->add_input("value", "MATRIX44", matrix44::identity());
	nt->add_output("value", "MATRIX44");
	
	nt = NodeGraph::add_function_node_type("INVERT_MATRIX44");
	nt->add_input("value", "MATRIX44", matrix44::identity());
	nt->add_output("value", "MATRIX44");
	
	nt = NodeGraph::add_function_node_type("ADJOINT_MATRIX44");
	nt->add_input("value", "MATRIX44", matrix44::identity());
	nt->add_output("value", "MATRIX44");
	
	nt = NodeGraph::add_function_node_type("DETERMINANT_MATRIX44");
	nt->add_input("value", "MATRIX44", matrix44::identity());
	nt->add_output("value", "FLOAT");
	
	nt = NodeGraph::add_function_node_type("MUL_MATRIX44_FLOAT3");
	nt->add_input("value_a", "MATRIX44", matrix44::identity());
	nt->add_input("value_b", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", "FLOAT3");
	
	nt = NodeGraph::add_function_node_type("MUL_MATRIX44_FLOAT4");
	nt->add_input("value_a", "MATRIX44", matrix44::identity());
	nt->add_input("value_b", "FLOAT4", float4(0.0f, 0.0f, 0.0f, 0.0f));
	nt->add_output("value", "FLOAT4");
	
	nt = NodeGraph::add_function_node_type("MATRIX44_TO_LOC");
	nt->add_input("matrix", "MATRIX44", matrix44::identity());
	nt->add_output("loc", "FLOAT3");
	
	nt = NodeGraph::add_function_node_type("MATRIX44_TO_EULER");
	nt->add_input("order", "INT", EULER_ORDER_DEFAULT, INPUT_CONSTANT);
	nt->add_input("matrix", "MATRIX44", matrix44::identity());
	nt->add_output("euler", "FLOAT3");
	
	nt = NodeGraph::add_function_node_type("MATRIX44_TO_AXISANGLE");
	nt->add_input("matrix", "MATRIX44", matrix44::identity());
	nt->add_output("axis", "FLOAT3");
	nt->add_output("angle", "FLOAT");
	
	nt = NodeGraph::add_function_node_type("MATRIX44_TO_SCALE");
	nt->add_input("matrix", "MATRIX44", matrix44::identity());
	nt->add_output("scale", "FLOAT3");
	
	nt = NodeGraph::add_function_node_type("LOC_TO_MATRIX44");
	nt->add_input("loc", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_output("matrix", "MATRIX44");
	
	nt = NodeGraph::add_function_node_type("EULER_TO_MATRIX44");
	nt->add_input("order", "INT", EULER_ORDER_DEFAULT, INPUT_CONSTANT);
	nt->add_input("euler", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_output("matrix", "MATRIX44");
	
	nt = NodeGraph::add_function_node_type("AXISANGLE_TO_MATRIX44");
	nt->add_input("axis", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_input("angle", "FLOAT", 0.0f);
	nt->add_output("matrix", "MATRIX44");
	
	nt = NodeGraph::add_function_node_type("SCALE_TO_MATRIX44");
	nt->add_input("scale", "FLOAT3", float3(0.0f, 0.0f, 0.0f));
	nt->add_output("matrix", "MATRIX44");
}

void nodes_init()
{
	create_empty_mesh(__empty_mesh__);
	
	register_typedefs();
	register_opcode_node_types();
}

void nodes_free()
{
	destroy_empty_mesh(__empty_mesh__);
}

} /* namespace bvm */

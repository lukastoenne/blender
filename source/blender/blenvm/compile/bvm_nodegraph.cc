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
                                     const TypeDesc &typedesc,
                                     Value *default_value,
                                     BVMInputValueType value_type)
{
	BLI_assert(!find_input(name));
	/* function inputs only allowed for kernel nodes */
	BLI_assert(m_is_kernel_node || value_type != INPUT_FUNCTION);
	m_inputs.push_back(NodeInput(name, typedesc, default_value, value_type));
	return &m_inputs.back();
}

const NodeOutput *NodeType::add_output(const string &name,
                                       const TypeDesc &typedesc,
                                       BVMOutputValueType value_type)
{
	BLI_assert(!find_output(name));
	/* local outputs only allowed for kernel nodes */
	BLI_assert(m_is_kernel_node || value_type != OUTPUT_LOCAL);
	m_outputs.push_back(NodeOutput(name, typedesc, value_type));
	return &m_outputs.back();
}

/* ------------------------------------------------------------------------- */

NodeInstance::NodeInstance(const NodeType *type, const string &name) :
    type(type), name(name)
{
}

NodeInstance::~NodeInstance()
{
}

SocketPair NodeInstance::input(const string &name)
{
	assert(type->find_input(name) != NULL);
	return SocketPair(this, name);
}

SocketPair NodeInstance::input(int index)
{
	assert(type->find_input(index) != NULL);
	return SocketPair(this, type->find_input(index)->name);
}

SocketPair NodeInstance::output(const string &name)
{
	assert(type->find_output(name) != NULL);
	return SocketPair(this, name);
}

SocketPair NodeInstance::output(int index)
{
	assert(type->find_output(index) != NULL);
	return SocketPair(this, type->find_output(index)->name);
}

ConstSocketPair NodeInstance::input(const string &name) const
{
	assert(type->find_input(name) != NULL);
	return ConstSocketPair(this, name);
}

ConstSocketPair NodeInstance::input(int index) const
{
	assert(type->find_input(index) != NULL);
	return ConstSocketPair(this, type->find_input(index)->name);
}

ConstSocketPair NodeInstance::output(const string &name) const
{
	assert(type->find_output(name) != NULL);
	return ConstSocketPair(this, name);
}

ConstSocketPair NodeInstance::output(int index) const
{
	assert(type->find_output(index) != NULL);
	return ConstSocketPair(this, type->find_output(index)->name);
}

NodeInstance *NodeInstance::find_input_link_node(const string &name) const
{
	InputMap::const_iterator it = inputs.find(name);
	return (it != inputs.end()) ? it->second.link_node : NULL;
}

NodeInstance *NodeInstance::find_input_link_node(int index) const
{
	const NodeInput *socket = type->find_input(index);
	return socket ? find_input_link_node(socket->name) : NULL;
}

const NodeOutput *NodeInstance::find_input_link_socket(const string &name) const
{
	InputMap::const_iterator it = inputs.find(name);
	return (it != inputs.end()) ? it->second.link_socket : NULL;
}

const NodeOutput *NodeInstance::find_input_link_socket(int index) const
{
	const NodeInput *socket = type->find_input(index);
	return socket ? find_input_link_socket(socket->name) : NULL;
}

SocketPair NodeInstance::link(const string &name) const
{
	InputMap::const_iterator it = inputs.find(name);
	if (it != inputs.end()) {
		const NodeInstance::InputInstance &input = it->second;
		return input.link_node && input.link_socket ?
		            SocketPair(input.link_node, input.link_socket->name) :
		            SocketPair(NULL, "");
	}
	else
		return SocketPair(NULL, "");
}

SocketPair NodeInstance::link(int index) const
{
	const NodeInput *socket = type->find_input(index);
	return socket ? link(socket->name) : SocketPair(NULL, "");
}

Value *NodeInstance::find_input_value(const string &name) const
{
	InputMap::const_iterator it = inputs.find(name);
	return (it != inputs.end()) ? it->second.value : NULL;
}

Value *NodeInstance::find_input_value(int index) const
{
	const NodeInput *socket = type->find_input(index);
	return socket ? find_input_value(socket->name) : NULL;
}

Value *NodeInstance::find_output_value(const string &name) const
{
	OutputMap::const_iterator it = outputs.find(name);
	return (it != outputs.end()) ? it->second.value : NULL;
}

Value *NodeInstance::find_output_value(int index) const
{
	const NodeOutput *socket = type->find_output(index);
	return socket ? find_output_value(socket->name) : NULL;
}

bool NodeInstance::set_input_value(const string &name, Value *value)
{
	InputInstance &input = inputs[name];
	if (input.value)
		return false;
	input.value = value;
	return true;
}

bool NodeInstance::set_input_link(const string &name, NodeInstance *from_node, const NodeOutput *from_socket)
{
	const NodeInput *socket = type->find_input(name);
	InputInstance &input = inputs[name];
	
	if (socket->typedesc.assignable(from_socket->typedesc)) {
		input.link_node = from_node;
		input.link_socket = from_socket;
		return true;
	}
	else
		return false;
}

bool NodeInstance::has_input_link(const string &name) const
{
	InputMap::const_iterator it = inputs.find(name);
	if (it != inputs.end()) {
		const InputInstance &input = it->second;
		return (input.link_node && input.link_socket);
	}
	else
		return false;
}

bool NodeInstance::has_input_link(int index) const
{
	const NodeInput *socket = type->find_input(index);
	return socket ? has_input_link(socket->name) : false;
}

bool NodeInstance::has_input_value(const string &name) const
{
	InputMap::const_iterator it = inputs.find(name);
	if (it != inputs.end()) {
		const InputInstance &input = it->second;
		return (input.value);
	}
	else
		return false;
}

bool NodeInstance::has_input_value(int index) const
{
	const NodeInput *socket = type->find_input(index);
	return socket ? has_input_value(socket->name) : false;
}

bool NodeInstance::is_input_constant(const string &name) const
{
	const NodeInput *socket = type->find_input(name);
	return socket ? socket->value_type == INPUT_CONSTANT : false;
}

bool NodeInstance::is_input_constant(int index) const
{
	const NodeInput *socket = type->find_input(index);
	return socket ? socket->value_type == INPUT_CONSTANT : false;
}

bool NodeInstance::is_input_function(const string &name) const
{
	const NodeInput *socket = type->find_input(name);
	return socket ? socket->value_type == INPUT_FUNCTION : false;
}

bool NodeInstance::is_input_function(int index) const
{
	const NodeInput *socket = type->find_input(index);
	return socket ? socket->value_type == INPUT_FUNCTION : false;
}

bool NodeInstance::set_output_value(const string &name, Value *value)
{
	OutputInstance &output = outputs[name];
	if (output.value)
		return false;
	output.value = value;
	return true;
}

bool NodeInstance::has_output_value(const string &name) const
{
	OutputMap::const_iterator it = outputs.find(name);
	if (it != outputs.end()) {
		const OutputInstance &output = it->second;
		return (output.value);
	}
	else
		return false;
}

bool NodeInstance::has_output_value(int index) const
{
	const NodeOutput *socket = type->find_output(index);
	return socket ? has_output_value(socket->name) : false;
}

/* ------------------------------------------------------------------------- */

NodeGraph::NodeTypeMap NodeGraph::node_types;

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
	if (nodes.find(final) != nodes.end()) {
		std::stringstream ss;
		ss << "_" << nodes.size();
		final = ss.str();
	}
	
	NodeInstance *node = new NodeInstance(nodetype, final);
	std::pair<NodeInstanceMap::iterator, bool> result =
	        nodes.insert(NodeInstanceMapPair(final, node));
	
	return (result.second)? result.first->second : NULL;
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

const NodeGraph::Input *NodeGraph::add_input(const string &name, const TypeDesc &typedesc)
{
	BLI_assert(!get_input(name));
	inputs.push_back(Input(name, typedesc, add_argument_node(typedesc)));
	return &inputs.back();
}

const NodeGraph::Output *NodeGraph::add_output(const string &name, const TypeDesc &typedesc, Value *default_value)
{
	BLI_assert(!get_output(name));
	outputs.push_back(Output(name, typedesc, add_proxy(typedesc, default_value)));
	return &outputs.back();
}

/* ------------------------------------------------------------------------- */

SocketPair NodeGraph::add_proxy(const TypeDesc &typedesc, Value *default_value)
{
	NodeInstance *node = NULL;
	switch (typedesc.buffer_type) {
		case BVM_BUFFER_SINGLE:
			switch (typedesc.base_type) {
				case BVM_FLOAT: node = add_node("PASS_FLOAT"); break;
				case BVM_FLOAT3: node = add_node("PASS_FLOAT3"); break;
				case BVM_FLOAT4: node = add_node("PASS_FLOAT4"); break;
				case BVM_INT: node = add_node("PASS_INT"); break;
				case BVM_MATRIX44: node = add_node("PASS_MATRIX44"); break;
				case BVM_STRING: node = add_node("PASS_STRING"); break;
				case BVM_POINTER: node = add_node("PASS_POINTER"); break;
				case BVM_MESH: node = add_node("PASS_MESH"); break;
				case BVM_DUPLIS: node = add_node("PASS_DUPLIS"); break;
			}
			break;
		case BVM_BUFFER_ARRAY:
			switch (typedesc.base_type) {
				case BVM_FLOAT: node = add_node("PASS_FLOAT_ARRAY"); break;
				case BVM_FLOAT3: node = add_node("PASS_FLOAT3_ARRAY"); break;
				case BVM_FLOAT4: node = add_node("PASS_FLOAT4_ARRAY"); break;
				case BVM_INT: node = add_node("PASS_INT_ARRAY"); break;
				case BVM_MATRIX44: node = add_node("PASS_MATRIX44_ARRAY"); break;
				case BVM_STRING: node = add_node("PASS_STRING_ARRAY"); break;
				case BVM_POINTER: node = add_node("PASS_POINTER_ARRAY"); break;
				case BVM_MESH: node = add_node("PASS_MESH_ARRAY"); break;
				case BVM_DUPLIS: node = add_node("PASS_DUPLIS_ARRAY"); break;
			}
			break;
	}
	if (node && default_value)
		node->set_input_value("value", default_value);
	return SocketPair(node, "value");
}

SocketPair NodeGraph::add_value_node(Value *value)
{
	NodeInstance *node = NULL;
	switch (value->typedesc().base_type) {
		case BVM_FLOAT: node = add_node("VALUE_FLOAT"); break;
		case BVM_FLOAT3: node = add_node("VALUE_FLOAT3"); break;
		case BVM_FLOAT4: node = add_node("VALUE_FLOAT4"); break;
		case BVM_INT: node = add_node("VALUE_INT"); break;
		case BVM_MATRIX44: node = add_node("VALUE_MATRIX44"); break;
		case BVM_STRING: node = add_node("VALUE_STRING"); break;
		case BVM_POINTER: node = add_node("VALUE_POINTER"); break;
		case BVM_MESH: node = add_node("VALUE_MESH"); break;
		case BVM_DUPLIS: node = add_node("VALUE_DUPLIS"); break;
	}
	if (node)
		node->set_input_value("value", value);
	return SocketPair(node, "value");
}

SocketPair NodeGraph::add_argument_node(const TypeDesc &typedesc)
{
	NodeInstance *node = NULL;
	switch (typedesc.base_type) {
		case BVM_FLOAT: node = add_node("ARG_FLOAT"); break;
		case BVM_FLOAT3: node = add_node("ARG_FLOAT3"); break;
		case BVM_FLOAT4: node = add_node("ARG_FLOAT4"); break;
		case BVM_INT: node = add_node("ARG_INT"); break;
		case BVM_MATRIX44: node = add_node("ARG_MATRIX44"); break;
		case BVM_STRING: node = add_node("ARG_STRING"); break;
		case BVM_POINTER: node = add_node("ARG_POINTER"); break;
		case BVM_MESH: node = add_node("ARG_MESH"); break;
		case BVM_DUPLIS: node = add_node("ARG_DUPLIS"); break;
	}
	return SocketPair(node, "value");
}

/* ------------------------------------------------------------------------- */
/* Optimization */

void NodeGraph::remove_all_nodes()
{
	for (NodeInstanceMap::iterator it = nodes.begin(); it != nodes.end(); ++it) {
		delete it->second;
	}
	nodes.clear();
}

SocketPair NodeGraph::find_root(const SocketPair &key)
{
	SocketPair root = key;
	/* value is used to create a valid root node if necessary */
	Value *value = NULL;
	while (root.node && root.node->type->is_pass_node()) {
		value = root.node->has_input_value(0) ?
		            root.node->find_input_value(0) :
		            root.node->type->find_input(0)->default_value;
		
		NodeInstance *link_node = root.node->find_input_link_node(0);
		const NodeOutput *link_socket = root.node->find_input_link_socket(0);
		root = SocketPair(link_node, link_socket ? link_socket->name : "");
	}
	
	/* create a value node as valid root if necessary */
	if (!root.node) {
		assert(value != NULL);
		root = add_value_node(value);
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
			
			if (input.link_node && input.link_socket) {
				SocketPair root_key = find_root(SocketPair(input.link_node, input.link_socket->name));
				input.link_node = root_key.node;
				input.link_socket = root_key.node->type->find_output(root_key.socket);
			}
		}
	}
	
	/* move output references upstream as well */
	for (OutputList::iterator it_output = outputs.begin(); it_output != outputs.end(); ++it_output) {
		Output &output = *it_output;
		assert(output.key.node);
		output.key = find_root(output.key);
	}
}

typedef std::set<NodeInstance *> NodeSet;

static void used_nodes_append(NodeInstance *node, NodeSet &used_nodes)
{
	if (used_nodes.find(node) != used_nodes.end())
		return;
	used_nodes.insert(node);
	
	for (NodeInstance::InputMap::iterator it = node->inputs.begin(); it != node->inputs.end(); ++it) {
		NodeInstance::InputInstance &input = it->second;
		if (input.link_node) {
			used_nodes_append(input.link_node, used_nodes);
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
	for (NodeGraph::InputList::iterator it = inputs.begin(); it != inputs.end(); ++it) {
		Input &input = *it;
		if (used_nodes.find(input.key.node) == used_nodes.end()) {
			input.key = SocketPair();
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

void NodeGraph::finalize()
{
	skip_pass_nodes();
	remove_unused_nodes();
}

/* ------------------------------------------------------------------------- */

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
	NODETYPE(VALUE_POINTER);
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
	
	NODETYPE(OBJECT_LOOKUP);
	NODETYPE(OBJECT_TRANSFORM);
	
	NODETYPE(EFFECTOR_TRANSFORM);
	NODETYPE(EFFECTOR_CLOSEST_POINT);
	
	NODETYPE(MESH_LOAD);
	NODETYPE(MESH_COMBINE);
	NODETYPE(MESH_ARRAY);
	NODETYPE(MESH_DISPLACE);
	NODETYPE(MESH_BOOLEAN);
	
	NODETYPE(CURVE_PATH);
	
	NODETYPE(MAKE_DUPLI);
	
	#undef NODETYPE
	
	return OP_NOOP;
}

static mesh_ptr __empty_mesh__;

static void register_opcode_node_types()
{
	NodeType *nt;
	
	nt = NodeGraph::add_function_node_type("FLOAT_TO_INT");
	nt->add_input("value", TYPE_FLOAT, 0.0f);
	nt->add_output("value", TYPE_INT);
	
	nt = NodeGraph::add_function_node_type("INT_TO_FLOAT");
	nt->add_input("value", TYPE_INT, 0);
	nt->add_output("value", TYPE_FLOAT);
	
	nt = NodeGraph::add_pass_node_type("PASS_FLOAT");
	nt->add_input("value", TYPE_FLOAT, 0.0f);
	nt->add_output("value", TYPE_FLOAT);
	
	nt = NodeGraph::add_pass_node_type("PASS_FLOAT3");
	nt->add_input("value", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", TYPE_FLOAT3);
	
	nt = NodeGraph::add_pass_node_type("PASS_FLOAT4");
	nt->add_input("value", TYPE_FLOAT4, float4(0.0f, 0.0f, 0.0f, 0.0f));
	nt->add_output("value", TYPE_FLOAT4);
	
	nt = NodeGraph::add_pass_node_type("PASS_INT");
	nt->add_input("value", TYPE_INT, 0);
	nt->add_output("value", TYPE_INT);
	
	nt = NodeGraph::add_pass_node_type("PASS_MATRIX44");
	nt->add_input("value", TYPE_MATRIX44, matrix44::identity());
	nt->add_output("value", TYPE_MATRIX44);
	
	nt = NodeGraph::add_pass_node_type("PASS_STRING");
	nt->add_input("value", TYPE_STRING, "");
	nt->add_output("value", TYPE_STRING);
	
	nt = NodeGraph::add_pass_node_type("PASS_POINTER");
	nt->add_input("value", TYPE_POINTER, PointerRNA_NULL);
	nt->add_output("value", TYPE_POINTER);
	
	nt = NodeGraph::add_pass_node_type("PASS_MESH");
	nt->add_input("value", TYPE_MESH, __empty_mesh__);
	nt->add_output("value", TYPE_MESH);
	
	nt = NodeGraph::add_pass_node_type("PASS_DUPLIS");
	nt->add_input("value", TYPE_DUPLIS, duplis_ptr(new DupliList()));
	nt->add_output("value", TYPE_DUPLIS);
	
	nt = NodeGraph::add_pass_node_type("PASS_FLOAT_ARRAY");
	nt->add_input("value", TYPE_FLOAT_ARRAY, array<BVM_FLOAT>());
	nt->add_output("value", TYPE_FLOAT_ARRAY);
	
	nt = NodeGraph::add_pass_node_type("PASS_FLOAT3_ARRAY");
	nt->add_input("value", TYPE_FLOAT3_ARRAY, array<BVM_FLOAT3>());
	nt->add_output("value", TYPE_FLOAT3_ARRAY);
	
	nt = NodeGraph::add_pass_node_type("PASS_FLOAT4_ARRAY");
	nt->add_input("value", TYPE_FLOAT4_ARRAY, array<BVM_FLOAT4>());
	nt->add_output("value", TYPE_FLOAT4_ARRAY);
	
	nt = NodeGraph::add_pass_node_type("PASS_INT_ARRAY");
	nt->add_input("value", TYPE_INT_ARRAY, array<BVM_INT>());
	nt->add_output("value", TYPE_INT_ARRAY);
	
	nt = NodeGraph::add_pass_node_type("PASS_MATRIX44_ARRAY");
	nt->add_input("value", TYPE_MATRIX44_ARRAY, array<BVM_MATRIX44>());
	nt->add_output("value", TYPE_MATRIX44_ARRAY);
	
	nt = NodeGraph::add_pass_node_type("PASS_STRING_ARRAY");
	nt->add_input("value", TYPE_STRING_ARRAY, array<BVM_STRING>());
	nt->add_output("value", TYPE_STRING_ARRAY);
	
	nt = NodeGraph::add_pass_node_type("PASS_POINTER_ARRAY");
	nt->add_input("value", TYPE_POINTER_ARRAY, array<BVM_POINTER>());
	nt->add_output("value", TYPE_POINTER_ARRAY);
	
	nt = NodeGraph::add_pass_node_type("PASS_MESH_ARRAY");
	nt->add_input("value", TYPE_MESH_ARRAY, array<BVM_MESH>());
	nt->add_output("value", TYPE_MESH_ARRAY);
	
	nt = NodeGraph::add_pass_node_type("PASS_DUPLIS_ARRAY");
	nt->add_input("value", TYPE_DUPLIS_ARRAY, array<BVM_DUPLIS>());
	nt->add_output("value", TYPE_DUPLIS_ARRAY);
	
	nt = NodeGraph::add_function_node_type("ARG_FLOAT");
	nt->add_output("value", TYPE_FLOAT);
	
	nt = NodeGraph::add_function_node_type("ARG_FLOAT3");
	nt->add_output("value", TYPE_FLOAT3);
	
	nt = NodeGraph::add_function_node_type("ARG_FLOAT4");
	nt->add_output("value", TYPE_FLOAT4);
	
	nt = NodeGraph::add_function_node_type("ARG_INT");
	nt->add_output("value", TYPE_INT);
	
	nt = NodeGraph::add_function_node_type("ARG_MATRIX44");
	nt->add_output("value", TYPE_MATRIX44);
	
	nt = NodeGraph::add_function_node_type("ARG_STRING");
	nt->add_output("value", TYPE_STRING);
	
	nt = NodeGraph::add_function_node_type("ARG_POINTER");
	nt->add_output("value", TYPE_POINTER);
	
	nt = NodeGraph::add_function_node_type("ARG_MESH");
	nt->add_output("value", TYPE_MESH);
	
	nt = NodeGraph::add_function_node_type("ARG_DUPLIS");
	nt->add_output("value", TYPE_DUPLIS);
	
	nt = NodeGraph::add_function_node_type("VALUE_FLOAT");
	nt->add_input("value", TYPE_FLOAT, 0.0f, INPUT_CONSTANT);
	nt->add_output("value", TYPE_FLOAT);
	
	nt = NodeGraph::add_function_node_type("VALUE_FLOAT3");
	nt->add_input("value", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f), INPUT_CONSTANT);
	nt->add_output("value", TYPE_FLOAT3);
	
	nt = NodeGraph::add_function_node_type("VALUE_FLOAT4");
	nt->add_input("value", TYPE_FLOAT4, float4(0.0f, 0.0f, 0.0f, 0.0f), INPUT_CONSTANT);
	nt->add_output("value", TYPE_FLOAT4);
	
	nt = NodeGraph::add_function_node_type("VALUE_INT");
	nt->add_input("value", TYPE_INT, 0, INPUT_CONSTANT);
	nt->add_output("value", TYPE_INT);
	
	nt = NodeGraph::add_function_node_type("VALUE_MATRIX44");
	nt->add_input("value", TYPE_MATRIX44, matrix44::identity(), INPUT_CONSTANT);
	nt->add_output("value", TYPE_MATRIX44);
	
	nt = NodeGraph::add_function_node_type("VALUE_STRING");
	nt->add_input("value", TYPE_STRING, "", INPUT_CONSTANT);
	nt->add_output("value", TYPE_STRING);
	
	nt = NodeGraph::add_function_node_type("VALUE_POINTER");
	nt->add_input("value", TYPE_POINTER, PointerRNA_NULL, INPUT_CONSTANT);
	nt->add_output("value", TYPE_POINTER);
	
	nt = NodeGraph::add_function_node_type("VALUE_MESH");
	nt->add_input("value", TYPE_MESH, __empty_mesh__, INPUT_CONSTANT);
	nt->add_output("value", TYPE_MESH);
	
	nt = NodeGraph::add_function_node_type("VALUE_DUPLIS");
	nt->add_input("value", TYPE_DUPLIS, duplis_ptr(new DupliList()), INPUT_CONSTANT);
	nt->add_output("value", TYPE_DUPLIS);
	
	nt = NodeGraph::add_function_node_type("GET_ELEM_FLOAT3");
	nt->add_input("index", TYPE_INT, 0, INPUT_CONSTANT);
	nt->add_input("value", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", TYPE_FLOAT);
	
	nt = NodeGraph::add_function_node_type("SET_FLOAT3");
	nt->add_input("value_x", TYPE_FLOAT, 0.0f);
	nt->add_input("value_y", TYPE_FLOAT, 0.0f);
	nt->add_input("value_z", TYPE_FLOAT, 0.0f);
	nt->add_output("value", TYPE_FLOAT3);
	
	nt = NodeGraph::add_function_node_type("GET_ELEM_FLOAT4");
	nt->add_input("index", TYPE_INT, 0, INPUT_CONSTANT);
	nt->add_input("value", TYPE_FLOAT4, float4(0.0f, 0.0f, 0.0f, 0.0f));
	nt->add_output("value", TYPE_FLOAT);
	
	nt = NodeGraph::add_function_node_type("SET_FLOAT4");
	nt->add_input("value_x", TYPE_FLOAT, 0.0f);
	nt->add_input("value_y", TYPE_FLOAT, 0.0f);
	nt->add_input("value_z", TYPE_FLOAT, 0.0f);
	nt->add_input("value_w", TYPE_FLOAT, 0.0f);
	nt->add_output("value", TYPE_FLOAT4);
	
	#define BINARY_MATH_NODE(name) \
	nt = NodeGraph::add_function_node_type(STRINGIFY(name)); \
	nt->add_input("value_a", TYPE_FLOAT, 0.0f); \
	nt->add_input("value_b", TYPE_FLOAT, 0.0f); \
	nt->add_output("value", TYPE_FLOAT)
	
	#define UNARY_MATH_NODE(name) \
	nt = NodeGraph::add_function_node_type(STRINGIFY(name)); \
	nt->add_input("value", TYPE_FLOAT, 0.0f); \
	nt->add_output("value", TYPE_FLOAT)
	
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
	nt->add_input("value_a", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", TYPE_FLOAT3);
	
	nt = NodeGraph::add_function_node_type("SUB_FLOAT3");
	nt->add_input("value_a", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", TYPE_FLOAT3);
	
	nt = NodeGraph::add_function_node_type("MUL_FLOAT3");
	nt->add_input("value_a", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", TYPE_FLOAT3);
	
	nt = NodeGraph::add_function_node_type("DIV_FLOAT3");
	nt->add_input("value_a", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", TYPE_FLOAT3);
	
	nt = NodeGraph::add_function_node_type("MUL_FLOAT3_FLOAT");
	nt->add_input("value_a", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", TYPE_FLOAT, 0.0f);
	nt->add_output("value", TYPE_FLOAT3);
	
	nt = NodeGraph::add_function_node_type("DIV_FLOAT3_FLOAT");
	nt->add_input("value_a", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", TYPE_FLOAT, 0.0f);
	nt->add_output("value", TYPE_FLOAT3);
	
	nt = NodeGraph::add_function_node_type("AVERAGE_FLOAT3");
	nt->add_input("value_a", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", TYPE_FLOAT3);
	
	nt = NodeGraph::add_function_node_type("DOT_FLOAT3");
	nt->add_input("value_a", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", TYPE_FLOAT);
	
	nt = NodeGraph::add_function_node_type("CROSS_FLOAT3");
	nt->add_input("value_a", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", TYPE_FLOAT3);
	
	nt = NodeGraph::add_function_node_type("NORMALIZE_FLOAT3");
	nt->add_input("value", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("vector", TYPE_FLOAT3);
	nt->add_output("value", TYPE_FLOAT);
	
	nt = NodeGraph::add_function_node_type("MIX_RGB");
	nt->add_input("mode", TYPE_INT, 0, INPUT_CONSTANT);
	nt->add_input("factor", TYPE_FLOAT, 0.0f);
	nt->add_input("color1", TYPE_FLOAT4, float4(0.0f, 0.0f, 0.0f, 1.0f));
	nt->add_input("color2", TYPE_FLOAT4, float4(0.0f, 0.0f, 0.0f, 1.0f));
	nt->add_output("color", TYPE_FLOAT4);
	
	nt = NodeGraph::add_function_node_type("INT_TO_RANDOM");
	nt->add_input("seed", TYPE_INT, 0, INPUT_CONSTANT);
	nt->add_input("value", TYPE_INT, 0);
	nt->add_output("irandom", TYPE_INT);
	nt->add_output("frandom", TYPE_FLOAT);
	
	nt = NodeGraph::add_function_node_type("FLOAT_TO_RANDOM");
	nt->add_input("seed", TYPE_INT, 0, INPUT_CONSTANT);
	nt->add_input("value", TYPE_FLOAT, 0);
	nt->add_output("irandom", TYPE_INT);
	nt->add_output("frandom", TYPE_FLOAT);
	
	nt = NodeGraph::add_function_node_type("TEX_PROC_VORONOI");
	nt->add_input("distance_metric", TYPE_INT, 0, INPUT_CONSTANT);
	nt->add_input("color_type", TYPE_INT, 0, INPUT_CONSTANT);
	nt->add_input("minkowski_exponent", TYPE_FLOAT, 2.5f);
	nt->add_input("scale", TYPE_FLOAT, 1.0f);
	nt->add_input("noise_size", TYPE_FLOAT, 1.0f);
	nt->add_input("nabla", TYPE_FLOAT, 0.05f);
	nt->add_input("w1", TYPE_FLOAT, 1.0f);
	nt->add_input("w2", TYPE_FLOAT, 0.0f);
	nt->add_input("w3", TYPE_FLOAT, 0.0f);
	nt->add_input("w4", TYPE_FLOAT, 0.0f);
	nt->add_input("position", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("intensity", TYPE_FLOAT);
	nt->add_output("color", TYPE_FLOAT4);
	nt->add_output("normal", TYPE_FLOAT3);
	
	nt = NodeGraph::add_function_node_type("TEX_PROC_CLOUDS");
	nt->add_input("position", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("nabla", TYPE_FLOAT, 0.05f);
	nt->add_input("size", TYPE_FLOAT, 1.0f);
	nt->add_input("depth", TYPE_INT, 2, INPUT_CONSTANT);
	nt->add_input("noise_basis", TYPE_INT, 0, INPUT_CONSTANT);
	nt->add_input("noise_hard", TYPE_INT, 0, INPUT_CONSTANT);
	nt->add_output("intensity", TYPE_FLOAT);
	nt->add_output("color", TYPE_FLOAT4);
	nt->add_output("normal", TYPE_FLOAT3);
	
	nt = NodeGraph::add_function_node_type("OBJECT_LOOKUP");
	nt->add_input("key", TYPE_INT, 0, INPUT_CONSTANT);
	nt->add_output("object", TYPE_POINTER);
	
	nt = NodeGraph::add_function_node_type("OBJECT_TRANSFORM");
	nt->add_input("object", TYPE_POINTER, PointerRNA_NULL);
	nt->add_output("transform", TYPE_MATRIX44);
	
	nt = NodeGraph::add_function_node_type("EFFECTOR_TRANSFORM");
	nt->add_input("object", TYPE_INT, 0, INPUT_CONSTANT);
	nt->add_output("transform", TYPE_MATRIX44);
	
	nt = NodeGraph::add_function_node_type("EFFECTOR_CLOSEST_POINT");
	nt->add_input("object", TYPE_POINTER, PointerRNA_NULL);
	nt->add_input("vector", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("position", TYPE_FLOAT3);
	nt->add_output("normal", TYPE_FLOAT3);
	nt->add_output("tangent", TYPE_FLOAT3);
	
	nt = NodeGraph::add_kernel_node_type("MESH_LOAD");
	nt->add_input("base_mesh", TYPE_POINTER, PointerRNA_NULL);
	nt->add_output("mesh", TYPE_MESH);
	
	nt = NodeGraph::add_kernel_node_type("MESH_COMBINE");
	nt->add_input("mesh_a", TYPE_MESH, __empty_mesh__);
	nt->add_input("mesh_b", TYPE_MESH, __empty_mesh__);
	nt->add_output("mesh_out", TYPE_MESH);
	
	nt = NodeGraph::add_kernel_node_type("MESH_ARRAY");
	nt->add_input("mesh_in", TYPE_MESH, __empty_mesh__);
	nt->add_input("count", TYPE_INT, 1);
	nt->add_input("transform", TYPE_MATRIX44, matrix44::identity(), INPUT_FUNCTION);
	nt->add_output("mesh_out", TYPE_MESH);
	nt->add_output("iteration", TYPE_INT, OUTPUT_LOCAL);
	
	nt = NodeGraph::add_kernel_node_type("MESH_DISPLACE");
	nt->add_input("mesh_in", TYPE_MESH, __empty_mesh__);
	nt->add_input("vector", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f), INPUT_FUNCTION);
	nt->add_output("mesh_out", TYPE_MESH);
	nt->add_output("element.index", TYPE_FLOAT3, OUTPUT_LOCAL);
	nt->add_output("element.location", TYPE_FLOAT3, OUTPUT_LOCAL);
	
	nt = NodeGraph::add_kernel_node_type("MESH_BOOLEAN");
	nt->add_input("mesh_in", TYPE_MESH, __empty_mesh__);
	nt->add_input("object", TYPE_POINTER, PointerRNA_NULL);
	nt->add_input("transform", TYPE_MATRIX44, matrix44::identity());
	nt->add_input("inverse_transform", TYPE_MATRIX44, matrix44::identity());
	nt->add_input("operation", TYPE_INT, -1);
	nt->add_input("separate", TYPE_INT, 0);
	nt->add_input("dissolve", TYPE_INT, 1);
	nt->add_input("connect_regions", TYPE_INT, 1);
	nt->add_input("threshold", TYPE_FLOAT, 0.0f);
	nt->add_output("mesh_out", TYPE_MESH);
	
	nt = NodeGraph::add_function_node_type("CURVE_PATH");
	nt->add_input("object", TYPE_POINTER, PointerRNA_NULL);
	nt->add_input("transform", TYPE_MATRIX44, matrix44::identity());
	nt->add_input("inverse_transform", TYPE_MATRIX44, matrix44::identity());
	nt->add_input("parameter", TYPE_FLOAT, 0.0f);
	nt->add_output("location", TYPE_FLOAT3);
	nt->add_output("direction", TYPE_FLOAT3);
	nt->add_output("normal", TYPE_FLOAT3);
	nt->add_output("rotation", TYPE_MATRIX44);
	nt->add_output("radius", TYPE_FLOAT);
	nt->add_output("weight", TYPE_FLOAT);
	nt->add_output("tilt", TYPE_FLOAT);
	
	nt = NodeGraph::add_function_node_type("MAKE_DUPLI");
	nt->add_input("object", TYPE_POINTER, PointerRNA_NULL);
	nt->add_input("transform", TYPE_MATRIX44, matrix44::identity());
	nt->add_input("index", TYPE_INT, 0);
	nt->add_input("hide", TYPE_INT, 0);
	nt->add_input("recursive", TYPE_INT, 1);
	nt->add_output("dupli", TYPE_DUPLIS);
	
	nt = NodeGraph::add_function_node_type("ADD_MATRIX44");
	nt->add_input("value_a", TYPE_MATRIX44, matrix44::identity());
	nt->add_input("value_b", TYPE_MATRIX44, matrix44::identity());
	nt->add_output("value", TYPE_MATRIX44);
	
	nt = NodeGraph::add_function_node_type("SUB_MATRIX44");
	nt->add_input("value_a", TYPE_MATRIX44, matrix44::identity());
	nt->add_input("value_b", TYPE_MATRIX44, matrix44::identity());
	nt->add_output("value", TYPE_MATRIX44);
	
	nt = NodeGraph::add_function_node_type("MUL_MATRIX44");
	nt->add_input("value_a", TYPE_MATRIX44, matrix44::identity());
	nt->add_input("value_b", TYPE_MATRIX44, matrix44::identity());
	nt->add_output("value", TYPE_MATRIX44);
	
	nt = NodeGraph::add_function_node_type("MUL_MATRIX44_FLOAT");
	nt->add_input("value_a", TYPE_MATRIX44, matrix44::identity());
	nt->add_input("value_b", TYPE_FLOAT, 0.0f);
	nt->add_output("value", TYPE_MATRIX44);
	
	nt = NodeGraph::add_function_node_type("DIV_MATRIX44_FLOAT");
	nt->add_input("value_a", TYPE_MATRIX44, matrix44::identity());
	nt->add_input("value_b", TYPE_FLOAT, 1.0f);
	nt->add_output("value", TYPE_MATRIX44);
	
	nt = NodeGraph::add_function_node_type("NEGATE_MATRIX44");
	nt->add_input("value", TYPE_MATRIX44, matrix44::identity());
	nt->add_output("value", TYPE_MATRIX44);
	
	nt = NodeGraph::add_function_node_type("TRANSPOSE_MATRIX44");
	nt->add_input("value", TYPE_MATRIX44, matrix44::identity());
	nt->add_output("value", TYPE_MATRIX44);
	
	nt = NodeGraph::add_function_node_type("INVERT_MATRIX44");
	nt->add_input("value", TYPE_MATRIX44, matrix44::identity());
	nt->add_output("value", TYPE_MATRIX44);
	
	nt = NodeGraph::add_function_node_type("ADJOINT_MATRIX44");
	nt->add_input("value", TYPE_MATRIX44, matrix44::identity());
	nt->add_output("value", TYPE_MATRIX44);
	
	nt = NodeGraph::add_function_node_type("DETERMINANT_MATRIX44");
	nt->add_input("value", TYPE_MATRIX44, matrix44::identity());
	nt->add_output("value", TYPE_FLOAT);
	
	nt = NodeGraph::add_function_node_type("MUL_MATRIX44_FLOAT3");
	nt->add_input("value_a", TYPE_MATRIX44, matrix44::identity());
	nt->add_input("value_b", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", TYPE_FLOAT3);
	
	nt = NodeGraph::add_function_node_type("MUL_MATRIX44_FLOAT4");
	nt->add_input("value_a", TYPE_MATRIX44, matrix44::identity());
	nt->add_input("value_b", TYPE_FLOAT4, float4(0.0f, 0.0f, 0.0f, 0.0f));
	nt->add_output("value", TYPE_FLOAT4);
	
	nt = NodeGraph::add_function_node_type("MATRIX44_TO_LOC");
	nt->add_input("matrix", TYPE_MATRIX44, matrix44::identity());
	nt->add_output("loc", TYPE_FLOAT3);
	
	nt = NodeGraph::add_function_node_type("MATRIX44_TO_EULER");
	nt->add_input("order", TYPE_INT, EULER_ORDER_DEFAULT, INPUT_CONSTANT);
	nt->add_input("matrix", TYPE_MATRIX44, matrix44::identity());
	nt->add_output("euler", TYPE_FLOAT3);
	
	nt = NodeGraph::add_function_node_type("MATRIX44_TO_AXISANGLE");
	nt->add_input("matrix", TYPE_MATRIX44, matrix44::identity());
	nt->add_output("axis", TYPE_FLOAT3);
	nt->add_output("angle", TYPE_FLOAT);
	
	nt = NodeGraph::add_function_node_type("MATRIX44_TO_SCALE");
	nt->add_input("matrix", TYPE_MATRIX44, matrix44::identity());
	nt->add_output("scale", TYPE_FLOAT3);
	
	nt = NodeGraph::add_function_node_type("LOC_TO_MATRIX44");
	nt->add_input("loc", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("matrix", TYPE_MATRIX44);
	
	nt = NodeGraph::add_function_node_type("EULER_TO_MATRIX44");
	nt->add_input("order", TYPE_INT, EULER_ORDER_DEFAULT, INPUT_CONSTANT);
	nt->add_input("euler", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("matrix", TYPE_MATRIX44);
	
	nt = NodeGraph::add_function_node_type("AXISANGLE_TO_MATRIX44");
	nt->add_input("axis", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("angle", TYPE_FLOAT, 0.0f);
	nt->add_output("matrix", TYPE_MATRIX44);
	
	nt = NodeGraph::add_function_node_type("SCALE_TO_MATRIX44");
	nt->add_input("scale", TYPE_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("matrix", TYPE_MATRIX44);
}

void nodes_init()
{
	create_empty_mesh(__empty_mesh__);
	
	register_opcode_node_types();
}

void nodes_free()
{
	destroy_empty_mesh(__empty_mesh__);
}

} /* namespace bvm */

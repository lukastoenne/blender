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
#include <vector>
#include <cassert>

#include "bvm_nodegraph.h"
#include "bvm_opcode.h"

namespace bvm {

NodeSocket::NodeSocket(const string &name, BVMType type, Value *default_value, bool constant) :
    name(name),
    type(type),
    default_value(default_value),
    constant(constant)
{
}

NodeSocket::~NodeSocket()
{
}

/* ------------------------------------------------------------------------- */

NodeType::NodeType(const string &name) :
    name(name)
{
}

NodeType::~NodeType()
{
}

const NodeSocket *NodeType::find_input(int index) const
{
	BLI_assert(index >= 0 && index < inputs.size());
	return &inputs[index];
}

const NodeSocket *NodeType::find_output(int index) const
{
	BLI_assert(index >= 0 && index < outputs.size());
	return &outputs[index];
}

const NodeSocket *NodeType::find_input(const string &name) const
{
	for (SocketList::const_iterator it = inputs.begin(); it != inputs.end(); ++it) {
		const NodeSocket &socket = *it;
		if (socket.name == name)
			return &socket;
	}
	return NULL;
}

const NodeSocket *NodeType::find_output(const string &name) const
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

const NodeSocket *NodeType::add_input(const string &name, BVMType type, Value *default_value, bool constant)
{
	BLI_assert(!find_input(name));
	inputs.push_back(NodeSocket(name, type, default_value, constant));
	return &inputs.back();
}

const NodeSocket *NodeType::add_output(const string &name, BVMType type, Value *default_value)
{
	BLI_assert(!find_output(name));
	outputs.push_back(NodeSocket(name, type, default_value, false));
	return &outputs.back();
}

/* ------------------------------------------------------------------------- */

NodeInstance::NodeInstance(const NodeType *type, const string &name) :
    type(type), name(name)
{
}

NodeInstance::~NodeInstance()
{
}

NodeInstance *NodeInstance::find_input_link_node(const string &name) const
{
	InputMap::const_iterator it = inputs.find(name);
	return (it != inputs.end()) ? it->second.link_node : NULL;
}

NodeInstance *NodeInstance::find_input_link_node(int index) const
{
	const NodeSocket *socket = type->find_input(index);
	return socket ? find_input_link_node(socket->name) : NULL;
}

const NodeSocket *NodeInstance::find_input_link_socket(const string &name) const
{
	InputMap::const_iterator it = inputs.find(name);
	return (it != inputs.end()) ? it->second.link_socket : NULL;
}

const NodeSocket *NodeInstance::find_input_link_socket(int index) const
{
	const NodeSocket *socket = type->find_input(index);
	return socket ? find_input_link_socket(socket->name) : NULL;
}

const NodeGraphInput *NodeInstance::find_input_extern(const string &name) const
{
	InputMap::const_iterator it = inputs.find(name);
	return (it != inputs.end()) ? it->second.graph_input : NULL;
}

const NodeGraphInput *NodeInstance::find_input_extern(int index) const
{
	const NodeSocket *socket = type->find_input(index);
	return socket ? find_input_extern(socket->name) : NULL;
}

Value *NodeInstance::find_input_value(const string &name) const
{
	InputMap::const_iterator it = inputs.find(name);
	return (it != inputs.end()) ? it->second.value : NULL;
}

Value *NodeInstance::find_input_value(int index) const
{
	const NodeSocket *socket = type->find_input(index);
	return socket ? find_input_value(socket->name) : NULL;
}

Value *NodeInstance::find_output_value(const string &name) const
{
	OutputMap::const_iterator it = outputs.find(name);
	return (it != outputs.end()) ? it->second.value : NULL;
}

Value *NodeInstance::find_output_value(int index) const
{
	const NodeSocket *socket = type->find_output(index);
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

bool NodeInstance::set_input_link(const string &name, NodeInstance *from_node, const NodeSocket *from_socket)
{
	InputInstance &input = inputs[name];
	if (input.link_node && input.link_socket)
		return false;
	
	input.link_node = from_node;
	input.link_socket = from_socket;
	return true;
}

bool NodeInstance::set_input_extern(const string &name, const NodeGraphInput *graph_input)
{
	InputInstance &input = inputs[name];
	if (input.graph_input)
		return false;
	
	input.graph_input = graph_input;
	return true;
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
	const NodeSocket *socket = type->find_input(index);
	return socket ? has_input_link(socket->name) : false;
}

bool NodeInstance::has_input_extern(const string &name) const
{
	InputMap::const_iterator it = inputs.find(name);
	if (it != inputs.end()) {
		const InputInstance &input = it->second;
		return (input.graph_input);
	}
	else
		return false;
}

bool NodeInstance::has_input_extern(int index) const
{
	const NodeSocket *socket = type->find_input(index);
	return socket ? has_input_extern(socket->name) : false;
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
	const NodeSocket *socket = type->find_input(index);
	return socket ? has_input_value(socket->name) : false;
}

bool NodeInstance::is_input_constant(const string &name) const
{
	const NodeSocket *socket = type->find_input(name);
	return socket ? socket->constant : false;
}

bool NodeInstance::is_input_constant(int index) const
{
	const NodeSocket *socket = type->find_input(index);
	return socket ? socket->constant : false;
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
	const NodeSocket *socket = type->find_output(index);
	return socket ? has_output_value(socket->name) : false;
}

/* ------------------------------------------------------------------------- */

NodeGraph::NodeTypeMap NodeGraph::node_types;

const NodeType *NodeGraph::find_node_type(const string &name)
{
	NodeTypeMap::const_iterator it = node_types.find(name);
	return (it != node_types.end())? &it->second : NULL;
}

NodeType *NodeGraph::add_node_type(const string &name)
{
	std::pair<NodeTypeMap::iterator, bool> result = node_types.insert(NodeTypeMapPair(name, NodeType(name)));
	if (result.second) {
		NodeType *type = &result.first->second;
		return type;
	}
	else
		return NULL;
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
}

NodeInstance *NodeGraph::get_node(const string &name)
{
	NodeInstanceMap::iterator it = nodes.find(name);
	return (it != nodes.end())? &it->second : NULL;
}

NodeInstance *NodeGraph::add_node(const string &type, const string &name)
{
	const NodeType *nodetype = find_node_type(type);
	std::pair<NodeInstanceMap::iterator, bool> result =
	        nodes.insert(NodeInstanceMapPair(name, NodeInstance(nodetype, name)));
	
	return (result.second)? &result.first->second : NULL;
}

const NodeGraphInput *NodeGraph::get_input(int index) const
{
	BLI_assert(index >= 0 && index < inputs.size());
	return &inputs[index];
}

const NodeGraphOutput *NodeGraph::get_output(int index) const
{
	BLI_assert(index >= 0 && index < outputs.size());
	return &outputs[index];
}

const NodeGraphInput *NodeGraph::get_input(const string &name) const
{
	for (InputList::const_iterator it = inputs.begin(); it != inputs.end(); ++it) {
		if (it->name == name)
			return &(*it);
	}
	return NULL;
}

const NodeGraphOutput *NodeGraph::get_output(const string &name) const
{
	for (OutputList::const_iterator it = outputs.begin(); it != outputs.end(); ++it) {
		if (it->name == name)
			return &(*it);
	}
	return NULL;
}

const NodeGraphInput *NodeGraph::add_input(const string &name, BVMType type)
{
	BLI_assert(!get_input(name));
	inputs.push_back(NodeGraphInput(name, type));
	return &inputs.back();
}

const NodeGraphOutput *NodeGraph::add_output(const string &name, BVMType type, Value *default_value)
{
	BLI_assert(!get_output(name));
	outputs.push_back(NodeGraphOutput(name, type, default_value));
	return &outputs.back();
}

void NodeGraph::set_input_argument(const string &name, Value *value)
{
	for (InputList::iterator it = inputs.begin(); it != inputs.end(); ++it) {
		NodeGraphInput &input = *it;
		if (input.name == name) {
			input.value = value;
		}
	}
}

void NodeGraph::set_output_link(const string &name, NodeInstance *link_node, const string &link_socket)
{
	for (OutputList::iterator it = outputs.begin(); it != outputs.end(); ++it) {
		NodeGraphOutput &output = *it;
		if (output.name == name) {
			output.link_node = link_node;
			output.link_socket = link_node->type->find_output(link_socket);
			BLI_assert(output.link_node && output.link_socket);
		}
	}
}

void NodeGraph::dump(std::ostream &s)
{
	s << "NodeGraph\n";
	
	for (NodeInstanceMap::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
		const NodeInstance *node = &it->second;
		const NodeType *type = node->type;
		s << "  Node '" << node->name << "'\n";
		
		for (int i = 0; i < type->inputs.size(); ++i) {
			const NodeSocket *socket = &type->inputs[i];
			
			s << "    Input '" << socket->name << "'";
			NodeInstance *link_node = node->find_input_link_node(i);
			const NodeSocket *link_socket = node->find_input_link_socket(i);
//			Value *value = node->find_input_value(i);
			
			BLI_assert((bool)link_node == (bool)link_socket);
			
			if (link_node && link_socket) {
				s << " <== " << link_node->name << ":" << link_socket->name;
				// TODO value print function
//				s << " (" << value << ")\n";
				s << "\n";
			}
			else {
				// TODO value print function
//				s << " " << value << "\n";
				s << "\n";
			}
		}
		
		for (int i = 0; i < type->outputs.size(); ++i) {
			const NodeSocket *socket = &type->outputs[i];
			
			s << "    Output '" << socket->name << "'";
//			Value *value = node->find_output_value(i);
			
			// TODO value print function
//			s << " " << value << "\n";
			s << "\n";
		}
	}
}

/* ------------------------------------------------------------------------- */

OpCode get_opcode_from_node_type(const string &node)
{
	#define NODETYPE(name) \
	if (node == STRINGIFY(name)) \
		return OP_##name
	
	NODETYPE(PASS_FLOAT);
	NODETYPE(PASS_FLOAT3);
	NODETYPE(SET_FLOAT3);
	NODETYPE(GET_ELEM_FLOAT3);
	
	NODETYPE(EFFECTOR_POSITION);
	NODETYPE(EFFECTOR_VELOCITY);
	
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
	
	NODETYPE(ADD_FLOAT3);
	NODETYPE(SUB_FLOAT3);
	NODETYPE(AVERAGE_FLOAT3);
	NODETYPE(DOT_FLOAT3);
	NODETYPE(CROSS_FLOAT3);
	NODETYPE(NORMALIZE_FLOAT3);
	
	NODETYPE(EFFECTOR_CLOSEST_POINT);
	
	#undef NODETYPE
	
	assert(!"Invalid node type");
	return OP_NOOP;
}

void register_opcode_node_types()
{
	NodeType *nt;
	
	nt = NodeGraph::add_node_type("PASS_FLOAT");
	nt->add_input("value", BVM_FLOAT, 0.0f);
	nt->add_output("value", BVM_FLOAT, 0.0f);
	
	nt = NodeGraph::add_node_type("PASS_FLOAT3");
	nt->add_input("value", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	
	nt = NodeGraph::add_node_type("GET_ELEM_FLOAT3");
	nt->add_input("index", BVM_INT, 0, true);
	nt->add_input("value", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", BVM_FLOAT, 0.0f);
	
	nt = NodeGraph::add_node_type("SET_FLOAT3");
	nt->add_input("value_x", BVM_FLOAT, 0.0f);
	nt->add_input("value_y", BVM_FLOAT, 0.0f);
	nt->add_input("value_z", BVM_FLOAT, 0.0f);
	nt->add_output("value", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	
	nt = NodeGraph::add_node_type("EFFECTOR_POSITION");
	nt->add_output("value", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	
	nt = NodeGraph::add_node_type("EFFECTOR_VELOCITY");
	nt->add_output("value", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	
	#define BINARY_MATH_NODE(name) \
	nt = NodeGraph::add_node_type(STRINGIFY(name)); \
	nt->add_input("value_a", BVM_FLOAT, 0.0f); \
	nt->add_input("value_b", BVM_FLOAT, 0.0f); \
	nt->add_output("value", BVM_FLOAT, 0.0f)
	
	#define UNARY_MATH_NODE(name) \
	nt = NodeGraph::add_node_type(STRINGIFY(name)); \
	nt->add_input("value", BVM_FLOAT, 0.0f); \
	nt->add_output("value", BVM_FLOAT, 0.0f)
	
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
	
	#undef BINARY_MATH_NODE
	#undef UNARY_MATH_NODE
	
	nt = NodeGraph::add_node_type("ADD_FLOAT3");
	nt->add_input("value_a", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	
	nt = NodeGraph::add_node_type("SUB_FLOAT3");
	nt->add_input("value_a", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	
	nt = NodeGraph::add_node_type("AVERAGE_FLOAT3");
	nt->add_input("value_a", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	
	nt = NodeGraph::add_node_type("DOT_FLOAT3");
	nt->add_input("value_a", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", BVM_FLOAT, 0.0f);
	
	nt = NodeGraph::add_node_type("CROSS_FLOAT3");
	nt->add_input("value_a", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	
	nt = NodeGraph::add_node_type("NORMALIZE_FLOAT3");
	nt->add_input("value", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("vector", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", BVM_FLOAT, 0.0f);
	
	nt = NodeGraph::add_node_type("EFFECTOR_CLOSEST_POINT");
	nt->add_input("object", BVM_INT, 0, true);
	nt->add_input("vector", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("position", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("normal", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("tangent", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
}

} /* namespace bvm */

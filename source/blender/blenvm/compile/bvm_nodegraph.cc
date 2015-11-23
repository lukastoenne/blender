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
#include <cstdio>
#include <sstream>

#include "bvm_nodegraph.h"
#include "bvm_opcode.h"

namespace bvm {

NodeSocket::NodeSocket(const string &name, const TypeDesc &typedesc, Value *default_value, bool constant) :
    name(name),
    typedesc(typedesc),
    default_value(default_value),
    constant(constant)
{
}

NodeSocket::~NodeSocket()
{
}

/* ------------------------------------------------------------------------- */

NodeType::NodeType(const string &name) :
    name(name),
    is_pass(false)
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
	assert(nodetype);
	
	string final = name;
	if (final.empty()) {
		std::stringstream ss;
		ss << nodetype->name;
		final = ss.str();
	}
	if (nodes.find(final) != nodes.end()) {
		std::stringstream ss;
		ss << "_" << nodes.size();
		final = ss.str();
	}
	
	std::pair<NodeInstanceMap::iterator, bool> result =
	        nodes.insert(NodeInstanceMapPair(final, NodeInstance(nodetype, final)));
	
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

/* ------------------------------------------------------------------------- */
/* Link Type Converters */

SocketPair NodeGraph::add_float_converter(const SocketPair &from, BVMType to_type)
{
	SocketPair result(NULL, "");
	switch (to_type) {
		case BVM_FLOAT3: {
			NodeInstance *node = add_node("SET_FLOAT3");
			add_link(from.node, from.socket, node, "value_x");
			add_link(from.node, from.socket, node, "value_y");
			add_link(from.node, from.socket, node, "value_z");
			result = SocketPair(node, "value");
			break;
		}
		case BVM_FLOAT4: {
			NodeInstance *node = add_node("SET_FLOAT4");
			add_link(from.node, from.socket, node, "value_x");
			add_link(from.node, from.socket, node, "value_y");
			add_link(from.node, from.socket, node, "value_z");
			add_link(from.node, from.socket, node, "value_w");
			result = SocketPair(node, "value");
			break;
		}
		case BVM_INT: {
			NodeInstance *node = add_node("FLOAT_TO_INT");
			add_link(from.node, from.socket, node, "value");
			result = SocketPair(node, "value");
			break;
		}
		default:
			break;
	}
	return result;
}

SocketPair NodeGraph::add_float3_converter(const SocketPair &from, BVMType to_type)
{
	SocketPair result(NULL, "");
	switch (to_type) {
		case BVM_FLOAT4: {
			NodeInstance *node_x = add_node("GET_ELEM_FLOAT3");
			node_x->set_input_value("index", 0);
			add_link(from.node, from.socket, node_x, "value");
			NodeInstance *node_y = add_node("GET_ELEM_FLOAT3");
			node_y->set_input_value("index", 1);
			add_link(from.node, from.socket, node_y, "value");
			NodeInstance *node_z = add_node("GET_ELEM_FLOAT3");
			node_z->set_input_value("index", 2);
			add_link(from.node, from.socket, node_z, "value");
			
			NodeInstance *node = add_node("SET_FLOAT4");
			add_link(node_x, "value", node, "value_x");
			add_link(node_y, "value", node, "value_y");
			add_link(node_z, "value", node, "value_z");
			node->set_input_value("value_w", 1.0f);
			result = SocketPair(node, "value");
			break;
		}
		default:
			break;
	}
	return result;
}

SocketPair NodeGraph::add_float4_converter(const SocketPair &from, BVMType to_type)
{
	SocketPair result(NULL, "");
	switch (to_type) {
		case BVM_FLOAT3: {
			NodeInstance *node_x = add_node("GET_ELEM_FLOAT4");
			node_x->set_input_value("index", 0);
			add_link(from.node, from.socket, node_x, "value");
			NodeInstance *node_y = add_node("GET_ELEM_FLOAT4");
			node_y->set_input_value("index", 1);
			add_link(from.node, from.socket, node_y, "value");
			NodeInstance *node_z = add_node("GET_ELEM_FLOAT4");
			node_z->set_input_value("index", 2);
			add_link(from.node, from.socket, node_z, "value");
			
			NodeInstance *node = add_node("SET_FLOAT3");
			add_link(node_x, "value", node, "value_x");
			add_link(node_y, "value", node, "value_y");
			add_link(node_z, "value", node, "value_z");
			result = SocketPair(node, "value");
			break;
		}
		default:
			break;
	}
	return result;
}

SocketPair NodeGraph::add_int_converter(const SocketPair &/*from*/, BVMType /*to_type*/)
{
	SocketPair result(NULL, "");
	return result;
}

SocketPair NodeGraph::add_matrix44_converter(const SocketPair &/*from*/, BVMType /*to_type*/)
{
	SocketPair result(NULL, "");
	return result;
}

SocketPair NodeGraph::add_type_converter(const SocketPair &from, const TypeDesc &to_typedesc)
{
	SocketPair result(NULL, "");
	const NodeSocket *from_socket = from.node->type->find_output(from.socket);
	/* TODO only uses base type so far */
	BVMType to_type = to_typedesc.base_type;
	BVMType from_type = from_socket->typedesc.base_type;
	
	if (from_type == to_type) {
		result = from;
	}
	else {
		switch (from_type) {
			case BVM_FLOAT: result = add_float_converter(from, to_type); break;
			case BVM_FLOAT3: result = add_float3_converter(from, to_type); break;
			case BVM_FLOAT4: result = add_float4_converter(from, to_type); break;
			case BVM_INT: result = add_int_converter(from, to_type); break;
			case BVM_MATRIX44: result = add_matrix44_converter(from, to_type); break;
			default: break;
		}
	}
	return result;
}

/* ------------------------------------------------------------------------- */
/* Optimization */

void NodeGraph::skip_pass_nodes()
{
	/* redirect links to skip over 'pass'-type nodes */
	for (NodeInstanceMap::iterator it = nodes.begin(); it != nodes.end(); ++it) {
		NodeInstance &node = it->second;
		
		if (node.type->is_pass)
			continue;
		
		for (NodeInstance::InputMap::iterator it_input = node.inputs.begin();
		     it_input != node.inputs.end();
		     ++it_input) {
			NodeInstance::InputInstance &input = it_input->second;
			
			NodeInstance *link_node = input.link_node;
			const NodeSocket *link_socket = input.link_socket;
			while (link_node && link_node->type->is_pass) {
				/* note: order of assignment is important here! */
				link_socket = link_node->find_input_link_socket(0);
				link_node = link_node->find_input_link_node(0);
			}
			input.link_node = link_node;
			input.link_socket = link_socket;
		}
	}
	/* same for graph outputs */
	for (OutputList::iterator it = outputs.begin(); it != outputs.end(); ++it) {
		NodeGraphOutput &output = *it;
		
		NodeInstance *link_node = output.link_node;
		const NodeSocket *link_socket = output.link_socket;
		while (link_node && link_node->type->is_pass) {
			/* note: order of assignment is important here! */
			link_socket = link_node->find_input_link_socket(0);
			link_node = link_node->find_input_link_node(0);
		}
		output.link_node = link_node;
		output.link_socket = link_socket;
	}
}

void NodeGraph::finalize()
{
	skip_pass_nodes();
}

/* === DEBUGGING === */

#define NL "\r\n"

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

static const char *debug_graphviz_fontname = "helvetica";
static float debug_graphviz_graph_label_size = 20.0f;
static float debug_graphviz_node_label_size = 14.0f;

struct DebugContext {
	FILE *file;
};

static void debug_fprintf(const DebugContext &ctx, const char *fmt, ...) ATTR_PRINTF_FORMAT(2, 3);
static void debug_fprintf(const DebugContext &ctx, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(ctx.file, fmt, args);
	va_end(args);
}

inline static int debug_input_index(const NodeInstance *node, const string &name)
{
	int index = 0;
	for (NodeType::SocketList::const_iterator it = node->type->inputs.begin();
	     it != node->type->inputs.end();
	     ++it, ++index) {
		if ((*it).name == name)
			return index;
	}
	return -1;
}

inline static int debug_output_index(const NodeInstance *node, const string &name)
{
	int index = 0;
	for (NodeType::SocketList::const_iterator it = node->type->outputs.begin();
	     it != node->type->outputs.end();
	     ++it, ++index) {
		if ((*it).name == name)
			return index;
	}
	return -1;
}

static void debug_graphviz_node(const DebugContext &ctx, const NodeInstance *node)
{
	const char *shape = "box";
	const char *style = "filled,rounded";
	const char *color = "black";
	const char *fillcolor = "gainsboro";
	float penwidth = 1.0f;
	string name = node->type->name;
	
	debug_fprintf(ctx, "// %s\n", node->name.c_str());
	debug_fprintf(ctx, "\"node_%p\"", node);
	debug_fprintf(ctx, "[");
	
	/* html label including rows for input/output sockets
	 * http://www.graphviz.org/doc/info/shapes.html#html
	 */
	debug_fprintf(ctx, "label=<<TABLE BORDER=\"0\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"4\">");
	debug_fprintf(ctx, "<TR><TD COLSPAN=\"2\">%s</TD></TR>", name.c_str());
	int numin = node->type->inputs.size();
	int numout = node->type->outputs.size();
	for (int i = 0; i < numin || i < numout; ++i) {
		debug_fprintf(ctx, "<TR>");
		
		if (i < numin) {
			string name_in = node->type->inputs[i].name;
			debug_fprintf(ctx, "<TD PORT=\"%s_%d\" BORDER=\"1\">%s</TD>", name_in.c_str(), i, name_in.c_str());
		}
		else
			debug_fprintf(ctx, "<TD></TD>");
			
		if (i < numout) {
			string name_out = node->type->outputs[i].name;
			debug_fprintf(ctx, "<TD PORT=\"%s_%d\" BORDER=\"1\">%s</TD>", name_out.c_str(), i, name_out.c_str());
		}
		else
			debug_fprintf(ctx, "<TD></TD>");
		
		debug_fprintf(ctx, "</TR>");
	}
	debug_fprintf(ctx, "</TABLE>>");
	
	debug_fprintf(ctx, ",fontname=\"%s\"", debug_graphviz_fontname);
	debug_fprintf(ctx, ",fontsize=\"%f\"", debug_graphviz_node_label_size);
	debug_fprintf(ctx, ",shape=\"%s\"", shape);
	debug_fprintf(ctx, ",style=\"%s\"", style);
	debug_fprintf(ctx, ",color=\"%s\"", color);
	debug_fprintf(ctx, ",fillcolor=\"%s\"", fillcolor);
	debug_fprintf(ctx, ",penwidth=\"%f\"", penwidth);
	debug_fprintf(ctx, "];" NL);
	debug_fprintf(ctx, NL);
}

static void debug_graphviz_input_output(const DebugContext &ctx,
                                        const NodeGraphInput *input,
                                        const NodeGraphOutput *output)
{
	const char *shape = "box";
	const char *style = "filled,rounded";
	const char *color = "black";
	const char *fillcolor = input ? "steelblue" : "orange";
	float penwidth = 1.0f;
	string name = input ? input->name : output->name;
	void *ptr = input ? (void *)input : (void *)output;
	
	debug_fprintf(ctx, "// %s\n", name.c_str());
	if (input)
		debug_fprintf(ctx, "\"input_%p\"", ptr);
	else
		debug_fprintf(ctx, "\"output_%p\"", ptr);
	debug_fprintf(ctx, "[");
	
	/* html label including rows for input/output sockets
	 * http://www.graphviz.org/doc/info/shapes.html#html
	 */
	debug_fprintf(ctx, "label=\"%s\"", name.c_str());
	debug_fprintf(ctx, ",fontname=\"%s\"", debug_graphviz_fontname);
	debug_fprintf(ctx, ",fontsize=\"%f\"", debug_graphviz_node_label_size);
	debug_fprintf(ctx, ",shape=\"%s\"", shape);
	debug_fprintf(ctx, ",style=\"%s\"", style);
	debug_fprintf(ctx, ",color=\"%s\"", color);
	debug_fprintf(ctx, ",fillcolor=\"%s\"", fillcolor);
	debug_fprintf(ctx, ",penwidth=\"%f\"", penwidth);
	debug_fprintf(ctx, "];" NL);
	debug_fprintf(ctx, NL);
}

static void debug_graphviz_output_links(const DebugContext &ctx,
                                        const NodeGraphOutput *output)
{
	float penwidth = 2.0f;
	
	if (output->link_node && output->link_socket) {
		const NodeInstance *tail = output->link_node;
		const string &tail_socket = output->link_socket->name;
		int tail_index = debug_output_index(tail, tail_socket);
		const NodeGraphOutput *head = output;
		debug_fprintf(ctx, "// %s:%s -> %s\n",
		              tail->name.c_str(), tail_socket.c_str(),
		              head->name.c_str());
		debug_fprintf(ctx, "\"node_%p\":\"%s_%d\"", tail, tail_socket.c_str(), tail_index);
		debug_fprintf(ctx, " -> ");
		debug_fprintf(ctx, "\"output_%p\"", head);

		debug_fprintf(ctx, "[");
		/* Note: without label an id seem necessary to avoid bugs in graphviz/dot */
		debug_fprintf(ctx, "id=\"A%sB%s:%s_%d\"",
		              head->name.c_str(),
		              tail->name.c_str(), tail_socket.c_str(), tail_index);
		
		debug_fprintf(ctx, ",penwidth=\"%f\"", penwidth);
		debug_fprintf(ctx, "];" NL);
		debug_fprintf(ctx, NL);
	}
}

static void debug_graphviz_node_links(const DebugContext &ctx, const NodeInstance *node)
{
	float penwidth = 2.0f;
	
	for (NodeInstance::InputMap::const_iterator it = node->inputs.begin(); it != node->inputs.end(); ++it) {
		const NodeInstance::InputInstance &input = it->second;
		
		if (input.link_node && input.link_socket) {
			const NodeInstance *tail = input.link_node;
			const string &tail_socket = input.link_socket->name;
			int tail_index = debug_output_index(tail, tail_socket);
			const NodeInstance *head = node;
			const string &head_socket = it->first;
			int head_index = debug_input_index(head, head_socket);
			debug_fprintf(ctx, "// %s:%s -> %s:%s\n",
			              tail->name.c_str(), tail_socket.c_str(),
			              head->name.c_str(), head_socket.c_str());
			debug_fprintf(ctx, "\"node_%p\":\"%s_%d\"", tail, tail_socket.c_str(), tail_index);
			debug_fprintf(ctx, " -> ");
			debug_fprintf(ctx, "\"node_%p\":\"%s_%d\"", head, head_socket.c_str(), head_index);
	
			debug_fprintf(ctx, "[");
			/* Note: without label an id seem necessary to avoid bugs in graphviz/dot */
			debug_fprintf(ctx, "id=\"A%s:%s_%dB%s:%s_%d\"",
			              head->name.c_str(), head_socket.c_str(), head_index,
			              tail->name.c_str(), tail_socket.c_str(), tail_index);
			
			debug_fprintf(ctx, ",penwidth=\"%f\"", penwidth);
			debug_fprintf(ctx, "];" NL);
			debug_fprintf(ctx, NL);
		}
		else if (input.graph_input) {
			const NodeGraphInput *tail = input.graph_input;
			const NodeInstance *head = node;
			const string &head_socket = it->first;
			int head_index = debug_input_index(head, head_socket);
			debug_fprintf(ctx, "// %s -> %s:%s\n",
			              tail->name.c_str(),
			              head->name.c_str(), head_socket.c_str());
			debug_fprintf(ctx, "\"input_%p\"", tail);
			debug_fprintf(ctx, " -> ");
			debug_fprintf(ctx, "\"node_%p\":\"%s_%d\"", head, head_socket.c_str(), head_index);
	
			debug_fprintf(ctx, "[");
			/* Note: without label an id seem necessary to avoid bugs in graphviz/dot */
			debug_fprintf(ctx, "id=\"A%s:%s_%dB%s\"",
			              head->name.c_str(), head_socket.c_str(), head_index,
			              tail->name.c_str());
			
			debug_fprintf(ctx, ",penwidth=\"%f\"", penwidth);
			debug_fprintf(ctx, "];" NL);
			debug_fprintf(ctx, NL);
		}
	}
}

void NodeGraph::dump_graphviz(FILE *f, const string &label)
{
	DebugContext ctx;
	ctx.file = f;
	
	debug_fprintf(ctx, "digraph depgraph {" NL);
	debug_fprintf(ctx, "rankdir=LR;" NL);
	debug_fprintf(ctx, "graph [");
	debug_fprintf(ctx, "labelloc=\"t\"");
	debug_fprintf(ctx, ",fontsize=%f", debug_graphviz_graph_label_size);
	debug_fprintf(ctx, ",fontname=\"%s\"", debug_graphviz_fontname);
	debug_fprintf(ctx, ",label=\"%s\"", label.c_str());
//	debug_fprintf(ctx, ",splines=ortho");
	debug_fprintf(ctx, "];" NL);

	for (NodeInstanceMap::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
		const NodeInstance *node = &it->second;
		debug_graphviz_node(ctx, node);
	}
	
	for (InputList::const_iterator it = inputs.begin(); it != inputs.end(); ++it) {
		const NodeGraphInput *input = &(*it);
		debug_graphviz_input_output(ctx, input, NULL);
	}
	for (OutputList::const_iterator it = outputs.begin(); it != outputs.end(); ++it) {
		const NodeGraphOutput *output = &(*it);
		debug_graphviz_input_output(ctx, NULL, output);
	}
	
	for (NodeInstanceMap::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
		const NodeInstance *node = &it->second;
		debug_graphviz_node_links(ctx, node);
	}
	for (OutputList::const_iterator it = outputs.begin(); it != outputs.end(); ++it) {
		const NodeGraphOutput *output = &(*it);
		debug_graphviz_output_links(ctx, output);
	}

//	deg_debug_graphviz_legend(ctx);

	debug_fprintf(ctx, "}" NL);
}

#undef NL

/* ------------------------------------------------------------------------- */

OpCode get_opcode_from_node_type(const string &node)
{
	#define NODETYPE(name) \
	if (node == STRINGIFY(name)) \
		return OP_##name
	
	NODETYPE(FLOAT_TO_INT);
	NODETYPE(INT_TO_FLOAT);
	NODETYPE(SET_FLOAT3);
	NODETYPE(GET_ELEM_FLOAT3);
	NODETYPE(SET_FLOAT4);
	NODETYPE(GET_ELEM_FLOAT4);
	NODETYPE(MATRIX44_TO_LOCROTSCALE);
	NODETYPE(LOCROTSCALE_TO_MATRIX44);
	
	NODETYPE(POINT_POSITION);
	NODETYPE(POINT_VELOCITY);
	
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
	NODETYPE(MUL_FLOAT3);
	NODETYPE(DIV_FLOAT3);
	NODETYPE(MUL_FLOAT3_FLOAT);
	NODETYPE(DIV_FLOAT3_FLOAT);
	NODETYPE(AVERAGE_FLOAT3);
	NODETYPE(DOT_FLOAT3);
	NODETYPE(CROSS_FLOAT3);
	NODETYPE(NORMALIZE_FLOAT3);
	
	NODETYPE(MIX_RGB);
	
	NODETYPE(TEX_COORD);
	NODETYPE(TEX_PROC_VORONOI);
	NODETYPE(TEX_PROC_CLOUDS);
	
	NODETYPE(EFFECTOR_OBJECT);
	NODETYPE(EFFECTOR_TRANSFORM);
	NODETYPE(EFFECTOR_CLOSEST_POINT);
	
	NODETYPE(MESH_LOAD);
	NODETYPE(MESH_ARRAY);
	
	#undef NODETYPE
	
	return OP_NOOP;
}

void register_opcode_node_types()
{
	NodeType *nt;
	
	nt = NodeGraph::add_node_type("FLOAT_TO_INT");
	nt->add_input("value", BVM_FLOAT, 0.0f);
	nt->add_output("value", BVM_INT, 0);
	
	nt = NodeGraph::add_node_type("INT_TO_FLOAT");
	nt->add_input("value", BVM_INT, 0);
	nt->add_output("value", BVM_FLOAT, 0.0f);
	
	nt = NodeGraph::add_node_type("PASS_FLOAT");
	nt->add_input("value", BVM_FLOAT, 0.0f);
	nt->add_output("value", BVM_FLOAT, 0.0f);
	nt->is_pass = true;
	
	nt = NodeGraph::add_node_type("PASS_FLOAT3");
	nt->add_input("value", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->is_pass = true;
	
	nt = NodeGraph::add_node_type("PASS_FLOAT4");
	nt->add_input("value", BVM_FLOAT4, float4(0.0f, 0.0f, 0.0f, 0.0f));
	nt->add_output("value", BVM_FLOAT4, float4(0.0f, 0.0f, 0.0f, 0.0f));
	nt->is_pass = true;
	
	nt = NodeGraph::add_node_type("PASS_INT");
	nt->add_input("value", BVM_INT, 0);
	nt->add_output("value", BVM_INT, 0);
	nt->is_pass = true;
	
	nt = NodeGraph::add_node_type("PASS_MATRIX44");
	nt->add_input("value", BVM_MATRIX44, matrix44::identity());
	nt->add_output("value", BVM_MATRIX44, matrix44::identity());
	nt->is_pass = true;
	
	nt = NodeGraph::add_node_type("PASS_POINTER");
	nt->add_input("value", BVM_POINTER, PointerRNA_NULL);
	nt->add_output("value", BVM_POINTER, PointerRNA_NULL);
	nt->is_pass = true;
	
	nt = NodeGraph::add_node_type("PASS_MESH");
	nt->add_input("value", BVM_MESH, __empty_mesh__);
	nt->add_output("value", BVM_MESH, __empty_mesh__);
	nt->is_pass = true;
	
	nt = NodeGraph::add_node_type("GET_ELEM_FLOAT3");
	nt->add_input("index", BVM_INT, 0, true);
	nt->add_input("value", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", BVM_FLOAT, 0.0f);
	
	nt = NodeGraph::add_node_type("SET_FLOAT3");
	nt->add_input("value_x", BVM_FLOAT, 0.0f);
	nt->add_input("value_y", BVM_FLOAT, 0.0f);
	nt->add_input("value_z", BVM_FLOAT, 0.0f);
	nt->add_output("value", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	
	nt = NodeGraph::add_node_type("GET_ELEM_FLOAT4");
	nt->add_input("index", BVM_INT, 0, true);
	nt->add_input("value", BVM_FLOAT4, float4(0.0f, 0.0f, 0.0f, 0.0f));
	nt->add_output("value", BVM_FLOAT, 0.0f);
	
	nt = NodeGraph::add_node_type("SET_FLOAT4");
	nt->add_input("value_x", BVM_FLOAT, 0.0f);
	nt->add_input("value_y", BVM_FLOAT, 0.0f);
	nt->add_input("value_z", BVM_FLOAT, 0.0f);
	nt->add_input("value_w", BVM_FLOAT, 0.0f);
	nt->add_output("value", BVM_FLOAT4, float4(0.0f, 0.0f, 0.0f, 0.0f));
	
	nt = NodeGraph::add_node_type("MATRIX44_TO_LOCROTSCALE");
	nt->add_input("matrix", BVM_MATRIX44, matrix44::identity());
	nt->add_output("loc", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("rot", BVM_FLOAT4, float4(1.0f, 0.0f, 0.0f, 0.0f));
	nt->add_output("scale", BVM_FLOAT3, float3(1.0f, 1.0f, 1.0f));
	
	nt = NodeGraph::add_node_type("LOCROTSCALE_TO_MATRIX44");
	nt->add_input("loc", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("rot", BVM_FLOAT4, float4(1.0f, 0.0f, 0.0f, 0.0f));
	nt->add_input("scale", BVM_FLOAT3, float3(1.0f, 1.0f, 1.0f));
	nt->add_output("matrix", BVM_MATRIX44, matrix44::identity());
	
	nt = NodeGraph::add_node_type("POINT_POSITION");
	nt->add_output("value", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	
	nt = NodeGraph::add_node_type("POINT_VELOCITY");
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
	
	nt = NodeGraph::add_node_type("MUL_FLOAT3");
	nt->add_input("value_a", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	
	nt = NodeGraph::add_node_type("DIV_FLOAT3");
	nt->add_input("value_a", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("value", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	
	nt = NodeGraph::add_node_type("MUL_FLOAT3_FLOAT");
	nt->add_input("value_a", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", BVM_FLOAT, 0.0f);
	nt->add_output("value", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	
	nt = NodeGraph::add_node_type("DIV_FLOAT3_FLOAT");
	nt->add_input("value_a", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("value_b", BVM_FLOAT, 0.0f);
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
	
	nt = NodeGraph::add_node_type("MIX_RGB");
	nt->add_input("mode", BVM_INT, 0, true);
	nt->add_input("factor", BVM_FLOAT, 0.0f);
	nt->add_input("color1", BVM_FLOAT4, float4(0.0f, 0.0f, 0.0f, 1.0f));
	nt->add_input("color2", BVM_FLOAT4, float4(0.0f, 0.0f, 0.0f, 1.0f));
	nt->add_output("color", BVM_FLOAT4, float4(0.0f, 0.0f, 0.0f, 1.0f));
	
	nt = NodeGraph::add_node_type("TEX_COORD");
	nt->add_output("value", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	
	nt = NodeGraph::add_node_type("TEX_PROC_VORONOI");
	nt->add_input("distance_metric", BVM_INT, 0, true);
	nt->add_input("color_type", BVM_INT, 0, true);
	nt->add_input("minkowski_exponent", BVM_FLOAT, 2.5f);
	nt->add_input("scale", BVM_FLOAT, 1.0f);
	nt->add_input("noise_size", BVM_FLOAT, 1.0f);
	nt->add_input("nabla", BVM_FLOAT, 0.05f);
	nt->add_input("w1", BVM_FLOAT, 1.0f);
	nt->add_input("w2", BVM_FLOAT, 0.0f);
	nt->add_input("w3", BVM_FLOAT, 0.0f);
	nt->add_input("w4", BVM_FLOAT, 0.0f);
	nt->add_input("position", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("intensity", BVM_FLOAT, 0.0f);
	nt->add_output("color", BVM_FLOAT4, float4(0.0f, 0.0f, 0.0f, 1.0f));
	nt->add_output("normal", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	
	nt = NodeGraph::add_node_type("TEX_PROC_CLOUDS");
	nt->add_input("position", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_input("nabla", BVM_FLOAT, 0.05f);
	nt->add_input("size", BVM_FLOAT, 1.0f);
	nt->add_input("depth", BVM_INT, 2, true);
	nt->add_input("noise_basis", BVM_INT, 0, true);
	nt->add_input("noise_hard", BVM_INT, 0, true);
	nt->add_output("intensity", BVM_FLOAT, 0.0f);
	nt->add_output("color", BVM_FLOAT4, float4(0.0f, 0.0f, 0.0f, 1.0f));
	nt->add_output("normal", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	
	nt = NodeGraph::add_node_type("EFFECTOR_OBJECT");
	nt->add_output("object", BVM_POINTER, PointerRNA_NULL);
	
	nt = NodeGraph::add_node_type("EFFECTOR_TRANSFORM");
	nt->add_input("object", BVM_INT, 0, true);
	nt->add_output("transform", BVM_MATRIX44, matrix44::identity());
	
	nt = NodeGraph::add_node_type("EFFECTOR_CLOSEST_POINT");
	nt->add_input("object", BVM_POINTER, PointerRNA_NULL);
	nt->add_input("vector", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("position", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("normal", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	nt->add_output("tangent", BVM_FLOAT3, float3(0.0f, 0.0f, 0.0f));
	
	nt = NodeGraph::add_node_type("MESH_LOAD");
	nt->add_output("mesh", BVM_MESH, __empty_mesh__);
	
	nt = NodeGraph::add_node_type("MESH_ARRAY");
	nt->add_input("mesh_in", BVM_MESH, __empty_mesh__);
	nt->add_input("count", BVM_INT, 1);
	nt->add_input("transform", BVM_MATRIX44, matrix44::identity());
	nt->add_output("mesh_out", BVM_MESH, __empty_mesh__);
}

} /* namespace bvm */

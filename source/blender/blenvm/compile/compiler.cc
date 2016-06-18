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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenvm/compile/compiler.cc
 *  \ingroup bvm
 */

#include <cstdio>
#include <set>
#include <sstream>

#include "compiler.h"
#include "node_graph.h"

#include "util_opcode.h"

namespace blenvm {

const ValueHandle VALUE_UNDEFINED = 0;

/* ------------------------------------------------------------------------- */

Scope::Scope(Scope *parent) :
    parent(parent)
{
}

bool Scope::has_node(const NodeInstance *node) const
{
	/* XXX this is not ideal, but we can expect all outputs
	 * to be mapped once a node is added.
	 */
	ConstOutputKey key(node, node->type->find_output(0));
	return has_value(key);
}

bool Scope::has_value(const ConstOutputKey &key) const
{
	const Scope *scope = this;
	while (scope) {
		SocketValueMap::const_iterator it = scope->values.find(key);
		if (it != scope->values.end()) {
			return true;
		}
		
		scope = scope->parent;
	}
	return false;
}

ValueHandle Scope::find_value(const ConstOutputKey &key) const
{
	const Scope *scope = this;
	while (scope) {
		SocketValueMap::const_iterator it = scope->values.find(key);
		if (it != scope->values.end()) {
			return it->second;
		}
		
		scope = scope->parent;
	}
	BLI_assert(false && "Value not defined in any scope!");
	return ValueHandle(0);
}

void Scope::set_value(const ConstOutputKey &key, ValueHandle value)
{
	bool ok = values.insert(SocketValueMap::value_type(key, value)).second;
	BLI_assert(ok && "Could not insert socket value!");
	UNUSED_VARS(ok);
}

/* ------------------------------------------------------------------------- */

Compiler::Compiler(CodeGenerator *codegen) :
    m_codegen(codegen)
{
}

Compiler::~Compiler()
{
}

void Compiler::compile_node_graph(const string &name, const NodeGraph &graph)
{
	m_codegen->node_graph_begin(name, &graph, true);
	
	compile_node_statements(graph);
	
	m_codegen->node_graph_end();
	
	m_codegen->finalize_function();
}

void Compiler::debug_node_graph(const string &name, const NodeGraph &graph, FILE *file)
{
	m_codegen->node_graph_begin(name, &graph, true);
	
	compile_node_statements(graph);
	
	m_codegen->node_graph_end();
	
	m_codegen->debug_function(file);
}

/* Compile nodes as a simple expression.
 * Every node can be treated as a single statement. Each node is translated
 * into a function call, with regular value arguments. The resulting value is
 * assigned to a variable and can be used for subsequent node function calls.
 */
void Compiler::compile_node_statements(const NodeGraph &graph)
{
	/* cache function arguments */
	int num_inputs = graph.inputs.size();
	int num_outputs = graph.outputs.size();
	
	for (int i = 0; i < num_inputs; ++i) {
		const NodeGraph::Input &input = graph.inputs[i];
		const TypeSpec *typespec = input.typedesc.get_typespec();
		
		if (input.key) {
			ValueHandle handle = m_codegen->map_argument(i, typespec);
			m_argument_values.insert(ArgumentValueMap::value_type(input.key, handle));
		}
	}
	
	Scope scope_main(NULL);
	
	for (int i = 0; i < num_outputs; ++i) {
		const NodeGraph::Output &output = graph.outputs[i];
		const TypeSpec *typespec = output.typedesc.get_typespec();
		
		expand_node(output.key.node, scope_main);
		ValueHandle value = scope_main.find_value(output.key);
		
		m_codegen->store_return_value(i, typespec, value);
	}
}


void Compiler::expand_node(const NodeInstance *node, Scope &scope)
{
	if (scope.has_node(node))
		return;
	
	switch (node->type->kind()) {
		case NODE_TYPE_FUNCTION:
		case NODE_TYPE_KERNEL:
			expand_expression_node(node, scope);
			break;
		case NODE_TYPE_PASS:
			expand_pass_node(node, scope);
			break;
		case NODE_TYPE_ARG:
			expand_argument_node(node, scope);
			break;
	}
}

void Compiler::expand_pass_node(const NodeInstance *node, Scope &scope)
{
	BLI_assert(node->num_inputs() == 1);
	BLI_assert(node->num_outputs() == 1);
	
	ConstInputKey input = node->input(0);
	BLI_assert(input.value_type() == INPUT_EXPRESSION);
	
	expand_node(input.link().node, scope);
}

void Compiler::expand_argument_node(const NodeInstance *node, Scope &scope)
{
	BLI_assert(node->num_outputs() == 1);
	
	ConstOutputKey output = node->output(0);
	scope.set_value(output, m_argument_values.at(output));
}

void Compiler::expand_expression_node(const NodeInstance *node, Scope &scope)
{
	/* function call arguments */
	std::vector<ValueHandle> input_args, output_args;
	
	for (int i = 0; i < node->num_outputs(); ++i) {
		ConstOutputKey output = node->output(i);
		const TypeSpec *typespec = output.socket->typedesc.get_typespec();
		
		ValueHandle value = m_codegen->alloc_node_value(typespec);
		output_args.push_back(value);
		
		scope.set_value(output, value);
	}
	
	/* set input arguments */
	for (int i = 0; i < node->num_inputs(); ++i) {
		ConstInputKey input = node->input(i);
		const TypeSpec *typespec = input.socket->typedesc.get_typespec();
		
		switch (input.value_type()) {
			case INPUT_CONSTANT: {
				ValueHandle value = m_codegen->create_constant(typespec, input.value());
				input_args.push_back(value);
				break;
			}
			case INPUT_EXPRESSION: {
				expand_node(input.link().node, scope);
				
				ValueHandle link_value = scope.find_value(input.link());
				input_args.push_back(link_value);
				break;
			}
			case INPUT_VARIABLE: {
				/* TODO */
				BLI_assert(false && "Variable inputs not supported yet!");
				break;
			}
		}
	}
	
	m_codegen->eval_node(node->type, input_args, output_args);
}

} /* namespace blenvm */

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

/** \file glsl_codegen.cc
 *  \ingroup glsl
 */

#include <cstdio>
#include <set>

extern "C" {
#include "BLI_utildefines.h"
}

#include "node_graph.h"

#include "glsl_codegen.h"

namespace blenvm {

GLSLValue::GLSLValue(const string &name) :
    m_name(name)
{
}

GLSLValue::~GLSLValue()
{
}

/* ------------------------------------------------------------------------- */

GLSLCodeGenerator::GLSLCodeGenerator()
{
}

GLSLCodeGenerator::~GLSLCodeGenerator()
{
}

ValueHandle GLSLCodeGenerator::get_handle(const GLSLValue *value)
{
	return (ValueHandle)value;
}

GLSLValue *GLSLCodeGenerator::create_value(const TypeSpec *typespec, const string &name, bool make_unique)
{
	string varname = name;
	if (make_unique) {
		stringstream ss;
		ss << varname << "_" << (m_values.size() + 1);
	}
	
	m_values.push_back(GLSLValue(varname));
	GLSLValue *value = &m_values.back();
	
	return value;
}

void GLSLCodeGenerator::finalize_function()
{
}

void GLSLCodeGenerator::debug_function(FILE *file)
{
	string s = m_code.str();
	fwrite(s.c_str(), sizeof(char), s.size(), file);
}

void GLSLCodeGenerator::node_graph_begin(const string &UNUSED(name), const NodeGraph *graph, bool UNUSED(use_globals))
{
	/* storage for function arguments */
	size_t num_inputs = graph->inputs.size();
	for (int i = 0; i < num_inputs; ++i) {
		const NodeGraph::Input *input = graph->get_input(i);
		const TypeSpec *typespec = input->typedesc.get_typespec();
		/* Note: argument names are unique! */
		GLSLValue *value = create_value(typespec, input->name, false);
		m_input_args.push_back(value);
	}
}

void GLSLCodeGenerator::node_graph_end()
{
}

void GLSLCodeGenerator::store_return_value(size_t output_index, const TypeSpec *typespec, ValueHandle value)
{
}

ValueHandle GLSLCodeGenerator::map_argument(size_t input_index, const TypeSpec *UNUSED(typespec))
{
	GLSLValue *arg = m_input_args[input_index];
	return get_handle(arg);
}

ValueHandle GLSLCodeGenerator::alloc_node_value(const TypeSpec *typespec, const string &name)
{
	GLSLValue *value = create_value(typespec, name, true);
	return get_handle(value);
}

ValueHandle GLSLCodeGenerator::create_constant(const TypeSpec *typespec, const NodeConstant *node_value)
{
	GLSLValue *value = create_value(typespec, "constval", true);
//	value->set_constant_value(node_value);
	return get_handle(value);
}

void GLSLCodeGenerator::eval_node(const NodeType *nodetype,
                                  ArrayRef<ValueHandle> input_args,
                                  ArrayRef<ValueHandle> output_args)
{
}


void GLSLCodeGenerator::append(const string &s)
{
	m_code << s;
}

void GLSLCodeGenerator::newline()
{
	m_code << "\n";
}

} /* namespace blenvm */

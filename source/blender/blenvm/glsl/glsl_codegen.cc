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
#include "glsl_types.h"

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

GLSLValue *GLSLCodeGenerator::get_value(ValueHandle handle)
{
	return (GLSLValue *)handle;
}

GLSLValue *GLSLCodeGenerator::create_value(const TypeSpec *UNUSED(typespec), const string &name, bool make_unique)
{
	stringstream varname;
	varname << name;
	if (make_unique)
		varname << "_" << (m_values.size() + 1);
	
	m_values.push_back(GLSLValue(varname.str()));
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

void GLSLCodeGenerator::node_graph_begin(const string &name, const NodeGraph *graph, bool UNUSED(use_globals))
{
	m_code << "void " << name;
	
	m_code << "(";
	/* storage for function arguments */
	size_t num_inputs = graph->inputs.size();
	for (int i = 0; i < num_inputs; ++i) {
		const NodeGraph::Input *input = graph->get_input(i);
		const TypeSpec *typespec = input->typedesc.get_typespec();
		
		/* Note: argument names are unique! */
		GLSLValue *value = create_value(typespec, input->name, false);
		m_input_args.push_back(value);
		
		if (i > 0)
			m_code << ", ";
		m_code << "in " << bvm_glsl_get_type(typespec, true)<< " " << value->name();
	}
	
	size_t num_outputs = graph->outputs.size();
	if (num_inputs > 0)
		m_code << ", ";
	for (int i = 0; i < num_outputs; ++i) {
		const NodeGraph::Output *output = graph->get_output(i);
		const TypeSpec *typespec = output->typedesc.get_typespec();
		
		/* Note: argument names are unique! */
		GLSLValue *value = create_value(typespec, output->name, false);
		m_output_args.push_back(value);
		
		if (i > 0)
			m_code << ", ";
		m_code << "out " << bvm_glsl_get_type(typespec, true)<< " " << value->name();
	}
	m_code << ")\n";
	
	m_code << "{\n";
}

void GLSLCodeGenerator::node_graph_end()
{
	m_code << "}\n";
}

void GLSLCodeGenerator::store_return_value(size_t output_index, const TypeSpec *typespec, ValueHandle handle)
{
	GLSLValue *arg = m_output_args[output_index];
	GLSLValue *val = get_value(handle);
	
//	if (bvm_glsl_type_has_dual_value(typespec)) {
//		Value *value_ptr = builder.CreateStructGEP(arg, 0, sanitize_name(arg->getName().str() + "_V"));
//		Value *dx_ptr = builder.CreateStructGEP(arg, 1, sanitize_name(arg->getName().str() + "_DX"));
//		Value *dy_ptr = builder.CreateStructGEP(arg, 2, sanitize_name(arg->getName().str() + "_DY"));
		
//		bvm_llvm_copy_value(context(), m_block, value_ptr, dval.value(), typespec);
//		bvm_llvm_copy_value(context(), m_block, dx_ptr, dval.dx(), typespec);
//		bvm_llvm_copy_value(context(), m_block, dy_ptr, dval.dy(), typespec);
//	}
//	else {
		bvm_glsl_copy_value(m_code, arg, val, typespec);
//	}
}

ValueHandle GLSLCodeGenerator::map_argument(size_t input_index, const TypeSpec *UNUSED(typespec))
{
	GLSLValue *arg = m_input_args[input_index];
	return get_handle(arg);
}

ValueHandle GLSLCodeGenerator::alloc_node_value(const TypeSpec *typespec, const string &name)
{
	GLSLValue *value = create_value(typespec, name, true);
	m_code << bvm_glsl_get_type(typespec, true)<< " " << value->name() << ";\n";
	return get_handle(value);
}

ValueHandle GLSLCodeGenerator::create_constant(const TypeSpec *typespec, const NodeConstant *node_value)
{
	GLSLValue *value = create_value(typespec, "constval", true);
	m_code << "const " << bvm_glsl_get_type(typespec, true) << " " << value->name()
	       << " = const " << bvm_glsl_create_constant(node_value) << ";\n";
	return get_handle(value);
}

void GLSLCodeGenerator::eval_node(const NodeType *nodetype,
                                  ArrayRef<ValueHandle> input_args,
                                  ArrayRef<ValueHandle> output_args)
{
}

} /* namespace blenvm */

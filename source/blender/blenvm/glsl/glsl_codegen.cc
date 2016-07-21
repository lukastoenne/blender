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

/* Remove non-alphanumeric chars
 * Note: GLSL does not allow double underscores __
 *       so just remove them to avoid issues.
 */
static string sanitize_name(const string &name)
{
	string s = name;
	
	string::const_iterator it = s.begin();
	string::iterator nit = s.begin();
	for (; it != s.end(); ++it) {
		const char &c = *it;
		if (isalnum(c)) {
			*nit = c;
			++nit;
		}
	}
	s = s.substr(0, nit - s.begin());
	return s;
}


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

ValueHandle GLSLCodeGenerator::get_handle(const GLSLCodeGenerator::DualValue &value)
{
	return (ValueHandle)value.value();
}

ValueHandle GLSLCodeGenerator::register_value(const DualValue &value)
{
	ValueHandle handle = get_handle(value);
	bool ok = m_valuemap.insert(HandleValueMap::value_type(handle, value)).second;
	BLI_assert(ok && "Could not register value");
	UNUSED_VARS(ok);
	return handle;
}

const GLSLCodeGenerator::DualValue &GLSLCodeGenerator::get_value(ValueHandle handle) const
{
	return m_valuemap.at(handle);
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
		string typestring = bvm_glsl_get_type(typespec, true);
		string basename = sanitize_name(input->name);
		
		DualValue dval;
		if (bvm_glsl_type_has_dual_value(typespec)) {
			/* Note: argument names are already unique */
			dval = DualValue(create_value(typespec, basename + "_V", false),
			                 create_value(typespec, basename + "_DX", false),
			                 create_value(typespec, basename + "_DY", false));
			
			if (i > 0)
				m_code << ", ";
			m_code << "in " << typestring << " " << dval.value()->name()
			       << ", in " << typestring << " " << dval.dx()->name()
			       << ", in " << typestring << " " << dval.dy()->name();
		}
		else {
			/* Note: argument names are already unique */
			dval = DualValue(create_value(typespec, basename, false), NULL, NULL);
			
			if (i > 0)
				m_code << ", ";
			m_code << "in " << typestring << " " << dval.value()->name();
		}
		
		register_value(dval);
		m_input_args.push_back(dval);
	}
	
	size_t num_outputs = graph->outputs.size();
	for (int i = 0; i < num_outputs; ++i) {
		const NodeGraph::Output *output = graph->get_output(i);
		const TypeSpec *typespec = output->typedesc.get_typespec();
		string typestring = bvm_glsl_get_type(typespec, true);
		string basename = sanitize_name(output->name);
		
		DualValue dval;
		if (bvm_glsl_type_has_dual_value(typespec)) {
			/* Note: argument names are already unique */
			dval = DualValue(create_value(typespec, basename + "_V", false),
			                 create_value(typespec, basename + "_DX", false),
			                 create_value(typespec, basename + "_DY", false));
			
			if (i + num_inputs > 0)
				m_code << ", ";
			m_code << "out " << typestring << " " << dval.value()->name()
			       << ", out " << typestring << " " << dval.dx()->name()
			       << ", out " << typestring << " " << dval.dy()->name();
		}
		else {
			/* Note: argument names are already unique */
			dval = DualValue(create_value(typespec, basename, false), NULL, NULL);
			
			if (i > 0)
				m_code << ", ";
			m_code << "out " << typestring << " " << dval.value()->name();
		}
		
		register_value(dval);
		m_output_args.push_back(dval);
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
	DualValue arg = m_output_args[output_index];
	DualValue val = get_value(handle);
	
	bvm_glsl_copy_value(m_code, arg.value(), val.value(), typespec);
	if (bvm_glsl_type_has_dual_value(typespec)) {
		bvm_glsl_copy_value(m_code, arg.dx(), val.dx(), typespec);
		bvm_glsl_copy_value(m_code, arg.dy(), val.dy(), typespec);
	}
}

ValueHandle GLSLCodeGenerator::map_argument(size_t input_index, const TypeSpec *UNUSED(typespec))
{
	return get_handle(m_input_args[input_index]);
}

ValueHandle GLSLCodeGenerator::alloc_node_value(const TypeSpec *typespec, const string &name)
{
	string typestring = bvm_glsl_get_type(typespec, false);
	string basename = sanitize_name(name);
	
	DualValue dval;
	if (bvm_glsl_type_has_dual_value(typespec)) {
		dval = DualValue(create_value(typespec, basename + "_V", true),
		                 create_value(typespec, basename + "_DX", true),
		                 create_value(typespec, basename + "_DY", true));
		
		m_code << typestring << " " << dval.value()->name() << ";\n";
		m_code << typestring << " " << dval.dx()->name() << ";\n";
		m_code << typestring << " " << dval.dy()->name() << ";\n";
	}
	else {
		dval = DualValue(create_value(typespec, basename + "_V", true), NULL, NULL);
		
		m_code << typestring << " " << dval.value()->name() << ";\n";
	}
	
	return register_value(dval);
}

ValueHandle GLSLCodeGenerator::create_constant(const TypeSpec *typespec, const NodeConstant *node_value)
{
	string typestring = bvm_glsl_get_type(typespec, false);
	string basename = "constval";
	
	DualValue dval;
	if (bvm_glsl_type_has_dual_value(typespec)) {
		dval = DualValue(create_value(typespec, basename + "_V", true),
		                 create_value(typespec, basename + "_DX", true),
		                 create_value(typespec, basename + "_DY", true));
		
		m_code << "const " << typestring << " " << dval.value()->name()
		       << " = const " << bvm_glsl_create_constant(node_value) << ";\n";
		m_code << "const " << typestring << " " << dval.dx()->name()
		       << " = const " << bvm_glsl_create_zero(typespec) << ";\n";
		m_code << "const " << typestring << " " << dval.dy()->name()
		       << " = const " << bvm_glsl_create_zero(typespec) << ";\n";
	}
	else {
		dval = DualValue(create_value(typespec, basename + "_V", true), NULL, NULL);
		
		m_code << "const " << typestring << " " << dval.value()->name()
		       << " = const " << bvm_glsl_create_constant(node_value) << ";\n";
	}
	
	return register_value(dval);
}

void GLSLCodeGenerator::eval_node(const NodeType *nodetype,
                                  ArrayRef<ValueHandle> input_args,
                                  ArrayRef<ValueHandle> output_args)
{
	stringstream args_value;
	stringstream args_dx;
	stringstream args_dy;
	const char *sep;
	
//	if (nodetype->use_globals()) {
//		evalargs.push_back(m_globals_ptr);
//	}
	
	for (int i = 0; i < nodetype->num_inputs(); ++i) {
		const NodeInput *input = nodetype->find_input(i);
		const TypeSpec *typespec = input->typedesc.get_typespec();
		bool is_constant = (input->value_type == INPUT_CONSTANT);
		const DualValue &dval = get_value(input_args[i]);
		
		sep = (i > 0) ? ", " : "";
		args_value << sep << dval.value()->name();
		args_dx << sep << dval.value()->name();
		args_dy << sep << dval.value()->name();
		if (!is_constant && bvm_glsl_type_has_dual_value(typespec)) {
			args_dx << ", " << dval.dx()->name();
			args_dy << ", " << dval.dy()->name();
		}
	}
	
	for (int i = 0; i < nodetype->num_outputs(); ++i) {
		const NodeOutput *output = nodetype->find_output(i);
		const TypeSpec *typespec = output->typedesc.get_typespec();
		const DualValue &dval = get_value(output_args[i]);
		
		sep = (i + nodetype->num_inputs() > 0) ? ", " : "";
		args_value << sep << dval.value()->name();
		if (bvm_glsl_type_has_dual_value(typespec)) {
			args_dx << sep << dval.dx()->name();
			args_dy << sep << dval.dy()->name();
		}
	}
	
	m_code << "V_" << nodetype->name() << "(" << args_value.str() << ");\n";
	m_code << "D_" << nodetype->name() << "(" << args_dx.str() << ");\n";
	m_code << "D_" << nodetype->name() << "(" << args_dy.str() << ");\n";
}

} /* namespace blenvm */

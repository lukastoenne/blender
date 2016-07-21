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

#ifndef __GLSL_CODEGEN_H__
#define __GLSL_CODEGEN_H__

/** \file glsl_codegen.h
 *  \ingroup glsl
 */

#include <cstdarg>
#include <list>
#include <set>
#include <vector>

#include "MEM_guardedalloc.h"

#include "compiler.h"
#include "node_graph.h"

#include "glsl_value.h"

#include "util_opcode.h"
#include "util_string.h"

namespace blenvm {

struct NodeGraph;
struct NodeInstance;
struct TypeDesc;

struct GLSLCodeGenerator : public CodeGenerator {
	typedef std::list<GLSLValue> Values;
	typedef Dual2<GLSLValue*> DualValue;
	typedef std::map<ValueHandle, DualValue> HandleValueMap;
	typedef std::vector<DualValue> Arguments;
	
	GLSLCodeGenerator();
	~GLSLCodeGenerator();
	
	const stringstream &code() const { return m_code; }
	
	static ValueHandle get_handle(const DualValue &value);
	ValueHandle register_value(const DualValue &value);
	const DualValue &get_value(ValueHandle handle) const;
	
	void finalize_function();
	void debug_function(FILE *file);
	
	void node_graph_begin(const string &name, const NodeGraph *graph, bool use_globals);
	void node_graph_end();
	
	void store_return_value(size_t output_index, const TypeSpec *typespec, ValueHandle value);
	ValueHandle map_argument(size_t input_index, const TypeSpec *typespec);
	
	ValueHandle alloc_node_value(const TypeSpec *typespec, const string &name);
	ValueHandle create_constant(const TypeSpec *typespec, const NodeConstant *node_value);
	
	void eval_node(const NodeType *nodetype,
	               ArrayRef<ValueHandle> input_args,
	               ArrayRef<ValueHandle> output_args);
	
protected:
	GLSLValue *create_value(const TypeSpec *typespec, const string &name, bool make_unique);
	
private:
	Values m_values;
	HandleValueMap m_valuemap;
	
	Arguments m_input_args;
	Arguments m_output_args;
	
	stringstream m_code;
};

} /* namespace blenvm */

#endif /* __GLSL_CODEGEN_H__ */

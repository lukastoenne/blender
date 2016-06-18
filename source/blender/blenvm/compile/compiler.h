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

#ifndef __COMPILER_H__
#define __COMPILER_H__

/** \file blender/blenvm/compile/compiler.h
 *  \ingroup bvm
 */

#include <set>
#include <vector>

#include "MEM_guardedalloc.h"

#include "node_graph.h"

#include "util_array.h"
#include "util_opcode.h"
#include "util_string.h"
#include "util_math.h"

namespace blenvm {

struct NodeGraph;
struct NodeInstance;
struct TypeDesc;

typedef void* ValueHandle;

extern const ValueHandle VALUE_UNDEFINED;

typedef std::map<ConstOutputKey, ValueHandle> SocketValueMap;
struct Scope {
	Scope(Scope *parent);
	
	bool has_node(const NodeInstance *node) const;
	bool has_value(const ConstOutputKey &key) const;
	ValueHandle find_value(const ConstOutputKey &key) const;
	void set_value(const ConstOutputKey &key, ValueHandle value);
	
	Scope *parent;
	SocketValueMap values;
};


struct CodeGenerator {
	virtual void finalize_function() = 0;
	virtual void debug_function(FILE *file) = 0;
	
	virtual void node_graph_begin(const string &name, const NodeGraph *graph, bool use_globals) = 0;
	virtual void node_graph_end() = 0;

	virtual void store_return_value(size_t output_index, const TypeSpec *typespec, ValueHandle value) = 0;
	virtual ValueHandle map_argument(size_t input_index, const TypeSpec *typespec) = 0;
	
	virtual ValueHandle alloc_node_value(const TypeSpec *typespec) = 0;
	virtual ValueHandle create_constant(const TypeSpec *typespec, const NodeConstant *node_value) = 0;

	virtual void eval_node(const NodeType *nodetype,
	                       ArrayRef<ValueHandle> input_args,
	                       ArrayRef<ValueHandle> output_args) = 0;
};


struct Compiler {
	typedef std::map<ConstOutputKey, ValueHandle> ArgumentValueMap;
	
	Compiler(CodeGenerator *codegen);
	~Compiler();
	
	void compile_node_graph(const string &name, const NodeGraph &graph);
	void debug_node_graph(const string &name, const NodeGraph &graph, FILE *file);
	
protected:
	void compile_node_statements(const NodeGraph &graph);
	void expand_node(const NodeInstance *node, Scope &scope);
	void expand_pass_node(const NodeInstance *node, Scope &scope);
	void expand_argument_node(const NodeInstance *node, Scope &scope);
	void expand_expression_node(const NodeInstance *node, Scope &scope);
	
private:
	CodeGenerator *m_codegen;
	ArgumentValueMap m_argument_values;
};

} /* namespace blenvm */

#endif /* __LLVM_COMPILER_H__ */

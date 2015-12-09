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

#ifndef __BVM_CODEGEN_H__
#define __BVM_CODEGEN_H__

/** \file bvm_codegen.h
 *  \ingroup bvm
 */

#include <set>
#include <vector>

#include "MEM_guardedalloc.h"

#include "bvm_function.h"
#include "bvm_nodegraph.h"
#include "bvm_opcode.h"
#include "bvm_util_string.h"

namespace bvm {

struct Function;
struct NodeGraph;
struct NodeInstance;
struct TypeDesc;

typedef std::vector<const NodeInstance *> NodeList;
typedef std::set<const NodeInstance *> NodeSet;
typedef std::map<ConstSocketPair, StackIndex> SocketIndexMap;
typedef std::map<ConstSocketPair, int> SocketUserMap;

struct BVMCompiler {
	struct FunctionInfo {
		FunctionInfo() : entry_point(0), return_index(BVM_STACK_INVALID) {}
		NodeList nodes;
		SocketIndexMap input_index;
		SocketIndexMap output_index;
		int entry_point;
		StackIndex return_index;
	};
	typedef std::map<ConstSocketPair, FunctionInfo> FunctionEntryMap;
	typedef std::vector<int> StackUsers;
	
	BVMCompiler();
	~BVMCompiler();
	
	StackIndex find_stack_index(int size) const;
	StackIndex assign_stack_index(const TypeDesc &typedesc);
	
	void push_opcode(OpCode op) const;
	void push_stack_index(StackIndex arg) const;
	void push_jump_address(int address) const;
	
	void push_float(float f) const;
	void push_float3(float3 f) const;
	void push_float4(float4 f) const;
	void push_int(int i) const;
	void push_matrix44(matrix44 m) const;
	void push_pointer(PointerRNA p) const;
	
	void push_constant(const Value *value) const;
	
	void codegen_value(const Value *value, StackIndex offset) const;
	int codegen_subgraph(const NodeList &nodes,
	                     const SocketUserMap &socket_users,
	                     const SocketIndexMap &input_index,
	                     const SocketIndexMap &output_index) const;
	Function *codegen(const NodeGraph &graph);
	
	void resolve_subgraph_symbols(const NodeList &nodes,
	                              SocketIndexMap &input_index,
	                              SocketIndexMap &output_index);
	void resolve_symbols(const NodeGraph &graph);
	
	Function *compile_function(const NodeGraph &graph);
	
protected:
	void expression_node_append(const NodeInstance *node, NodeList &sorted_nodes, NodeSet &visited);
	void graph_node_append(const NodeInstance *node, NodeList &sorted_nodes, NodeSet &visited);
	void sort_graph_nodes(const NodeGraph &graph);
	
private:
	NodeList main_nodes;
	SocketIndexMap main_input_index;
	SocketIndexMap main_output_index;
	FunctionEntryMap func_entry_map;
	StackUsers stack_users;
	Function *fn;

	MEM_CXX_CLASS_ALLOC_FUNCS("BVM:BVMCompiler")
};

} /* namespace bvm */

#endif /* __BVM_CODEGEN_H__ */

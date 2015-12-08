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
		int entry_point;
		StackIndex return_index;
	};
	typedef std::map<ConstSocketPair, FunctionInfo> FunctionEntryMap;
	typedef std::vector<int> StackUsers;
	
	BVMCompiler();
	~BVMCompiler();
	
	StackIndex find_stack_index(int size) const;
	StackIndex assign_stack_index(const TypeDesc &typedesc);
	
	void push_opcode(OpCode op);
	void push_stack_index(StackIndex arg);
	void push_jump_address(int address);
	
	void push_float(float f);
	void push_float3(float3 f);
	void push_float4(float4 f);
	void push_int(int i);
	void push_matrix44(matrix44 m);
	void push_pointer(PointerRNA p);
	
	StackIndex codegen_value(const Value *value);
	void codegen_constant(const Value *value);
	int codegen_subgraph(const NodeList &nodes,
	                     const SocketUserMap &socket_users,
	                     SocketIndexMap &output_index);
	Function *codegen_function(const NodeGraph &graph);
	
protected:
	void graph_node_append(const NodeInstance *node, NodeList &sorted_nodes, NodeSet &visited);
	void sort_graph_nodes(const NodeGraph &graph, NodeList &sorted_nodes);
	
private:
	FunctionEntryMap func_entry_map;
	StackUsers stack_users;
	Function *fn;

	MEM_CXX_CLASS_ALLOC_FUNCS("BVM:BVMCompiler")
};

} /* namespace bvm */

#endif /* __BVM_CODEGEN_H__ */

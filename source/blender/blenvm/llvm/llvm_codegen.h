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

#ifndef __LLVM_CODEGEN_H__
#define __LLVM_CODEGEN_H__

/** \file blender/blenvm/llvm/llvm_codegen.h
 *  \ingroup llvm
 */

#include <set>
#include <vector>

#include "MEM_guardedalloc.h"

#include "compiler.h"
#include "node_graph.h"

#include "util_opcode.h"
#include "util_string.h"
#include "util_math.h"

namespace llvm {
class LLVMContext;
class Argument;
class BasicBlock;
class CallInst;
class Constant;
class Function;
class FunctionType;
class Module;
class StructType;
class Type;
class Value;
}

namespace blenvm {

struct NodeGraph;
struct NodeInstance;
struct TypeDesc;
struct FunctionLLVM;

struct LLVMCodeGenerator : public CodeGenerator {
	typedef Dual2<llvm::Value*> DualValue;
	typedef std::map<ValueHandle, DualValue> HandleValueMap;
	typedef std::vector<llvm::Argument*> OutputArguments;
	typedef std::vector<llvm::Argument*> InputArguments;
	
	LLVMCodeGenerator(int opt_level);
	~LLVMCodeGenerator();
	
	static void define_nodes_module(llvm::LLVMContext &context);
	
	uint64_t function_address() const { return m_function_address; }
	
	void finalize_function();
	void debug_function(FILE *file);
	
	void node_graph_begin(const string &name, const NodeGraph *graph, bool use_globals);
	void node_graph_end();
	
	void store_return_value(size_t output_index, const TypeSpec *typespec, ValueHandle value);
	ValueHandle map_argument(size_t input_index, const TypeSpec *typespec);
	
	ValueHandle alloc_node_value(const TypeSpec *typespec);
	ValueHandle create_constant(const TypeSpec *typespec, const NodeConstant *node_value);
	
	void eval_node(const NodeType *nodetype,
	               ArrayRef<ValueHandle> input_args,
	               ArrayRef<ValueHandle> output_args);
	
protected:
	llvm::LLVMContext &context() const;
	
	static ValueHandle get_handle(const DualValue &value);
	
	void create_module(const string &name);
	void destroy_module();
	
private:
	static llvm::Module *m_nodes_module;
	
	int m_opt_level;
	uint64_t m_function_address;
	
	llvm::Module *m_module;
	llvm::Function *m_function;
	llvm::Argument *m_globals_ptr;
	InputArguments m_input_args;
	OutputArguments m_output_args;
	llvm::BasicBlock *m_block;
	
	HandleValueMap m_values;
};

} /* namespace blenvm */

#endif /* __LLVM_COMPILER_H__ */

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

#ifndef __LLVM_COMPILER_H__
#define __LLVM_COMPILER_H__

/** \file blender/blenvm/llvm/llvm_compiler.h
 *  \ingroup llvm
 */

#include <set>
#include <vector>

#include "MEM_guardedalloc.h"

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

struct LLVMCompilerBase {
	typedef std::map<ConstOutputKey, llvm::Value*> OutputValueMap;
	
	virtual ~LLVMCompilerBase();
	
	FunctionLLVM *compile_function(const string &name, const NodeGraph &graph, int opt_level);
	void debug_function(const string &name, const NodeGraph &graph, int opt_level, FILE *file);
	
protected:
	LLVMCompilerBase();
	
	llvm::LLVMContext &context() const;
	llvm::Module *module() const { return m_module; }
	void create_module(const string &name);
	void destroy_module();
	
	void optimize_function(llvm::Function *func, int opt_level);
	
	void codegen_node(llvm::BasicBlock *block,
	                  const NodeInstance *node);
	
	llvm::BasicBlock *codegen_function_body_expression(const NodeGraph &graph, llvm::Function *func);
	llvm::Function *codegen_node_function(const string &name, const NodeGraph &graph);
	
	void map_argument(llvm::BasicBlock *block, const OutputKey &output, llvm::Argument *arg);
	void store_return_value(llvm::BasicBlock *block, const OutputKey &output, llvm::Value *arg);
	
	void expand_pass_node(llvm::BasicBlock *block, const NodeInstance *node);
	void expand_argument_node(llvm::BasicBlock *block, const NodeInstance *node);
	void expand_function_node(llvm::BasicBlock *block, const NodeInstance *node);
	llvm::FunctionType *get_node_function_type(const std::vector<llvm::Type*> &inputs,
	                                           const std::vector<llvm::Type*> &outputs);
	
	virtual llvm::Type *get_value_type(const TypeSpec *spec, bool is_constant) = 0;
	virtual bool use_argument_pointer(const TypeSpec *typespec, bool is_constant) = 0;

	virtual llvm::Constant *create_node_value_constant(const NodeValue *node_value) = 0;
	
	llvm::Function *declare_node_function(llvm::Module *mod, const NodeType *nodetype);
	virtual void define_nodes_module() = 0;
	virtual llvm::Module *get_nodes_module() const = 0;
	
private:
	llvm::Module *m_module;
	OutputValueMap m_output_values;
};

struct LLVMSimpleCompilerImpl : public LLVMCompilerBase {
	llvm::Type *get_value_type(const TypeSpec *spec, bool is_constant);
	
	llvm::Module *get_nodes_module() const { return m_nodes_module; }
	
	bool use_argument_pointer(const TypeSpec *typespec, bool is_constant);
	
	llvm::Constant *create_node_value_constant(const NodeValue *node_value);
	
	bool set_node_function_impl(OpCode op, const NodeType *nodetype, llvm::Function *func);
	void define_nodes_module();
	
private:
	static llvm::Module *m_nodes_module;
};

struct LLVMTextureCompiler : public LLVMCompilerBase {
	llvm::Type *get_value_type(const TypeSpec *spec, bool is_constant);
	llvm::Function *declare_elementary_node_function(
	        llvm::Module *mod, const NodeType *nodetype, const string &name,
	        bool with_derivatives);
	
	llvm::Module *get_nodes_module() const { return m_nodes_module; }
	
	bool use_argument_pointer(const TypeSpec *typespec, bool is_constant);
	bool use_elementary_argument_pointer(const TypeSpec *typespec);
	
	llvm::Constant *create_node_value_constant(const NodeValue *node_value);
	
	bool set_node_function_impl(OpCode op, const NodeType *nodetype,
	                            llvm::Function *value_func, llvm::Function * dual_func);
	void define_elementary_functions(OpCode op, llvm::Module *mod, const string &nodetype_name);
	void define_dual_function_wrapper(llvm::Module *mod, const string &nodetype_name);
	void define_nodes_module();
	
private:
	static llvm::Module *m_nodes_module;
};

} /* namespace blenvm */

#endif /* __LLVM_COMPILER_H__ */

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

struct FunctionParameter {
	FunctionParameter(llvm::Type *type, const std::string &name) :
	    type(type), name(name)
	{}
	
	llvm::Type *type;
	std::string name;
};
typedef std::vector<FunctionParameter> FunctionParameterList;

struct LLVMCompilerBase {
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
	
	void expand_pass_node(llvm::BasicBlock *block, const NodeInstance *node);
	void expand_argument_node(llvm::BasicBlock *block, const NodeInstance *node);
	void expand_function_node(llvm::BasicBlock *block, const NodeInstance *node);
	
	virtual void node_graph_begin() = 0;
	virtual void node_graph_end() = 0;
	
	virtual bool has_node_value(const ConstOutputKey &output) const = 0;
	virtual void alloc_node_value(llvm::BasicBlock *block, const ConstOutputKey &output) = 0;
	virtual void copy_node_value(const ConstOutputKey &from, const ConstOutputKey &to) = 0;
	virtual void append_output_arguments(std::vector<llvm::Value*> &args, const ConstOutputKey &output) = 0;
	virtual void append_input_value(llvm::BasicBlock *block, std::vector<llvm::Value*> &args,
	                                const TypeSpec *typespec, const ConstOutputKey &link) = 0;
	virtual void append_input_constant(llvm::BasicBlock *block, std::vector<llvm::Value*> &args,
	                                   const TypeSpec *typespec, const NodeConstant *node_value) = 0;
	virtual void map_argument(llvm::BasicBlock *block, const OutputKey &output, llvm::Argument *arg) = 0;
	virtual void store_return_value(llvm::BasicBlock *block, const OutputKey &output, llvm::Value *arg) = 0;
	
	virtual llvm::Type *get_argument_type(const TypeSpec *spec) const = 0;
	virtual llvm::Type *get_return_type(const TypeSpec *spec) const = 0;
	virtual void append_input_types(FunctionParameterList &params, const NodeInput *input) const = 0;
	virtual void append_output_types(FunctionParameterList &params, const NodeOutput *output) const = 0;
	
	llvm::Function *declare_function(llvm::Module *mod, const string &name,
	                                 const FunctionParameterList &input_types,
	                                 const FunctionParameterList &output_types,
	                                 bool use_globals);
	llvm::Function *declare_node_function(llvm::Module *mod, const NodeType *nodetype);
	virtual void define_nodes_module() = 0;
	virtual llvm::Module *get_nodes_module() const = 0;
	
private:
	llvm::Module *m_module;
	llvm::Value *m_globals_ptr;
};

struct LLVMTextureCompiler : public LLVMCompilerBase {
	typedef Dual2<llvm::Value*> DualValue;
	typedef std::map<ConstOutputKey, DualValue> OutputValueMap;
	
	void node_graph_begin();
	void node_graph_end();
	
	bool has_node_value(const ConstOutputKey &output) const;
	void alloc_node_value(llvm::BasicBlock *block, const ConstOutputKey &output);
	void copy_node_value(const ConstOutputKey &from, const ConstOutputKey &to);
	void append_output_arguments(std::vector<llvm::Value*> &args, const ConstOutputKey &output);
	void append_input_value(llvm::BasicBlock *block, std::vector<llvm::Value*> &args,
	                            const TypeSpec *typespec, const ConstOutputKey &link);
	void append_input_constant(llvm::BasicBlock *block, std::vector<llvm::Value*> &args,
	                           const TypeSpec *typespec, const NodeConstant *node_value);
	void map_argument(llvm::BasicBlock *block, const OutputKey &output, llvm::Argument *arg);
	void store_return_value(llvm::BasicBlock *block, const OutputKey &output, llvm::Value *arg);
	
	llvm::Type *get_argument_type(const TypeSpec *spec) const;
	llvm::Type *get_return_type(const TypeSpec *spec) const;
	void append_input_types(FunctionParameterList &params, const NodeInput *input) const;
	void append_output_types(FunctionParameterList &params, const NodeOutput *output) const;
	
	llvm::Module *get_nodes_module() const { return m_nodes_module; }
	
	bool use_argument_pointer(const TypeSpec *typespec, bool use_dual) const;
	bool use_elementary_argument_pointer(const TypeSpec *typespec) const;
	
	void define_node_function(llvm::Module *mod, OpCode op, const string &nodetype_name);
	void define_nodes_module();
	
	llvm::Function *declare_elementary_node_function(
	        llvm::Module *mod, const NodeType *nodetype, const string &name,
	        bool with_derivatives);
	
	void define_elementary_functions(llvm::Module *mod, OpCode op, const NodeType *nodetype);
	void define_dual_function_wrapper(llvm::Module *mod, llvm::Function *func, OpCode op, const NodeType *nodetype);
	
private:
	static llvm::Module *m_nodes_module;
	OutputValueMap m_output_values;
};

} /* namespace blenvm */

#endif /* __LLVM_COMPILER_H__ */

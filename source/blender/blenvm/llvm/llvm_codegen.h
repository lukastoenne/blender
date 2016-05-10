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

#include "node_graph.h"

#include "util_opcode.h"
#include "util_string.h"

namespace llvm {
class LLVMContext;
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

typedef std::map<ConstOutputKey, llvm::Value*> OutputValueMap;
typedef std::pair<ConstOutputKey, llvm::Value*> OutputValuePair;

struct LLVMCompilerBase {
	~LLVMCompilerBase();
	
protected:
	LLVMCompilerBase();
	
	llvm::LLVMContext &context() const;
	llvm::Module *module() const { return m_module; }
	void create_module(const string &name);
	void destroy_module();
	
	void optimize_function(llvm::Function *func, int opt_level);
	
	llvm::Constant *codegen_constant(const NodeValue *node_value);
	
	llvm::CallInst *codegen_node_call(llvm::BasicBlock *block, const NodeInstance *node, OutputValueMap &output_values);
	llvm::BasicBlock *codegen_function_body_expression(const NodeGraph &graph, llvm::Function *func);
	llvm::Function *codegen_node_function(const string &name, const NodeGraph &graph);
	
private:
	llvm::Module *m_module;
};

struct LLVMCompiler : public LLVMCompilerBase {
	FunctionLLVM *compile_function(const string &name, const NodeGraph &graph, int opt_level);
};

struct DebugLLVMCompiler : public LLVMCompilerBase {
	void compile_function(const string &name, const NodeGraph &graph, int opt_level, FILE *file);
};

} /* namespace blenvm */

#endif /* __LLVM_CODEGEN_H__ */

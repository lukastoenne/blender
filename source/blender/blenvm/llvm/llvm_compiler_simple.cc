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

/** \file blender/blenvm/llvm/llvm_compiler_simple.cc
 *  \ingroup llvm
 */

#include <cstdio>
#include <set>
#include <sstream>

#include "node_graph.h"

#include "llvm_compiler.h"
#include "llvm_engine.h"
#include "llvm_function.h"
#include "llvm_headers.h"
#include "llvm_modules.h"
#include "llvm_types.h"

#include "util_opcode.h"

#include "modules.h"

namespace blenvm {

llvm::Module *LLVMSimpleCompilerImpl::m_nodes_module = NULL;

llvm::Type *LLVMSimpleCompilerImpl::get_value_type(const TypeSpec *spec, bool UNUSED(is_constant))
{
	return bvm_get_llvm_type(context(), spec, false);
}

bool LLVMSimpleCompilerImpl::use_argument_pointer(const TypeSpec *typespec, bool UNUSED(is_constant))
{
	using namespace llvm;
	
	if (typespec->is_aggregate() || typespec->is_structure()) {
		/* pass by reference */
		return true;
	}
	else {
		return false;
	}
}

llvm::Constant *LLVMSimpleCompilerImpl::create_node_value_constant(const NodeConstant *node_value)
{
	return bvm_create_llvm_constant(context(), node_value);
}

bool LLVMSimpleCompilerImpl::set_node_function_impl(OpCode op, const NodeType *UNUSED(nodetype),
                                                    llvm::Function *func)
{
	using namespace llvm;
	
	std::vector<Value*> args;
	args.reserve(func->arg_size());
	for (Function::arg_iterator a = func->arg_begin(); a != func->arg_end(); ++a)
		args.push_back(a);
	
	switch (op) {
		case OP_VALUE_FLOAT: {
			BasicBlock *block = BasicBlock::Create(context(), "entry", func);
			def_node_VALUE_FLOAT(context(), block, args[0], args[1]);
			return true;
		}
		case OP_VALUE_INT: {
			BasicBlock *block = BasicBlock::Create(context(), "entry", func);
			def_node_VALUE_INT(context(), block, args[0], args[1]);
			return true;
		}
		case OP_VALUE_FLOAT3: {
			BasicBlock *block = BasicBlock::Create(context(), "entry", func);
			def_node_VALUE_FLOAT3(context(), block, args[0], args[1]);
			return true;
		}
		case OP_VALUE_FLOAT4: {
			BasicBlock *block = BasicBlock::Create(context(), "entry", func);
			def_node_VALUE_FLOAT4(context(), block, args[0], args[1]);
			return true;
		}
		case OP_VALUE_MATRIX44: {
			BasicBlock *block = BasicBlock::Create(context(), "entry", func);
			def_node_VALUE_MATRIX44(context(), block, args[0], args[1]);
			return true;
		}
		
		default:
			return false;
	}
}

void LLVMSimpleCompilerImpl::define_nodes_module()
{
	using namespace llvm;
	
	Module *mod = new llvm::Module("simple_nodes", context());
	
#define DEF_OPCODE(op) \
	{ \
		const NodeType *nodetype = NodeGraph::find_node_type(STRINGIFY(op)); \
		if (nodetype != NULL) { \
			Function *func = declare_node_function(mod, nodetype); \
			if (func != NULL) { \
				bool has_impl = set_node_function_impl(OP_##op, nodetype, func); \
				if (!has_impl) { \
					/* use C callback as implementation of the function */ \
					void *cb_ptr = modules::get_node_impl_value<OP_##op>(); \
					BLI_assert(cb_ptr != NULL && "No implementation for OpCode!"); \
					llvm_execution_engine()->addGlobalMapping(func, cb_ptr); \
				} \
			} \
		} \
	}
	
	BVM_DEFINE_OPCODES
	
#undef DEF_OPCODE
	
	llvm_execution_engine()->addModule(mod);
	
	m_nodes_module = mod;
}

} /* namespace blenvm */

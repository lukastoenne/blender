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

#ifndef __LLVM_MODULES_H__
#define __LLVM_MODULES_H__

/** \file blender/blenvm/llvm/llvm_modules.h
 *  \ingroup llvm
 */

#include "util_string.h"
#include "util_opcode.h"

namespace llvm {
class LLVMContext;
class BasicBlock;
class Function;
class Module;
class Value;
}

namespace blenvm {

struct NodeType;

void register_extern_node_functions();

bool llvm_has_external_impl_value(OpCode op);
bool llvm_has_external_impl_deriv(OpCode op);

void def_node_VALUE_FLOAT(llvm::LLVMContext &context, llvm::Function *func);
void def_node_VALUE_INT(llvm::LLVMContext &context, llvm::Function *func);
void def_node_VALUE_FLOAT3(llvm::LLVMContext &context, llvm::Function *func);
void def_node_VALUE_FLOAT4(llvm::LLVMContext &context, llvm::Function *func);
void def_node_VALUE_MATRIX44(llvm::LLVMContext &context, llvm::Function *func);

void def_node_FLOAT_TO_INT(llvm::LLVMContext &context, llvm::Function *func);
void def_node_INT_TO_FLOAT(llvm::LLVMContext &context, llvm::Function *func);

void def_node_SET_FLOAT3(llvm::LLVMContext &context, llvm::Function *func);
void def_node_GET_ELEM_FLOAT3(llvm::LLVMContext &context, llvm::Function *func);
void def_node_SET_FLOAT4(llvm::LLVMContext &context, llvm::Function *func);
void def_node_GET_ELEM_FLOAT4(llvm::LLVMContext &context, llvm::Function *func);

void def_node_GET_DERIVATIVE_FLOAT(llvm::LLVMContext &context, llvm::Function *func);
void def_node_GET_DERIVATIVE_FLOAT3(llvm::LLVMContext &context, llvm::Function *func);
void def_node_GET_DERIVATIVE_FLOAT4(llvm::LLVMContext &context, llvm::Function *func);

} /* namespace blenvm */

#endif /* __LLVM_MODULES_H__ */

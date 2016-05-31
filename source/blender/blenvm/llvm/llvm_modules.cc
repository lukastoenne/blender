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

/** \file blender/blenvm/llvm/llvm_modules.cc
 *  \ingroup llvm
 */

#include <map>
#include <sstream>
#include <stdint.h>

extern "C" {
#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_path_util.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.h"
}

#include "node_graph.h"

#include "llvm_engine.h"
#include "llvm_modules.h"
#include "llvm_headers.h"
#include "llvm_types.h"

#include "util_math.h"
#include "util_opcode.h"

#include "modules.h"

namespace blenvm {

void def_node_VALUE_FLOAT(llvm::LLVMContext &context, llvm::BasicBlock *block,
                          llvm::Value *result, llvm::Value *value)
{
	using namespace llvm;
	
	IRBuilder<> builder(context);
	builder.SetInsertPoint(block);
	
	builder.CreateStore(value, result);
	
	builder.CreateRetVoid();
}

void def_node_VALUE_INT(llvm::LLVMContext &context, llvm::BasicBlock *block,
                        llvm::Value *result, llvm::Value *value)
{
	using namespace llvm;
	
	IRBuilder<> builder(context);
	builder.SetInsertPoint(block);
	
	builder.CreateStore(value, result);
	
	builder.CreateRetVoid();
}

void def_node_VALUE_FLOAT3(llvm::LLVMContext &context, llvm::BasicBlock *block,
                           llvm::Value *result, llvm::Value *value)
{
	using namespace llvm;
	
	IRBuilder<> builder(context);
	builder.SetInsertPoint(block);
	
	size_t size = sizeof(float3);
	Value *size_val = ConstantInt::get(context, APInt(32, size));
	builder.CreateMemCpy(result, value, size_val, 0);
	
	builder.CreateRetVoid();
}

void def_node_VALUE_FLOAT4(llvm::LLVMContext &context, llvm::BasicBlock *block,
                           llvm::Value *result, llvm::Value *value)
{
	using namespace llvm;
	
	IRBuilder<> builder(context);
	builder.SetInsertPoint(block);
	
	size_t size = sizeof(float4);
	Value *size_val = ConstantInt::get(context, APInt(32, size));
	builder.CreateMemCpy(result, value, size_val, 0);
	
	builder.CreateRetVoid();
}

void def_node_VALUE_MATRIX44(llvm::LLVMContext &context, llvm::BasicBlock *block,
                             llvm::Value *result, llvm::Value *value)
{
	using namespace llvm;
	
	IRBuilder<> builder(context);
	builder.SetInsertPoint(block);
	
	size_t size = sizeof(matrix44);
	Value *size_val = ConstantInt::get(context, APInt(32, size));
	builder.CreateMemCpy(result, value, size_val, 0);
	
	builder.CreateRetVoid();
}

} /* namespace llvm */

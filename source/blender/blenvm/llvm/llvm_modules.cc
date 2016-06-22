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

typedef llvm::IRBuilder<> Builder;

static llvm::Value *float_vector_at(Builder &builder, llvm::Value *p_vec, llvm::Value *idx)
{
	using namespace llvm;
	
	Type *float_ptr_type = bvm_get_llvm_type(builder.getContext(), BVM_FLOAT, false)->getPointerTo();
	Value *p_elem = builder.CreatePointerCast(p_vec, float_ptr_type);
	return builder.CreateInBoundsGEP(p_elem, idx);
}

static void def_node_VALUE_t(llvm::LLVMContext &context, llvm::Function *func, const TypeSpec *typespec)
{
	using namespace llvm;
	
	bool has_derivs = bvm_type_has_dual_value(typespec);
	
	Function::arg_iterator arg_it = func->arg_begin();
	Argument *p_out_val = arg_it++;
	Argument *p_out_dx = NULL, *p_out_dy = NULL;
	if (has_derivs) {
		p_out_dx = arg_it++;
		p_out_dy = arg_it++;
	}
	Argument *in_val = arg_it++;
	
	BasicBlock *block = BasicBlock::Create(context, "entry", func);
	
	IRBuilder<> builder(context);
	builder.SetInsertPoint(block);
	
	bvm_llvm_copy_value(context, block, p_out_val, in_val, typespec);
	if (has_derivs) {
		bvm_llvm_set_zero(context, block, p_out_dx, typespec);
		bvm_llvm_set_zero(context, block, p_out_dy, typespec);
	}
	
	builder.CreateRetVoid();
}

void def_node_VALUE_FLOAT(llvm::LLVMContext &context, llvm::Function *func)
{
	def_node_VALUE_t(context, func, TypeSpec::get_typespec("FLOAT"));
}

void def_node_VALUE_INT(llvm::LLVMContext &context, llvm::Function *func)
{
	def_node_VALUE_t(context, func, TypeSpec::get_typespec("INT"));
}

void def_node_VALUE_FLOAT3(llvm::LLVMContext &context, llvm::Function *func)
{
	def_node_VALUE_t(context, func, TypeSpec::get_typespec("FLOAT3"));
}

void def_node_VALUE_FLOAT4(llvm::LLVMContext &context, llvm::Function *func)
{
	def_node_VALUE_t(context, func, TypeSpec::get_typespec("FLOAT4"));
}

void def_node_VALUE_MATRIX44(llvm::LLVMContext &context, llvm::Function *func)
{
	def_node_VALUE_t(context, func, TypeSpec::get_typespec("MATRIX44"));
}

void def_node_FLOAT_TO_INT(llvm::LLVMContext &context, llvm::Function *func)
{
	using namespace llvm;
	
	Function::arg_iterator arg_it = func->arg_begin();
	Argument *p_out_val = arg_it++;
	Argument *in_val = arg_it++;
	Argument *in_dx = arg_it++;
	Argument *in_dy = arg_it++;
	UNUSED_VARS(in_dx, in_dy);
	
	BasicBlock *block = BasicBlock::Create(context, "entry", func);
	
	IRBuilder<> builder(context);
	builder.SetInsertPoint(block);
	
	Type *target_type = bvm_get_llvm_type(context, BVM_INT, false);
	Value *ival = builder.CreateFPToSI(in_val, target_type);
	builder.CreateStore(ival, p_out_val);
	
	builder.CreateRetVoid();
}

void def_node_INT_TO_FLOAT(llvm::LLVMContext &context, llvm::Function *func)
{
	using namespace llvm;
	
	Function::arg_iterator arg_it = func->arg_begin();
	Argument *p_out_val = arg_it++;
	Argument *p_out_dx = arg_it++;
	Argument *p_out_dy = arg_it++;
	Argument *in_val = arg_it++;
	
	BasicBlock *block = BasicBlock::Create(context, "entry", func);
	
	IRBuilder<> builder(context);
	builder.SetInsertPoint(block);
	
	Type *target_type = bvm_get_llvm_type(context, BVM_FLOAT, false);
	Value *fval = builder.CreateSIToFP(in_val, target_type);
	builder.CreateStore(fval, p_out_val);
	
	Constant *fzero = ConstantFP::get(builder.getContext(), APFloat(0.0f));
	builder.CreateStore(fzero, p_out_dx);
	builder.CreateStore(fzero, p_out_dy);
	
	builder.CreateRetVoid();
}

static void def_node_SET_FLOATn(llvm::LLVMContext &context, llvm::Function *func, unsigned int n)
{
	using namespace llvm;
	
	Function::arg_iterator arg_it = func->arg_begin();
	Argument *p_out_val = arg_it++;
	Argument *p_out_dx = arg_it++;
	Argument *p_out_dy = arg_it++;
	
	BasicBlock *block = BasicBlock::Create(context, "entry", func);
	
	IRBuilder<> builder(context);
	builder.SetInsertPoint(block);
	
	for (unsigned int i = 0; i < n; ++i) {
		Argument *val = arg_it++;
		Argument *dx = arg_it++;
		Argument *dy = arg_it++;
		
		builder.CreateStore(val, builder.CreateStructGEP(p_out_val, i));
		builder.CreateStore(dx, builder.CreateStructGEP(p_out_dx, i));
		builder.CreateStore(dy, builder.CreateStructGEP(p_out_dy, i));
	}
	
	builder.CreateRetVoid();
}

void def_node_SET_FLOAT3(llvm::LLVMContext &context, llvm::Function *func)
{
	def_node_SET_FLOATn(context, func, 3);
}

void def_node_SET_FLOAT4(llvm::LLVMContext &context, llvm::Function *func)
{
	def_node_SET_FLOATn(context, func, 4);
}

static void def_node_GET_ELEM_FLOATn(llvm::LLVMContext &context, llvm::Function *func)
{
	using namespace llvm;
	
	Function::arg_iterator arg_it = func->arg_begin();
	Argument *p_out_val = arg_it++;
	Argument *p_out_dx = arg_it++;
	Argument *p_out_dy = arg_it++;
	Argument *index = arg_it++;
	Argument *vec_val = arg_it++;
	Argument *vec_dx = arg_it++;
	Argument *vec_dy = arg_it++;
	
	BasicBlock *block = BasicBlock::Create(context, "entry", func);
	
	IRBuilder<> builder(context);
	builder.SetInsertPoint(block);
	
	builder.CreateStore(builder.CreateLoad(float_vector_at(builder, vec_val, index)), p_out_val);
	builder.CreateStore(builder.CreateLoad(float_vector_at(builder, vec_dx, index)), p_out_dx);
	builder.CreateStore(builder.CreateLoad(float_vector_at(builder, vec_dy, index)), p_out_dy);
	
	builder.CreateRetVoid();
}

void def_node_GET_ELEM_FLOAT3(llvm::LLVMContext &context, llvm::Function *func)
{
	def_node_GET_ELEM_FLOATn(context, func);
}

void def_node_GET_ELEM_FLOAT4(llvm::LLVMContext &context, llvm::Function *func)
{
	def_node_GET_ELEM_FLOATn(context, func);
}

/* ------------------------------------------------------------------------- */

static void def_node_GET_DERIVATIVE_t(llvm::LLVMContext &context, llvm::Function *func, const TypeSpec *typespec)
{
	using namespace llvm;
	
	ConstantInt* idx0 = ConstantInt::get(context, APInt(32, 0));
	ConstantInt* idx1 = ConstantInt::get(context, APInt(32, 1));
	
	Function::arg_iterator arg_it = func->arg_begin();
	Argument *out_val = arg_it++;
	Argument *out_dx = arg_it++;
	Argument *out_dy = arg_it++;
	Argument *var = arg_it++;
	Argument *in_val = arg_it++;
	Argument *in_dx = arg_it++;
	Argument *in_dy = arg_it++;
	UNUSED_VARS(in_val);
	
	BasicBlock *block = BasicBlock::Create(context, "entry", func);
	BasicBlock *block_var0 = BasicBlock::Create(context, "var0", func);
	BasicBlock *block_var1 = BasicBlock::Create(context, "var1", func);
	BasicBlock *block_end = BasicBlock::Create(context, "end", func);
	
	{
		IRBuilder<> builder(context);
		builder.SetInsertPoint(block);
		
		/* zero derivatives */
		bvm_llvm_set_zero(context, block, out_dx, typespec);
		bvm_llvm_set_zero(context, block, out_dy, typespec);
		
		SwitchInst *sw = builder.CreateSwitch(var, block_end, 2);
		sw->addCase(idx0, block_var0);
		sw->addCase(idx1, block_var1);
	}
	
	{
		IRBuilder<> builder(context);
		builder.SetInsertPoint(block_var0);
		
		bvm_llvm_copy_value(context, block_var0, out_val, in_dx, typespec);
		builder.CreateBr(block_end);
	}
	
	{
		IRBuilder<> builder(context);
		builder.SetInsertPoint(block_var1);
		
		bvm_llvm_copy_value(context, block_var1, out_val, in_dy, typespec);
		builder.CreateBr(block_end);
	}
	
	{
		IRBuilder<> builder(context);
		builder.SetInsertPoint(block_end);
		
		builder.CreateRetVoid();
	}
}

void def_node_GET_DERIVATIVE_FLOAT(llvm::LLVMContext &context, llvm::Function *func)
{
	const TypeSpec *typespec = TypeSpec::get_typespec("FLOAT");
	return def_node_GET_DERIVATIVE_t(context, func, typespec);
}

void def_node_GET_DERIVATIVE_FLOAT3(llvm::LLVMContext &context, llvm::Function *func)
{
	const TypeSpec *typespec = TypeSpec::get_typespec("FLOAT3");
	return def_node_GET_DERIVATIVE_t(context, func, typespec);
}

void def_node_GET_DERIVATIVE_FLOAT4(llvm::LLVMContext &context, llvm::Function *func)
{
	const TypeSpec *typespec = TypeSpec::get_typespec("FLOAT4");
	return def_node_GET_DERIVATIVE_t(context, func, typespec);
}

} /* namespace llvm */

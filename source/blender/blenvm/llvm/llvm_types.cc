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

/** \file blender/blenvm/llvm/llvm_types.cc
 *  \ingroup llvm
 */

#include "llvm_headers.h"
#include "llvm_types.h"


namespace blenvm {

llvm::Type *bvm_get_llvm_type(llvm::LLVMContext &context, const TypeSpec *spec, bool use_dual)
{
	using namespace llvm;
	
	if (spec->is_structure()) {
		const StructSpec *sspec = spec->structure();
		std::vector<Type*> fields;
		for (int i = 0; i < sspec->num_fields(); ++i) {
			Type *ftype = bvm_get_llvm_type(context, sspec->field(i).typespec, use_dual);
			fields.push_back(ftype);
		}
		return StructType::create(context, ArrayRef<Type*>(fields));
	}
	else {
		if (use_dual) {
#define BVM_BUILD_TYPE(t) \
	TypeBuilder<BVMTypeLLVMTraits<t>::dual2_type, false>::get(context)
			switch (spec->base_type()) {
				case BVM_FLOAT: return BVM_BUILD_TYPE(BVM_FLOAT);
				case BVM_FLOAT3: return BVM_BUILD_TYPE(BVM_FLOAT3);
				case BVM_FLOAT4: return BVM_BUILD_TYPE(BVM_FLOAT4);
				case BVM_INT: return BVM_BUILD_TYPE(BVM_INT);
				case BVM_MATRIX44: return BVM_BUILD_TYPE(BVM_MATRIX44);
				case BVM_STRING: return BVM_BUILD_TYPE(BVM_STRING);
				case BVM_RNAPOINTER: return BVM_BUILD_TYPE(BVM_RNAPOINTER);
				case BVM_MESH: return BVM_BUILD_TYPE(BVM_MESH);
				case BVM_DUPLIS: return BVM_BUILD_TYPE(BVM_DUPLIS);
			}
#undef BVM_BUILD_TYPE
		}
		else {
#define BVM_BUILD_TYPE(t) \
	TypeBuilder<BVMTypeLLVMTraits<t>::value_type, false>::get(context)
			switch (spec->base_type()) {
				case BVM_FLOAT: return BVM_BUILD_TYPE(BVM_FLOAT);
				case BVM_FLOAT3: return BVM_BUILD_TYPE(BVM_FLOAT3);
				case BVM_FLOAT4: return BVM_BUILD_TYPE(BVM_FLOAT4);
				case BVM_INT: return BVM_BUILD_TYPE(BVM_INT);
				case BVM_MATRIX44: return BVM_BUILD_TYPE(BVM_MATRIX44);
				case BVM_STRING: return BVM_BUILD_TYPE(BVM_STRING);
				case BVM_RNAPOINTER: return BVM_BUILD_TYPE(BVM_RNAPOINTER);
				case BVM_MESH: return BVM_BUILD_TYPE(BVM_MESH);
				case BVM_DUPLIS: return BVM_BUILD_TYPE(BVM_DUPLIS);
			}
#undef BVM_BUILD_TYPE
		}
	}
	return NULL;
}

llvm::Constant *bvm_create_llvm_constant(llvm::LLVMContext &context, const NodeConstant *node_value)
{
	using namespace llvm;
	
	const TypeSpec *spec = node_value->typedesc().get_typespec();
	if (spec->is_structure()) {
//		const StructSpec *sspec = spec->structure();
		/* TODO don't have value storage for this yet */
		return NULL;
	}
	else {
#define BVM_MAKE_CONSTANT(t) \
	{ \
		BVMTypeLLVMTraits<t>::value_type c; \
		node_value->get(&c); \
		return make_constant(context, c); \
	} (void)0
		switch (spec->base_type()) {
			case BVM_FLOAT: BVM_MAKE_CONSTANT(BVM_FLOAT);
			case BVM_FLOAT3: BVM_MAKE_CONSTANT(BVM_FLOAT3);
			case BVM_FLOAT4: BVM_MAKE_CONSTANT(BVM_FLOAT4);
			case BVM_INT: BVM_MAKE_CONSTANT(BVM_INT);
			case BVM_MATRIX44: BVM_MAKE_CONSTANT(BVM_MATRIX44);
			case BVM_STRING: BVM_MAKE_CONSTANT(BVM_STRING);
			case BVM_RNAPOINTER: BVM_MAKE_CONSTANT(BVM_RNAPOINTER);
			case BVM_MESH: BVM_MAKE_CONSTANT(BVM_MESH);
			case BVM_DUPLIS: BVM_MAKE_CONSTANT(BVM_DUPLIS);
		}
#undef BVM_MAKE_CONSTANT
	}
	return NULL;
}

bool bvm_type_has_dual_value(const TypeSpec *spec)
{
	using namespace llvm;
	
	if (spec->is_structure()) {
		/* for structs we use invidual dual values for fields */
		return false;
	}
	else {
		switch (spec->base_type()) {
			case BVM_FLOAT: return BVMTypeLLVMTraits<BVM_FLOAT>::use_dual_value;
			case BVM_FLOAT3: return BVMTypeLLVMTraits<BVM_FLOAT3>::use_dual_value;
			case BVM_FLOAT4: return BVMTypeLLVMTraits<BVM_FLOAT4>::use_dual_value;
			case BVM_INT: return BVMTypeLLVMTraits<BVM_INT>::use_dual_value;
			case BVM_MATRIX44: return BVMTypeLLVMTraits<BVM_MATRIX44>::use_dual_value;
			case BVM_STRING: return BVMTypeLLVMTraits<BVM_STRING>::use_dual_value;
			case BVM_RNAPOINTER: return BVMTypeLLVMTraits<BVM_RNAPOINTER>::use_dual_value;
			case BVM_MESH: return BVMTypeLLVMTraits<BVM_MESH>::use_dual_value;
			case BVM_DUPLIS: return BVMTypeLLVMTraits<BVM_DUPLIS>::use_dual_value;
		}
	}
	return false;
}

void bvm_llvm_set_zero(llvm::LLVMContext &context, llvm::BasicBlock *block, llvm::Value *ptr, const TypeSpec *spec)
{
	using namespace llvm;
	
	IRBuilder<> builder(context);
	builder.SetInsertPoint(block);
	
	if (spec->is_structure()) {
		/* TODO */
		BLI_assert(false);
	}
	else {
#define BVM_SET_ZERO(t) \
		BVMTypeLLVMTraits<t>::set_zero(builder, ptr)
		
		switch (spec->base_type()) {
			case BVM_FLOAT: BVM_SET_ZERO(BVM_FLOAT); break;
			case BVM_FLOAT3: BVM_SET_ZERO(BVM_FLOAT3); break;
			case BVM_FLOAT4: BVM_SET_ZERO(BVM_FLOAT4); break;
			case BVM_INT: BVM_SET_ZERO(BVM_INT); break;
			case BVM_MATRIX44: BVM_SET_ZERO(BVM_MATRIX44); break;
			case BVM_STRING: BVM_SET_ZERO(BVM_STRING); break;
			case BVM_RNAPOINTER: BVM_SET_ZERO(BVM_RNAPOINTER); break;
			case BVM_MESH: BVM_SET_ZERO(BVM_MESH); break;
			case BVM_DUPLIS: BVM_SET_ZERO(BVM_DUPLIS); break;
		}
		
#undef BVM_SET_ZERO
	}
}

void bvm_llvm_copy_value(llvm::LLVMContext &context, llvm::BasicBlock *block,
                         llvm::Value *ptr, llvm::Value *value,
                         const TypeSpec *spec)
{
	using namespace llvm;
	
	IRBuilder<> builder(context);
	builder.SetInsertPoint(block);
	
	if (spec->is_structure()) {
		/* XXX TODO */
		return;
	}
	else {
#define BVM_COPY_VALUE(t) \
		BVMTypeLLVMTraits<t>::copy_value(builder, ptr, value)
		
		switch (spec->base_type()) {
			case BVM_FLOAT: BVM_COPY_VALUE(BVM_FLOAT); break;
			case BVM_FLOAT3: BVM_COPY_VALUE(BVM_FLOAT3); break;
			case BVM_FLOAT4: BVM_COPY_VALUE(BVM_FLOAT4); break;
			case BVM_INT: BVM_COPY_VALUE(BVM_INT); break;
			case BVM_MATRIX44: BVM_COPY_VALUE(BVM_MATRIX44); break;
			case BVM_STRING: BVM_COPY_VALUE(BVM_STRING); break;
			case BVM_RNAPOINTER: BVM_COPY_VALUE(BVM_RNAPOINTER); break;
			case BVM_MESH: BVM_COPY_VALUE(BVM_MESH); break;
			case BVM_DUPLIS: BVM_COPY_VALUE(BVM_DUPLIS); break;
		}
		
#undef BVM_COPY_VALUE
	}
}

} /* namespace blenvm */

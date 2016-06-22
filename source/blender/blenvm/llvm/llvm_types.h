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

#ifndef __LLVM_TYPES_H__
#define __LLVM_TYPES_H__

/** \file blender/blenvm/llvm/llvm_types.h
 *  \ingroup llvm
 */

#include "MEM_guardedalloc.h"

#include "node_value.h"
#include "typedesc.h"

#include "llvm_headers.h"

#include "util_string.h"

namespace llvm {
class LLVMContext;
class Constant;
class FunctionType;
class StructType;
class Type;
class Value;
}


/* TypeBuilder specializations for own structs */

namespace llvm {

template <bool xcompile>
class TypeBuilder<blenvm::float3, xcompile> {
public:
	static StructType *get(LLVMContext &context) {
		return StructType::get(
		            TypeBuilder<types::ieee_float, xcompile>::get(context),
		            TypeBuilder<types::ieee_float, xcompile>::get(context),
		            TypeBuilder<types::ieee_float, xcompile>::get(context),
		            NULL);
	}
	
	enum Fields {
		FIELD_X = 0,
		FIELD_Y = 1,
		FIELD_Z = 2,
	};
};

template <bool xcompile>
class TypeBuilder<blenvm::float4, xcompile> {
public:
	static StructType *get(LLVMContext &context) {
		return StructType::get(
		            TypeBuilder<types::ieee_float, xcompile>::get(context),
		            TypeBuilder<types::ieee_float, xcompile>::get(context),
		            TypeBuilder<types::ieee_float, xcompile>::get(context),
		            TypeBuilder<types::ieee_float, xcompile>::get(context),
		            NULL);
	}
	
	enum Fields {
		FIELD_X = 0,
		FIELD_Y = 1,
		FIELD_Z = 2,
		FIELD_W = 3,
	};
};

template <bool xcompile>
class TypeBuilder<blenvm::matrix44, xcompile> {
public:
	static StructType *get(LLVMContext &context) {
		return StructType::get(
		            ArrayType::get(
		                ArrayType::get(
		                    TypeBuilder<types::ieee_float, xcompile>::get(context),
		                    4),
		                4),
		            NULL);
	}
};

template <typename T, bool xcompile>
class TypeBuilder<blenvm::Dual2<T>, xcompile> {
public:
	static StructType *get(LLVMContext &context) {
		return StructType::get(
		            TypeBuilder<T, xcompile>::get(context),
		            TypeBuilder<T, xcompile>::get(context),
		            TypeBuilder<T, xcompile>::get(context),
		            NULL);
	}
	
	enum Fields {
		FIELD_VALUE = 0,
		FIELD_DX = 1,
		FIELD_DY = 2,
	};
};

#if 0
/* Specialization to enable TypeBuilder for Dual2<float> */
template <bool xcompile>
class TypeBuilder<blenvm::Dual2<float>, xcompile> {
public:
	static StructType *get(LLVMContext &context) {
		return StructType::get(
		            TypeBuilder<types::ieee_float, xcompile>::get(context),
		            TypeBuilder<types::ieee_float, xcompile>::get(context),
		            TypeBuilder<types::ieee_float, xcompile>::get(context),
		            NULL);
	}
	
	enum Fields {
		FIELD_VALUE = 0,
		FIELD_DX = 1,
		FIELD_DY = 2,
	};
};
#endif

} /* namespace llvm */

/* ------------------------------------------------------------------------- */

namespace blenvm {

inline llvm::Constant *make_constant(llvm::LLVMContext &context, float f)
{
	using namespace llvm;
	
	return ConstantFP::get(context, APFloat(f));
}

inline llvm::Constant *make_constant(llvm::LLVMContext &context, const float3 &f)
{
	using namespace llvm;
	
	StructType *stype = TypeBuilder<float3, false>::get(context);
	return ConstantStruct::get(stype,
	                           ConstantFP::get(context, APFloat(f.x)),
	                           ConstantFP::get(context, APFloat(f.y)),
	                           ConstantFP::get(context, APFloat(f.z)),
	                           NULL);
}

inline llvm::Constant *make_constant(llvm::LLVMContext &context, const float4 &f)
{
	using namespace llvm;
	
	StructType *stype = TypeBuilder<float4, false>::get(context);
	return ConstantStruct::get(stype,
	                           ConstantFP::get(context, APFloat(f.x)),
	                           ConstantFP::get(context, APFloat(f.y)),
	                           ConstantFP::get(context, APFloat(f.z)),
	                           ConstantFP::get(context, APFloat(f.w)),
	                           NULL);
}

inline llvm::Constant *make_constant(llvm::LLVMContext &context, int i)
{
	using namespace llvm;
	
	return ConstantInt::get(context, APInt(32, i, true));
}

inline llvm::Constant *make_constant(llvm::LLVMContext &context, const matrix44 &m)
{
	using namespace llvm;
	
	Type *elem_t = TypeBuilder<float, false>::get(context);
	ArrayType *inner_t = ArrayType::get(elem_t, 4);
	ArrayType *outer_t = ArrayType::get(inner_t, 4);
	StructType *matrix_t = StructType::get(outer_t, NULL);
	
	Constant *constants[4][4];
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			constants[i][j] = ConstantFP::get(context, APFloat(m.data[i][j]));
	Constant *cols[4];
	for (int i = 0; i < 4; ++i)
		cols[i] = ConstantArray::get(inner_t, llvm::ArrayRef<Constant*>(constants[i], 4));
	Constant *data = ConstantArray::get(outer_t, llvm::ArrayRef<Constant*>(cols, 4));
	return ConstantStruct::get(matrix_t,
	                           data, NULL);
}

template <typename T>
inline llvm::Constant *make_constant(llvm::LLVMContext &context, const Dual2<T> &d)
{
	using namespace llvm;
	StructType *stype = TypeBuilder<Dual2<T>, false>::get(context);
	return ConstantStruct::get(stype,
	                           make_constant(context, d.value()),
	                           make_constant(context, d.dx()),
	                           make_constant(context, d.dy()),
	                           NULL);
}

/* ------------------------------------------------------------------------- */

template <BVMType type>
struct BVMTypeLLVMTraits;

template <>
struct BVMTypeLLVMTraits<BVM_FLOAT> {
	enum { use_dual_value = true };
	
	typedef float value_type;
	typedef Dual2<value_type> dual2_type;
	
	template <typename BuilderT>
	static void copy_value(BuilderT &builder, llvm::Value *ptr, llvm::Value *value)
	{
		using namespace llvm;
		builder.CreateStore(value, ptr);
	}
	
	template <typename BuilderT>
	static void set_zero(BuilderT &builder, llvm::Value *ptr)
	{
		using namespace llvm;
		Constant *c = ConstantFP::get(builder.getContext(), APFloat(0.0f));
		builder.CreateStore(c, ptr);
	}
};

template <>
struct BVMTypeLLVMTraits<BVM_FLOAT3> {
	enum { use_dual_value = true };
	
	typedef float3 value_type;
	typedef Dual2<value_type> dual2_type;
	
	template <typename BuilderT>
	static void copy_value(BuilderT &builder, llvm::Value *ptr, llvm::Value *value)
	{
		using namespace llvm;
		Value *ivalue = builder.CreateLoad(value);
		builder.CreateStore(ivalue, ptr);
	}
	
	template <typename BuilderT>
	static void set_zero(BuilderT &builder, llvm::Value *ptr)
	{
		using namespace llvm;
		Constant *zero = ConstantAggregateZero::get(TypeBuilder<value_type, false>::get(builder.getContext()));
		builder.CreateStore(zero, ptr);
	}
};

template <>
struct BVMTypeLLVMTraits<BVM_FLOAT4> {
	enum { use_dual_value = true };
	
	typedef float4 value_type;
	typedef Dual2<value_type> dual2_type;
	
	template <typename BuilderT>
	static void copy_value(BuilderT &builder, llvm::Value *ptr, llvm::Value *value)
	{
		using namespace llvm;
		Value *ivalue = builder.CreateLoad(value);
		builder.CreateStore(ivalue, ptr);
	}
	
	template <typename BuilderT>
	static void set_zero(BuilderT &builder, llvm::Value *ptr)
	{
		using namespace llvm;
		Constant *zero = ConstantAggregateZero::get(TypeBuilder<value_type, false>::get(builder.getContext()));
		builder.CreateStore(zero, ptr);
	}
};

template <>
struct BVMTypeLLVMTraits<BVM_INT> {
	enum { use_dual_value = false };
	
	typedef int value_type;
	typedef value_type dual2_type;
	
	template <typename BuilderT>
	static void copy_value(BuilderT &builder, llvm::Value *ptr, llvm::Value *value)
	{
		using namespace llvm;
		builder.CreateStore(value, ptr);
	}
	
	template <typename BuilderT>
	static void set_zero(BuilderT &builder, llvm::Value *ptr)
	{
		using namespace llvm;
		Constant *c = builder.getInt32(0);
		builder.CreateStore(c, ptr);
	}
};

template <>
struct BVMTypeLLVMTraits<BVM_MATRIX44> {
	enum { use_dual_value = false };
	
	typedef matrix44 value_type;
	typedef value_type dual2_type;
	
	template <typename BuilderT>
	static void copy_value(BuilderT &builder, llvm::Value *ptr, llvm::Value *value)
	{
		using namespace llvm;
		Constant *size = builder.getInt32(sizeof(value_type));
		builder.CreateMemCpy(ptr, value, size, 0);
	}
	
	template <typename BuilderT>
	static void set_zero(BuilderT &builder, llvm::Value *ptr)
	{
		using namespace llvm;
		Constant *size = builder.getInt32(sizeof(value_type));
		builder.CreateMemSet(ptr, builder.getInt8(0), size, 0);
	}
};

template <>
struct BVMTypeLLVMTraits<BVM_STRING> {
	enum { use_dual_value = false };
	
	typedef int value_type;
	typedef value_type dual2_type;
	
	template <typename BuilderT>
	static void copy_value(BuilderT &builder, llvm::Value *ptr, llvm::Value *value)
	{
		using namespace llvm;
		/* TODO */
		BLI_assert(false);
		UNUSED_VARS(builder, ptr, value);
	}
	
	template <typename BuilderT>
	static void set_zero(BuilderT &builder, llvm::Value *ptr)
	{
		using namespace llvm;
		/* TODO */
		BLI_assert(false);
		UNUSED_VARS(builder, ptr);
	}
};

template <>
struct BVMTypeLLVMTraits<BVM_RNAPOINTER> {
	enum { use_dual_value = false };
	
	typedef int value_type;
	typedef value_type dual2_type;
	
	template <typename BuilderT>
	static void copy_value(BuilderT &builder, llvm::Value *ptr, llvm::Value *value)
	{
		using namespace llvm;
		/* TODO */
		BLI_assert(false);
		UNUSED_VARS(builder, ptr, value);
	}
	
	template <typename BuilderT>
	static void set_zero(BuilderT &builder, llvm::Value *ptr)
	{
		using namespace llvm;
		/* TODO */
		BLI_assert(false);
		UNUSED_VARS(builder, ptr);
	}
};

template <>
struct BVMTypeLLVMTraits<BVM_MESH> {
	enum { use_dual_value = false };
	
	typedef int value_type;
	typedef value_type dual2_type;
	
	template <typename BuilderT>
	static void copy_value(BuilderT &builder, llvm::Value *ptr, llvm::Value *value)
	{
		using namespace llvm;
		/* TODO */
		BLI_assert(false);
		UNUSED_VARS(builder, ptr, value);
	}
	
	template <typename BuilderT>
	static void set_zero(BuilderT &builder, llvm::Value *ptr)
	{
		using namespace llvm;
		/* TODO */
		BLI_assert(false);
		UNUSED_VARS(builder, ptr);
	}
};

template <>
struct BVMTypeLLVMTraits<BVM_DUPLIS> {
	enum { use_dual_value = false };
	
	typedef int value_type;
	typedef value_type dual2_type;
	
	template <typename BuilderT>
	static void copy_value(BuilderT &builder, llvm::Value *ptr, llvm::Value *value)
	{
		using namespace llvm;
		/* TODO */
		BLI_assert(false);
		UNUSED_VARS(builder, ptr, value);
	}
	
	template <typename BuilderT>
	static void set_zero(BuilderT &builder, llvm::Value *ptr)
	{
		using namespace llvm;
		/* TODO */
		BLI_assert(false);
		UNUSED_VARS(builder, ptr);
	}
};

llvm::Type *bvm_get_llvm_type(llvm::LLVMContext &context, const TypeSpec *spec, bool use_dual);
llvm::Type *bvm_get_llvm_type(llvm::LLVMContext &context, BVMType type, bool use_dual);

llvm::Constant *bvm_create_llvm_constant(llvm::LLVMContext &context, const NodeConstant *node_value);
bool bvm_type_has_dual_value(const TypeSpec *spec);

void bvm_llvm_set_zero(llvm::LLVMContext &context, llvm::BasicBlock *block,
                       llvm::Value *ptr,
                       const TypeSpec *spec);
void bvm_llvm_copy_value(llvm::LLVMContext &context, llvm::BasicBlock *block,
                         llvm::Value *ptr, llvm::Value *value,
                         const TypeSpec *spec);

} /* namespace blenvm */

#endif /* __LLVM_TYPES_H__ */

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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenjit/BJIT_types.h
 *  \ingroup bjit
 */

#ifndef __BJIT_TYPES_H__
#define __BJIT_TYPES_H__

#ifdef BJIT_RUNTIME
#include "bjit_intern.h"
#else
#include <iostream>

#include "bjit_llvm.h"
#endif

#include "bjit_util_math.h"

namespace bjit {

using namespace llvm;

#ifndef BJIT_RUNTIME

typedef enum {
	BJIT_TYPE_FLOAT,
	BJIT_TYPE_INT,
	BJIT_TYPE_VEC3,
	BJIT_TYPE_MAT4,
	
	BJIT_NUMTYPES
} SocketTypeID;

inline Type *bjit_get_socket_llvm_type(SocketTypeID type, LLVMContext &context, Module *module);
template <typename T> Constant *bjit_get_socket_llvm_constant(SocketTypeID type, T value, LLVMContext &context);
inline Value *bjit_get_socket_llvm_argument(SocketTypeID type, Value *value, IRBuilder<> &builder);

/* ------------------------------------------------------------------------- */

template <SocketTypeID type>
struct SocketTypeImpl;

template <> struct SocketTypeImpl<BJIT_TYPE_FLOAT> {
	typedef FP type;
	typedef float extern_type;
	typedef float extern_type_arg;
	
	static Type *get_llvm_type(LLVMContext &context, Module */*module*/)
	{
		return TypeBuilder<types::ieee_float, true>::get(context);
	}
	
	static Constant *create_constant(float value, LLVMContext &context)
	{
		return ConstantFP::get(context, APFloat(value));
	}
	
	template <typename T>
	static Constant *create_constant(T /*value*/, LLVMContext &/*context*/)
	{
		return NULL;
	}
	
	static Value *as_argument(IRBuilder<> &builder, Value *value)
	{
		return builder.CreateLoad(value);
	}
};

template <> struct SocketTypeImpl<BJIT_TYPE_INT> {
	typedef typename types::i<32> type;
	typedef int32_t extern_type;
	typedef int32_t extern_type_arg;
	
	static Type *get_llvm_type(LLVMContext &context, Module */*module*/)
	{
		return TypeBuilder<types::i<32>, true>::get(context);
	}
	
	static Constant *create_constant(int value, LLVMContext &context)
	{
		return ConstantInt::get(context, APInt(32, value));
	}
	
	template <typename T>
	static Constant *create_constant(T /*value*/, LLVMContext &/*context*/)
	{
		return NULL;
	}
	
	static Value *as_argument(IRBuilder<> &builder, Value *value)
	{
		return builder.CreateLoad(value);
	}
};

template <> struct SocketTypeImpl<BJIT_TYPE_VEC3> {
	typedef vec3_t type;
	typedef float extern_type[3];
	typedef const float extern_type_arg[3];
	
	static Type *get_llvm_type(LLVMContext &/*context*/, Module *module)
	{
		Type *type = module->getTypeByName("struct.bjit::Vec3Type");
//		Type *type = TypeBuilder<vec3_t, true>::get(context);
		type->dump();
		return type;
	}
	
	static Constant *create_constant(const vec3_t &value, LLVMContext &context)
	{
		return ConstantDataVector::get(context, ArrayRef<float>(value, 3));
	}
	
	static Constant *create_constant(const float *value, LLVMContext &context)
	{
		return ConstantDataVector::get(context, ArrayRef<float>(value, 3));
	}
	
	template <typename T>
	static Constant *create_constant(T /*value*/, LLVMContext &/*context*/)
	{
		return NULL;
	}
	
	static Value *as_argument(IRBuilder<> &/*builder*/, Value *value)
	{
		return value;
	}
};

template <> struct SocketTypeImpl<BJIT_TYPE_MAT4> {
	typedef mat4_t type;
	typedef float extern_type[4][4];
	typedef const float extern_type_arg[4][4];
	
	static Type *get_llvm_type(LLVMContext &context, Module */*module*/)
	{
		return TypeBuilder<mat4_t, true>::get(context);
	}
	
	static Constant *create_constant(const float *value, LLVMContext &context)
	{
		Constant *data[4];
		for (int i = 0; i < 4; ++i)
			data[i] = ConstantDataArray::get(context, ArrayRef<float>(value + i*4, 4));
		return ConstantArray::get(ArrayType::get(data[0]->getType(), 4), ArrayRef<Constant*>(data, 4));
	}
	
	static Constant *create_constant(const float (*value)[4], LLVMContext &context)
	{
		Constant *data[4];
		for (int i = 0; i < 4; ++i)
			data[i] = ConstantDataArray::get(context, ArrayRef<float>(value[i], 4));;
		return ConstantArray::get(ArrayType::get(data[0]->getType(), 4), ArrayRef<Constant*>(data, 4));
	}
	
	template <typename T>
	static Constant *create_constant(T /*value*/, LLVMContext &/*context*/)
	{
		return NULL;
	}
	
	static Value *as_argument(IRBuilder<> &builder, Value *value)
	{
		Type *type = TypeBuilder<types::ieee_float (*)[4], true>::get(builder.getContext());
		value = builder.CreatePointerCast(value, type);
		return value;
	}
};

/* ========================================================================= */
/* inline functions */

namespace internal {
} /* namespace internal */

#define FOREACH_SOCKET_TYPE(type, eval, result) \
	switch (type) { \
		case BJIT_TYPE_FLOAT:   result = SocketTypeImpl<BJIT_TYPE_FLOAT>::eval; break; \
		case BJIT_TYPE_INT:     result = SocketTypeImpl<BJIT_TYPE_INT>::eval; break; \
		case BJIT_TYPE_VEC3:    result = SocketTypeImpl<BJIT_TYPE_VEC3>::eval; break; \
		case BJIT_TYPE_MAT4:    result = SocketTypeImpl<BJIT_TYPE_MAT4>::eval; break; \
		case BJIT_NUMTYPES: break; \
		default: assert(false); break; \
	}

Type *bjit_get_socket_llvm_type(SocketTypeID type, LLVMContext &context, Module *module)
{
	Type *result = NULL;
	FOREACH_SOCKET_TYPE(type, get_llvm_type(context, module), result);
	return result;
}

template <typename T>
Constant *bjit_get_socket_llvm_constant(SocketTypeID type, T value, LLVMContext &context)
{
	Constant *result = NULL;
	FOREACH_SOCKET_TYPE(type, create_constant(value, context), result);
	return result;
}

Value *bjit_get_socket_llvm_argument(SocketTypeID type, Value *value, IRBuilder<> &builder)
{
	Value *result = NULL;
	FOREACH_SOCKET_TYPE(type, as_argument(builder, value), result);
	return result;
}

#undef FOREACH_SOCKET_TYPE

#endif /* BJIT_RUNTIME */

/* ========================================================================= */

typedef enum EffectorEvalSettings_Flag {
	EFF_FIELD_USE_MIN               = (1 << 0),
	EFF_FIELD_USE_MAX               = (1 << 1),
	EFF_FIELD_USE_MIN_RAD           = (1 << 2),
	EFF_FIELD_USE_MAX_RAD           = (1 << 3),
} EffectorEvalSettings_Flag;

typedef enum EffectorEvalSettings_FalloffType {
	EFF_FIELD_FALLOFF_SPHERE        = 0,
	EFF_FIELD_FALLOFF_TUBE          = 1,
	EFF_FIELD_FALLOFF_CONE          = 2,
} EffectorEvalSettings_FalloffType;

typedef enum EffectorEvalSettings_Shape{
	EFF_FIELD_SHAPE_POINT           = 0,
	EFF_FIELD_SHAPE_PLANE           = 1,
	EFF_FIELD_SHAPE_SURFACE         = 2,
	EFF_FIELD_SHAPE_POINTS          = 3,
} EffectorEvalSettings_Shape;

} /* namespace bjit */

#endif

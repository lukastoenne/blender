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

/** \file blender/blenvm/intern/bvm_type_traits.h
 *  \ingroup bvm
 */

#ifndef __BVM_TYPE_TRAITS_H__
#define __BVM_TYPE_TRAITS_H__

#include <cassert>

#include "BVM_types.h"

namespace bvm {

struct Value;


struct float3 {
	float3()
	{}
	
	float3(float x, float y, float z) :
	    x(x), y(y), z(z)
	{}
	
	float& operator[] (int index)
	{
		return ((float*)(&x))[index];
	}
	float operator[] (int index) const
	{
		return ((float*)(&x))[index];
	}
	
	float x;
	float y;
	float z;
};


template <BVMType type>
struct BaseTypeTraits;

template <>
struct BaseTypeTraits<BVM_FLOAT> {
	typedef float POD;
	
	enum eStackSize { stack_size = 1 };
	
	static inline void copy(float *to, const float *from)
	{
		*to = *from;
	}
};

template <>
struct BaseTypeTraits<BVM_FLOAT3> {
	typedef float3 POD;
	
	enum eStackSize { stack_size = 3 };
	
	static inline void copy(POD *to, const POD *from)
	{
		*to = *from;
	}
};

/* ------------------------------------------------------------------------- */

struct TypeDesc {
	TypeDesc(BVMType base_type) :
	    base_type(base_type)
	{}
	
	inline bool assignable(const TypeDesc &other) const;
	
	BVMType base_type;
	
	inline int stack_size() const;
	inline void copy_value(void *to, const void *from) const;
};

/* ------------------------------------------------------------------------- */

struct Value {
	template <typename T>
	static Value *create(BVMType type, T data);
	
	virtual ~Value()
	{}
	
	const TypeDesc &typedesc() const { return m_typedesc; }
	
	template <typename T>
	bool get(T *data) const;
	
protected:
	Value(const TypeDesc &typedesc) :
	    m_typedesc(typedesc)
	{}
	
	TypeDesc m_typedesc;
};

template <BVMType type>
struct ValueType : public Value {
	typedef BaseTypeTraits<type> traits;
	typedef typename traits::POD POD;
	
	ValueType(typename traits::POD data) :
	    Value(TypeDesc(type)),
	    m_data(data)
	{}
	
	template <typename T>
	ValueType(T data) :
	    Value(TypeDesc(type))
	{ (void)data; }
	
	const POD &data() const { return m_data; }
	
	bool get(POD *data) const
	{
		*data = m_data;
		return true;
	}
	
	template <typename T>
	bool get(T *data) const
	{
		assert(!"Data type mismatch");
		(void)data;
		return false;
	}
	
private:
	typename traits::POD m_data;
};

/* ========================================================================= */

template <typename T>
Value *Value::create(BVMType type, T data)
{
	switch (type) {
		case BVM_FLOAT: return new ValueType<BVM_FLOAT>(data);
		case BVM_FLOAT3: return new ValueType<BVM_FLOAT3>(data);
	}
	return 0;
}

template <typename T>
bool Value::get(T *data) const
{
	switch (m_typedesc.base_type) {
		case BVM_FLOAT: return static_cast< const ValueType<BVM_FLOAT>* >(this)->get(data);
		case BVM_FLOAT3: return static_cast< const ValueType<BVM_FLOAT3>* >(this)->get(data);
	}
	return false;
}

/* ------------------------------------------------------------------------- */

bool TypeDesc::assignable(const TypeDesc &other) const
{
	// TODO
	return base_type == other.base_type;
}

int TypeDesc::stack_size() const
{
	switch (base_type) {
		case BVM_FLOAT: return BaseTypeTraits<BVM_FLOAT>::stack_size;
		case BVM_FLOAT3: return BaseTypeTraits<BVM_FLOAT3>::stack_size;
	}
	return 0;
}

void TypeDesc::copy_value(void *to, const void *from) const
{
	#define COPY_TYPE(a, b, type) \
	BaseTypeTraits<type>::copy((typename BaseTypeTraits<type>::POD*)to, (typename BaseTypeTraits<type>::POD const *)from);
	
	switch (base_type) {
		case BVM_FLOAT: COPY_TYPE(to, from, BVM_FLOAT);
		case BVM_FLOAT3: COPY_TYPE(to, from, BVM_FLOAT3);
	}
	
	#undef COPY_TYPE
}

} /* namespace bvm */

#endif

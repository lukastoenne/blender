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

/** \file blender/blenvm/intern/bvm_typedesc.h
 *  \ingroup bvm
 */

#ifndef __BVM_TYPEDESC_H__
#define __BVM_TYPEDESC_H__

#include <cassert>
#include <vector>

extern "C" {
#include "RNA_access.h"
}

#include "BVM_types.h"
#include "bvm_util_data_ptr.h"
#include "bvm_util_math.h"

namespace bvm {

struct NodeGraph;
struct Value;

template <BVMType type>
struct BaseTypeTraits;

template <>
struct BaseTypeTraits<BVM_FLOAT> {
	typedef float POD;
	
	enum eStackSize { stack_size = 1 };
	
	static inline void copy(POD *to, const POD *from)
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

template <>
struct BaseTypeTraits<BVM_FLOAT4> {
	typedef float4 POD;
	
	enum eStackSize { stack_size = 4 };
	
	static inline void copy(POD *to, const POD *from)
	{
		*to = *from;
	}
};

template <>
struct BaseTypeTraits<BVM_INT> {
	typedef int POD;
	
	enum eStackSize { stack_size = 1 };
	
	static inline void copy(POD *to, const POD *from)
	{
		*to = *from;
	}
};

template <>
struct BaseTypeTraits<BVM_MATRIX44> {
	typedef matrix44 POD;
	
	enum eStackSize { stack_size = 16 };
	
	static inline void copy(POD *to, const POD *from)
	{
		*to = *from;
	}
};

template <>
struct BaseTypeTraits<BVM_STRING> {
	typedef const char* POD;
	
	enum eStackSize { stack_size = 2 };
	
	static inline void copy(POD *to, const POD *from)
	{
		*to = *from;
	}
};

template <>
struct BaseTypeTraits<BVM_POINTER> {
	typedef PointerRNA POD;
	
	enum eStackSize { stack_size = 6 };
	
	static inline void copy(POD *to, const POD *from)
	{
		*to = *from;
	}
};

template <>
struct BaseTypeTraits<BVM_MESH> {
	typedef mesh_ptr POD;
	
	enum eStackSize { stack_size = 8 };
	
	static inline void copy(POD *to, const POD *from)
	{
		*to = *from;
	}
};

template <>
struct BaseTypeTraits<BVM_DUPLIS> {
	typedef duplis_ptr POD;
	
	enum eStackSize { stack_size = 8 };
	
	static inline void copy(POD *to, const POD *from)
	{
		*to = *from;
	}
};

/* ------------------------------------------------------------------------- */

template <BVMType type>
struct const_array {
	typedef BaseTypeTraits<type> traits;
	typedef typename traits::POD POD;
	
	const_array(const POD *data, size_t size) :
	    m_data(data),
	    m_size(size)
	{}
	
	~const_array()
	{}
	
	const POD *data() const { return m_data; }
	
	const POD& operator [] (size_t index)
	{
		return m_data[index];
	}
	
private:
	POD *m_data;
	size_t m_size;
};

template <BVMType type>
struct array {
	typedef BaseTypeTraits<type> traits;
	typedef typename traits::POD POD;
	
	array() :
	    m_data(NULL),
	    m_size(0)
	{}
	
	array(POD *data, size_t size) :
	    m_data(data),
	    m_size(size)
	{}
	
	~array()
	{}
	
	operator const_array<type>() const
	{
		return const_array<type>(m_data, m_size);
	}
	
	POD *data() const { return m_data; }
	
	POD& operator [] (size_t index)
	{
		return m_data[index];
	}
	
private:
	POD *m_data;
	size_t m_size;
};

/* ------------------------------------------------------------------------- */

struct TypeDesc {
	explicit TypeDesc(BVMType base_type, BVMBufferType buffer_type = BVM_BUFFER_SINGLE) :
	    base_type(base_type),
	    buffer_type(buffer_type)
	{}
	
	BVMType base_type;
	BVMBufferType buffer_type;
	
	inline bool assignable(const TypeDesc &other) const;
	
	inline int stack_size() const;
	inline void copy_value(void *to, const void *from) const;
};

/* ------------------------------------------------------------------------- */

struct Value {
	template <typename T>
	static Value *create(const TypeDesc &typedesc, T *data, size_t size);
	template <typename T>
	static Value *create(const TypeDesc &typedesc, T data);
	
	virtual ~Value()
	{}
	
	const TypeDesc &typedesc() const { return m_typedesc; }
	
	template <BVMType type>
	bool get(array<type> *data) const;
	
	template <typename T>
	bool get(T *data) const;
	
	virtual Value *copy() const = 0;
	
protected:
	Value(const TypeDesc &typedesc) :
	    m_typedesc(typedesc)
	{}
	
	TypeDesc m_typedesc;
};

template <BVMType type>
struct SingleValue : public Value {
	typedef BaseTypeTraits<type> traits;
	typedef typename traits::POD POD;
	
	SingleValue(typename traits::POD data) :
	    Value(TypeDesc(type)),
	    m_data(data)
	{}
	
	template <typename T>
	SingleValue(T data) :
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
	
	Value *copy() const
	{
		return new SingleValue<type>(m_data);
	}
	
private:
	POD m_data;
};

template <BVMType type>
struct ArrayValue : public Value {
	typedef BaseTypeTraits<type> traits;
	typedef typename traits::POD POD;
	typedef array<type> array_t;
	typedef const_array<type> const_array_t;
	
	ArrayValue(const array_t &data) :
	    Value(TypeDesc(type, BVM_BUFFER_ARRAY)),
	    m_data(data)
	{}
	
	ArrayValue(POD *data, size_t size) :
	    Value(TypeDesc(type, BVM_BUFFER_ARRAY)),
	    m_data(array_t(data, size))
	{}
	
	template <typename T>
	ArrayValue(T data) :
	    Value(TypeDesc(type, BVM_BUFFER_ARRAY))
	{ (void)data; }
	
	template <typename T>
	ArrayValue(T *data, size_t size) :
	    Value(TypeDesc(type, BVM_BUFFER_ARRAY))
	{ (void)data; (void)size; }
	
	const array_t &data() const { return m_data; }
	
	bool get(array_t *data) const
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
	
	Value *copy() const
	{
		return new ArrayValue<type>(m_data);
	}
	
private:
	array_t m_data;
};

/* ========================================================================= */

template <typename T>
static Value *create(const TypeDesc &typedesc, T *data, size_t size)
{
	if (typedesc.buffer_type == BVM_BUFFER_ARRAY) {
		switch (typedesc.base_type) {
			case BVM_FLOAT: return new ArrayValue<BVM_FLOAT>(data, size);
			case BVM_FLOAT3: return new ArrayValue<BVM_FLOAT3>(data, size);
			case BVM_FLOAT4: return new ArrayValue<BVM_FLOAT4>(data, size);
			case BVM_INT: return new ArrayValue<BVM_INT>(data, size);
			case BVM_MATRIX44: return new ArrayValue<BVM_MATRIX44>(data, size);
			case BVM_STRING: return new ArrayValue<BVM_STRING>(data, size);
			case BVM_POINTER: return new ArrayValue<BVM_POINTER>(data, size);
			case BVM_MESH: return new ArrayValue<BVM_MESH>(data, size);
			case BVM_DUPLIS: return new ArrayValue<BVM_DUPLIS>(data, size);
		}
	}
	return NULL;
}

template <typename T>
Value *Value::create(const TypeDesc &typedesc, T data)
{
	if (typedesc.buffer_type == BVM_BUFFER_SINGLE) {
		switch (typedesc.base_type) {
			case BVM_FLOAT: return new SingleValue<BVM_FLOAT>(data);
			case BVM_FLOAT3: return new SingleValue<BVM_FLOAT3>(data);
			case BVM_FLOAT4: return new SingleValue<BVM_FLOAT4>(data);
			case BVM_INT: return new SingleValue<BVM_INT>(data);
			case BVM_MATRIX44: return new SingleValue<BVM_MATRIX44>(data);
			case BVM_STRING: return new SingleValue<BVM_STRING>(data);
			case BVM_POINTER: return new SingleValue<BVM_POINTER>(data);
			case BVM_MESH: return new SingleValue<BVM_MESH>(data);
			case BVM_DUPLIS: return new SingleValue<BVM_DUPLIS>(data);
		}
	}
	else if (typedesc.buffer_type == BVM_BUFFER_ARRAY) {
		switch (typedesc.base_type) {
			case BVM_FLOAT: return new ArrayValue<BVM_FLOAT>(data);
			case BVM_FLOAT3: return new ArrayValue<BVM_FLOAT3>(data);
			case BVM_FLOAT4: return new ArrayValue<BVM_FLOAT4>(data);
			case BVM_INT: return new ArrayValue<BVM_INT>(data);
			case BVM_MATRIX44: return new ArrayValue<BVM_MATRIX44>(data);
			case BVM_STRING: return new ArrayValue<BVM_STRING>(data);
			case BVM_POINTER: return new ArrayValue<BVM_POINTER>(data);
			case BVM_MESH: return new ArrayValue<BVM_MESH>(data);
			case BVM_DUPLIS: return new ArrayValue<BVM_DUPLIS>(data);
		}
	}
	return NULL;
}


template <BVMType type>
bool Value::get(array<type> *data) const
{
	if (m_typedesc.buffer_type == BVM_BUFFER_ARRAY) {
		switch (m_typedesc.base_type) {
			case BVM_FLOAT: return static_cast< const ArrayValue<BVM_FLOAT>* >(this)->get(data);
			case BVM_FLOAT3: return static_cast< const ArrayValue<BVM_FLOAT3>* >(this)->get(data);
			case BVM_FLOAT4: return static_cast< const ArrayValue<BVM_FLOAT4>* >(this)->get(data);
			case BVM_INT: return static_cast< const ArrayValue<BVM_INT>* >(this)->get(data);
			case BVM_MATRIX44: return static_cast< const ArrayValue<BVM_MATRIX44>* >(this)->get(data);
			case BVM_STRING: return static_cast< const ArrayValue<BVM_STRING>* >(this)->get(data);
			case BVM_POINTER: return static_cast< const ArrayValue<BVM_POINTER>* >(this)->get(data);
			case BVM_MESH: return static_cast< const ArrayValue<BVM_MESH>* >(this)->get(data);
			case BVM_DUPLIS: return static_cast< const ArrayValue<BVM_DUPLIS>* >(this)->get(data);
		}
	}
	return false;
}

template <typename T>
bool Value::get(T *data) const
{
	if (m_typedesc.buffer_type == BVM_BUFFER_SINGLE) {
		switch (m_typedesc.base_type) {
			case BVM_FLOAT: return static_cast< const SingleValue<BVM_FLOAT>* >(this)->get(data);
			case BVM_FLOAT3: return static_cast< const SingleValue<BVM_FLOAT3>* >(this)->get(data);
			case BVM_FLOAT4: return static_cast< const SingleValue<BVM_FLOAT4>* >(this)->get(data);
			case BVM_INT: return static_cast< const SingleValue<BVM_INT>* >(this)->get(data);
			case BVM_MATRIX44: return static_cast< const SingleValue<BVM_MATRIX44>* >(this)->get(data);
			case BVM_STRING: return static_cast< const SingleValue<BVM_STRING>* >(this)->get(data);
			case BVM_POINTER: return static_cast< const SingleValue<BVM_POINTER>* >(this)->get(data);
			case BVM_MESH: return static_cast< const SingleValue<BVM_MESH>* >(this)->get(data);
			case BVM_DUPLIS: return static_cast< const SingleValue<BVM_DUPLIS>* >(this)->get(data);
		}
	}
	return false;
}

/* ------------------------------------------------------------------------- */

bool TypeDesc::assignable(const TypeDesc &other) const
{
	return base_type == other.base_type && buffer_type == other.buffer_type;
}

int TypeDesc::stack_size() const
{
	switch (buffer_type) {
		case BVM_BUFFER_SINGLE:
			switch (base_type) {
				case BVM_FLOAT: return BaseTypeTraits<BVM_FLOAT>::stack_size;
				case BVM_FLOAT3: return BaseTypeTraits<BVM_FLOAT3>::stack_size;
				case BVM_FLOAT4: return BaseTypeTraits<BVM_FLOAT4>::stack_size;
				case BVM_INT: return BaseTypeTraits<BVM_INT>::stack_size;
				case BVM_MATRIX44: return BaseTypeTraits<BVM_MATRIX44>::stack_size;
				case BVM_STRING: return BaseTypeTraits<BVM_STRING>::stack_size;
				case BVM_POINTER: return BaseTypeTraits<BVM_POINTER>::stack_size;
				case BVM_MESH: return BaseTypeTraits<BVM_MESH>::stack_size;
				case BVM_DUPLIS: return BaseTypeTraits<BVM_DUPLIS>::stack_size;
			}
			break;
		case BVM_BUFFER_ARRAY:
			return 4;
	}
	
	return 0;
}

void TypeDesc::copy_value(void *to, const void *from) const
{
	
	switch (buffer_type) {
		case BVM_BUFFER_SINGLE:
			#define COPY_TYPE(a, b, type) \
			BaseTypeTraits<type>::copy((BaseTypeTraits<type>::POD*)(a), (BaseTypeTraits<type>::POD const *)(b));
			switch (base_type) {
				case BVM_FLOAT: COPY_TYPE(to, from, BVM_FLOAT); break;
				case BVM_FLOAT3: COPY_TYPE(to, from, BVM_FLOAT3); break;
				case BVM_FLOAT4: COPY_TYPE(to, from, BVM_FLOAT4); break;
				case BVM_INT: COPY_TYPE(to, from, BVM_INT); break;
				case BVM_MATRIX44: COPY_TYPE(to, from, BVM_MATRIX44); break;
				case BVM_STRING: COPY_TYPE(to, from, BVM_STRING); break;
				case BVM_POINTER: COPY_TYPE(to, from, BVM_POINTER); break;
				case BVM_MESH: COPY_TYPE(to, from, BVM_MESH); break;
				case BVM_DUPLIS: COPY_TYPE(to, from, BVM_DUPLIS); break;
			}
			#undef COPY_TYPE
			break;
		case BVM_BUFFER_ARRAY:
			#define COPY_TYPE(a, b, type) \
			*(array<type> *)(a) = *(array<type> const *)(b);
			switch (base_type) {
				case BVM_FLOAT: COPY_TYPE(to, from, BVM_FLOAT); break;
				case BVM_FLOAT3: COPY_TYPE(to, from, BVM_FLOAT3); break;
				case BVM_FLOAT4: COPY_TYPE(to, from, BVM_FLOAT4); break;
				case BVM_INT: COPY_TYPE(to, from, BVM_INT); break;
				case BVM_MATRIX44: COPY_TYPE(to, from, BVM_MATRIX44); break;
				case BVM_STRING: COPY_TYPE(to, from, BVM_STRING); break;
				case BVM_POINTER: COPY_TYPE(to, from, BVM_POINTER); break;
				case BVM_MESH: COPY_TYPE(to, from, BVM_MESH); break;
				case BVM_DUPLIS: COPY_TYPE(to, from, BVM_DUPLIS); break;
			}
			#undef COPY_TYPE
			break;
	}
}

} /* namespace bvm */

#endif

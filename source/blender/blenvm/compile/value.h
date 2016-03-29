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

/** \file blender/blenvm/intern/bvm_value.h
 *  \ingroup bvm
 */

#ifndef __BVM_VALUE_H__
#define __BVM_VALUE_H__

#include <vector>

#include "bvm_typedesc.h" /* XXX bad level include, values should be agnostic to runtime backend! */

#include "util_math.h"
#include "util_string.h"

namespace blenvm {

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
	
	const POD& operator [] (size_t index) const
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

template <BVMType type>
struct const_image {
	typedef BaseTypeTraits<type> traits;
	typedef typename traits::POD POD;
	
	const_image(const POD *data, size_t width, size_t height) :
	    m_data(data),
	    m_width(width),
	    m_height(height)
	{}
	
	~const_image()
	{}
	
	const POD *data() const { return m_data; }
	
	const POD& get(size_t x, size_t y) const
	{
		return m_data[x + y * m_width];
	}
	
private:
	POD *m_data;
	size_t m_width;
	size_t m_height;
};

template <BVMType type>
struct image {
	typedef BaseTypeTraits<type> traits;
	typedef typename traits::POD POD;
	
	image() :
	    m_data(NULL),
	    m_width(0),
	    m_height(0)
	{}
	
	image(const POD *data, size_t width, size_t height) :
	    m_data(data),
	    m_width(width),
	    m_height(height)
	{}
	
	~image()
	{}
	
	operator const_image<type>() const
	{
		return const_image<type>(m_data, m_width, m_height);
	}
	
	POD *data() const { return m_data; }
	
	POD& get(size_t x, size_t y)
	{
		return m_data[x + y * m_width];
	}
	
private:
	POD *m_data;
	size_t m_width;
	size_t m_height;
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

template <BVMType type>
struct ImageValue : public Value {
	typedef BaseTypeTraits<type> traits;
	typedef typename traits::POD POD;
	typedef image<type> image_t;
	typedef const_image<type> const_image_t;
	
	ImageValue(const image_t &data) :
	    Value(TypeDesc(type, BVM_BUFFER_ARRAY)),
	    m_data(data)
	{}
	
	ImageValue(POD *data, size_t width, size_t height) :
	    Value(TypeDesc(type, BVM_BUFFER_ARRAY)),
	    m_data(image_t(data, width, height))
	{}
	
	template <typename T>
	ImageValue(T data) :
	    Value(TypeDesc(type, BVM_BUFFER_ARRAY))
	{ (void)data; }
	
	template <typename T>
	ImageValue(T *data, size_t width, size_t height) :
	    Value(TypeDesc(type, BVM_BUFFER_ARRAY))
	{ (void)data; (void)width; (void)height; }
	
	const image_t &data() const { return m_data; }
	
	bool get(image_t *data) const
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
		return new ImageValue<type>(m_data);
	}
	
private:
	image_t m_data;
};

/* ========================================================================= */

template <typename T>
static Value *create(const TypeDesc &typedesc, T *data, size_t size)
{
	if (typedesc.buffer_type() == BVM_BUFFER_ARRAY) {
		switch (typedesc.base_type()) {
			case BVM_FLOAT: return new ArrayValue<BVM_FLOAT>(data, size);
			case BVM_FLOAT3: return new ArrayValue<BVM_FLOAT3>(data, size);
			case BVM_FLOAT4: return new ArrayValue<BVM_FLOAT4>(data, size);
			case BVM_INT: return new ArrayValue<BVM_INT>(data, size);
			case BVM_MATRIX44: return new ArrayValue<BVM_MATRIX44>(data, size);
			case BVM_STRING: return new ArrayValue<BVM_STRING>(data, size);
			case BVM_RNAPOINTER: return new ArrayValue<BVM_RNAPOINTER>(data, size);
			case BVM_MESH: return new ArrayValue<BVM_MESH>(data, size);
			case BVM_DUPLIS: return new ArrayValue<BVM_DUPLIS>(data, size);
		}
	}
	return NULL;
}

template <typename T>
Value *Value::create(const TypeDesc &typedesc, T data)
{
	if (typedesc.buffer_type() == BVM_BUFFER_SINGLE) {
		switch (typedesc.base_type()) {
			case BVM_FLOAT: return new SingleValue<BVM_FLOAT>(data);
			case BVM_FLOAT3: return new SingleValue<BVM_FLOAT3>(data);
			case BVM_FLOAT4: return new SingleValue<BVM_FLOAT4>(data);
			case BVM_INT: return new SingleValue<BVM_INT>(data);
			case BVM_MATRIX44: return new SingleValue<BVM_MATRIX44>(data);
			case BVM_STRING: return new SingleValue<BVM_STRING>(data);
			case BVM_RNAPOINTER: return new SingleValue<BVM_RNAPOINTER>(data);
			case BVM_MESH: return new SingleValue<BVM_MESH>(data);
			case BVM_DUPLIS: return new SingleValue<BVM_DUPLIS>(data);
		}
	}
	else if (typedesc.buffer_type() == BVM_BUFFER_ARRAY) {
		switch (typedesc.base_type()) {
			case BVM_FLOAT: return new ArrayValue<BVM_FLOAT>(data);
			case BVM_FLOAT3: return new ArrayValue<BVM_FLOAT3>(data);
			case BVM_FLOAT4: return new ArrayValue<BVM_FLOAT4>(data);
			case BVM_INT: return new ArrayValue<BVM_INT>(data);
			case BVM_MATRIX44: return new ArrayValue<BVM_MATRIX44>(data);
			case BVM_STRING: return new ArrayValue<BVM_STRING>(data);
			case BVM_RNAPOINTER: return new ArrayValue<BVM_RNAPOINTER>(data);
			case BVM_MESH: return new ArrayValue<BVM_MESH>(data);
			case BVM_DUPLIS: return new ArrayValue<BVM_DUPLIS>(data);
		}
	}
	return NULL;
}


template <BVMType type>
bool Value::get(array<type> *data) const
{
	if (m_typedesc.buffer_type() == BVM_BUFFER_ARRAY) {
		switch (m_typedesc.base_type()) {
			case BVM_FLOAT: return static_cast< const ArrayValue<BVM_FLOAT>* >(this)->get(data);
			case BVM_FLOAT3: return static_cast< const ArrayValue<BVM_FLOAT3>* >(this)->get(data);
			case BVM_FLOAT4: return static_cast< const ArrayValue<BVM_FLOAT4>* >(this)->get(data);
			case BVM_INT: return static_cast< const ArrayValue<BVM_INT>* >(this)->get(data);
			case BVM_MATRIX44: return static_cast< const ArrayValue<BVM_MATRIX44>* >(this)->get(data);
			case BVM_STRING: return static_cast< const ArrayValue<BVM_STRING>* >(this)->get(data);
			case BVM_RNAPOINTER: return static_cast< const ArrayValue<BVM_RNAPOINTER>* >(this)->get(data);
			case BVM_MESH: return static_cast< const ArrayValue<BVM_MESH>* >(this)->get(data);
			case BVM_DUPLIS: return static_cast< const ArrayValue<BVM_DUPLIS>* >(this)->get(data);
		}
	}
	return false;
}

template <typename T>
bool Value::get(T *data) const
{
	if (m_typedesc.buffer_type() == BVM_BUFFER_SINGLE) {
		switch (m_typedesc.base_type()) {
			case BVM_FLOAT: return static_cast< const SingleValue<BVM_FLOAT>* >(this)->get(data);
			case BVM_FLOAT3: return static_cast< const SingleValue<BVM_FLOAT3>* >(this)->get(data);
			case BVM_FLOAT4: return static_cast< const SingleValue<BVM_FLOAT4>* >(this)->get(data);
			case BVM_INT: return static_cast< const SingleValue<BVM_INT>* >(this)->get(data);
			case BVM_MATRIX44: return static_cast< const SingleValue<BVM_MATRIX44>* >(this)->get(data);
			case BVM_STRING: return static_cast< const SingleValue<BVM_STRING>* >(this)->get(data);
			case BVM_RNAPOINTER: return static_cast< const SingleValue<BVM_RNAPOINTER>* >(this)->get(data);
			case BVM_MESH: return static_cast< const SingleValue<BVM_MESH>* >(this)->get(data);
			case BVM_DUPLIS: return static_cast< const SingleValue<BVM_DUPLIS>* >(this)->get(data);
		}
	}
	return false;
}

} /* namespace bvm */

#endif

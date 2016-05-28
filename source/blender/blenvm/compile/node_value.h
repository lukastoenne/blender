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

/** \file blender/blenvm/intern/node_value.h
 *  \ingroup bvm
 */

#ifndef __BVM_VALUE_H__
#define __BVM_VALUE_H__

#include <vector>

#include "BVM_types.h"

#include "typedesc.h"

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

struct NodeConstant {
	template <typename T>
	static NodeConstant *create(const TypeDesc &typedesc, T *data, size_t size);
	template <typename T>
	static NodeConstant *create(const TypeDesc &typedesc, T data);
	
	virtual ~NodeConstant()
	{}
	
	const TypeDesc &typedesc() const { return m_typedesc; }
	
	template <BVMType type>
	bool get(array<type> *data) const;
	
	template <typename T>
	bool get(T *data) const;
	
	virtual NodeConstant *copy() const = 0;
	
protected:
	NodeConstant(const TypeDesc &typedesc) :
	    m_typedesc(typedesc)
	{}
	
	TypeDesc m_typedesc;
};

template <BVMType type>
struct SingleNodeValue : public NodeConstant {
	typedef BaseTypeTraits<type> traits;
	typedef typename traits::POD POD;
	
	SingleNodeValue(const TypeDesc &td, typename traits::POD data) :
	    NodeConstant(td),
	    m_data(data)
	{}
	
	template <typename T>
	SingleNodeValue(const TypeDesc &td, T data) :
	    NodeConstant(td)
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
	
	NodeConstant *copy() const
	{
		return new SingleNodeValue<type>(m_typedesc, m_data);
	}
	
private:
	POD m_data;
};

template <BVMType type>
struct ArrayNodeValue : public NodeConstant {
	typedef BaseTypeTraits<type> traits;
	typedef typename traits::POD POD;
	typedef array<type> array_t;
	typedef const_array<type> const_array_t;
	
	ArrayNodeValue(const TypeDesc &td, const array_t &data) :
	    NodeConstant(td),
	    m_data(data)
	{}
	
	ArrayNodeValue(const TypeDesc &td, POD *data, size_t size) :
	    NodeConstant(td),
	    m_data(array_t(data, size))
	{}
	
	template <typename T>
	ArrayNodeValue(const TypeDesc &td, T data) :
	    NodeConstant(td)
	{ (void)data; }
	
	template <typename T>
	ArrayNodeValue(const TypeDesc &td, T *data, size_t size) :
	    NodeConstant(td)
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
	
	NodeConstant *copy() const
	{
		return new ArrayNodeValue<type>(m_typedesc, m_data);
	}
	
private:
	array_t m_data;
};

template <BVMType type>
struct ImageNodeValue : public NodeConstant {
	typedef BaseTypeTraits<type> traits;
	typedef typename traits::POD POD;
	typedef image<type> image_t;
	typedef const_image<type> const_image_t;
	
	ImageNodeValue(const image_t &data) :
	    NodeConstant(TypeSpec(type, BVM_BUFFER_ARRAY)),
	    m_data(data)
	{}
	
	ImageNodeValue(POD *data, size_t width, size_t height) :
	    NodeConstant(TypeDesc(type, BVM_BUFFER_ARRAY)),
	    m_data(image_t(data, width, height))
	{}
	
	template <typename T>
	ImageNodeValue(T data) :
	    NodeConstant(TypeDesc(type, BVM_BUFFER_ARRAY))
	{ (void)data; }
	
	template <typename T>
	ImageNodeValue(T *data, size_t width, size_t height) :
	    NodeConstant(TypeDesc(type, BVM_BUFFER_ARRAY))
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
	
	NodeConstant *copy() const
	{
		return new ImageNodeValue<type>(m_data);
	}
	
private:
	image_t m_data;
};

/* ========================================================================= */

template <typename T>
static NodeConstant *create(const TypeDesc &td, T *data, size_t size)
{
	const TypeSpec *typespec = td.get_typespec();
	
	if (typespec->buffer_type() == BVM_BUFFER_ARRAY) {
		switch (typespec->base_type()) {
			case BVM_FLOAT: return new ArrayNodeValue<BVM_FLOAT>(data, size);
			case BVM_FLOAT3: return new ArrayNodeValue<BVM_FLOAT3>(data, size);
			case BVM_FLOAT4: return new ArrayNodeValue<BVM_FLOAT4>(data, size);
			case BVM_INT: return new ArrayNodeValue<BVM_INT>(data, size);
			case BVM_MATRIX44: return new ArrayNodeValue<BVM_MATRIX44>(data, size);
			case BVM_STRING: return new ArrayNodeValue<BVM_STRING>(data, size);
			case BVM_RNAPOINTER: return new ArrayNodeValue<BVM_RNAPOINTER>(data, size);
			case BVM_MESH: return new ArrayNodeValue<BVM_MESH>(data, size);
			case BVM_DUPLIS: return new ArrayNodeValue<BVM_DUPLIS>(data, size);
		}
	}
	return NULL;
}

template <typename T>
NodeConstant *NodeConstant::create(const TypeDesc &td, T data)
{
	const TypeSpec *typespec = td.get_typespec();
	
	if (typespec->buffer_type() == BVM_BUFFER_SINGLE) {
		switch (typespec->base_type()) {
			case BVM_FLOAT: return new SingleNodeValue<BVM_FLOAT>(td, data);
			case BVM_FLOAT3: return new SingleNodeValue<BVM_FLOAT3>(td, data);
			case BVM_FLOAT4: return new SingleNodeValue<BVM_FLOAT4>(td, data);
			case BVM_INT: return new SingleNodeValue<BVM_INT>(td, data);
			case BVM_MATRIX44: return new SingleNodeValue<BVM_MATRIX44>(td, data);
			case BVM_STRING: return new SingleNodeValue<BVM_STRING>(td, data);
			case BVM_RNAPOINTER: return new SingleNodeValue<BVM_RNAPOINTER>(td, data);
			case BVM_MESH: return new SingleNodeValue<BVM_MESH>(td, data);
			case BVM_DUPLIS: return new SingleNodeValue<BVM_DUPLIS>(td, data);
		}
	}
	else if (typespec->buffer_type() == BVM_BUFFER_ARRAY) {
		switch (typespec->base_type()) {
			case BVM_FLOAT: return new ArrayNodeValue<BVM_FLOAT>(td, data);
			case BVM_FLOAT3: return new ArrayNodeValue<BVM_FLOAT3>(td, data);
			case BVM_FLOAT4: return new ArrayNodeValue<BVM_FLOAT4>(td, data);
			case BVM_INT: return new ArrayNodeValue<BVM_INT>(td, data);
			case BVM_MATRIX44: return new ArrayNodeValue<BVM_MATRIX44>(td, data);
			case BVM_STRING: return new ArrayNodeValue<BVM_STRING>(td, data);
			case BVM_RNAPOINTER: return new ArrayNodeValue<BVM_RNAPOINTER>(td, data);
			case BVM_MESH: return new ArrayNodeValue<BVM_MESH>(td, data);
			case BVM_DUPLIS: return new ArrayNodeValue<BVM_DUPLIS>(td, data);
		}
	}
	return NULL;
}


template <BVMType type>
bool NodeConstant::get(array<type> *data) const
{
	const TypeSpec *typespec = m_typedesc.get_typespec();
	
	if (typespec->buffer_type() == BVM_BUFFER_ARRAY) {
		switch (typespec->base_type()) {
			case BVM_FLOAT: return static_cast< const ArrayNodeValue<BVM_FLOAT>* >(this)->get(data);
			case BVM_FLOAT3: return static_cast< const ArrayNodeValue<BVM_FLOAT3>* >(this)->get(data);
			case BVM_FLOAT4: return static_cast< const ArrayNodeValue<BVM_FLOAT4>* >(this)->get(data);
			case BVM_INT: return static_cast< const ArrayNodeValue<BVM_INT>* >(this)->get(data);
			case BVM_MATRIX44: return static_cast< const ArrayNodeValue<BVM_MATRIX44>* >(this)->get(data);
			case BVM_STRING: return static_cast< const ArrayNodeValue<BVM_STRING>* >(this)->get(data);
			case BVM_RNAPOINTER: return static_cast< const ArrayNodeValue<BVM_RNAPOINTER>* >(this)->get(data);
			case BVM_MESH: return static_cast< const ArrayNodeValue<BVM_MESH>* >(this)->get(data);
			case BVM_DUPLIS: return static_cast< const ArrayNodeValue<BVM_DUPLIS>* >(this)->get(data);
		}
	}
	return false;
}

template <typename T>
bool NodeConstant::get(T *data) const
{
	const TypeSpec *typespec = m_typedesc.get_typespec();
	
	if (typespec->buffer_type() == BVM_BUFFER_SINGLE) {
		switch (typespec->base_type()) {
			case BVM_FLOAT: return static_cast< const SingleNodeValue<BVM_FLOAT>* >(this)->get(data);
			case BVM_FLOAT3: return static_cast< const SingleNodeValue<BVM_FLOAT3>* >(this)->get(data);
			case BVM_FLOAT4: return static_cast< const SingleNodeValue<BVM_FLOAT4>* >(this)->get(data);
			case BVM_INT: return static_cast< const SingleNodeValue<BVM_INT>* >(this)->get(data);
			case BVM_MATRIX44: return static_cast< const SingleNodeValue<BVM_MATRIX44>* >(this)->get(data);
			case BVM_STRING: return static_cast< const SingleNodeValue<BVM_STRING>* >(this)->get(data);
			case BVM_RNAPOINTER: return static_cast< const SingleNodeValue<BVM_RNAPOINTER>* >(this)->get(data);
			case BVM_MESH: return static_cast< const SingleNodeValue<BVM_MESH>* >(this)->get(data);
			case BVM_DUPLIS: return static_cast< const SingleNodeValue<BVM_DUPLIS>* >(this)->get(data);
		}
	}
	return false;
}

} /* namespace blenvm */

#endif

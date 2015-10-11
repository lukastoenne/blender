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

#include "BVM_types.h"

namespace bvm {

struct Value;

template <BVMType type>
struct BaseTypeTraits;

template <>
struct BaseTypeTraits<BVM_FLOAT> {
	typedef float POD;
};

template <>
struct BaseTypeTraits<BVM_FLOAT3> {
	typedef float POD[3];
};

/* ------------------------------------------------------------------------- */

struct TypeDesc {
	TypeDesc(BVMType base_type) :
	    base_type(base_type)
	{}
	
	BVMType base_type;
};

/* ------------------------------------------------------------------------- */

struct Value {
	template <typename T>
	static Value *create(BVMType type, T data);
	
	virtual ~Value()
	{}
	
	const TypeDesc &typedesc() const { return m_typedesc; }
	
	template <typename T>
	void get(T data) const;
	
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
	ValueType(T data)
	{}
	
	const POD &data() const { return m_data; }
	
	bool get(POD &data)
	{
		data = m_data;
		return true;
	}
	
	template <typename T>
	bool get(T data)
	{
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
		case BVM_FLOAT: return ValueType<BVM_FLOAT>(data);
		case BVM_FLOAT3: return ValueType<BVM_FLOAT3>(data);
	}
}

template <typename T>
void Value::get(T data) const
{
	switch (m_typedesc.base_type) {
		case BVM_FLOAT: return static_cast< ValueType<BVM_FLOAT>* >(this)->get(data);
		case BVM_FLOAT3: return static_cast< ValueType<BVM_FLOAT3>* >(this)->get(data);
	}
}

} /* namespace bvm */

#endif

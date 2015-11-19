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

extern "C" {
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"

#include "RNA_access.h"
}

#include "BVM_types.h"

namespace bvm {

struct Value;

struct float3 {
	float3()
	{}
	
	float3(float x, float y, float z) :
	    x(x), y(y), z(z)
	{}
	
	float* data() { return &x; }
	const float* data() const { return &x; }
	
	inline static float3 from_data(const float *values)
	{
		float3 f;
		f.x = values[0];
		f.y = values[1];
		f.z = values[2];
		return f;
	}
	
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

struct float4 {
	float4()
	{}
	
	float4(float x, float y, float z, float w) :
	    x(x), y(y), z(z), w(w)
	{}
	
	float* data() { return &x; }
	const float* data() const { return &x; }
	
	inline static float4 from_data(const float *values)
	{
		float4 f;
		f.x = values[0];
		f.y = values[1];
		f.z = values[2];
		f.w = values[3];
		return f;
	}
	
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
	float w;
};

struct matrix44 {
	enum Layout {
		COL_MAJOR,
		ROW_MAJOR,
	};
	
	matrix44()
	{}
	
	matrix44(const float3 &x, const float3 &y, const float3 &z, const float3 &t)
	{
		data[0][0] = x.x;	data[1][0] = y.x;	data[2][0] = z.x;	data[3][0] = t.x;
		data[0][1] = x.y;	data[1][1] = y.y;	data[2][1] = z.y;	data[3][1] = t.y;
		data[0][2] = x.z;	data[1][2] = y.z;	data[2][2] = z.z;	data[3][2] = t.z;
		data[0][3] = 0.0f;	data[1][3] = 0.0f;	data[2][3] = 0.0f;	data[3][3] = 1.0f;
	}
	
	matrix44(const float3 &t)
	{
		data[0][0] = 1.0f;	data[1][0] = 0.0f;	data[2][0] = 0.0f;	data[3][0] = t.x;
		data[0][1] = 0.0f;	data[1][1] = 1.0f;	data[2][1] = 0.0f;	data[3][1] = t.y;
		data[0][2] = 0.0f;	data[1][2] = 0.0f;	data[2][2] = 1.0f;	data[3][2] = t.z;
		data[0][3] = 0.0f;	data[1][3] = 0.0f;	data[2][3] = 0.0f;	data[3][3] = 1.0f;
	}
	
	inline static matrix44 from_data(const float *values, Layout layout = COL_MAJOR) {
		matrix44 m;
		if (layout == COL_MAJOR) {
			m.data[0][0] = values[0]; m.data[1][0] = values[1]; m.data[2][0] = values[2]; m.data[3][0] = values[3];
			m.data[0][1] = values[4]; m.data[1][1] = values[5]; m.data[2][1] = values[6]; m.data[3][1] = values[7];
			m.data[0][2] = values[8]; m.data[1][2] = values[9]; m.data[2][2] = values[10]; m.data[3][2] = values[11];
			m.data[0][3] = values[12]; m.data[1][3] = values[13]; m.data[2][3] = values[14]; m.data[3][3] = values[15];
		}
		else {
			m.data[0][0] = values[0]; m.data[1][0] = values[4]; m.data[2][0] = values[8]; m.data[3][0] = values[12];
			m.data[0][1] = values[1]; m.data[1][1] = values[5]; m.data[2][1] = values[9]; m.data[3][1] = values[13];
			m.data[0][2] = values[2]; m.data[1][2] = values[6]; m.data[2][2] = values[10]; m.data[3][2] = values[14];
			m.data[0][3] = values[3]; m.data[1][3] = values[7]; m.data[2][3] = values[11]; m.data[3][3] = values[15];
		}
		return m;
	}
	
	inline static matrix44 identity()
	{
		return matrix44(float3(1.0f, 0.0f, 0.0f),
		                float3(0.0f, 1.0f, 0.0f),
		                float3(0.0f, 0.0f, 1.0f),
		                float3(0.0f, 0.0f, 0.0f));
	}
	
	float data[4][4];
};

/* generic default deletor, using 'delete' operator */
template <typename T>
struct DeleteDestructor {
	static void destroy(T *data)
	{
		delete data;
	}
};

/* reference-counted pointer for managing transient data on the stack */
template <typename T, typename DestructorT = DeleteDestructor<T> >
struct node_data_ptr {
	typedef T element_type;
	typedef node_data_ptr<T> self_type;
	
	node_data_ptr() :
	    m_data(0),
	    m_refs(0)
	{}
	
	explicit node_data_ptr(element_type *data) :
	    m_data(data),
	    m_refs(0)
	{}
	
	~node_data_ptr()
	{
	}
	
	element_type* get() const { return m_data; }
	
	element_type& operator * () const { return *m_data; }
	element_type* operator -> () const { return m_data; }
	
	void set_use_count(size_t use_count)
	{
		assert(m_refs == 0);
		if (use_count > 0)
			m_refs = create_refs(use_count);
	}
	
	void decrement_use_count()
	{
		assert(m_refs != 0 && *m_refs > 0);
		size_t count = --(*m_refs);
		if (count == 0) {
			if (m_data) {
				DestructorT::destroy(m_data);
				m_data = 0;
			}
			if (m_refs) {
				destroy_refs(m_refs);
			}
		}
	}
	
	void clear_use_count()
	{
		if (m_data) {
			DestructorT::destroy(m_data);
			m_data = 0;
		}
		if (m_refs) {
			destroy_refs(m_refs);
		}
	}
	
protected:
	/* TODO this could be handled by a common memory manager with a mempool */
	static size_t *create_refs(size_t use_count) { return new size_t(use_count); }
	static void destroy_refs(size_t *refs) { delete refs; }
	
private:
	T *m_data;
	size_t *m_refs;
};

/* TODO THIS IS IMPORTANT!
 * In the future we will want to manage references to allocated data
 * on the stack in a 'manager'. This is because when cancelling a
 * calculation we want to make sure all temporary data is freed cleanly.
 * The resulting output data from a kernel function must be registered
 * in the manager and all the active storage can be removed in one go
 * if the calculation gets cancelled.
 * 
 * We don't want to leave this registration to the kernel itself though,
 * so it has to happen through another instruction. This instruction has
 * to be placed _before_ the kernel call though, because otherwise canceling
 * could still happen in between. Since the actual pointer is still set by
 * the kernel function, this means the manager has to keep a double pointer.
 */
#if 0
template <typename ptr_type>
struct NodeDataManager {
	typedef unordered_set
	
	NodeDataManager()
	{
	}
	
	void insert(T *data, size_t use_count)
	{
	}
	
	void destroy(T *data)
	{
	}
	
protected:
	NodeDataManager()
	{
	}
	
private:
	
};
#endif

struct DerivedMeshDestructor {
	static void destroy(DerivedMesh *dm)
	{
		DM_release(dm);
	}
};

typedef node_data_ptr<DerivedMesh, DerivedMeshDestructor> mesh_ptr;

namespace detail {

static mesh_ptr create_empty_mesh()
{
	DerivedMesh *dm = CDDM_new(0, 0, 0, 0, 0);
	dm->needsFree = 0;
	return mesh_ptr(dm);
}

}

static const mesh_ptr __empty_mesh__ = detail::create_empty_mesh();


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
	
	static inline void init(POD *data, size_t use_count)
	{
		data->set_use_count(use_count);
	}
	
	static inline void release(POD *data)
	{
		data->decrement_use_count();
	}
};

/* ------------------------------------------------------------------------- */

struct TypeDesc {
	TypeDesc(BVMType base_type) :
	    base_type(base_type)
	{}
	
	inline bool assignable(const TypeDesc &other) const;
	inline bool is_kernel_type() const;
	
	BVMType base_type;
	
	inline int stack_size() const;
	inline void copy_value(void *to, const void *from) const;
};

/* ------------------------------------------------------------------------- */

struct Value {
	template <typename T>
	static Value *create(const TypeDesc &typedesc, T data);
	
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
Value *Value::create(const TypeDesc &typedesc, T data)
{
	/* TODO take extended TypeDesc into account */
	switch (typedesc.base_type) {
		case BVM_FLOAT: return new ValueType<BVM_FLOAT>(data);
		case BVM_FLOAT3: return new ValueType<BVM_FLOAT3>(data);
		case BVM_FLOAT4: return new ValueType<BVM_FLOAT4>(data);
		case BVM_INT: return new ValueType<BVM_INT>(data);
		case BVM_MATRIX44: return new ValueType<BVM_MATRIX44>(data);
		case BVM_POINTER: return new ValueType<BVM_POINTER>(data);
		
		case BVM_MESH: return new ValueType<BVM_MESH>(data);
	}
	return 0;
}

template <typename T>
bool Value::get(T *data) const
{
	switch (m_typedesc.base_type) {
		case BVM_FLOAT: return static_cast< const ValueType<BVM_FLOAT>* >(this)->get(data);
		case BVM_FLOAT3: return static_cast< const ValueType<BVM_FLOAT3>* >(this)->get(data);
		case BVM_FLOAT4: return static_cast< const ValueType<BVM_FLOAT4>* >(this)->get(data);
		case BVM_INT: return static_cast< const ValueType<BVM_INT>* >(this)->get(data);
		case BVM_MATRIX44: return static_cast< const ValueType<BVM_MATRIX44>* >(this)->get(data);
		case BVM_POINTER: return static_cast< const ValueType<BVM_POINTER>* >(this)->get(data);
			
		case BVM_MESH: return static_cast< const ValueType<BVM_MESH>* >(this)->get(data);
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
		case BVM_FLOAT4: return BaseTypeTraits<BVM_FLOAT4>::stack_size;
		case BVM_INT: return BaseTypeTraits<BVM_INT>::stack_size;
		case BVM_MATRIX44: return BaseTypeTraits<BVM_MATRIX44>::stack_size;
		case BVM_POINTER: return BaseTypeTraits<BVM_POINTER>::stack_size;
		
		case BVM_MESH: return BaseTypeTraits<BVM_MESH>::stack_size;
	}
	return 0;
}

void TypeDesc::copy_value(void *to, const void *from) const
{
	#define COPY_TYPE(a, b, type) \
	BaseTypeTraits<type>::copy((BaseTypeTraits<type>::POD*)to, (BaseTypeTraits<type>::POD const *)from);
	
	switch (base_type) {
		case BVM_FLOAT: COPY_TYPE(to, from, BVM_FLOAT); break;
		case BVM_FLOAT3: COPY_TYPE(to, from, BVM_FLOAT3); break;
		case BVM_FLOAT4: COPY_TYPE(to, from, BVM_FLOAT4); break;
		case BVM_INT: COPY_TYPE(to, from, BVM_INT); break;
		case BVM_MATRIX44: COPY_TYPE(to, from, BVM_MATRIX44); break;
		case BVM_POINTER: COPY_TYPE(to, from, BVM_POINTER); break;
		
		case BVM_MESH: COPY_TYPE(to, from, BVM_MESH); break;
	}
	
	#undef COPY_TYPE
}

} /* namespace bvm */

#endif

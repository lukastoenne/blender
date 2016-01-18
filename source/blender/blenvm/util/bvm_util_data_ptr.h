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

/** \file blender/blenvm/intern/bvm_util_data_ptr.h
 *  \ingroup bvm
 */

#ifndef __BVM_UTIL_DATA_PTR_H__
#define __BVM_UTIL_DATA_PTR_H__

#include <assert.h>

#include "MEM_guardedalloc.h"

extern "C" {
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
}

#include "bvm_util_math.h"

namespace bvm {

/* generic default deleter, using 'delete' operator */
template <typename T>
struct DefaultDeleter {
	void operator() (T *data) const
	{
		delete data;
	}
};

/* reference-counted pointer for managing transient data on the stack */
template <typename T, typename Deleter = DefaultDeleter<T> >
struct node_counted_ptr {
	typedef T data_t;
	typedef node_counted_ptr<T, Deleter> self_t;
	
	node_counted_ptr() :
	    m_data(NULL),
	    m_refs(NULL)
	{}
	
	explicit node_counted_ptr(data_t *data) :
	    m_data(data),
	    m_refs(NULL)
	{}
	
	template <typename Y, typename YDeleter>
	node_counted_ptr(const node_counted_ptr<Y, YDeleter> &other) :
	    m_data(other.m_data),
	    m_refs(other.m_refs)
	{
	}
	
	~node_counted_ptr()
	{
	}
	
	template <typename Y, typename YDeleter>
	self_t& operator = (const node_counted_ptr<Y, YDeleter> &other)
	{
		m_data = other.m_data;
	    m_refs = other.m_refs;
	}
	
	operator bool() const
	{
		return m_refs != NULL;
	}
	
	data_t* get() const
	{
		return m_data;
	}
	
	void reset()
	{
		m_data = NULL;
		if (m_refs) {
			destroy_refs(m_refs);
			m_refs = NULL;
		}
	}
	
	void reset(data_t *data)
	{
		if (m_data != data) {
			if (m_data) {
				Deleter del;
				del(m_data);
			}
			m_data = data;
		}
		if (m_refs) {
			destroy_refs(m_refs);
			m_refs = NULL;
		}
	}
	
	data_t& operator * () const { return *m_data; }
	data_t* operator -> () const { return m_data; }
	
	void retain()
	{
		if (m_refs == NULL)
			m_refs = create_refs();
		++(*m_refs);
	}
	
	void release()
	{
		assert(m_refs != NULL && *m_refs > 0);
		size_t count = --(*m_refs);
		if (count == 0) {
			clear();
		}
	}
	
protected:
	/* TODO this could be handled by a common memory manager with a mempool */
	static size_t *create_refs() { return new size_t(0); }
	static void destroy_refs(size_t *refs) { delete refs; }
	
	void clear()
	{
		if (m_data) {
			Deleter del;
			del(m_data);
			m_data = NULL;
		}
		if (m_refs) {
			destroy_refs(m_refs);
			m_refs = NULL;
		}
	}
	
private:
	T *m_data;
	size_t *m_refs;
};

/* reference-counted pointer for managing transient data on the stack */
template <typename T, typename Deleter = DefaultDeleter<T> >
struct node_scoped_ptr {
	typedef T data_t;
	typedef node_counted_ptr<T, Deleter> counted_ptr_t;
	typedef node_scoped_ptr<T, Deleter> self_t;
	
	node_scoped_ptr() :
	    m_ptr(NULL),
	    m_refs(0)
	{}
	
	explicit node_scoped_ptr(data_t *data) :
	    m_ptr(data),
	    m_refs(0)
	{}
	
	~node_scoped_ptr()
	{
	}
	
	counted_ptr_t& ptr() { return m_ptr; }
	const counted_ptr_t& ptr() const { return m_ptr; }
	
	data_t* get() const { return m_ptr.get(); }
	
	void reset()
	{
		m_refs = 0;
		if (m_ptr)
			m_ptr.release();
	}
	
	void set(data_t *data)
	{
		if (m_ptr)
			m_ptr.release();
		m_ptr = counted_ptr_t(data);
		if (data)
			m_ptr.retain();
	}
	
	void set_use_count(size_t use_count)
	{
		assert(m_refs == 0);
		m_refs = use_count;
	}
	
	void decrement_use_count()
	{
		assert(m_refs > 0);
		--m_refs;
		if (m_refs == 0)
			reset();
	}
	
private:
	counted_ptr_t m_ptr;
	size_t m_refs;
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

/* XXX should use node_counted_ptr<DerivedMesh> instead,
 * so mesh_ptr is the pointer type, and node_scoped_ptr
 * is the type of variables on the stack (a scoped variable)
 */

struct DerivedMeshDeleter {
	void operator () (DerivedMesh *dm) const
	{
		if (dm)
			dm->release(dm);
	}
};
typedef node_scoped_ptr<DerivedMesh, DerivedMeshDeleter> mesh_ptr;

struct Dupli {
	Object *object;
	matrix44 transform;
	int index;
	bool hide;
	bool recursive;
};
typedef std::vector<Dupli> DupliList;
typedef node_scoped_ptr<DupliList> duplis_ptr;

inline void create_empty_mesh(mesh_ptr &p)
{
	DerivedMesh *dm = CDDM_new(0, 0, 0, 0, 0);
	/* prevent the DM from getting freed */
	dm->needsFree = 0;
	
	p.set(dm);
}

inline void destroy_empty_mesh(mesh_ptr &p)
{
	DerivedMesh *dm = p.get();
	p.reset();
	
	/* have to set this back so the DM actually gets freed */
	dm->needsFree = 1;
	dm->release(dm);
}

} /* namespace bvm */

#endif

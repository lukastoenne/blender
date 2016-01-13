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

/** \file blender/blenvm/intern/bvm_type_data_ptr.h
 *  \ingroup bvm
 */

#ifndef __BVM_TYPE_DATA_PTR_H__
#define __BVM_TYPE_DATA_PTR_H__

#include "MEM_guardedalloc.h"

extern "C" {
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
}

#include "bvm_util_math.h"

namespace bvm {

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
	
	element_type* get() const
	{
		return m_data;
	}
	
	void reset()
	{
		m_data = 0;
		if (m_refs) {
			destroy_refs(m_refs);
			m_refs = 0;
		}
	}
	
	void set(element_type *data)
	{
		if (m_data != data) {
			if (m_data)
				DestructorT::destroy(m_data);
			m_data = data;
		}
	}
	
	element_type& operator * () const { return *m_data; }
	element_type* operator -> () const { return m_data; }
	
	void set_use_count(size_t use_count)
	{
		assert(m_refs == 0);
		if (use_count > 0)
			m_refs = create_refs(use_count);
		else if (m_refs) {
			destroy_refs(m_refs);
			m_refs = 0;
		}
	}
	
	void decrement_use_count()
	{
		assert(m_refs != 0 && *m_refs > 0);
		size_t count = --(*m_refs);
		if (count == 0) {
			clear();
		}
	}
	
	void clear()
	{
		if (m_data) {
			DestructorT::destroy(m_data);
			m_data = 0;
		}
		if (m_refs) {
			destroy_refs(m_refs);
			m_refs = 0;
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
		dm->release(dm);
	}
};
typedef node_data_ptr<DerivedMesh, DerivedMeshDestructor> mesh_ptr;

struct Dupli {
	Object *object;
	matrix44 transform;
	int index;
	bool hide;
	bool recursive;
};
typedef std::vector<Dupli> DupliList;
typedef node_data_ptr<DupliList> duplis_ptr;

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
	/* have to set this back so the DM actually gets freed */
	dm->needsFree = 1;
	p.clear();
}

} /* namespace bvm */

#endif

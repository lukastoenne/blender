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

#ifndef __UTIL_ARRAY_H__
#define __UTIL_ARRAY_H__

/** \file util_array.h
 *  \ingroup bvm
 */

#include <assert.h>
#include <string>
#include <vector>

namespace blenvm {

/* Constant reference to an array in memory.
 * Data is not owned, ArrayRef should not be stored.
 */
template <typename T>
struct ArrayRef {
	typedef const T * iterator;
	typedef const T * const_iterator;
	typedef std::reverse_iterator<iterator> reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
	typedef size_t size_type;
	
	ArrayRef() : m_data(NULL), m_size(0) {}
	ArrayRef(const T &elem) : m_data(&elem), m_size(1) {}
	ArrayRef(const T *data, size_type size) : m_data(data), m_size(size) {}
	ArrayRef(const T *begin, const T *end) : m_data(begin), m_size(end - begin) {}
	template <typename U>
	ArrayRef(const ArrayRef<U> &other) : m_data(other.data()), m_size(other.size()) {}
	template <typename U>
	ArrayRef(const std::vector<U> &other) : m_data(other.data()), m_size(other.size()) {}
	
	size_type size() const { return m_size; }
	const T *data() const { return m_data; }
	
	iterator begin() const { return m_data; }
	iterator end() const { return m_data + m_size; }
	
	reverse_iterator rbegin() const { return reverse_iterator(end()); }
	reverse_iterator rend() const { return reverse_iterator(begin()); }
	
	bool empty() const { return m_size == 0; }
	
	const T &front() const { assert(!empty()); return m_data[0]; }
	const T &back() const { assert(!empty()); return m_data[m_size-1]; }
	
	const T &operator [] (size_type index) const
	{
		assert(index < m_size && "Invalid index");
		return m_data[index];
	}
	
	template <typename Allocator>
	ArrayRef<T> copy(Allocator &alloc)
	{
		T *buf = static_cast<T*>(alloc.allocate(sizeof(T) * m_size));
		std::uninitialized_copy(begin(), end(), buf);
		return ArrayRef<T>(buf, m_size);
	}
	
private:
	const T *m_data;
	size_type m_size;
};

/* Convenient constructors
 * (can be called without template argument)
 */
template <typename T>
ArrayRef<T> make_array_ref(const T &elem)
{
	return ArrayRef<T>(elem);
}

} /* namespace blenvm */

#endif /* __UTIL_ARRAY_H__ */

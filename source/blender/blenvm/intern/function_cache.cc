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

/** \file blender/blenvm/intern/function_cache.cc
 *  \ingroup blenvm
 */

#include "MEM_guardedalloc.h"

#include "function_cache.h"

#include "bvm_function.h"
#include "llvm_function.h"

#include "util_thread.h"

namespace blenvm {

static mutex function_cache_mutex;
static spin_lock function_cache_lock = spin_lock(function_cache_mutex);

template <typename T>
struct FunctionCache {
	typedef T function_type;
	typedef unordered_map<void*, function_type*> FunctionMap;
	typedef typename FunctionMap::iterator iterator;
	typedef typename FunctionMap::const_iterator const_iterator;
	
	FunctionMap m_functions;
	
	function_type *acquire(void *key)
	{
		function_cache_lock.lock();
		const_iterator it = m_functions.find(key);
		function_type *fn = NULL;
		if (it != m_functions.end()) {
			fn = it->second;
			function_type::retain(fn);
		}
		function_cache_lock.unlock();
		return fn;
	}
	
	void release(function_type *fn)
	{
		if (!fn)
			return;
		
		if (function_type::release(fn)) {
			function_cache_lock.lock();
			iterator it = m_functions.begin();
			while (it != m_functions.end()) {
				if (it->second == fn) {
					iterator it_del = it++;
					m_functions.erase(it_del);
				}
				else
					++it;
			}
			function_cache_lock.unlock();
			
			delete fn;
		}
	}
	
	void set(void *key, function_type *fn)
	{
		typedef std::pair<void*, function_type*> FunctionCachePair;
		
		function_cache_lock.lock();
		if (fn) {
			iterator it = m_functions.find(key);
			if (it == m_functions.end()) {
				function_type::retain(fn);
				m_functions.insert(FunctionCachePair(key, fn));
			}
			else if (fn != it->second) {
				if (function_type::release(it->second))
					delete it->second;
				function_type::retain(fn);
				it->second = fn;
			}
		}
		else {
			iterator it = m_functions.find(key);
			if (it != m_functions.end()) {
				if (function_type::release(it->second))
					delete it->second;
				m_functions.erase(it);
			}
		}
		function_cache_lock.unlock();
	}
	
	void remove(void *key)
	{
		function_cache_lock.lock();
		iterator it = m_functions.find(key);
		if (it != m_functions.end()) {
			if (function_type::release(it->second))
				delete it->second;
			
			m_functions.erase(it);
		}
		function_cache_lock.unlock();
	}
	
	void clear(void)
	{
		function_cache_lock.lock();
		for (iterator it = m_functions.begin(); it != m_functions.end(); ++it) {
			if (function_type::release(it->second))
				delete it->second;
		}
		m_functions.clear();
		function_cache_lock.unlock();
	}
};

/* BVM cache */

static FunctionCache<FunctionBVM> bvm_function_cache;

FunctionBVM *function_bvm_cache_acquire(void *key)
{
	return bvm_function_cache.acquire(key);
}

void function_bvm_cache_release(FunctionBVM *fn)
{
	bvm_function_cache.release(fn);
}

void function_bvm_cache_set(void *key, FunctionBVM *fn)
{
	bvm_function_cache.set(key, fn);
}

void function_bvm_cache_remove(void *key)
{
	bvm_function_cache.remove(key);
}

void function_bvm_cache_clear(void)
{
	bvm_function_cache.clear();
}

#ifdef WITH_LLVM

/* LLVM cache */

static FunctionCache<FunctionLLVM> llvm_function_cache;

FunctionLLVM *function_llvm_cache_acquire(void *key)
{
	return llvm_function_cache.acquire(key);
}

void function_llvm_cache_release(FunctionLLVM *fn)
{
	llvm_function_cache.release(fn);
}

void function_llvm_cache_set(void *key, FunctionLLVM *fn)
{
	llvm_function_cache.set(key, fn);
}

void function_llvm_cache_remove(void *key)
{
	llvm_function_cache.remove(key);
}

void function_llvm_cache_clear(void)
{
	llvm_function_cache.clear();
}

#endif

} /* namespace blenvm */

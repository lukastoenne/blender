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

#include "util_thread.h"

namespace blenvm {

static FunctionCache bvm_function_cache;
static mutex bvm_function_cache_mutex;
static spin_lock bvm_function_cache_lock = spin_lock(bvm_function_cache_mutex);

Function *function_cache_acquire(void *key)
{
	bvm_function_cache_lock.lock();
	FunctionCache::const_iterator it = bvm_function_cache.find(key);
	Function *fn = NULL;
	if (it != bvm_function_cache.end()) {
		fn = it->second;
		Function::retain(fn);
	}
	bvm_function_cache_lock.unlock();
	return fn;
}

void function_release(Function *fn)
{
	if (!fn)
		return;
	
	Function::release(&fn);
	
	if (fn == NULL) {
		bvm_function_cache_lock.lock();
		FunctionCache::iterator it = bvm_function_cache.begin();
		while (it != bvm_function_cache.end()) {
			if (it->second == fn) {
				FunctionCache::iterator it_del = it++;
				bvm_function_cache.erase(it_del);
			}
			else
				++it;
		}
		bvm_function_cache_lock.unlock();
	}
}

void function_cache_set(void *key, Function *fn)
{
	bvm_function_cache_lock.lock();
	if (fn) {
		FunctionCache::iterator it = bvm_function_cache.find(key);
		if (it == bvm_function_cache.end()) {
			Function::retain(fn);
			bvm_function_cache.insert(FunctionCachePair(key, fn));
		}
		else if (fn != it->second) {
			Function::release(&it->second);
			Function::retain(fn);
			it->second = fn;
		}
	}
	else {
		FunctionCache::iterator it = bvm_function_cache.find(key);
		if (it != bvm_function_cache.end()) {
			Function::release(&it->second);
			bvm_function_cache.erase(it);
		}
	}
	bvm_function_cache_lock.unlock();
}

void function_cache_remove(void *key)
{
	bvm_function_cache_lock.lock();
	FunctionCache::iterator it = bvm_function_cache.find(key);
	if (it != bvm_function_cache.end()) {
		Function::release(&it->second);
		
		bvm_function_cache.erase(it);
	}
	bvm_function_cache_lock.unlock();
}

void function_cache_clear(void)
{
	bvm_function_cache_lock.lock();
	for (FunctionCache::iterator it = bvm_function_cache.begin(); it != bvm_function_cache.end(); ++it) {
		Function::release(&it->second);
	}
	bvm_function_cache.clear();
	bvm_function_cache_lock.unlock();
}

} /* namespace blenvm */

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

#ifndef __FUNCTION_CACHE_H__
#define __FUNCTION_CACHE_H__

/** \file blender/blenvm/intern/function_cache.h
 *  \ingroup blenvm
 */

#include "util_map.h"

namespace blenvm {

struct FunctionBVM;

FunctionBVM *function_bvm_cache_acquire(void *key);
void function_bvm_cache_release(FunctionBVM *fn);
void function_bvm_cache_set(void *key, FunctionBVM *fn);
void function_bvm_cache_remove(void *key);
void function_bvm_cache_clear(void);

#ifdef WITH_LLVM
struct FunctionLLVM;

FunctionLLVM *function_llvm_cache_acquire(void *key);
void function_llvm_cache_release(FunctionLLVM *fn);
void function_llvm_cache_set(void *key, FunctionLLVM *fn);
void function_llvm_cache_remove(void *key);
void function_llvm_cache_clear(void);
#endif

} /* namespace blenvm */

#endif /* __FUNCTION_CACHE_H__ */

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

/** \file function_cache.h
 *  \ingroup blenvm
 */

#include "util_map.h"

namespace blenvm {

struct Function;

typedef unordered_map<void*, Function*> FunctionCache;
typedef std::pair<void*, Function*> FunctionCachePair;

Function *function_cache_acquire(void *key);
void function_release(Function *fn);
void function_cache_set(void *key, Function *fn);
void function_cache_remove(void *key);
void function_cache_clear(void);

} /* namespace blenvm */

#endif /* __FUNCTION_CACHE_H__ */

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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_CACHE_LIBRARY_H__
#define __BKE_CACHE_LIBRARY_H__

/** \file BKE_cache_library.h
 *  \ingroup bke
 */

#include "DNA_cache_library_types.h"

struct ListBase;
struct Main;
struct Object;

struct CacheLibrary *BKE_cache_library_add(struct Main *bmain, const char *name);
struct CacheLibrary *BKE_cache_library_copy(struct CacheLibrary *cachelib);
void BKE_cache_library_free(struct CacheLibrary *cachelib);

void BKE_cache_library_make_object_list(struct Main *main, struct CacheLibrary *cachelib, struct ListBase *lb);

typedef struct CacheLibraryObjectsIterator {
	ListBase objects;
	LinkData *cur;
} CacheLibraryObjectsIterator;

void BKE_object_cache_iter_init(CacheLibraryObjectsIterator *iter, struct CacheLibrary *cachelib);
bool BKE_object_cache_iter_valid(CacheLibraryObjectsIterator *iter);
void BKE_object_cache_iter_next(CacheLibraryObjectsIterator *iter);
void BKE_object_cache_iter_end(CacheLibraryObjectsIterator *iter);
struct Object *BKE_object_cache_iter_get(CacheLibraryObjectsIterator *iter);

typedef struct CacheLibraryItemsIterator {
	struct Object *ob;
	struct CacheItem *items;
	int totitems;
	
	struct CacheItem *cur;
} CacheLibraryItemsIterator;

void BKE_cache_item_iter_init(CacheLibraryItemsIterator *iter, struct Object *ob);
bool BKE_cache_item_iter_valid(CacheLibraryItemsIterator *iter);
void BKE_cache_item_iter_next(CacheLibraryItemsIterator *iter);
void BKE_cache_item_iter_end(CacheLibraryItemsIterator *iter);

#if 0
typedef void (*CacheGroupWalkFunc)(void *userdata, struct CacheLibrary *cachelib, const struct CacheItemPath *path);
void BKE_cache_library_walk(struct CacheLibrary *cachelib, CacheGroupWalkFunc walk, void *userdata);
#endif

struct CacheItem *BKE_cache_library_find_item(struct CacheLibrary *cachelib, struct Object *ob, int type, int index);
#if 0
struct CacheItem *BKE_cache_library_add_item(struct CacheLibrary *cachelib, const struct CacheItem *path);
bool BKE_cache_library_remove_item(struct CacheLibrary *cachelib, const struct CacheItem *path);
#endif

#endif

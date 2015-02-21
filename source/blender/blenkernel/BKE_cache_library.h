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

struct Main;
struct CacheLibrary;
struct CacheItem;
struct CacheItemPath;

struct CacheLibrary *BKE_cache_library_add(struct Main *bmain, const char *name);
struct CacheLibrary *BKE_cache_library_copy(struct CacheLibrary *cachelib);
void BKE_cache_library_free(struct CacheLibrary *cachelib);

bool BKE_cache_item_path_cmp(const struct CacheItemPath *a, const struct CacheItemPath *b);
int BKE_cache_item_path_len(const struct CacheItemPath *path);
bool BKE_cache_item_path_append(struct CacheItemPath *path, const char *name);
void BKE_cache_item_path_replace(struct CacheItemPath *path, const char *name, int index);
void BKE_cache_item_path_clear(struct CacheItemPath *path);
void BKE_cache_item_path_truncate(struct CacheItemPath *path, int len);
void BKE_cache_item_path_copy(struct CacheItemPath *dst, const struct CacheItemPath *src);

typedef void (*CacheGroupWalkFunc)(void *userdata, struct CacheLibrary *cachelib, const struct CacheItemPath *path);
void BKE_cache_library_walk(struct CacheLibrary *cachelib, CacheGroupWalkFunc walk, void *userdata);

struct CacheItem *BKE_cache_library_find_item(struct CacheLibrary *cachelib, const struct CacheItemPath *path);
struct CacheItem *BKE_cache_library_add_item(struct CacheLibrary *cachelib, const struct CacheItemPath *path);
bool BKE_cache_library_remove_item(struct CacheLibrary *cachelib, const struct CacheItemPath *path);

#endif

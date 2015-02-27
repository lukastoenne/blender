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
 * The Original Code is Copyright (C) 2015 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_cache_library_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_CACHE_LIBRARY_TYPES_H__
#define __DNA_CACHE_LIBRARY_TYPES_H__

#include "DNA_defs.h"
#include "DNA_ID.h"
#include "DNA_listBase.h"

#define MAX_CACHE_GROUP_LEVEL 8

typedef enum eCacheItemType {
	CACHE_TYPE_OBJECT               = 0,
	CACHE_TYPE_DERIVED_MESH         = 1,
	CACHE_TYPE_HAIR                 = 2,
	CACHE_TYPE_HAIR_PATHS           = 3,
	CACHE_TYPE_PARTICLES            = 4,
} eCacheItemType;

typedef struct CacheItem {
	struct CacheItem *next, *prev;
	
	struct Object *ob;
	int type;
	int index;
	
	int flag;
	int pad;
} CacheItem;

typedef enum eCacheItem_Flag {
	CACHE_ITEM_ENABLED              = 1,
} eCacheItem_Flag;

typedef struct CacheLibrary {
	ID id;
	
	char filepath[1024]; /* 1024 = FILE_MAX */
	struct Group *group;
	
	ListBase items;				/* cached items */
	struct GHash *items_hash;	/* runtime: cached items hash for fast lookup */
} CacheLibrary;

#endif


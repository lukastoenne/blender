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
 * The Original Code is Copyright (C) 2015 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/cache_library.c
 *  \ingroup bke
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_cache_library_types.h"
#include "DNA_group_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "BKE_cache_library.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"

#include "PTC_api.h"

CacheLibrary *BKE_cache_library_add(Main *bmain, const char *name)
{
	CacheLibrary *cachelib;

	cachelib = BKE_libblock_alloc(bmain, ID_CL, name);

	BLI_strncpy(cachelib->filepath, "//cache/", sizeof(cachelib->filepath));

	return cachelib;
}

CacheLibrary *BKE_cache_library_copy(CacheLibrary *cachelib)
{
	CacheLibrary *cachelibn;
	
	cachelibn = BKE_libblock_copy(&cachelib->id);
	
	BLI_duplicatelist(&cachelibn->items, &cachelib->items);
	
	if (cachelib->id.lib) {
		BKE_id_lib_local_paths(G.main, cachelib->id.lib, &cachelibn->id);
	}
	
	return cachelibn;
}

void BKE_cache_library_free(CacheLibrary *cachelib)
{
	BLI_freelistN(&cachelib->items);
	
	if (cachelib->items_hash)
		BLI_ghash_free(cachelib->items_hash, NULL, NULL);
}

/* ========================================================================= */

static void cache_library_tag_recursive(CacheLibrary *cachelib, int level, Object *ob)
{
	if (level > MAX_CACHE_GROUP_LEVEL)
		return;
	
	ob->id.flag |= LIB_DOIT;
	
	/* dupli group recursion */
	if ((ob->transflag & OB_DUPLIGROUP) && ob->dup_group) {
		GroupObject *gob;
		
		for (gob = ob->dup_group->gobject.first; gob; gob = gob->next) {
			cache_library_tag_recursive(cachelib, level + 1, gob->ob);
		}
	}
}

/* tag IDs contained in the cache library group */
void BKE_cache_library_make_object_list(Main *bmain, CacheLibrary *cachelib, ListBase *lb)
{
	if (cachelib && cachelib->group) {
		Object *ob;
		LinkData *link;
		GroupObject *gob;
		
		/* clear tags */
		BKE_main_id_tag_idcode(bmain, ID_OB, false);
		
		for (gob = cachelib->group->gobject.first; gob; gob = gob->next) {
			cache_library_tag_recursive(cachelib, 0, gob->ob);
		}
		
		/* store object pointers in the list */
		for (ob = bmain->object.first; ob; ob = ob->id.next) {
			if (ob->id.flag & LIB_DOIT) {
				link = MEM_callocN(sizeof(LinkData), "cache library ID link");
				link->data = ob;
				BLI_addtail(lb, link);
			}
		}
	}
}

void BKE_object_cache_iter_init(CacheLibraryObjectsIterator *iter, CacheLibrary *cachelib)
{
	BLI_listbase_clear(&iter->objects);
	BKE_cache_library_make_object_list(G.main, cachelib, &iter->objects);
	
	iter->cur = iter->objects.first;
}

bool BKE_object_cache_iter_valid(CacheLibraryObjectsIterator *iter)
{
	return iter->cur != NULL;
}

void BKE_object_cache_iter_next(CacheLibraryObjectsIterator *iter)
{
	iter->cur = iter->cur->next;
}

Object *BKE_object_cache_iter_get(CacheLibraryObjectsIterator *iter)
{
	return iter->cur->data;
}

void BKE_object_cache_iter_end(CacheLibraryObjectsIterator *iter)
{
	BLI_freelistN(&iter->objects);
}

/* ========================================================================= */

static int cache_count_items(Object *ob) {
	ParticleSystem *psys;
	int totitem = 1; /* base object */
	
	if (ob->type == OB_MESH)
		totitem += 1; /* derived mesh */
	
	for (psys = ob->particlesystem.first; psys; psys = psys->next) {
		if (psys->part->type == PART_HAIR) {
			totitem += 2; /* hair and hair paths */
		}
	}
	
	return totitem;
}

static void cache_make_items(Object *ob, CacheItem *item) {
	ParticleSystem *psys;
	int i;
	
	/* base object */
	item->ob = ob;
	item->type = CACHE_TYPE_OBJECT;
	item->index = -1;
	++item;
	
	if (ob->type == OB_MESH) {
		/* derived mesh */
		item->ob = ob;
		item->type = CACHE_TYPE_DERIVED_MESH;
		item->index = -1;
		++item;
	}
	
	for (psys = ob->particlesystem.first, i = 0; psys; psys = psys->next, ++i) {
		if (psys->part->type == PART_HAIR) {
			/* hair */
			item->ob = ob;
			item->type = CACHE_TYPE_HAIR;
			item->index = i;
			++item;
			
			/* hair paths */
			item->ob = ob;
			item->type = CACHE_TYPE_HAIR_PATHS;
			item->index = i;
			++item;
		}
	}
}

void BKE_cache_item_iter_init(CacheLibraryItemsIterator *iter, Object *ob)
{
	iter->ob = ob;
	iter->totitems = cache_count_items(ob);
	iter->items = MEM_mallocN(sizeof(CacheItem) * iter->totitems, "object cache items");
	cache_make_items(ob, iter->items);
	
	iter->cur = iter->items;
}

bool BKE_cache_item_iter_valid(CacheLibraryItemsIterator *iter)
{
	return (int)(iter->cur - iter->items) < iter->totitems;
}

void BKE_cache_item_iter_next(CacheLibraryItemsIterator *iter)
{
	++iter->cur;
}

void BKE_cache_item_iter_end(CacheLibraryItemsIterator *iter)
{
	if (iter->items)
		MEM_freeN(iter->items);
}

#if 0
static void cache_library_walk_recursive(CacheLibrary *cachelib, CacheGroupWalkFunc walk, void *userdata, int level, Object *ob)
{
	CacheItemPath path;
	
	if (level > MAX_CACHE_GROUP_LEVEL)
		return;
	
	/* object dm */
	cache_path_object(&path, ob);
	walk(userdata, cachelib, &path);
	
	/* dupli group recursion */
	if ((ob->transflag & OB_DUPLIGROUP) && ob->dup_group) {
		GroupObject *gob;
		
		for (gob = ob->dup_group->gobject.first; gob; gob = gob->next) {
			cache_library_walk_recursive(cachelib, walk, userdata, level + 1, gob->ob);
		}
	}
}

void BKE_cache_library_walk(CacheLibrary *cachelib, CacheGroupWalkFunc walk, void *userdata)
{
	if (cachelib && cachelib->group) {
		GroupObject *gob;
		
		for (gob = cachelib->group->gobject.first; gob; gob = gob->next) {
			cache_library_walk_recursive(cachelib, walk, userdata, 0, gob->ob);
		}
	}
}
#endif

/* ========================================================================= */

BLI_INLINE unsigned int hash_int_2d(unsigned int kx, unsigned int ky)
{
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

	uint a, b, c;

	a = b = c = 0xdeadbeef + (2 << 2) + 13;
	a += kx;
	b += ky;

	c ^= b; c -= rot(b,14);
	a ^= c; a -= rot(c,11);
	b ^= a; b -= rot(a,25);
	c ^= b; c -= rot(b,16);
	a ^= c; a -= rot(c,4);
	b ^= a; b -= rot(a,14);
	c ^= b; c -= rot(b,24);

	return c;

#undef rot
}

static unsigned int cache_item_hash(const void *key)
{
	const CacheItem *item = key;
	unsigned int hash;
	
	hash = BLI_ghashutil_inthash(item->type);
	
	if (item->ob)
		hash = hash_int_2d(hash, BLI_ghashutil_ptrhash(item->ob));
	if (item->index >= 0)
		hash = hash_int_2d(hash, BLI_ghashutil_inthash(item->index));
	
	return hash;
}

static bool cache_item_cmp(const void *key_a, const void *key_b)
{
	const CacheItem *item_a = key_a, *item_b = key_b;
	
	if (item_a->type != item_b->type)
		return true;
	if (item_a->ob != item_b->ob)
		return true;
	if (item_a->index >= 0 || item_b->index >= 0) {
		if (item_a->index != item_b->index)
			return true;
	}
	
	return false;
}

const char *BKE_cache_item_name_prefix(int type)
{
	/* note: avoid underscores and the like here,
	 * the prefixes must be unique and safe when combined with arbitrary strings!
	 */
	switch (type) {
		case CACHE_TYPE_OBJECT: return "OBJECT";
		case CACHE_TYPE_DERIVED_MESH: return "MESH";
		case CACHE_TYPE_HAIR: return "HAIR";
		case CACHE_TYPE_HAIR_PATHS: return "HAIRPATHS";
		default: BLI_assert(false); return NULL; break;
	}
}

void BKE_cache_item_name(Object *ob, int type, int index, char *name)
{
	if (index >= 0)
		sprintf(name, "%s_%s_%d", BKE_cache_item_name_prefix(type), ob->id.name+2, index);
	else
		sprintf(name, "%s_%s", BKE_cache_item_name_prefix(type), ob->id.name+2);
}

int BKE_cache_item_name_length(Object *ob, int type, int index)
{
	if (index >= 0)
		return snprintf(NULL, 0, "%s_%s_%d", BKE_cache_item_name_prefix(type), ob->id.name+2, index);
	else
		return snprintf(NULL, 0, "%s_%s", BKE_cache_item_name_prefix(type), ob->id.name+2);
}

static void cache_library_insert_item_hash(CacheLibrary *cachelib, CacheItem *item, bool replace)
{
	CacheItem *exist = BLI_ghash_lookup(cachelib->items_hash, item);
	if (exist && replace) {
		BLI_remlink(&cachelib->items, exist);
		BLI_ghash_remove(cachelib->items_hash, item, NULL, NULL);
		MEM_freeN(exist);
	}
	if (!exist || replace)
		BLI_ghash_insert(cachelib->items_hash, item, item);
}

/* make sure the items hash exists (lazy init after loading files) */
static void cache_library_ensure_items_hash(CacheLibrary *cachelib)
{
	CacheItem *item;
	
	if (!cachelib->items_hash) {
		cachelib->items_hash = BLI_ghash_new(cache_item_hash, cache_item_cmp, "cache item hash");
		
		for (item = cachelib->items.first; item; item = item->next) {
			cache_library_insert_item_hash(cachelib, item, true);
		}
	}
}

CacheItem *BKE_cache_library_find_item(CacheLibrary *cachelib, Object *ob, int type, int index)
{
	CacheItem item;
	item.next = item.prev = NULL;
	item.ob = ob;
	item.type = type;
	item.index = index;
	
	cache_library_ensure_items_hash(cachelib);
	
	return BLI_ghash_lookup(cachelib->items_hash, &item);
}

CacheItem *BKE_cache_library_add_item(CacheLibrary *cachelib, struct Object *ob, int type, int index)
{
	CacheItem *item;
	
	cache_library_ensure_items_hash(cachelib);
	
	item = BKE_cache_library_find_item(cachelib, ob, type, index);
	
	if (!item) {
		item = MEM_callocN(sizeof(CacheItem), "cache library item");
		item->ob = ob;
		item->type = type;
		item->index = index;
		
		BLI_addtail(&cachelib->items, item);
		cache_library_insert_item_hash(cachelib, item, false);
	}
	
	return item;
}

void BKE_cache_library_remove_item(CacheLibrary *cachelib, CacheItem *item)
{
	if (item) {
		if (cachelib->items_hash)
			BLI_ghash_remove(cachelib->items_hash, item, NULL, NULL);
		BLI_remlink(&cachelib->items, item);
		MEM_freeN(item);
	}
}

void BKE_cache_library_clear(CacheLibrary *cachelib)
{
	CacheItem *item, *item_next;
	
	if (cachelib->items_hash)
		BLI_ghash_clear(cachelib->items_hash, NULL, NULL);
	
	for (item = cachelib->items.first; item; item = item_next) {
		item_next = item->next;
		MEM_freeN(item);
	}
	BLI_listbase_clear(&cachelib->items);
}

void BKE_cache_library_group_update(Main *bmain, CacheLibrary *cachelib)
{
	if (cachelib) {
		CacheItem *item, *item_next;
		
		/* clear tags */
		BKE_main_id_tag_idcode(bmain, ID_OB, false);
		
		if (cachelib->group) {
			GroupObject *gob;
			for (gob = cachelib->group->gobject.first; gob; gob = gob->next) {
				cache_library_tag_recursive(cachelib, 0, gob->ob);
			}
		}
		
		/* remove unused items */
		for (item = cachelib->items.first; item; item = item_next) {
			item_next = item->next;
			
			if (!item->ob || !(item->ob->id.flag & LIB_DOIT)) {
				BKE_cache_library_remove_item(cachelib, item);
			}
		}
	}
}

/* ========================================================================= */

#if 0
typedef struct UpdateItemsData {
	CacheItem *cur;
} UpdateItemsData;

static void cache_library_update_items_walk(void *userdata, CacheLibrary *cachelib)
{
	UpdateItemsData *data = userdata;
	CacheItem *item;
	
	if (data->cur) {
		item = data->cur;
		data->cur = data->cur->next;
	}
	else {
		item = MEM_callocN(sizeof(CacheItem), "cache library item");
		BLI_addtail(&cachelib->items, item);
	}
}

void BKE_cache_library_update_items(CacheLibrary *cachelib)
{
	UpdateItemsData data;
	
	data.cur = cachelib->items.first;
	BKE_cache_library_walk(cachelib, cache_library_update_items_walk, &data);
	
	/* truncate items list */
	if (data.cur) {
		cachelib->items.last = data.cur->prev;
		while (data.cur) {
			CacheItem *item = data.cur;
			data.cur = data.cur->next;
			
			BLI_remlink(&cachelib->items, item);
			MEM_freeN(item);
		}
	}
}
#endif

/* ========================================================================= */

/* XXX this needs work: the order of cachelibraries in bmain is arbitrary!
 * If there are multiple cachelibs applying data, which should take preference?
 */

bool BKE_cache_read_derived_mesh(Main *bmain, Scene *scene, float frame, Object *ob, struct DerivedMesh **r_dm)
{
	CacheLibrary *cachelib;
	
	for (cachelib = bmain->cache_library.first; cachelib; cachelib = cachelib->id.next) {
		if (PTC_cachelib_read_sample_derived_mesh(scene, frame, cachelib, ob, r_dm) != PTC_READ_SAMPLE_INVALID)
			return true;
	}
	return false;
}

bool BKE_cache_read_cloth(Main *bmain, Scene *scene, float frame, Object *ob, struct ClothModifierData *clmd)
{
//	CacheLibrary *cachelib;
	
//	for (cachelib = bmain->cache_library.first; cachelib; cachelib = cachelib->id.next) {
//		if (PTC_cachelib_read_sample_cloth(scene, frame, cachelib, ob, clmd) != PTC_READ_SAMPLE_INVALID)
//			return true;
//	}
	return false;
}

bool BKE_cache_read_hair_dynamics(Main *bmain, Scene *scene, float frame, Object *ob, struct ParticleSystem *psys)
{
	CacheLibrary *cachelib;
	
	for (cachelib = bmain->cache_library.first; cachelib; cachelib = cachelib->id.next) {
		if (PTC_cachelib_read_sample_hair_dynamics(scene, frame, cachelib, ob, psys) != PTC_READ_SAMPLE_INVALID)
			return true;
	}
	return false;
}

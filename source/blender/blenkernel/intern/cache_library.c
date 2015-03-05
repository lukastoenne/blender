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
#include "BLI_fileops.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_cache_library_types.h"
#include "DNA_group_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "BKE_cache_library.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_modifier.h"

#include "PTC_api.h"

CacheLibrary *BKE_cache_library_add(Main *bmain, const char *name)
{
	CacheLibrary *cachelib;
	char basename[MAX_NAME];

	cachelib = BKE_libblock_alloc(bmain, ID_CL, name);

	BLI_strncpy(basename, cachelib->id.name+2, sizeof(basename));
	BLI_filename_make_safe(basename);
	BLI_snprintf(cachelib->filepath, sizeof(cachelib->filepath), "//cache/%s.%s", basename, PTC_get_default_archive_extension());

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

void BKE_cache_library_unlink(CacheLibrary *UNUSED(cachelib))
{
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
		else {
			totitem += 1; /* particles */
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
		else {
			/* hair paths */
			item->ob = ob;
			item->type = CACHE_TYPE_PARTICLES;
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

BLI_INLINE void print_cachelib_items(CacheLibrary *cachelib)
{
	CacheItem *item;
	int i;
	
	printf("Cache Library %s:\n", cachelib->id.name+2);
	for (item = cachelib->items.first, i = 0; item; item = item->next, ++i) {
		printf("  Item %d: ob=%s, type=%d, index=%d, hash=%d\n", i, item->ob ? item->ob->id.name+2 : "!!!", item->type, item->index, cache_item_hash(item));
	}
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
		case CACHE_TYPE_PARTICLES: return "PARTICLES";
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

eCacheReadSampleResult BKE_cache_read_result(int ptc_result)
{
	switch (ptc_result) {
		case PTC_READ_SAMPLE_INVALID: return CACHE_READ_SAMPLE_INVALID;
		case PTC_READ_SAMPLE_EARLY: return CACHE_READ_SAMPLE_EARLY;
		case PTC_READ_SAMPLE_LATE: return CACHE_READ_SAMPLE_LATE;
		case PTC_READ_SAMPLE_EXACT: return CACHE_READ_SAMPLE_EXACT;
		case PTC_READ_SAMPLE_INTERPOLATED: return CACHE_READ_SAMPLE_INTERPOLATED;
		default: BLI_assert(false); break; /* should never happen, enums out of sync? */
	}
	return CACHE_READ_SAMPLE_INVALID;
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
	
	/* assert validity */
	BLI_assert(BKE_cache_library_validate_item(cachelib, ob, type, index));
	
	cache_library_ensure_items_hash(cachelib);
	
	item = BKE_cache_library_find_item(cachelib, ob, type, index);
	
	if (!item) {
		item = MEM_callocN(sizeof(CacheItem), "cache library item");
		item->ob = ob;
		item->type = type;
		item->index = index;
		
		BLI_addtail(&cachelib->items, item);
		cache_library_insert_item_hash(cachelib, item, false);
		
		id_lib_extern((ID *)item->ob);
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

bool BKE_cache_library_validate_item(CacheLibrary *cachelib, Object *ob, int type, int index)
{
	if (!cachelib || !cachelib->group)
		return false;
	
	if (!BKE_group_object_exists(cachelib->group, ob))
		return false;
	
	if (ELEM(type, CACHE_TYPE_DERIVED_MESH)) {
		if (ob->type != OB_MESH)
			return false;
	}
	else if (ELEM(type, CACHE_TYPE_PARTICLES, CACHE_TYPE_HAIR, CACHE_TYPE_HAIR_PATHS)) {
		ParticleSystem *psys = BLI_findlink(&ob->particlesystem, index);
		
		if (!psys)
			return false;
		
		if (ELEM(type, CACHE_TYPE_PARTICLES)) {
			if (psys->part->type != PART_EMITTER)
				return false;
		}
		
		if (ELEM(type, CACHE_TYPE_HAIR, CACHE_TYPE_HAIR_PATHS)) {
			if (psys->part->type != PART_HAIR)
				return false;
		}
	}
	
	return true;
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

static const char *default_filename = "blendcache";

BLI_INLINE bool path_is_dirpath(const char *path)
{
	/* last char is a slash? */
	return *(BLI_last_slash(path) + 1) == '\0';
}

bool BKE_cache_archive_path_test(const char *path, ID *UNUSED(id), Library *lib)
{
	if (BLI_path_is_rel(path)) {
		if (!(G.relbase_valid || lib))
			return false;
	}
	
	return true;
	
}

void BKE_cache_archive_path(const char *path, ID *id, Library *lib, char *result, int max)
{
	char abspath[FILE_MAX];
	
	result[0] = '\0';
	
	if (BLI_path_is_rel(path)) {
		if (G.relbase_valid || lib) {
			const char *relbase = lib ? lib->filepath : G.main->name;
			
			BLI_strncpy(abspath, path, sizeof(abspath));
			BLI_path_abs(abspath, relbase);
		}
		else {
			/* can't construct a valid path */
			return;
		}
	}
	else {
		BLI_strncpy(abspath, path, sizeof(abspath));
	}
	
	if (path_is_dirpath(abspath) || BLI_is_dir(abspath)) {
		BLI_join_dirfile(result, max, abspath, id ? id->name+2 : default_filename);
	}
	else {
		BLI_strncpy(result, abspath, max);
	}
}


static void cachelib_add_writer(ListBase *writers, struct CacheItem *item, struct PTCWriter *writer)
{
	if (writer) {
		CacheLibraryWriterLink *link = MEM_callocN(sizeof(CacheLibraryWriterLink), "cachelib writers link");
		
		link->item = item;
		link->writer = writer;
		BLI_addtail(writers, link);
	}
}

static int cachelib_writers_cmp(const void *a, const void *b)
{
	const CacheLibraryWriterLink *la = a, *lb = b;
	return la->item->ob > lb->item->ob;
}

BLI_INLINE int cache_required_mode(CacheLibrary *cachelib)
{
	switch (cachelib->eval_mode) {
		case CACHE_LIBRARY_EVAL_RENDER : return eModifierMode_Render;
		case CACHE_LIBRARY_EVAL_VIEWPORT : return eModifierMode_Realtime;
	}
	return 0;
}

void BKE_cache_library_writers(CacheLibrary *cachelib, Scene *scene, DerivedMesh **render_dm_ptr, ListBase *writers)
{
	const eCacheLibrary_EvalMode eval_mode = cachelib->eval_mode;
	const int required_mode = cache_required_mode(cachelib);
	CacheItem *item;
	
	BLI_listbase_clear(writers);
	
	for (item = cachelib->items.first; item; item = item->next) {
		char name[2*MAX_NAME];
		
		if (!item->ob || !(item->flag & CACHE_ITEM_ENABLED))
			continue;
		
		BKE_cache_item_name(item->ob, item->type, item->index, name);
		
		switch (item->type) {
			case CACHE_TYPE_DERIVED_MESH: {
				if (item->ob->type == OB_MESH) {
					CacheModifierData *cachemd = (CacheModifierData *)mesh_find_cache_modifier(scene, item->ob, required_mode);
					if (cachemd) {
						switch (eval_mode) {
							case CACHE_LIBRARY_EVAL_VIEWPORT:
								cachelib_add_writer(writers, item, PTC_writer_cache_modifier_realtime(name, item->ob, cachemd));
								break;
							case CACHE_LIBRARY_EVAL_RENDER:
								cachelib_add_writer(writers, item, PTC_writer_cache_modifier_render(name, scene, item->ob, cachemd));
								break;
						}
					}
					else {
						switch (eval_mode) {
							case CACHE_LIBRARY_EVAL_VIEWPORT:
								cachelib_add_writer(writers, item, PTC_writer_derived_final_realtime(name, item->ob));
								break;
							case CACHE_LIBRARY_EVAL_RENDER:
								cachelib_add_writer(writers, item, PTC_writer_derived_final_render(name, scene, item->ob, render_dm_ptr));
								break;
						}
					}
				}
				break;
			}
			case CACHE_TYPE_HAIR: {
				ParticleSystem *psys = (ParticleSystem *)BLI_findlink(&item->ob->particlesystem, item->index);
				if (psys && psys->part && psys->part->type == PART_HAIR && psys->clmd) {
					cachelib_add_writer(writers, item, PTC_writer_hair_dynamics(name, item->ob, psys));
				}
				break;
			}
			case CACHE_TYPE_HAIR_PATHS: {
				ParticleSystem *psys = (ParticleSystem *)BLI_findlink(&item->ob->particlesystem, item->index);
				if (psys && psys->part && psys->part->type == PART_HAIR) {
					cachelib_add_writer(writers, item, PTC_writer_particles_pathcache_parents(name, item->ob, psys));
					cachelib_add_writer(writers, item, PTC_writer_particles_pathcache_children(name, item->ob, psys));
				}
				break;
			}
			case CACHE_TYPE_PARTICLES: {
				ParticleSystem *psys = (ParticleSystem *)BLI_findlink(&item->ob->particlesystem, item->index);
				if (psys && psys->part && psys->part->type != PART_HAIR) {
					cachelib_add_writer(writers, item, PTC_writer_particles(name, item->ob, psys));
				}
				break;
			}
			default:
				break;
		}
	}
	
	/* Sort writers by their object.
	 * This is necessary so objects can be evaluated with render settings
	 * and all cached items exported, without having to reevaluate the same object multiple times.
	 */
	BLI_listbase_sort(writers, cachelib_writers_cmp);
}

struct PTCWriterArchive *BKE_cache_library_writers_open_archive(Scene *scene, CacheLibrary *cachelib, ListBase *writers)
{
	char filename[FILE_MAX];
	struct PTCWriterArchive *archive;
	CacheLibraryWriterLink *link;
	
	BKE_cache_archive_path(cachelib->filepath, (ID *)cachelib, cachelib->id.lib, filename, sizeof(filename));
	archive = PTC_open_writer_archive(scene, filename);
	
	for (link = writers->first; link; link = link->next) {
		PTC_writer_set_archive(link->writer, archive);
	}
	
	return archive;
}

void BKE_cache_library_writers_free(struct PTCWriterArchive *archive, ListBase *writers)
{
	CacheLibraryWriterLink *link;
	
	for (link = writers->first; link; link = link->next) {
		PTC_writer_free(link->writer);
	}
	BLI_freelistN(writers);
	
	PTC_close_writer_archive(archive);
}


static struct PTCReader *cache_library_reader_derived_mesh(CacheLibrary *cachelib, Object *ob, CacheItem **r_item)
{
	CacheItem *item;
	
	if (!(cachelib->flag & CACHE_LIBRARY_READ))
		return NULL;
	*r_item = item = BKE_cache_library_find_item(cachelib, ob, CACHE_TYPE_DERIVED_MESH, -1);
	if (item && (item->flag & CACHE_ITEM_ENABLED)) {
		char name[2*MAX_NAME];
		BKE_cache_item_name(ob, CACHE_TYPE_DERIVED_MESH, -1, name);
		
		return PTC_reader_derived_mesh(name, ob);
	}
	
	return NULL;
}

static struct PTCReader *cache_library_reader_hair_dynamics(CacheLibrary *cachelib, Object *ob, ParticleSystem *psys, CacheItem **r_item)
{
	CacheItem *item;
	int index;
	
	if (!(cachelib->flag & CACHE_LIBRARY_READ))
		return NULL;
	if (!(psys && psys->part && psys->part->type == PART_HAIR && psys->clmd))
		return NULL;
	
	index = BLI_findindex(&ob->particlesystem, psys);
	*r_item = item = BKE_cache_library_find_item(cachelib, ob, CACHE_TYPE_HAIR, index);
	if (item && (item->flag & CACHE_ITEM_ENABLED)) {
		char name[2*MAX_NAME];
		BKE_cache_item_name(ob, CACHE_TYPE_HAIR, index, name);
		
		return PTC_reader_hair_dynamics(name, ob, psys);
	}
	
	return NULL;
}

static struct PTCReader *cache_library_reader_particles(CacheLibrary *cachelib, Object *ob, ParticleSystem *psys, CacheItem **r_item)
{
	CacheItem *item;
	int index;
	
	if (!(cachelib->flag & CACHE_LIBRARY_READ))
		return NULL;
	if (!(psys && psys->part && psys->part->type != PART_HAIR))
		return NULL;
	
	index = BLI_findindex(&ob->particlesystem, psys);
	*r_item = item = BKE_cache_library_find_item(cachelib, ob, CACHE_TYPE_PARTICLES, index);
	if (item && (item->flag & CACHE_ITEM_ENABLED)) {
		char name[2*MAX_NAME];
		BKE_cache_item_name(ob, CACHE_TYPE_PARTICLES, index, name);
		
		return PTC_reader_particles(name, ob, psys);
	}
	
	return NULL;
}

static struct PTCReader *cache_library_reader_particles_pathcache_parents(CacheLibrary *cachelib, Object *ob, ParticleSystem *psys, CacheItem **r_item)
{
	CacheItem *item;
	int index;
	
	if (!(cachelib->flag & CACHE_LIBRARY_READ))
		return NULL;
	if (!(psys && psys->part && psys->part->type == PART_HAIR))
		return NULL;
	
	index = BLI_findindex(&ob->particlesystem, psys);
	*r_item = item = BKE_cache_library_find_item(cachelib, ob, CACHE_TYPE_HAIR_PATHS, index);
	if (item && (item->flag & CACHE_ITEM_ENABLED)) {
		char name[2*MAX_NAME];
		BKE_cache_item_name(ob, CACHE_TYPE_HAIR_PATHS, index, name);
		
		return PTC_reader_particles_pathcache_parents(name, ob, psys);
	}
	
	return NULL;
}

static struct PTCReader *cache_library_reader_particles_pathcache_children(CacheLibrary *cachelib, Object *ob, ParticleSystem *psys, CacheItem **r_item)
{
	CacheItem *item;
	int index;
	
	if (!(cachelib->flag & CACHE_LIBRARY_READ))
		return NULL;
	if (!(psys && psys->part && psys->part->type == PART_HAIR))
		return NULL;
	
	index = BLI_findindex(&ob->particlesystem, psys);
	*r_item = item = BKE_cache_library_find_item(cachelib, ob, CACHE_TYPE_HAIR_PATHS, index);
	if (item && (item->flag & CACHE_ITEM_ENABLED)) {
		char name[2*MAX_NAME];
		BKE_cache_item_name(ob, CACHE_TYPE_HAIR_PATHS, index, name);
		
		return PTC_reader_particles_pathcache_children(name, ob, psys);
	}
	
	return NULL;
}


eCacheReadSampleResult BKE_cache_library_read_derived_mesh(Scene *scene, float frame, CacheLibrary *cachelib, struct Object *ob, struct DerivedMesh **r_dm)
{
	struct PTCReader *reader;
	CacheItem *item;
	eCacheReadSampleResult result = CACHE_READ_SAMPLE_INVALID;
	
	if (!(cachelib->flag & CACHE_LIBRARY_READ))
		return result;
	
	reader = cache_library_reader_derived_mesh(cachelib, ob, &item);
	if (reader) {
		char filename[FILE_MAX];
		struct PTCReaderArchive *archive;
		
		BKE_cache_archive_path(cachelib->filepath, (ID *)cachelib, cachelib->id.lib, filename, sizeof(filename));
		archive = PTC_open_reader_archive(scene, filename);
		PTC_reader_set_archive(reader, archive);
		
		result = BKE_cache_read_result(PTC_read_sample(reader, frame));
		item->read_result = result;
		if (result != CACHE_READ_SAMPLE_INVALID)
			*r_dm = PTC_reader_derived_mesh_acquire_result(reader);
		
		PTC_close_reader_archive(archive);
		PTC_reader_free(reader);
	}
	
	return result;
}

eCacheReadSampleResult BKE_cache_library_read_hair_dynamics(Scene *scene, float frame, CacheLibrary *cachelib, Object *ob, ParticleSystem *psys)
{
	struct PTCReader *reader;
	CacheItem *item;
	eCacheReadSampleResult result = CACHE_READ_SAMPLE_INVALID;
	
	if (!(cachelib->flag & CACHE_LIBRARY_READ))
		return result;
	
	reader = cache_library_reader_hair_dynamics(cachelib, ob, psys, &item);
	if (reader) {
		char filename[FILE_MAX];
		struct PTCReaderArchive *archive;
		
		BKE_cache_archive_path(cachelib->filepath, (ID *)cachelib, cachelib->id.lib, filename, sizeof(filename));
		archive = PTC_open_reader_archive(scene, filename);
		PTC_reader_set_archive(reader, archive);
		
		result = BKE_cache_read_result(PTC_read_sample(reader, frame));
		item->read_result = result;
		
		PTC_close_reader_archive(archive);
		PTC_reader_free(reader);
	}
	
	return result;
}

eCacheReadSampleResult BKE_cache_library_read_particles(Scene *scene, float frame, CacheLibrary *cachelib, Object *ob, ParticleSystem *psys)
{
	struct PTCReader *reader;
	CacheItem *item;
	eCacheReadSampleResult result = CACHE_READ_SAMPLE_INVALID;
	
	if (!(cachelib->flag & CACHE_LIBRARY_READ))
		return result;
	
	reader = cache_library_reader_particles(cachelib, ob, psys, &item);
	if (reader) {
		char filename[FILE_MAX];
		struct PTCReaderArchive *archive;
		
		BKE_cache_archive_path(cachelib->filepath, (ID *)cachelib, cachelib->id.lib, filename, sizeof(filename));
		archive = PTC_open_reader_archive(scene, filename);
		PTC_reader_set_archive(reader, archive);
		
		result = BKE_cache_read_result(PTC_read_sample(reader, frame));
		item->read_result = result;
		
		PTC_close_reader_archive(archive);
		PTC_reader_free(reader);
	}
	
	return result;
}

eCacheReadSampleResult BKE_cache_library_read_particles_pathcache_parents(Scene *scene, float frame, CacheLibrary *cachelib, Object *ob, ParticleSystem *psys)
{
	struct PTCReader *reader;
	CacheItem *item;
	eCacheReadSampleResult result = CACHE_READ_SAMPLE_INVALID;
	
	if (!(cachelib->flag & CACHE_LIBRARY_READ))
		return result;
	
	reader = cache_library_reader_particles_pathcache_parents(cachelib, ob, psys, &item);
	if (reader) {
		char filename[FILE_MAX];
		struct PTCReaderArchive *archive;
		
		BKE_cache_archive_path(cachelib->filepath, (ID *)cachelib, cachelib->id.lib, filename, sizeof(filename));
		archive = PTC_open_reader_archive(scene, filename);
		PTC_reader_set_archive(reader, archive);
		
		result = BKE_cache_read_result(PTC_read_sample(reader, frame));
		item->read_result = result;
		
		PTC_close_reader_archive(archive);
		PTC_reader_free(reader);
	}
	
	return result;
}

eCacheReadSampleResult BKE_cache_library_read_particles_pathcache_children(Scene *scene, float frame, CacheLibrary *cachelib, Object *ob, ParticleSystem *psys)
{
	struct PTCReader *reader;
	CacheItem *item;
	eCacheReadSampleResult result = CACHE_READ_SAMPLE_INVALID;
	
	if (!(cachelib->flag & CACHE_LIBRARY_READ))
		return result;
	
	reader = cache_library_reader_particles_pathcache_children(cachelib, ob, psys, &item);
	if (reader) {
		char filename[FILE_MAX];
		struct PTCReaderArchive *archive;
		
		BKE_cache_archive_path(cachelib->filepath, (ID *)cachelib, cachelib->id.lib, filename, sizeof(filename));
		archive = PTC_open_reader_archive(scene, filename);
		PTC_reader_set_archive(reader, archive);
		
		result = BKE_cache_read_result(PTC_read_sample(reader, frame));
		item->read_result = result;
		
		PTC_close_reader_archive(archive);
		PTC_reader_free(reader);
	}
	
	return result;
}


/* XXX this needs work: the order of cachelibraries in bmain is arbitrary!
 * If there are multiple cachelibs applying data, which should take preference?
 */

static CacheLibrary *cachelib_skip_read(CacheLibrary *cachelib, eCacheLibrary_EvalMode eval_mode)
{
	for (; cachelib; cachelib = cachelib->id.next) {
		if (!(cachelib->flag & CACHE_LIBRARY_READ))
			continue;
		if (cachelib->eval_mode != eval_mode)
			continue;
		break;
	}
	return cachelib;
}

#define FOREACH_CACHELIB_READ(bmain, cachelib, eval_mode) \
	for (cachelib = cachelib_skip_read(bmain->cache_library.first, eval_mode); cachelib; cachelib = cachelib_skip_read(cachelib->id.next, eval_mode))

bool BKE_cache_read_derived_mesh(Main *bmain, Scene *scene, float frame, eCacheLibrary_EvalMode eval_mode,
                                 Object *ob, struct DerivedMesh **r_dm)
{
	CacheLibrary *cachelib;
	
	FOREACH_CACHELIB_READ(bmain, cachelib, eval_mode) {
		if (BKE_cache_library_read_derived_mesh(scene, frame, cachelib, ob, r_dm) != CACHE_READ_SAMPLE_INVALID)
			return true;
	}
	return false;
}

bool BKE_cache_read_cloth(Main *bmain, Scene *scene, float frame, eCacheLibrary_EvalMode eval_mode,
                          Object *ob, struct ClothModifierData *clmd)
{
//	CacheLibrary *cachelib;
	
//	FOREACH_CACHELIB_READ(bmain, cachelib, eval_mode) {
//		if (BKE_cache_library_read_cloth(scene, frame, cachelib, ob, clmd) != CACHE_READ_SAMPLE_INVALID)
//			return true;
//	}
	return false;
}

bool BKE_cache_read_hair_dynamics(Main *bmain, Scene *scene, float frame, eCacheLibrary_EvalMode eval_mode,
                                  Object *ob, struct ParticleSystem *psys)
{
	CacheLibrary *cachelib;
	
	FOREACH_CACHELIB_READ(bmain, cachelib, eval_mode) {
		if (BKE_cache_library_read_hair_dynamics(scene, frame, cachelib, ob, psys) != CACHE_READ_SAMPLE_INVALID)
			return true;
	}
	return false;
}

bool BKE_cache_read_particles(struct Main *bmain, struct Scene *scene, float frame, eCacheLibrary_EvalMode eval_mode,
                              struct Object *ob, struct ParticleSystem *psys)
{
	CacheLibrary *cachelib;
	
	FOREACH_CACHELIB_READ(bmain, cachelib, eval_mode) {
		if (BKE_cache_library_read_particles(scene, frame, cachelib, ob, psys) != CACHE_READ_SAMPLE_INVALID)
			return true;
	}
	return false;
}

bool BKE_cache_read_particles_pathcache_parents(Main *bmain, Scene *scene, float frame, eCacheLibrary_EvalMode eval_mode,
                                                Object *ob, struct ParticleSystem *psys)
{
	CacheLibrary *cachelib;
	
	FOREACH_CACHELIB_READ(bmain, cachelib, eval_mode) {
		if (BKE_cache_library_read_particles_pathcache_parents(scene, frame, cachelib, ob, psys) != CACHE_READ_SAMPLE_INVALID)
			return true;
	}
	return false;
}

bool BKE_cache_read_particles_pathcache_children(Main *bmain, Scene *scene, float frame, eCacheLibrary_EvalMode eval_mode,
                                                 Object *ob, struct ParticleSystem *psys)
{
	CacheLibrary *cachelib;
	
	FOREACH_CACHELIB_READ(bmain, cachelib, eval_mode) {
		if (BKE_cache_library_read_particles_pathcache_children(scene, frame, cachelib, ob, psys) != CACHE_READ_SAMPLE_INVALID)
			return true;
	}
	return false;
}


void BKE_cache_library_dag_recalc_tag(EvaluationContext *eval_ctx, Main *bmain)
{
	eCacheLibrary_EvalMode eval_mode = (eval_ctx->mode == DAG_EVAL_RENDER) ? CACHE_LIBRARY_EVAL_RENDER : CACHE_LIBRARY_EVAL_VIEWPORT;
	CacheLibrary *cachelib;
	
	FOREACH_CACHELIB_READ(bmain, cachelib, eval_mode) {
		if (cachelib->flag & CACHE_LIBRARY_READ) {
			CacheItem *item;
			
			for (item = cachelib->items.first; item; item = item->next) {
				if (item->ob && (item->flag & CACHE_ITEM_ENABLED)) {
					
					switch (item->type) {
						case CACHE_TYPE_OBJECT:
							DAG_id_tag_update(&item->ob->id, OB_RECALC_OB | OB_RECALC_TIME);
							break;
						case CACHE_TYPE_DERIVED_MESH:
						case CACHE_TYPE_PARTICLES:
						case CACHE_TYPE_HAIR:
						case CACHE_TYPE_HAIR_PATHS:
							DAG_id_tag_update(&item->ob->id, OB_RECALC_DATA | OB_RECALC_TIME);
							break;
					}
				}
			}
		}
	}
}

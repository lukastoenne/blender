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
struct bContext;
struct Group;
struct Object;
struct Scene;
struct EvaluationContext;
struct ParticleSystem;
struct DupliCache;
struct DupliObjectData;
struct CacheModifier;
struct ID;

struct ClothModifierData;

struct CacheLibrary *BKE_cache_library_add(struct Main *bmain, const char *name);
struct CacheLibrary *BKE_cache_library_copy(struct CacheLibrary *cachelib);
void BKE_cache_library_free(struct CacheLibrary *cachelib);
void BKE_cache_library_unlink(struct CacheLibrary *cachelib);

void BKE_cache_library_make_object_list(struct Main *bmain, struct CacheLibrary *cachelib, struct ListBase *lb);

typedef struct CacheLibraryObjectsIterator {
	ListBase objects;
	LinkData *cur;
} CacheLibraryObjectsIterator;

void BKE_object_cache_iter_init(CacheLibraryObjectsIterator *iter, struct CacheLibrary *cachelib);
bool BKE_object_cache_iter_valid(CacheLibraryObjectsIterator *iter);
void BKE_object_cache_iter_next(CacheLibraryObjectsIterator *iter);
void BKE_object_cache_iter_end(CacheLibraryObjectsIterator *iter);
struct Object *BKE_object_cache_iter_get(CacheLibraryObjectsIterator *iter);

const char *BKE_cache_item_name_prefix(int type);
void BKE_cache_item_name(struct Object *ob, int type, int index, char *name);
int BKE_cache_item_name_length(struct Object *ob, int type, int index);
eCacheReadSampleResult BKE_cache_read_result(int ptc_result);

bool BKE_cache_library_validate_item(struct CacheLibrary *cachelib, struct Object *ob, int type, int index);

/* ========================================================================= */

bool BKE_cache_archive_path_test(struct CacheLibrary *cachelib, const char *path);
void BKE_cache_archive_path_ex(const char *path, struct Library *lib, const char *default_filename, char *result, int max);
void BKE_cache_archive_input_path(struct CacheLibrary *cachelib, char *result, int max);
void BKE_cache_archive_output_path(struct CacheLibrary *cachelib, char *result, int max);

void BKE_cache_library_dag_recalc_tag(struct EvaluationContext *eval_ctx, struct Main *bmain);

bool BKE_cache_read_dupli_cache(struct Scene *scene, float frame, eCacheLibrary_EvalMode eval_mode,
                                struct Group *dupgroup, struct DupliCache *dupcache, struct CacheLibrary *cachelib);
bool BKE_cache_read_dupli_object(struct Scene *scene, float frame, eCacheLibrary_EvalMode eval_mode,
                                 struct Object *ob, struct DupliObjectData *data, struct CacheLibrary *cachelib);

/* ========================================================================= */

typedef void (*CacheModifier_IDWalkFunc)(void *userdata, struct CacheLibrary *cachelib, struct CacheModifier *md, struct ID **id_ptr);

typedef struct CacheBakeContext {
	struct CacheLibrary *cachelib;
	struct CacheModifier *md;
	
	struct Main *bmain;
	struct Scene *scene;
	int startframe, endframe;
	struct Group *group;
	
	short *stop;
	short *do_update;
	float *progress;
} CacheBakeContext;

typedef void (*CacheModifier_InitFunc)(struct CacheModifier *md);
typedef void (*CacheModifier_FreeFunc)(struct CacheModifier *md);
typedef void (*CacheModifier_CopyFunc)(struct CacheModifier *md, struct CacheModifier *target);
typedef void (*CacheModifier_ForeachIDLinkFunc)(struct CacheModifier *md, struct CacheLibrary *cachelib,
                                                CacheModifier_IDWalkFunc walk, void *userData);
typedef void (*CacheModifier_BakeFunc)(struct CacheModifier *md, struct CacheLibrary *cachelib, CacheBakeContext *ctx);

typedef struct CacheModifierTypeInfo {
	/* The user visible name for this modifier */
	char name[32];

	/* The DNA struct name for the modifier data type,
	 * used to write the DNA data out.
	 */
	char struct_name[32];

	/* The size of the modifier data type, used by allocation. */
	int struct_size;

	/********************* Non-optional functions *********************/

	/* Copy instance data for this modifier type. Should copy all user
	 * level settings to the target modifier.
	 */
	CacheModifier_CopyFunc copy;

	/* Should call the given walk function with a pointer to each ID
	 * pointer (i.e. each datablock pointer) that the modifier data
	 * stores. This is used for linking on file load and for
	 * unlinking datablocks or forwarding datablock references.
	 *
	 * This function is optional.
	 */
	CacheModifier_ForeachIDLinkFunc foreachIDLink;

	/* Process data and write results to the modifier's output archive */
	CacheModifier_BakeFunc bake;

	/********************* Optional functions *********************/

	/* Initialize new instance data for this modifier type, this function
	 * should set modifier variables to their default values.
	 * 
	 * This function is optional.
	 */
	CacheModifier_InitFunc init;

	/* Free internal modifier data variables, this function should
	 * not free the md variable itself.
	 *
	 * This function is optional.
	 */
	CacheModifier_FreeFunc free;
} CacheModifierTypeInfo;

void BKE_cache_modifier_init(void);

const char *BKE_cache_modifier_type_name(eCacheModifier_Type type);
const char *BKE_cache_modifier_type_struct_name(eCacheModifier_Type type);
int BKE_cache_modifier_type_struct_size(eCacheModifier_Type type);

bool BKE_cache_modifier_unique_name(struct ListBase *modifiers, struct CacheModifier *md);
struct CacheModifier *BKE_cache_modifier_add(struct CacheLibrary *cachelib, const char *name, eCacheModifier_Type type);
void BKE_cache_modifier_remove(struct CacheLibrary *cachelib, struct CacheModifier *md);
void BKE_cache_modifier_clear(struct CacheLibrary *cachelib);
struct CacheModifier *BKE_cache_modifier_copy(struct CacheLibrary *cachelib, struct CacheModifier *md);

void BKE_cache_modifier_foreachIDLink(struct CacheLibrary *cachelib, struct CacheModifier *md, CacheModifier_IDWalkFunc walk, void *userdata);

void BKE_cache_modifier_bake(const struct bContext *C, struct Group *group, struct CacheLibrary *cachelib, struct CacheModifier *md, struct Scene *scene, int startframe, int endframe);

#endif

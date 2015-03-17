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
 * Contributor(s): Blender Foundation (2015).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_cache_library.c
 *  \ingroup RNA
 */

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "DNA_cache_library_types.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_types.h"

EnumPropertyItem cache_library_item_type_items[] = {
    {CACHE_TYPE_OBJECT,         "OBJECT",			ICON_OBJECT_DATA,       "Object", "Object base properties"},
    {CACHE_TYPE_DERIVED_MESH,   "DERIVED_MESH",     ICON_OUTLINER_OB_MESH,  "Derived Mesh", "Mesh result from modifiers"},
    {CACHE_TYPE_HAIR,           "HAIR",             ICON_PARTICLE_POINT,    "Hair", "Hair parent strands"},
    {CACHE_TYPE_HAIR_PATHS,     "HAIR_PATHS",       ICON_PARTICLE_PATH,     "Hair Paths", "Full hair paths"},
    {CACHE_TYPE_PARTICLES,      "PARTICLES",        ICON_PARTICLES,         "Particles", "Emitter particles"},
    {0, NULL, 0, NULL, NULL}
};

EnumPropertyItem cache_library_read_result_items[] = {
    {CACHE_READ_SAMPLE_INVALID,         "INVALID",      ICON_ERROR,             "Invalid",      "No valid sample found"},
    {CACHE_READ_SAMPLE_EXACT,           "EXACT",        ICON_SPACE3,            "Exact",        "Found sample for requested frame"},
    {CACHE_READ_SAMPLE_INTERPOLATED,    "INTERPOLATED", ICON_TRIA_DOWN_BAR,     "Interpolated", "Enclosing samples found for interpolation"},
    {CACHE_READ_SAMPLE_EARLY,           "EARLY",        ICON_TRIA_RIGHT_BAR,    "Early",        "Requested frame before the first sample"},
    {CACHE_READ_SAMPLE_LATE,            "LATE",         ICON_TRIA_LEFT_BAR,     "Late",         "Requested frame after the last sample"},
    {0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_object_types.h"

#include "BKE_cache_library.h"
#include "BKE_main.h"

#include "RNA_access.h"

#include "WM_api.h"

static void rna_CacheItem_name_get(PointerRNA *ptr, char *value)
{
	CacheItem *item = ptr->data;
	BKE_cache_item_name(item->ob, item->type, item->index, value);
}

static int rna_CacheItem_name_length(PointerRNA *ptr)
{
	CacheItem *item = ptr->data;
	return BKE_cache_item_name_length(item->ob, item->type, item->index);
}

static void rna_CacheItem_get_name(struct Object *ob, int type, int index, char *name)
{
	BKE_cache_item_name(ob, type, index, name);
}

/* ========================================================================= */

static void rna_CacheLibrary_update(Main *UNUSED(main), Scene *UNUSED(scene), PointerRNA *UNUSED(ptr))
{
}

static void rna_CacheLibrary_group_update(Main *main, Scene *scene, PointerRNA *ptr)
{
	CacheLibrary *cachelib = ptr->data;
	
	BKE_cache_library_group_update(main, cachelib);
	
	rna_CacheLibrary_update(main, scene, ptr);
}

/* ========================================================================= */

#if 0 /* UNUSED */
static PointerRNA rna_ObjectCache_object_get(PointerRNA *ptr)
{
	Object *ob = ptr->data;
	PointerRNA rptr;
	RNA_id_pointer_create((ID *)ob, &rptr);
	return rptr;
}

static void rna_CacheLibrary_object_caches_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	CacheLibraryObjectsIterator *internal = (CacheLibraryObjectsIterator *)(&iter->internal.listbase);
	CacheLibrary *cachelib = ptr->data;
	
	/* XXX this is not particularly elegant, but works:
	 * abuse the internal union for storing our own iterator
	 */
	BLI_STATIC_ASSERT(sizeof(iter->internal) >= sizeof(CacheLibraryObjectsIterator), "CollectionPropertyIterator internal not large enough");
	
	BKE_object_cache_iter_init(internal, cachelib);
	iter->valid = BKE_object_cache_iter_valid(internal);
}

void rna_CacheLibrary_object_caches_next(CollectionPropertyIterator *iter)
{
	CacheLibraryObjectsIterator *internal = (CacheLibraryObjectsIterator *)(&iter->internal.listbase);
	
	BKE_object_cache_iter_next(internal);
	iter->valid = BKE_object_cache_iter_valid(internal);
}

void rna_CacheLibrary_object_caches_end(CollectionPropertyIterator *iter)
{
	CacheLibraryObjectsIterator *internal = (CacheLibraryObjectsIterator *)(&iter->internal.listbase);
	
	BKE_object_cache_iter_end(internal);
}

PointerRNA rna_CacheLibrary_object_caches_get(CollectionPropertyIterator *iter)
{
	CacheLibraryObjectsIterator *internal = (CacheLibraryObjectsIterator *)(&iter->internal.listbase);
	PointerRNA rptr;
	
	RNA_pointer_create((ID *)iter->parent.id.data, &RNA_ObjectCache, BKE_object_cache_iter_get(internal), &rptr);
	
	return rptr;
}
#endif

static PointerRNA rna_CacheLibrary_cache_item_find(CacheLibrary *cachelib, Object *ob, int type, int index)
{
	CacheItem *item = BKE_cache_library_find_item(cachelib, ob, type, index);
	PointerRNA rptr;
	
	RNA_pointer_create((ID *)cachelib, &RNA_CacheItem, item, &rptr);
	return rptr;
}

/* ========================================================================= */

#if 0 /* UNUSED */
static void rna_ObjectCache_caches_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	CacheLibraryItemsIterator *internal = (CacheLibraryItemsIterator *)(&iter->internal.listbase);
	Object *ob = ptr->data;
	
	/* XXX this is not particularly elegant, but works:
	 * abuse the internal union for storing our own iterator
	 */
	BLI_STATIC_ASSERT(sizeof(iter->internal) >= sizeof(CacheLibraryItemsIterator), "CollectionPropertyIterator internal not large enough");
	
	BKE_cache_item_iter_init(internal, ob);
	iter->valid = BKE_cache_item_iter_valid(internal);
}

void rna_ObjectCache_caches_next(CollectionPropertyIterator *iter)
{
	CacheLibraryItemsIterator *internal = (CacheLibraryItemsIterator *)(&iter->internal.listbase);
	
	BKE_cache_item_iter_next(internal);
	iter->valid = BKE_cache_item_iter_valid(internal);
}

void rna_ObjectCache_caches_end(CollectionPropertyIterator *iter)
{
	CacheLibraryItemsIterator *internal = (CacheLibraryItemsIterator *)(&iter->internal.listbase);
	
	BKE_cache_item_iter_end(internal);
}

PointerRNA rna_ObjectCache_caches_get(CollectionPropertyIterator *iter)
{
	CacheLibraryItemsIterator *internal = (CacheLibraryItemsIterator *)(&iter->internal.listbase);
	PointerRNA rptr;
	
	/* XXX this returns a temporary pointer that becomes invalid after iteration, potentially dangerous! */
	RNA_pointer_create((ID *)iter->parent.id.data, &RNA_CacheItem, internal->cur, &rptr);
	
	return rptr;
}
#endif

#else

static void rna_def_cache_item(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *prop, *parm;
	
	srna = RNA_def_struct(brna, "CacheItem", NULL);
	RNA_def_struct_ui_text(srna, "Cache Item", "Description of a cacheable item in an object");
	
	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ob");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object", "");
	
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, cache_library_item_type_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Type", "Type of cached data");
	
	prop = RNA_def_property(srna, "index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "index");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Index", "Index of the cached data");
	
	prop = RNA_def_property(srna, "enabled", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CACHE_ITEM_ENABLED);
	RNA_def_property_ui_text(prop, "Enabled", "Enable caching for this item");
	
	prop = RNA_def_property(srna, "read_result", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "read_result");
	RNA_def_property_enum_items(prop, cache_library_read_result_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Read Result", "Result of cache read");
	
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_maxlength(prop, 2*MAX_NAME);
	RNA_def_property_string_funcs(prop, "rna_CacheItem_name_get", "rna_CacheItem_name_length", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);
	
	func = RNA_def_function(srna, "get_name", "rna_CacheItem_get_name");
	RNA_def_function_flag(func, FUNC_NO_SELF);
	RNA_def_function_ui_description(func, "Get name of items from properties without an instance");
	parm = RNA_def_pointer(func, "object", "Object", "Object", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_enum(func, "type", cache_library_item_type_items, CACHE_TYPE_OBJECT, "Type", "Type of cache item");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_int(func, "index", -1, -1, INT_MAX, "Index", "Index of the data in it's' collection", -1, INT_MAX);
	parm = RNA_def_string(func, "name", NULL, 2*MAX_NAME, "Name", "");
	RNA_def_property_flag(parm, PROP_THICK_WRAP);
	RNA_def_function_output(func, parm);
}

#if 0 /* UNUSED */
static void rna_def_object_cache(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "ObjectCache", NULL);
	RNA_def_struct_ui_text(srna, "Object Cache", "Cacheable data in an Object");
	
	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_funcs(prop, "rna_ObjectCache_object_get", NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object", "");
	
	prop = RNA_def_property(srna, "caches", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "CacheItem");
	RNA_def_property_collection_funcs(prop, "rna_ObjectCache_caches_begin", "rna_ObjectCache_caches_next", "rna_ObjectCache_caches_end",
	                                  "rna_ObjectCache_caches_get", NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Caches", "Cacheable items for in an object cache");
}
#endif

static void rna_def_cache_library(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *prop, *parm;
	
	static EnumPropertyItem eval_mode_items[] = {
	    {CACHE_LIBRARY_EVAL_VIEWPORT,   "VIEWPORT",     ICON_RESTRICT_VIEW_OFF,     "Viewport",     "Evaluate data with viewport settings"},
	    {CACHE_LIBRARY_EVAL_RENDER,     "RENDER",       ICON_RESTRICT_RENDER_OFF,   "Render",       "Evaluate data with render settings"},
	    {0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "CacheLibrary", "ID");
	RNA_def_struct_ui_text(srna, "Cache Library", "Cache Library datablock for constructing an archive of caches");
	RNA_def_struct_ui_icon(srna, ICON_PHYSICS);
	
	prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_sdna(prop, NULL, "filepath");
	RNA_def_property_ui_text(prop, "File Path", "Path to cache library storage");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
	prop = RNA_def_property(srna, "group", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Group", "Cached object group");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_group_update");
	
	prop = RNA_def_property(srna, "eval_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "eval_mode");
	RNA_def_property_enum_items(prop, eval_mode_items);
	RNA_def_property_enum_default(prop, CACHE_LIBRARY_EVAL_VIEWPORT);
	RNA_def_property_ui_text(prop, "Evaluation Mode", "Mode to use when evaluating data");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
#if 0 /* UNUSED */
	prop = RNA_def_property(srna, "object_caches", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "ObjectCache");
	RNA_def_property_collection_funcs(prop, "rna_CacheLibrary_object_caches_begin", "rna_CacheLibrary_object_caches_next", "rna_CacheLibrary_object_caches_end",
	                                  "rna_CacheLibrary_object_caches_get", NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Object Caches", "Cacheable objects inside the cache library group");
#endif
	
	func = RNA_def_function(srna, "cache_item_find", "rna_CacheLibrary_cache_item_find");
	RNA_def_function_ui_description(func, "Find item for an object cache item");
	parm = RNA_def_pointer(func, "object", "Object", "Object", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_enum(func, "type", cache_library_item_type_items, CACHE_TYPE_OBJECT, "Type", "Type of cache item");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_int(func, "index", -1, -1, INT_MAX, "Index", "Index of the data in it's' collection", -1, INT_MAX);
	parm = RNA_def_pointer(func, "item", "CacheItem", "Item", "Item in the cache");
	RNA_def_property_flag(parm, PROP_RNAPTR);
	RNA_def_function_return(func, parm);
}

void RNA_def_cache_library(BlenderRNA *brna)
{
	rna_def_cache_item(brna);
#if 0 /* UNUSED */
	rna_def_object_cache(brna);
#endif
	rna_def_cache_library(brna);
}

#endif

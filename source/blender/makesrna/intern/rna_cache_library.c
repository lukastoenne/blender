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

#include "DNA_cache_library_types.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"

#include "DNA_object_types.h"

#include "BKE_cache_library.h"
#include "BKE_main.h"

#include "RNA_access.h"

#include "WM_api.h"

static void rna_CacheLibrary_update(Main *UNUSED(main), Scene *UNUSED(scene), PointerRNA *UNUSED(ptr))
{
}

/* ========================================================================= */

static PointerRNA rna_ObjectCache_object_get(PointerRNA *ptr)
{
	Object *ob = ptr->data;
	PointerRNA rptr;
	RNA_id_pointer_create((ID *)ob, &rptr);
	return rptr;
}

/* ========================================================================= */

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

#else

static void rna_def_cache_item_path(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem cache_path_type_items[] = {
	    {CACHE_TYPE_OBJECT,         "OBJECT",			ICON_OBJECT_DATA,       "Object", "Object base properties"},
	    {CACHE_TYPE_DERIVED_MESH,   "DERIVED_MESH",     ICON_OUTLINER_OB_MESH,  "Derived Mesh", "Mesh result from modifiers"},
	    {CACHE_TYPE_HAIR,           "HAIR",             ICON_PARTICLE_POINT,    "Hair", "Hair parent strands"},
	    {CACHE_TYPE_HAIR_PATHS,     "HAIR_PATHS",       ICON_PARTICLE_PATH,     "Hair Paths", "Full hair paths"},
	    {0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "CacheItemPath", NULL);
	RNA_def_struct_ui_text(srna, "Cache Path", "Description of a cacheable item in an object group");
	
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, cache_path_type_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Type", "Type of cached data");
	
	prop = RNA_def_property(srna, "cache_id_data", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "ID", "ID datablock");
	
	prop = RNA_def_property(srna, "index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "index");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Index", "Index of the cached data");
}

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
}

static void rna_def_cache_item(BlenderRNA *brna)
{
	StructRNA *srna;
//	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "CacheItem", NULL);
	RNA_def_struct_ui_text(srna, "Cache Item", "Cached item included in a Cache Library");
}

static void rna_def_cache_library(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
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
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
	prop = RNA_def_property(srna, "object_caches", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "ObjectCache");
	RNA_def_property_collection_funcs(prop, "rna_CacheLibrary_object_caches_begin", "rna_CacheLibrary_object_caches_next", "rna_CacheLibrary_object_caches_end",
	                                  "rna_CacheLibrary_object_caches_get", NULL, NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Object Caches", "Cacheable objects inside the cache library group");
}

void RNA_def_cache_library(BlenderRNA *brna)
{
	rna_def_cache_item_path(brna);
	rna_def_object_cache(brna);
	rna_def_cache_item(brna);
	rna_def_cache_library(brna);
}

#endif

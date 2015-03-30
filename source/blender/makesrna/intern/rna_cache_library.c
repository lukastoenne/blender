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

EnumPropertyItem cache_modifier_type_items[] = {
	{eCacheModifierType_HairSimulation, "HAIR_SIMULATION", ICON_HAIR, "Hair Simulation", ""},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_object_types.h"

#include "BKE_animsys.h"
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

static PointerRNA rna_CacheLibrary_cache_item_find(CacheLibrary *cachelib, Object *ob, int type, int index)
{
	CacheItem *item = BKE_cache_library_find_item(cachelib, ob, type, index);
	PointerRNA rptr;
	
	RNA_pointer_create((ID *)cachelib, &RNA_CacheItem, item, &rptr);
	return rptr;
}

/* ========================================================================= */

static void rna_CacheLibraryModifier_name_set(PointerRNA *ptr, const char *value)
{
	CacheModifier *md = ptr->data;
	char oldname[sizeof(md->name)];
	
	/* make a copy of the old name first */
	BLI_strncpy(oldname, md->name, sizeof(md->name));
	
	/* copy the new name into the name slot */
	BLI_strncpy_utf8(md->name, value, sizeof(md->name));
	
	/* make sure the name is truly unique */
	if (ptr->id.data) {
		CacheLibrary *cachelib = ptr->id.data;
		BKE_cache_modifier_unique_name(&cachelib->modifiers, md);
	}
	
	/* fix all the animation data which may link to this */
	BKE_all_animdata_fix_paths_rename(NULL, "modifiers", oldname, md->name);
}

static char *rna_CacheLibraryModifier_path(PointerRNA *ptr)
{
	CacheModifier *md = ptr->data;
	char name_esc[sizeof(md->name) * 2];

	BLI_strescape(name_esc, md->name, sizeof(name_esc));
	return BLI_sprintfN("modifiers[\"%s\"]", name_esc);
}

static CacheModifier *rna_CacheLibrary_modifier_new(CacheLibrary *cachelib, bContext *UNUSED(C), ReportList *UNUSED(reports),
                                                    const char *name, int type)
{
	return BKE_cache_modifier_add(cachelib, name, type);
}

static void rna_CacheLibrary_modifier_remove(CacheLibrary *cachelib, bContext *UNUSED(C), ReportList *UNUSED(reports), PointerRNA *md_ptr)
{
	CacheModifier *md = md_ptr->data;
	
	BKE_cache_modifier_remove(cachelib, md);

	RNA_POINTER_INVALIDATE(md_ptr);
}

static void rna_CacheLibrary_modifier_clear(CacheLibrary *cachelib, bContext *UNUSED(C))
{
	BKE_cache_modifier_clear(cachelib);
}

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

static void rna_def_cache_modifier(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "CacheLibraryModifier", NULL);
	RNA_def_struct_sdna(srna, "CacheModifier");
	RNA_def_struct_path_func(srna, "rna_CacheLibraryModifier_path");
	RNA_def_struct_ui_text(srna, "Cache Modifier", "Cache Modifier");
	RNA_def_struct_ui_icon(srna, ICON_PHYSICS);
	
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, cache_modifier_type_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Type", "Type of the cache modifier");
	
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_CacheLibraryModifier_name_set");
	RNA_def_property_ui_text(prop, "Name", "Modifier name");
	RNA_def_property_update(prop, NC_ID | NA_RENAME, NULL);
	RNA_def_struct_name_property(srna, prop);
	
	prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_sdna(prop, NULL, "filepath");
	RNA_def_property_ui_text(prop, "File Path", "Path to cache modifier output storage");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
}

static void rna_def_cache_library_modifiers(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;
	
	RNA_def_property_srna(cprop, "CacheLibraryModifiers");
	srna = RNA_def_struct(brna, "CacheLibraryModifiers", NULL);
	RNA_def_struct_sdna(srna, "CacheLibrary");
	RNA_def_struct_ui_text(srna, "Cache Modifiers", "Collection of cache modifiers");
	
	/* add modifier */
	func = RNA_def_function(srna, "new", "rna_CacheLibrary_modifier_new");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Add a new modifier");
	parm = RNA_def_string(func, "name", "Name", 0, "", "New name for the modifier");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* modifier to add */
	parm = RNA_def_enum(func, "type", cache_modifier_type_items, 1, "", "Modifier type to add");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm = RNA_def_pointer(func, "modifier", "CacheLibraryModifier", "", "Newly created modifier");
	RNA_def_function_return(func, parm);
	
	/* remove modifier */
	func = RNA_def_function(srna, "remove", "rna_CacheLibrary_modifier_remove");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Remove an existing modifier");
	/* modifier to remove */
	parm = RNA_def_pointer(func, "modifier", "CacheLibraryModifier", "", "Modifier to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);
	
	/* clear all modifiers */
	func = RNA_def_function(srna, "clear", "rna_CacheLibrary_modifier_clear");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Remove all modifiers");
}

static void rna_def_cache_library(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *prop, *parm;
	
	static EnumPropertyItem eval_mode_items[] = {
	    {CACHE_LIBRARY_EVAL_REALTIME,   "REALTIME",     ICON_RESTRICT_VIEW_OFF,     "Realtime",     "Evaluate data with realtime settings"},
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
	
	prop = RNA_def_property(srna, "eval_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "eval_mode");
	RNA_def_property_enum_items(prop, eval_mode_items);
	RNA_def_property_flag(prop, PROP_ENUM_FLAG);
	RNA_def_property_ui_text(prop, "Evaluation Mode", "Mode to use when evaluating data");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
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
	
	/* modifiers */
	prop = RNA_def_property(srna, "modifiers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "CacheLibraryModifier");
	RNA_def_property_ui_text(prop, "Modifiers", "Modifiers applying to the cached data");
	rna_def_cache_library_modifiers(brna, prop);
}

void RNA_def_cache_library(BlenderRNA *brna)
{
	rna_def_cache_item(brna);
	rna_def_cache_modifier(brna);
	rna_def_cache_library(brna);
}

#endif

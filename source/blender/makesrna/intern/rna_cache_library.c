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

#include "RNA_access.h"

#include "BKE_cache_library.h"

#include "WM_api.h"

static void rna_CacheLibrary_update(Main *UNUSED(main), Scene *UNUSED(scene), PointerRNA *UNUSED(ptr))
{
}

#else

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
}

void RNA_def_cache_library(BlenderRNA *brna)
{
	rna_def_cache_library(brna);
}

#endif

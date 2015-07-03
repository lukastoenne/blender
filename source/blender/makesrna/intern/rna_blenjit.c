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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_blenjit.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "BLI_utildefines.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "rna_internal.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "BKE_report.h"

static void rna_blenjit_loaded_modules_begin(struct CollectionPropertyIterator *iter, struct PointerRNA *ptr)
{
//	iter->
}

static void rna_blenjit_loaded_modules_next(struct CollectionPropertyIterator *iter)
{
}

static void rna_blenjit_loaded_modules_end(struct CollectionPropertyIterator *iter)
{
}

static PointerRNA rna_blenjit_loaded_modules_get(struct CollectionPropertyIterator *iter)
{
	PointerRNA r_ptr;
	return r_ptr;
}

static int rna_blenjit_loaded_modules_length(struct PointerRNA *ptr)
{
	return 0;
}

static int rna_blenjit_loaded_modules_lookupint(struct PointerRNA *ptr, int key, struct PointerRNA *r_ptr)
{
	return 0;
}

static int rna_blenjit_loaded_modules_lookupstring(struct PointerRNA *ptr, const char *key, struct PointerRNA *r_ptr)
{
	return 0;
}

#else

static void rna_def_blenjit_module(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "BlenJITModule", NULL);
	RNA_def_struct_ui_text(srna, "Module", "Collection of JIT-compiled functions");
}

static void rna_def_blenjit_manager(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	PropertyRNA *parm;

	srna = RNA_def_struct(brna, "BlenJITManager", NULL);
	RNA_def_struct_ui_text(srna, "BlenJIT Manager", "Manager for JIT-compiled code");

	prop = RNA_def_property(srna, "loaded_modules", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_blenjit_loaded_modules_begin", "rna_blenjit_loaded_modules_next", "rna_blenjit_loaded_modules_end",
	                                  "rna_blenjit_loaded_modules_get", "rna_blenjit_loaded_modules_length",
	                                  "rna_blenjit_loaded_modules_lookupint", "rna_blenjit_loaded_modules_lookupstring", NULL);
	RNA_def_property_struct_type(prop, "BlenJITModule");
	RNA_def_property_ui_text(prop, "Loaded Modules", "Loaded modules");
}

void RNA_def_blenjit(BlenderRNA *brna)
{
	rna_def_blenjit_module(brna);
	rna_def_blenjit_manager(brna);
}

#endif

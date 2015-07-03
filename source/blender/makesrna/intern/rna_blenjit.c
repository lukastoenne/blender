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
#include <string.h>

#include "BLI_utildefines.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "rna_internal.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "BKE_report.h"

#include "BJIT_modules.h"

static PointerRNA rna_blenjit_manager_get(void)
{
	/* XXX actually BlenJITManager is a set of global functions,
	 * we use a dummy value to create a non-NULL pointer.
	 */
	static const int dummy_value = 1;
	PointerRNA r_ptr;
	RNA_pointer_create(NULL, &RNA_BlenJITManager, (void *)(&dummy_value), &r_ptr);
	return r_ptr;
}

static void rna_blenjit_loaded_modules_begin(struct CollectionPropertyIterator *iter, struct PointerRNA *UNUSED(ptr))
{
	int length = BJIT_num_loaded_modules();
	/* we use the array iterator simply as a counter */
	ArrayIterator *internal;
	
	internal = &iter->internal.array;
	memset(internal, 0, sizeof(*internal));
	internal->ptr = SET_INT_IN_POINTER(0);
	internal->length = length;
	
	iter->valid = (length > 0);
}

static void rna_blenjit_loaded_modules_next(struct CollectionPropertyIterator *iter)
{
	ArrayIterator *internal = &iter->internal.array;
	int index = GET_INT_FROM_POINTER(internal->ptr) + 1;

	internal->ptr = SET_INT_IN_POINTER(index);
	iter->valid = (index < internal->length);
}

static void rna_blenjit_loaded_modules_end(struct CollectionPropertyIterator *UNUSED(iter))
{
}

static PointerRNA rna_blenjit_loaded_modules_get(struct CollectionPropertyIterator *iter)
{
	ArrayIterator *internal = &iter->internal.array;
	int index = GET_INT_FROM_POINTER(internal->ptr);
	struct LLVMModule *module = BJIT_get_loaded_module_n(index);
	PointerRNA r_ptr;
	
	RNA_pointer_create(iter->ptr.id.data, &RNA_BlenJITModule, module, &r_ptr);
	return r_ptr;
}

static int rna_blenjit_loaded_modules_length(struct PointerRNA *UNUSED(ptr))
{
	return BJIT_num_loaded_modules();
}

static int rna_blenjit_loaded_modules_lookupint(struct PointerRNA *ptr, int key, struct PointerRNA *r_ptr)
{
	int length = BJIT_num_loaded_modules();
	if (key < length) {
		struct LLVMModule *module = BJIT_get_loaded_module_n(key);
		RNA_pointer_create(ptr->id.data, &RNA_BlenJITModule, module, r_ptr);
		return (module != NULL);
	}
	return false;
}

static int rna_blenjit_loaded_modules_lookupstring(struct PointerRNA *ptr, const char *key, struct PointerRNA *r_ptr)
{
	struct LLVMModule *module = BJIT_get_loaded_module(key);
	if (module) {
		RNA_pointer_create(ptr->id.data, &RNA_BlenJITModule, module, r_ptr);
		return true;
	}
	return false;
}

/* ------------------------------------------------------------------------- */

static void rna_BlenJITModule_name_get(PointerRNA *ptr, char *value)
{
	struct LLVMModule *module = ptr->data;
	strcpy(value, BJIT_module_name(module));
}

static int rna_BlenJITModule_name_length(PointerRNA *ptr)
{
	struct LLVMModule *module = ptr->data;
	return strlen(BJIT_module_name(module));
}

static void rna_blenjit_functions_begin(struct CollectionPropertyIterator *iter, struct PointerRNA *ptr)
{
	struct LLVMModule *module = ptr->data;
	int length = BJIT_module_num_functions(module);
	/* we use the array iterator simply as a counter */
	ArrayIterator *internal;
	
	internal = &iter->internal.array;
	memset(internal, 0, sizeof(*internal));
	internal->ptr = SET_INT_IN_POINTER(0);
	internal->length = length;
	
	iter->valid = (length > 0);
}

static void rna_blenjit_functions_next(struct CollectionPropertyIterator *iter)
{
	ArrayIterator *internal = &iter->internal.array;
	int index = GET_INT_FROM_POINTER(internal->ptr) + 1;

	internal->ptr = SET_INT_IN_POINTER(index);
	iter->valid = (index < internal->length);
}

static void rna_blenjit_functions_end(struct CollectionPropertyIterator *UNUSED(iter))
{
}

static PointerRNA rna_blenjit_functions_get(struct CollectionPropertyIterator *iter)
{
	struct LLVMModule *module = iter->ptr.data;
	ArrayIterator *internal = &iter->internal.array;
	int index = GET_INT_FROM_POINTER(internal->ptr);
	struct LLVMFunction *function = BJIT_module_get_function_n(module, index);
	PointerRNA r_ptr;
	
	RNA_pointer_create(iter->ptr.id.data, &RNA_BlenJITFunction, function, &r_ptr);
	return r_ptr;
}

static int rna_blenjit_functions_length(struct PointerRNA *ptr)
{
	struct LLVMModule *module = ptr->data;
	return BJIT_module_num_functions(module);
}

static int rna_blenjit_functions_lookupint(struct PointerRNA *ptr, int key, struct PointerRNA *r_ptr)
{
	struct LLVMModule *module = ptr->data;
	int length = BJIT_module_num_functions(module);
	if (key < length) {
		struct LLVMFunction *function = BJIT_module_get_function_n(module, key);
		RNA_pointer_create(ptr->id.data, &RNA_BlenJITFunction, function, r_ptr);
		return (function != NULL);
	}
	return false;
}

static int rna_blenjit_functions_lookupstring(struct PointerRNA *ptr, const char *key, struct PointerRNA *r_ptr)
{
	struct LLVMModule *module = ptr->data;
	struct LLVMFunction *function = BJIT_module_get_function(module, key);
	if (function) {
		RNA_pointer_create(ptr->id.data, &RNA_BlenJITFunction, function, r_ptr);
		return true;
	}
	return false;
}

#else

static void rna_def_blenjit_function(BlenderRNA *brna)
{
	StructRNA *srna;
	
	srna = RNA_def_struct(brna, "BlenJITFunction", NULL);
	RNA_def_struct_ui_text(srna, "Function", "");
}

static void rna_def_blenjit_module(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "BlenJITModule", NULL);
	RNA_def_struct_ui_text(srna, "Module", "Collection of JIT-compiled functions");
	
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_BlenJITModule_name_get", "rna_BlenJITModule_name_length", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Name", "Name of the module");
	
	prop = RNA_def_property(srna, "functions", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_blenjit_functions_begin", "rna_blenjit_functions_next", "rna_blenjit_functions_end",
	                                  "rna_blenjit_functions_get", "rna_blenjit_functions_length",
	                                  "rna_blenjit_functions_lookupint", "rna_blenjit_functions_lookupstring", NULL);
	RNA_def_property_struct_type(prop, "BlenJITFunction");
	RNA_def_property_ui_text(prop, "Functions", "Functions contained in the module");
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
	
	func = RNA_def_function(srna, "get", "rna_blenjit_manager_get");
	RNA_def_function_flag(func, FUNC_NO_SELF);
	RNA_def_property_ui_text(prop, "Loaded Modules", "Loaded modules");
	parm = RNA_def_property(func, "result", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(parm, "BlenJITManager");
	RNA_def_property_flag(parm, PROP_RNAPTR);
	RNA_def_function_return(func, parm);
}

void RNA_def_blenjit(BlenderRNA *brna)
{
	rna_def_blenjit_function(brna);
	rna_def_blenjit_module(brna);
	rna_def_blenjit_manager(brna);
}

#endif

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
 * Contributor(s): Blender Foundation (2015), Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_object_dupli.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "DNA_object_types.h"

#include "RNA_define.h"

#include "rna_internal.h"

#include "WM_api.h"
#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_string.h"

#include "BKE_anim.h"
#include "BKE_context.h"

#include "RNA_access.h"

static void rna_DupliContainer_add(struct DupliContainer *cont, struct Object *ob, float *matrix, int index,
                                   int recursive)
{
	bool animated = false;
	bool hide = false;
	
	BKE_dupli_add_instance(cont, ob, (float (*)[4])matrix, index, animated, hide, recursive);
}

#else

static void rna_def_dupli_container(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;

	srna = RNA_def_struct(brna, "DupliContainer", NULL);
	RNA_def_struct_ui_text(srna, "Dupli Container", "Container for creating dupli instances");

	func = RNA_def_function(srna, "add", "rna_DupliContainer_add");
	RNA_def_function_ui_description(func, "Create a dupli instance");
	parm = RNA_def_pointer(func, "object", "Object", "Object", "Object to instantiate");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_property(func, "matrix", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_multi_array(parm, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_ui_text(parm, "Matrix", "Worldspace transformation matrix of the instance");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_int(func, "index", 0, INT_MIN, INT_MAX, "Index", "Index of the instance", INT_MIN, INT_MAX);
	RNA_def_boolean(func, "recursive", false, "Recursive", "Recursively add duplis from the instanced object");
}

void RNA_def_object_dupli(BlenderRNA *brna)
{
	rna_def_dupli_container(brna);
}

#endif

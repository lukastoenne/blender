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
 * Contributor(s): Blender Foundation (2014).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_depsgraph.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "BLI_utildefines.h"
#include "BLI_path_util.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"


static void rna_Depsgraph_debug_graphviz(Depsgraph *graph, const char *filename)
{
	FILE *f = fopen(filename, "w");
	if (f == NULL)
		return;
	
	DEG_debug_graphviz(graph, f);
	
	fclose(f);
}

#else

static void rna_def_depsgraph(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	PropertyRNA *parm;
	
	srna = RNA_def_struct(brna, "Depsgraph", NULL);
	RNA_def_struct_ui_text(srna, "Dependency Graph", "");
	
	func = RNA_def_function(srna, "debug_graphviz", "rna_Depsgraph_debug_graphviz");
	parm = RNA_def_string_file_path(func, "filename", NULL, FILE_MAX, "File Name",
	                                "File in which to store graphviz debug output");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

void RNA_def_depsgraph(BlenderRNA *brna)
{
	rna_def_depsgraph(brna);
}

#endif

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

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

#include <string.h>

#include "BLI_string.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "intern/depsgraph_types.h"

static void rna_DepsNode_name_get(PointerRNA *ptr, char *value)
{
	DepsNode *node = ptr->data;
	strcpy(value, node->name);
}

static int rna_DepsNode_name_length(PointerRNA *ptr)
{
	DepsNode *node = ptr->data;
	return strlen(node->name);
}

static PointerRNA rna_Depsgraph_root_node_get(PointerRNA *ptr)
{
	Depsgraph *graph = ptr->data;
	PointerRNA root_node_ptr;
	RNA_pointer_create(ptr->id.data, &RNA_DepsNode, graph->root_node, &root_node_ptr);
	return root_node_ptr;
}

static void rna_Depsgraph_debug_graphviz(Depsgraph *graph, const char *filename)
{
	FILE *f = fopen(filename, "w");
	if (f == NULL)
		return;
	
	DEG_debug_graphviz(graph, f);
	
	fclose(f);
}

#else

static void rna_def_depsnode(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "DepsNode", NULL);
	RNA_def_struct_ui_text(srna, "Dependency Node", "");
	
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_DepsNode_name_get", "rna_DepsNode_name_length", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Name", "Identifier of the node");
	RNA_def_struct_name_property(srna, prop);
}

static void rna_def_depsgraph(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	PropertyRNA *parm;
	
	srna = RNA_def_struct(brna, "Depsgraph", NULL);
	RNA_def_struct_ui_text(srna, "Dependency Graph", "");
	
	prop = RNA_def_property(srna, "root_node", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "DepsNode");
	RNA_def_property_pointer_funcs(prop, "rna_Depsgraph_root_node_get", NULL, NULL, NULL);
	
	func = RNA_def_function(srna, "debug_graphviz", "rna_Depsgraph_debug_graphviz");
	parm = RNA_def_string_file_path(func, "filename", NULL, FILE_MAX, "File Name",
	                                "File in which to store graphviz debug output");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

void RNA_def_depsgraph(BlenderRNA *brna)
{
	rna_def_depsnode(brna);
	rna_def_depsgraph(brna);
}

#endif

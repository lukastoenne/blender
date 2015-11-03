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
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_blenvm.c
 *  \ingroup RNA
 */

#include <stdlib.h>
#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BLI_path_util.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

#include "BVM_api.h"

#include "RNA_access.h"

#include "WM_api.h"

static int rna_BVMCompileContext_get_object_index(struct BVMCompileContext *context, struct Object *ob)
{
	return BVM_compile_get_object_index(context, ob);
}

static struct BVMNodeInstance *rna_BVMNodeGraph_add_node(struct BVMNodeGraph *graph, const char *type, const char *name)
{
	return BVM_nodegraph_add_node(graph, type, name);
}

static void rna_BVMNodeGraph_add_link(struct BVMNodeGraph *graph,
                                    struct BVMNodeInstance *from_node, const char *from_socket,
                                    struct BVMNodeInstance *to_node, const char *to_socket)
{
	return BVM_nodegraph_add_link(graph, from_node, from_socket, to_node, to_socket);
}

static void rna_BVMNodeGraph_set_output(struct BVMNodeGraph *graph,
                                      const char *name, struct BVMNodeInstance *node, const char *socket)
{
	return BVM_nodegraph_set_output_link(graph, name, node, socket);
}

/* ------------------------------------------------------------------------- */

static void rna_BVMNodeInstance_set_value_float(struct BVMNodeInstance *node, const char *socket, float value)
{
	BVM_node_set_input_value_float(node, socket, value);
}

static void rna_BVMNodeInstance_set_value_float3(struct BVMNodeInstance *node, const char *socket, float value[3])
{
	BVM_node_set_input_value_float3(node, socket, value);
}

static void rna_BVMNodeInstance_set_value_float4(struct BVMNodeInstance *node, const char *socket, float value[4])
{
	BVM_node_set_input_value_float4(node, socket, value);
}

static void rna_BVMNodeInstance_set_value_matrix44(struct BVMNodeInstance *node, const char *socket, float value[16])
{
	BVM_node_set_input_value_matrix44(node, socket, (float (*)[4])value);
}

static void rna_BVMNodeInstance_set_value_int(struct BVMNodeInstance *node, const char *socket, int value)
{
	BVM_node_set_input_value_int(node, socket, value);
}

#else

static void rna_def_bvm_compile_context(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;
	
	srna = RNA_def_struct(brna, "BVMCompileContext", NULL);
	RNA_def_struct_ui_text(srna, "Compile Context", "Context information for compiling a node tree");
	
	func = RNA_def_function(srna, "get_object_index", "rna_BVMCompileContext_get_object_index");
	RNA_def_function_ui_description(func, "Get the index for an object to use in compiled code");
	parm = RNA_def_pointer(func, "object", "Object", "Object", "");
	RNA_def_property_flag(parm, PROP_NEVER_NULL);
	parm = RNA_def_int(func, "index", -1, -1, INT_MAX, "Index", "Index used for the object in compiled code", -1, INT_MAX);
	RNA_def_function_return(func, parm);
}

static void rna_def_bvm_node_instance(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	static const float zeros[16] = {0.0f};
	
	srna = RNA_def_struct(brna, "BVMNodeInstance", NULL);
	RNA_def_struct_ui_text(srna, "Node Instance", "Node in the internal BVM graph");
	
	func = RNA_def_function(srna, "set_value_float", "rna_BVMNodeInstance_set_value_float");
	RNA_def_function_ui_description(func, "Set an input value constant");
	RNA_def_string(func, "name", NULL, 0, "Name", "Name of the output");
	RNA_def_float(func, "value", 0.0f, -FLT_MAX, FLT_MAX, "Value", "", -FLT_MAX, FLT_MAX);
	
	func = RNA_def_function(srna, "set_value_float3", "rna_BVMNodeInstance_set_value_float3");
	RNA_def_function_ui_description(func, "Set an input value constant");
	RNA_def_string(func, "name", NULL, 0, "Name", "Name of the output");
	RNA_def_float_array(func, "value", 3, zeros, -FLT_MAX, FLT_MAX, "Value", "", -FLT_MAX, FLT_MAX);
	
	func = RNA_def_function(srna, "set_value_float4", "rna_BVMNodeInstance_set_value_float4");
	RNA_def_function_ui_description(func, "Set an input value constant");
	RNA_def_string(func, "name", NULL, 0, "Name", "Name of the output");
	RNA_def_float_array(func, "value", 4, zeros, -FLT_MAX, FLT_MAX, "Value", "", -FLT_MAX, FLT_MAX);
	
	func = RNA_def_function(srna, "set_value_matrix44", "rna_BVMNodeInstance_set_value_matrix44");
	RNA_def_function_ui_description(func, "Set an input value constant");
	RNA_def_string(func, "name", NULL, 0, "Name", "Name of the output");
	RNA_def_float_matrix(func, "value", 4, 4, zeros, -FLT_MAX, FLT_MAX, "Value", "", -FLT_MAX, FLT_MAX);
	
	func = RNA_def_function(srna, "set_value_int", "rna_BVMNodeInstance_set_value_int");
	RNA_def_function_ui_description(func, "Set an input value constant");
	RNA_def_string(func, "name", NULL, 0, "Name", "Name of the output");
	RNA_def_int(func, "value", 0, INT_MIN, INT_MAX, "Value", "", INT_MIN, INT_MAX);
}

static void rna_def_bvm_node_graph(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;
	
	srna = RNA_def_struct(brna, "BVMNodeGraph", NULL);
	RNA_def_struct_ui_text(srna, "Node Graph", "Graph of instruction nodes");
	
	func = RNA_def_function(srna, "add_node", "rna_BVMNodeGraph_add_node");
	RNA_def_function_ui_description(func, "Add a new internal node");
	RNA_def_string(func, "type", NULL, 0, "Type", "Type of the node");
	RNA_def_string(func, "name", NULL, 0, "Name", "Name of the node");
	parm = RNA_def_pointer(func, "node", "BVMNodeInstance", "Node", "");
	RNA_def_function_return(func, parm);
	
	func = RNA_def_function(srna, "add_link", "rna_BVMNodeGraph_add_link");
	RNA_def_function_ui_description(func, "Add a new node connection");
	parm = RNA_def_pointer(func, "from_node", "BVMNodeInstance", "Source Node", "");
	RNA_def_property_flag(parm, PROP_NEVER_NULL);
	RNA_def_string(func, "from_socket", NULL, 0, "Source Socket", "Source socket name");
	parm = RNA_def_pointer(func, "to_node", "BVMNodeInstance", "Target Node", "");
	RNA_def_property_flag(parm, PROP_NEVER_NULL);
	RNA_def_string(func, "to_socket", NULL, 0, "Target Socket", "Target socket name");
	
	func = RNA_def_function(srna, "set_output", "rna_BVMNodeGraph_set_output");
	RNA_def_function_ui_description(func, "Define a node socket as an output value");
	RNA_def_string(func, "name", NULL, 0, "Name", "Output slot name");
	parm = RNA_def_pointer(func, "node", "BVMNodeInstance", "Node", "");
	RNA_def_property_flag(parm, PROP_NEVER_NULL);
	RNA_def_string(func, "socket", NULL, 0, "Socket", "Socket name");
}

void RNA_def_blenvm(BlenderRNA *brna)
{
	rna_def_bvm_compile_context(brna);
	rna_def_bvm_node_instance(brna);
	rna_def_bvm_node_graph(brna);
}

#endif

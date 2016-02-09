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

#include "BVM_api.h"

#ifdef RNA_RUNTIME

#include "RNA_access.h"

#include "WM_api.h"

static struct BVMNodeInstance *rna_BVMNodeCompiler_add_node(struct BVMNodeCompiler *compiler, const char *type)
{
	return BVM_node_compiler_add_node(compiler, type);
}

static struct BVMNodeInput *rna_BVMNodeCompiler_get_input(struct BVMNodeCompiler *compiler, const char *name)
{
	return BVM_node_compiler_get_input(compiler, name);
}

static struct BVMNodeOutput *rna_BVMNodeCompiler_get_output(struct BVMNodeCompiler *compiler, const char *name)
{
	return BVM_node_compiler_get_output(compiler, name);
}

/* ------------------------------------------------------------------------- */

static void rna_BVMNodeInput_name_get(PointerRNA *ptr, char *value)
{
	strcpy(value, BVM_node_input_name(ptr->data));
}

static int rna_BVMNodeInput_name_length(PointerRNA *ptr)
{
	return strlen(BVM_node_input_name(ptr->data));
}

static PointerRNA rna_BVMNodeInput_typedesc_get(PointerRNA *ptr)
{
	struct BVMTypeDesc *typedesc = BVM_node_input_typedesc(ptr->data);
	PointerRNA r_ptr;
	RNA_pointer_create(ptr->id.data, &RNA_BVMTypeDesc, typedesc, &r_ptr);
	return r_ptr;
}

static int rna_BVMNodeInput_value_type_get(PointerRNA *ptr)
{
	return BVM_node_input_value_type(ptr->data);
}

static void rna_BVMNodeOutput_name_get(PointerRNA *ptr, char *value)
{
	strcpy(value, BVM_node_output_name(ptr->data));
}

static int rna_BVMNodeOutput_name_length(PointerRNA *ptr)
{
	return strlen(BVM_node_output_name(ptr->data));
}

static PointerRNA rna_BVMNodeOutput_typedesc_get(PointerRNA *ptr)
{
	struct BVMTypeDesc *typedesc = BVM_node_output_typedesc(ptr->data);
	PointerRNA r_ptr;
	RNA_pointer_create(ptr->id.data, &RNA_BVMTypeDesc, typedesc, &r_ptr);
	return r_ptr;
}

static int rna_BVMNodeOutput_value_type_get(PointerRNA *ptr)
{
	return BVM_node_output_value_type(ptr->data);
}

static void rna_BVMNodeInstance_inputs_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	struct BVMNodeInstance *node = ptr->data;
	ArrayIterator *internal = &iter->internal.array;
	
	internal->ptr = SET_INT_IN_POINTER(0);
	internal->length = BVM_node_num_inputs(node);
	
	iter->valid = (0 < internal->length);
}

static void rna_BVMNodeInstance_inputs_next(CollectionPropertyIterator *iter)
{
	ArrayIterator *internal = &iter->internal.array;
	int index = GET_INT_FROM_POINTER(internal->ptr);
	
	internal->ptr = SET_INT_IN_POINTER(index + 1);
	
	iter->valid = (index + 1 < internal->length);
}

static void rna_BVMNodeInstance_inputs_end(CollectionPropertyIterator *UNUSED(iter))
{
}

PointerRNA rna_BVMNodeInstance_inputs_get(CollectionPropertyIterator *iter)
{
	struct BVMNodeInstance *node = iter->parent.data;
	ArrayIterator *internal = &iter->internal.array;
	int index = GET_INT_FROM_POINTER(internal->ptr);
	
	struct BVMNodeInput *input = BVM_node_get_input_n(node, index);
	PointerRNA result;
	RNA_pointer_create(iter->ptr.id.data, &RNA_BVMNodeInput, input, &result);
	return result;
}

int rna_BVMNodeInstance_inputs_length(PointerRNA *ptr)
{
	struct BVMNodeInstance *node = ptr->data;
	return BVM_node_num_inputs(node);
}

int rna_BVMNodeInstance_inputs_lookupint(PointerRNA *ptr, int key, struct PointerRNA *r_ptr)
{
	struct BVMNodeInstance *node = ptr->data;
	
	struct BVMNodeInput *input = BVM_node_get_input_n(node, key);
	RNA_pointer_create(ptr->id.data, &RNA_BVMNodeInput, input, r_ptr);
	return input != NULL;
}

int rna_BVMNodeInstance_inputs_lookupstring(PointerRNA *ptr, const char *key, PointerRNA *r_ptr)
{
	struct BVMNodeInstance *node = ptr->data;
	
	struct BVMNodeInput *input = BVM_node_get_input(node, key);
	RNA_pointer_create(ptr->id.data, &RNA_BVMNodeInput, input, r_ptr);
	return input != NULL;
}

static void rna_BVMNodeInstance_outputs_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	struct BVMNodeInstance *node = ptr->data;
	ArrayIterator *internal = &iter->internal.array;
	
	internal->ptr = SET_INT_IN_POINTER(0);
	internal->length = BVM_node_num_outputs(node);
	
	iter->valid = (0 < internal->length);
}

static void rna_BVMNodeInstance_outputs_next(CollectionPropertyIterator *iter)
{
	ArrayIterator *internal = &iter->internal.array;
	int index = GET_INT_FROM_POINTER(internal->ptr);
	
	internal->ptr = SET_INT_IN_POINTER(index + 1);
	
	iter->valid = (index + 1 < internal->length);
}

static void rna_BVMNodeInstance_outputs_end(CollectionPropertyIterator *UNUSED(iter))
{
}

PointerRNA rna_BVMNodeInstance_outputs_get(CollectionPropertyIterator *iter)
{
	struct BVMNodeInstance *node = iter->parent.data;
	ArrayIterator *internal = &iter->internal.array;
	int index = GET_INT_FROM_POINTER(internal->ptr);
	
	struct BVMNodeOutput *output = BVM_node_get_output_n(node, index);
	PointerRNA result;
	RNA_pointer_create(iter->ptr.id.data, &RNA_BVMNodeOutput, output, &result);
	return result;
}

int rna_BVMNodeInstance_outputs_length(PointerRNA *ptr)
{
	struct BVMNodeInstance *node = ptr->data;
	return BVM_node_num_outputs(node);
}

int rna_BVMNodeInstance_outputs_lookupint(PointerRNA *ptr, int key, struct PointerRNA *r_ptr)
{
	struct BVMNodeInstance *node = ptr->data;
	
	struct BVMNodeOutput *output = BVM_node_get_output_n(node, key);
	RNA_pointer_create(ptr->id.data, &RNA_BVMNodeOutput, output, r_ptr);
	return output != NULL;
}

int rna_BVMNodeInstance_outputs_lookupstring(PointerRNA *ptr, const char *key, PointerRNA *r_ptr)
{
	struct BVMNodeInstance *node = ptr->data;
	
	struct BVMNodeOutput *output = BVM_node_get_output(node, key);
	RNA_pointer_create(ptr->id.data, &RNA_BVMNodeOutput, output, r_ptr);
	return output != NULL;
}

static int rna_BVMNodeInstance_set_input_link(struct BVMNodeInstance *node, struct BVMNodeInput *input,
                                              struct BVMNodeInstance *from_node, struct BVMNodeOutput *from_output)
{
	return BVM_node_set_input_link(node, input, from_node, from_output);
}

static void rna_BVMNodeInstance_set_value_float(struct BVMNodeInstance *node, struct BVMNodeInput *input, float value)
{
	BVM_node_set_input_value_float(node, input, value);
}

static void rna_BVMNodeInstance_set_value_float3(struct BVMNodeInstance *node, struct BVMNodeInput *input, float value[3])
{
	BVM_node_set_input_value_float3(node, input, value);
}

static void rna_BVMNodeInstance_set_value_float4(struct BVMNodeInstance *node, struct BVMNodeInput *input, float value[4])
{
	BVM_node_set_input_value_float4(node, input, value);
}

static void rna_BVMNodeInstance_set_value_matrix44(struct BVMNodeInstance *node, struct BVMNodeInput *input, float value[16])
{
	BVM_node_set_input_value_matrix44(node, input, (float (*)[4])value);
}

static void rna_BVMNodeInstance_set_value_int(struct BVMNodeInstance *node, struct BVMNodeInput *input, int value)
{
	BVM_node_set_input_value_int(node, input, value);
}

static int rna_BVMTypeDesc_base_type_get(PointerRNA *ptr)
{
	return BVM_typedesc_base_type(ptr->data);
}

/* ------------------------------------------------------------------------- */

static void rna_BVMEvalGlobals_add_object(struct BVMEvalGlobals *globals, int key, struct Object *ob)
{
	BVM_globals_add_object(globals, key, ob);
}

static int rna_BVMEvalGlobals_get_id_key(struct ID *id)
{
	return BVM_get_id_key(id);
}

#else

static void rna_def_bvm_typedesc(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem base_type_items[] = {
	    {BVM_FLOAT, "FLOAT", 0, "Float", "Floating point number"},
	    {BVM_FLOAT3, "FLOAT3", 0, "Float3", "3D vector"},
	    {BVM_FLOAT4, "FLOAT4", 0, "Float4", "4D vector"},
	    {BVM_INT, "INT", 0, "Int", "Integer number"},
	    {BVM_MATRIX44, "MATRIX44", 0, "Matrix44", "4x4 matrix"},
	    {BVM_STRING, "STRING", 0, "String", "Character string"},
	    {BVM_RNAPOINTER, "RNAPOINTER", 0, "RNA Pointer", "Blender data pointer (read-only)"},
	    {BVM_MESH, "MESH", 0, "Mesh", "Mesh data"},
	    {BVM_DUPLIS, "DUPLIS", 0, "Duplis", "Dupli instances list"},
	    {0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "BVMTypeDesc", NULL);
	RNA_def_struct_ui_text(srna, "Type Descriptor", "Extended definition of a data type");
	
	prop = RNA_def_property(srna, "base_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, base_type_items);
	RNA_def_property_enum_funcs(prop, "rna_BVMTypeDesc_base_type_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Base Type", "Base type of each data element");
}

static void rna_def_bvm_node_input(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem value_type_items[] = {
	    {INPUT_CONSTANT, "CONSTANT", 0, "Constant", "Fixed value that must be defined at compile time"},
	    {INPUT_EXPRESSION, "EXPRESSION", 0, "Expression", "Use an expression formed by other nodes"},
	    {INPUT_VARIABLE, "VARIABLE", 0, "Variable", "Local variable value"},
	    {0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "BVMNodeInput", NULL);
	RNA_def_struct_ui_text(srna, "Node Input", "Input of a node");
	
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_BVMNodeInput_name_get", "rna_BVMNodeInput_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "Name of the input");
	RNA_def_struct_name_property(srna, prop);
	
	prop = RNA_def_property(srna, "typedesc", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "BVMTypeDesc");
	RNA_def_property_pointer_funcs(prop, "rna_BVMNodeInput_typedesc_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Type Descriptor", "Type of data accepted by the input");
	
	prop = RNA_def_property(srna, "value_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, value_type_items);
	RNA_def_property_enum_funcs(prop, "rna_BVMNodeInput_value_type_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Value Type", "Limits the kind of data connections the input accepts");
}

static void rna_def_bvm_node_output(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem value_type_items[] = {
	    {OUTPUT_EXPRESSION, "EXPRESSION", 0, "Expression", "Expression that can be used by other nodes"},
	    {OUTPUT_VARIABLE, "VARIABLE", 0, "Variable", "Local variable for input expressions"},
	    {0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "BVMNodeOutput", NULL);
	RNA_def_struct_ui_text(srna, "Node Output", "Output of a node");
	
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_BVMNodeOutput_name_get", "rna_BVMNodeOutput_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "Name of the output");
	RNA_def_struct_name_property(srna, prop);
	
	prop = RNA_def_property(srna, "typedesc", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "BVMTypeDesc");
	RNA_def_property_pointer_funcs(prop, "rna_BVMNodeOutput_typedesc_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Type Descriptor", "Type of data accepted by the input");
	
	prop = RNA_def_property(srna, "value_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, value_type_items);
	RNA_def_property_enum_funcs(prop, "rna_BVMNodeOutput_value_type_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Value Type", "Limits the data connections the output allows");
}

static void rna_def_bvm_node_instance(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	PropertyRNA *parm;
	static const float zeros[16] = {0.0f};
	
	srna = RNA_def_struct(brna, "BVMNodeInstance", NULL);
	RNA_def_struct_ui_text(srna, "Node Instance", "Node in the internal BVM graph");
	
	prop = RNA_def_property(srna, "inputs", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_BVMNodeInstance_inputs_begin",
	                                  "rna_BVMNodeInstance_inputs_next", "rna_BVMNodeInstance_inputs_end",
	                                  "rna_BVMNodeInstance_inputs_get", "rna_BVMNodeInstance_inputs_length", 
	                                  "rna_BVMNodeInstance_inputs_lookupint", "rna_BVMNodeInstance_inputs_lookupstring",
	                                  NULL);
	RNA_def_property_struct_type(prop, "BVMNodeInput");
	RNA_def_property_ui_text(prop, "Inputs", "Input sockets of the node");
	
	prop = RNA_def_property(srna, "outputs", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_BVMNodeInstance_outputs_begin",
	                                  "rna_BVMNodeInstance_outputs_next", "rna_BVMNodeInstance_outputs_end",
	                                  "rna_BVMNodeInstance_outputs_get", "rna_BVMNodeInstance_outputs_length", 
	                                  "rna_BVMNodeInstance_outputs_lookupint", "rna_BVMNodeInstance_outputs_lookupstring",
	                                  NULL);
	RNA_def_property_struct_type(prop, "BVMNodeOutput");
	RNA_def_property_ui_text(prop, "Outputs", "Output sockets of the node");
	
	func = RNA_def_function(srna, "set_input_link", "rna_BVMNodeInstance_set_input_link");
	RNA_def_function_ui_description(func, "Add a new node connection");
	parm = RNA_def_pointer(func, "input", "BVMNodeInput", "Input", "Input to connect");
	RNA_def_property_flag(parm, PROP_NEVER_NULL);
	parm = RNA_def_pointer(func, "from_node", "BVMNodeInstance", "Source Node", "");
	RNA_def_property_flag(parm, PROP_NEVER_NULL);
	parm = RNA_def_pointer(func, "from_output", "BVMNodeOutput", "Source Output", "");
	RNA_def_property_flag(parm, PROP_NEVER_NULL);
	parm = RNA_def_boolean(func, "result", true, "Result", "True if adding the connection succeeded");
	RNA_def_function_return(func, parm);
	
	func = RNA_def_function(srna, "set_value_float", "rna_BVMNodeInstance_set_value_float");
	RNA_def_function_ui_description(func, "Set an input value constant");
	parm = RNA_def_pointer(func, "input", "BVMNodeInput", "Input", "Set value for this input");
	RNA_def_property_flag(parm, PROP_NEVER_NULL);
	RNA_def_float(func, "value", 0.0f, -FLT_MAX, FLT_MAX, "Value", "", -FLT_MAX, FLT_MAX);
	
	func = RNA_def_function(srna, "set_value_float3", "rna_BVMNodeInstance_set_value_float3");
	RNA_def_function_ui_description(func, "Set an input value constant");
	parm = RNA_def_pointer(func, "input", "BVMNodeInput", "Input", "Set value for this input");
	RNA_def_property_flag(parm, PROP_NEVER_NULL);
	RNA_def_float_array(func, "value", 3, zeros, -FLT_MAX, FLT_MAX, "Value", "", -FLT_MAX, FLT_MAX);
	
	func = RNA_def_function(srna, "set_value_float4", "rna_BVMNodeInstance_set_value_float4");
	RNA_def_function_ui_description(func, "Set an input value constant");
	parm = RNA_def_pointer(func, "input", "BVMNodeInput", "Input", "Set value for this input");
	RNA_def_property_flag(parm, PROP_NEVER_NULL);
	RNA_def_float_array(func, "value", 4, zeros, -FLT_MAX, FLT_MAX, "Value", "", -FLT_MAX, FLT_MAX);
	
	func = RNA_def_function(srna, "set_value_matrix44", "rna_BVMNodeInstance_set_value_matrix44");
	RNA_def_function_ui_description(func, "Set an input value constant");
	parm = RNA_def_pointer(func, "input", "BVMNodeInput", "Input", "Set value for this input");
	RNA_def_property_flag(parm, PROP_NEVER_NULL);
	RNA_def_float_matrix(func, "value", 4, 4, zeros, -FLT_MAX, FLT_MAX, "Value", "", -FLT_MAX, FLT_MAX);
	
	func = RNA_def_function(srna, "set_value_int", "rna_BVMNodeInstance_set_value_int");
	RNA_def_function_ui_description(func, "Set an input value constant");
	parm = RNA_def_pointer(func, "input", "BVMNodeInput", "Input", "Set value for this input");
	RNA_def_property_flag(parm, PROP_NEVER_NULL);
	RNA_def_int(func, "value", 0, INT_MIN, INT_MAX, "Value", "", INT_MIN, INT_MAX);
}

static void rna_def_bvm_node_compiler(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;
	
	srna = RNA_def_struct(brna, "BVMNodeCompiler", NULL);
	RNA_def_struct_ui_text(srna, "Node Compiler", "Compiler interface for a node tree");
	
	func = RNA_def_function(srna, "add_node", "rna_BVMNodeCompiler_add_node");
	RNA_def_function_ui_description(func, "Add a new bvm node");
	RNA_def_string(func, "type", NULL, 0, "Type", "Type of the node");
	parm = RNA_def_pointer(func, "node", "BVMNodeInstance", "Node", "");
	RNA_def_function_return(func, parm);
	
	func = RNA_def_function(srna, "get_input", "rna_BVMNodeCompiler_get_input");
	RNA_def_function_ui_description(func, "Get a node/socket pair used as input of the graph");
	RNA_def_string(func, "name", NULL, 0, "Name", "Input slot name");
	parm = RNA_def_pointer(func, "input", "BVMNodeInput", "Input", "Global input");
	RNA_def_function_return(func, parm);
	
	func = RNA_def_function(srna, "get_output", "rna_BVMNodeCompiler_get_output");
	RNA_def_function_ui_description(func, "Get a node/socket pair used as output of the graph");
	RNA_def_string(func, "name", NULL, 0, "Name", "Output slot name");
	parm = RNA_def_pointer(func, "output", "BVMNodeOutput", "Output", "Global output");
	RNA_def_function_return(func, parm);
}

static void rna_def_bvm_eval_globals(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;
	
	srna = RNA_def_struct(brna, "BVMEvalGlobals", NULL);
	RNA_def_struct_ui_text(srna, "Globals", "Global data used during node evaluation");
	
	func = RNA_def_function(srna, "add_object", "rna_BVMEvalGlobals_add_object");
	RNA_def_function_ui_description(func, "Register a used object");
	parm = RNA_def_int(func, "key", 0, INT_MIN, INT_MAX, "Key", "Unique key of the object", INT_MIN, INT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "object", "Object", "Object", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	
	func = RNA_def_function(srna, "get_id_key", "rna_BVMEvalGlobals_get_id_key");
	RNA_def_function_flag(func, FUNC_NO_SELF);
	RNA_def_function_ui_description(func, "Get a key value to look up the object during evaluation");
	parm = RNA_def_pointer(func, "id_data", "ID", "ID", "ID datablock");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_int(func, "key", 0, INT_MIN, INT_MAX, "Key", "Key value for this datablock", INT_MIN, INT_MAX);
	RNA_def_function_return(func, parm);
}

void RNA_def_blenvm(BlenderRNA *brna)
{
	rna_def_bvm_typedesc(brna);
	rna_def_bvm_node_input(brna);
	rna_def_bvm_node_output(brna);
	rna_def_bvm_node_instance(brna);
	rna_def_bvm_node_compiler(brna);
	rna_def_bvm_eval_globals(brna);
}

#endif

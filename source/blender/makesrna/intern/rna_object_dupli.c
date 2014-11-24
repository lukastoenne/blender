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
 * Contributor(s): Blender Foundation (2014), Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_object_dupli.c
 *  \ingroup RNA
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "BLI_string.h"

#include "DNA_object_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "rna_internal.h"

#include "WM_api.h"
#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "BKE_anim.h"

static PointerRNA rna_DupliContext_scene_get(PointerRNA *ptr)
{
	struct DupliContext *ctx = ptr->data;
	Scene *scene = BKE_dupli_context_scene(ctx);
	PointerRNA r_ptr;
	RNA_id_pointer_create(&scene->id, &r_ptr);
	return r_ptr;
}

static PointerRNA rna_DupliContext_object_get(PointerRNA *ptr)
{
	struct DupliContext *ctx = ptr->data;
	Object *ob = BKE_dupli_context_object(ctx);
	PointerRNA r_ptr;
	RNA_id_pointer_create(&ob->id, &r_ptr);
	return r_ptr;
}

/* ------------------------------------------------------------------------- */

static void rna_DupliResult_add(struct DupliResult *result, struct DupliContext *ctx, struct Object *ob, float mat[16], int index, int animated, int hide)
{
	BKE_dupli_result_add(ctx, result, ob, (float (*)[4])mat, index, animated, hide);
}

/* ------------------------------------------------------------------------- */

static StructRNA *rna_ObjectDuplicator_refine(struct PointerRNA *ptr)
{
	ObjectDuplicator *dup = ptr->data;
	
	if (dup->type->ext.srna)
		return dup->type->ext.srna;
	else
		return &RNA_ObjectDuplicator;
}

static void rna_ObjectDuplicator_unregister(Main *UNUSED(bmain), StructRNA *type)
{
	ObjectDuplicatorType *duptype = RNA_struct_blender_type_get(type);

	if (!duptype)
		return;

	RNA_struct_free_extension(type, &duptype->ext);

	object_duplilist_free_type(duptype);

	RNA_struct_free(&BLENDER_RNA, type);
}

static void rna_ObjectDuplicator_make_duplis(const ObjectDuplicatorType *duptype, const struct DupliContext *context, struct DupliResult *result)
{
	extern FunctionRNA rna_ObjectDuplicator_make_duplis_func;

	ObjectDuplicator dup;
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	dup.type = (ObjectDuplicatorType *)duptype;
	RNA_pointer_create(NULL, duptype->ext.srna, &dup, &ptr); /* dummy */
	func = &rna_ObjectDuplicator_make_duplis_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "context", &context);
	RNA_parameter_set_lookup(&list, "result", &result);
	duptype->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static StructRNA *rna_ObjectDuplicator_register(
        Main *bmain, ReportList *reports, void *data, const char *identifier,
        StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	ObjectDuplicatorType *duptype, dummy_duptype;
	ObjectDuplicator dummy_dup;
	PointerRNA dummy_ptr;
	int have_function[1];

	/* setup dummy tree & tree type to store static properties in */
	memset(&dummy_duptype, 0, sizeof(ObjectDuplicatorType));
	memset(&dummy_dup, 0, sizeof(ObjectDuplicator));
	dummy_dup.type = &dummy_duptype;
	RNA_pointer_create(NULL, &RNA_ObjectDuplicator, &dummy_dup, &dummy_ptr);

	/* validate the python class */
	if (validate(&dummy_ptr, data, have_function) != 0)
		return NULL;

	if (strlen(identifier) >= sizeof(dummy_duptype.idname)) {
		BKE_reportf(reports, RPT_ERROR, "Registering object duplicator class: '%s' is too long, maximum length is %d",
		            identifier, (int)sizeof(dummy_duptype.idname));
		return NULL;
	}

	/* check if we have registered this tree type before, and remove it */
	duptype = object_duplilist_find_type(dummy_duptype.idname);
	if (duptype)
		rna_ObjectDuplicator_unregister(bmain, duptype->ext.srna);
	
	/* create a new node tree type */
	duptype = MEM_callocN(sizeof(ObjectDuplicatorType), "object duplicator type");
	memcpy(duptype, &dummy_duptype, sizeof(dummy_duptype));

	duptype->ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, duptype->idname, &RNA_ObjectDuplicator); 
	duptype->ext.data = data;
	duptype->ext.call = call;
	duptype->ext.free = free;
	RNA_struct_blender_type_set(duptype->ext.srna, duptype);

	RNA_def_struct_ui_text(duptype->ext.srna, duptype->ui_name, duptype->ui_description);
	RNA_def_struct_ui_icon(duptype->ext.srna, duptype->ui_icon);

	duptype->make_duplis = (have_function[0]) ? rna_ObjectDuplicator_make_duplis : NULL;

	object_duplilist_add_type(duptype);

	return duptype->ext.srna;
}

static void rna_ObjectDuplicator_idname_get(PointerRNA *ptr, char *value)
{
	ObjectDuplicator *dup = ptr->data;
	strcpy(value, dup->type->idname);
}

static int rna_ObjectDuplicator_idname_length(PointerRNA *ptr)
{
	ObjectDuplicator *dup = ptr->data;
	return strlen(dup->type->idname);
}

static void rna_ObjectDuplicator_idname_set(PointerRNA *ptr, const char *value)
{
	ObjectDuplicator *dup = ptr->data;
	BLI_strncpy(dup->type->idname, value, sizeof(dup->type->idname));
}

static void rna_ObjectDuplicator_label_get(PointerRNA *ptr, char *value)
{
	ObjectDuplicator *dup = ptr->data;
	strcpy(value, dup->type->ui_name);
}

static int rna_ObjectDuplicator_label_length(PointerRNA *ptr)
{
	ObjectDuplicator *dup = ptr->data;
	return strlen(dup->type->ui_name);
}

static void rna_ObjectDuplicator_label_set(PointerRNA *ptr, const char *value)
{
	ObjectDuplicator *dup = ptr->data;
	BLI_strncpy(dup->type->ui_name, value, sizeof(dup->type->ui_name));
}

static void rna_ObjectDuplicator_description_get(PointerRNA *ptr, char *value)
{
	ObjectDuplicator *dup = ptr->data;
	strcpy(value, dup->type->ui_description);
}

static int rna_ObjectDuplicator_description_length(PointerRNA *ptr)
{
	ObjectDuplicator *dup = ptr->data;
	return strlen(dup->type->ui_description);
}

static void rna_ObjectDuplicator_description_set(PointerRNA *ptr, const char *value)
{
	ObjectDuplicator *dup = ptr->data;
	BLI_strncpy(dup->type->ui_description, value, sizeof(dup->type->ui_description));
}

static int rna_ObjectDuplicator_icon_get(PointerRNA *ptr)
{
	ObjectDuplicator *dup = ptr->data;
	return dup->type->ui_icon;
}

static void rna_ObjectDuplicator_icon_set(PointerRNA *ptr, int value)
{
	ObjectDuplicator *dup = ptr->data;
	dup->type->ui_icon = value;
}

#else

static void rna_def_dupli_context(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "DupliContext", NULL);
	RNA_def_struct_ui_text(srna, "Dupli Context", "Context data for dupli generation");

	prop = RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Scene");
	RNA_def_property_pointer_funcs(prop, "rna_DupliContext_scene_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Scene", "");

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_funcs(prop, "rna_DupliContext_object_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Object", "");
}

static void rna_def_dupli_result(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;

	srna = RNA_def_struct(brna, "DupliResult", NULL);
	RNA_def_struct_ui_text(srna, "Dupli Result", "Object dupli instance storage");

	func = RNA_def_function(srna, "add", "rna_DupliResult_add");
	RNA_def_function_ui_description(func, "Add a new dupli instance to the result");
	parm = RNA_def_pointer(func, "context", "DupliContext", "Context", "Context for object duplication");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "object", "Object", "Object", "Object to instance");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_property(func, "matrix", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(parm, "Matrix", "Worldspace transformation matrix");
	RNA_def_property_multi_array(parm, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_int(func, "index", 0, INT_MIN, INT_MAX, "Index", "Index for identifying a dupli instance", INT_MIN, INT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_boolean(func, "animated", false, "Animated", "True if the dupli instance is animated");
	RNA_def_boolean(func, "hide", false, "Hide", "True if the dupli instance itself should be hidden (only showing child duplis)");
}

static void rna_def_object_duplicator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	PropertyRNA *parm;

	#define DEF_ICON_BLANK_SKIP
	#define DEF_ICON(name) {ICON_##name, (#name), 0, (#name), ""},
	#define DEF_VICO(name)
	static EnumPropertyItem icon_items[] = {
		#include "UI_icons.h"
		{0, NULL, 0, NULL, NULL}};
	#undef DEF_ICON_BLANK_SKIP
	#undef DEF_ICON
	#undef DEF_VICO

	srna = RNA_def_struct(brna, "ObjectDuplicator", NULL);
	RNA_def_struct_ui_text(srna, "Object Duplicator", "Method for generating object duplis");
	RNA_def_struct_refine_func(srna, "rna_ObjectDuplicator_refine");
	RNA_def_struct_register_funcs(srna, "rna_ObjectDuplicator_register", "rna_ObjectDuplicator_unregister", NULL);

	/* registration */
	prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ObjectDuplicator_idname_get", "rna_ObjectDuplicator_idname_length", "rna_ObjectDuplicator_idname_set");
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "Identifier", "");

	prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ObjectDuplicator_label_get", "rna_ObjectDuplicator_label_length", "rna_ObjectDuplicator_label_set");
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "Label", "Label string for UI");

	prop = RNA_def_property(srna, "bl_description", PROP_STRING, PROP_TRANSLATION);
	RNA_def_property_string_funcs(prop, "rna_ObjectDuplicator_description_get", "rna_ObjectDuplicator_description_length", "rna_ObjectDuplicator_description_set");
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_icon", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_funcs(prop, "rna_ObjectDuplicator_icon_get", "rna_ObjectDuplicator_icon_set", NULL);
	RNA_def_property_enum_items(prop, icon_items);
	RNA_def_property_enum_default(prop, ICON_NONE);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
	RNA_def_property_ui_text(prop, "Icon", "Icon for UI");

	/* make duplis */
	func = RNA_def_function(srna, "make_duplis", NULL);
	RNA_def_function_ui_description(func, "Create a number of duplis for the given context");
	RNA_def_function_flag(func, FUNC_REGISTER);
	parm = RNA_def_pointer(func, "context", "DupliContext", "Context", "Context for object duplication");
	RNA_def_property_flag(parm, PROP_NEVER_NULL);
	parm = RNA_def_pointer(func, "result", "DupliResult", "Result", "Storage for dupli instances");
	RNA_def_property_flag(parm, PROP_NEVER_NULL);
}

void RNA_def_object_dupli(BlenderRNA *brna)
{
	rna_def_dupli_context(brna);
	rna_def_dupli_result(brna);
	rna_def_object_duplicator(brna);
}

#endif

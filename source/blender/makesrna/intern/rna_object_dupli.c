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

static StructRNA *rna_DupliGenerator_refine(struct PointerRNA *ptr)
{
	DupliGenerator *gen = ptr->data;
	
	if (gen->ext.srna)
		return gen->ext.srna;
	else
		return ptr->type;
}

static void rna_DupliGenerator_make_duplis(const struct DupliContext *ctx)
{
	extern FunctionRNA rna_DupliGenerator_make_duplis_func;

	DupliGenerator *gen = BKE_dupli_context_generator(ctx);
	Object *ob = BKE_dupli_context_object(ctx);
	struct DupliContainer *cont = BKE_dupli_context_container(ctx);

	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, gen->ext.srna, NULL, &ptr);
	func = &rna_DupliGenerator_make_duplis_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "container", &cont);
	RNA_parameter_set_lookup(&list, "object", &ob);
	gen->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void rna_DupliGenerator_unregister(Main *UNUSED(bmain), StructRNA *type)
{
	DupliGenerator *gen = RNA_struct_blender_type_get(type);

	if (!gen)
		return;

	RNA_struct_free_extension(type, &gen->ext);

	/* this also frees the allocated gen pointer, no MEM_free call needed! */
	BKE_dupli_gen_unregister(gen);

	RNA_struct_free(&BLENDER_RNA, type);

	/* update while blender is running */
	WM_main_add_notifier(NC_OBJECT | NA_EDITED, NULL);
}

/* Generic internal registration function.
 * Can be used to implement callbacks for registerable RNA node subtypes.
 */
static StructRNA *rna_DupliGenerator_register(Main *bmain, ReportList *reports,
                                              void *data, const char *identifier,
                                              StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	DupliGenerator *gen, dummygen;
	PointerRNA dummyptr;
	int have_function[1];

	/* setup dummy generator to store static properties in */
	memset(&dummygen, 0, sizeof(DupliGenerator));

	RNA_pointer_create(NULL, &RNA_DupliGenerator, &dummygen, &dummyptr);

	/* validate the python class */
	if (validate(&dummyptr, data, have_function) != 0)
		return NULL;

	if (strlen(identifier) >= sizeof(dummygen.idname)) {
		BKE_reportf(reports, RPT_ERROR, "Registering dupli generator class: '%s' is too long, maximum length is %d",
		            identifier, (int)sizeof(dummygen.idname));
		return NULL;
	}

	/* check if we have registered this generator before, and remove it */
	gen = BKE_dupli_gen_find(dummygen.idname);
	if (gen)
		rna_DupliGenerator_unregister(bmain, gen->ext.srna);
	
	/* create a new generator */
	gen = MEM_callocN(sizeof(DupliGenerator), "dupli generator");
	memcpy(gen, &dummygen, sizeof(dummygen));

	gen->ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, gen->idname, &RNA_DupliGenerator);
	gen->ext.data = data;
	gen->ext.call = call;
	gen->ext.free = free;
	RNA_struct_blender_type_set(gen->ext.srna, gen);

	RNA_def_struct_ui_text(gen->ext.srna, gen->name, gen->description);

	gen->make_duplis = (have_function[0]) ? rna_DupliGenerator_make_duplis : NULL;
	
	BKE_dupli_gen_register(gen);
	
	/* update while blender is running */
	WM_main_add_notifier(NC_OBJECT | NA_EDITED, NULL);
	
	return gen->ext.srna;
}

static void rna_DupliGenerator_idname_get(PointerRNA *ptr, char *value)
{
	DupliGenerator *gen = ptr->data;
	strcpy(value, gen->idname);
}

static int rna_DupliGenerator_idname_length(PointerRNA *ptr)
{
	DupliGenerator *gen = ptr->data;
	return strlen(gen->idname);
}

static void rna_DupliGenerator_idname_set(PointerRNA *ptr, const char *value)
{
	DupliGenerator *gen = ptr->data;
	BLI_strncpy(gen->idname, value, sizeof(gen->idname));
}

static void rna_DupliGenerator_name_get(PointerRNA *ptr, char *value)
{
	DupliGenerator *gen = ptr->data;
	strcpy(value, gen->name);
}

static int rna_DupliGenerator_name_length(PointerRNA *ptr)
{
	DupliGenerator *gen = ptr->data;
	return strlen(gen->name);
}

static void rna_DupliGenerator_name_set(PointerRNA *ptr, const char *value)
{
	DupliGenerator *gen = ptr->data;
	BLI_strncpy(gen->name, value, sizeof(gen->name));
}

static void rna_DupliGenerator_description_get(PointerRNA *ptr, char *value)
{
	DupliGenerator *gen = ptr->data;
	strcpy(value, gen->description);
}

static int rna_DupliGenerator_description_length(PointerRNA *ptr)
{
	DupliGenerator *gen = ptr->data;
	return strlen(gen->description);
}

static void rna_DupliGenerator_description_set(PointerRNA *ptr, const char *value)
{
	DupliGenerator *gen = ptr->data;
	BLI_strncpy(gen->description, value, sizeof(gen->description));
}

static void rna_DupliContainer_add(struct DupliContainer *cont, struct Object *ob, float *matrix)
{
	bool animated = false;
	bool hide = false;
	int index = 0;
	
	BKE_dupli_add_instance(cont, ob, (float (*)[4])matrix, index, animated, hide);
}

#else

static void rna_def_dupli_generator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	PropertyRNA *parm;

	srna = RNA_def_struct(brna, "DupliGenerator", NULL);
	RNA_def_struct_sdna(srna, "DupliGenerator");
	RNA_def_struct_refine_func(srna, "rna_DupliGenerator_refine");
	RNA_def_struct_register_funcs(srna, "rna_DupliGenerator_register", "rna_DupliGenerator_unregister", NULL);
	RNA_def_struct_ui_text(srna, "Dupli Generator", "Generator type for object instances");

	/* registration */
	prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_DupliGenerator_idname_get", "rna_DupliGenerator_idname_length",
	                              "rna_DupliGenerator_idname_set");
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "ID Name", "");

	prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_DupliGenerator_name_get", "rna_DupliGenerator_name_length",
	                              "rna_DupliGenerator_name_set");
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "Label", "UI Name");

	prop = RNA_def_property(srna, "bl_description", PROP_STRING, PROP_TRANSLATION);
	RNA_def_property_string_funcs(prop, "rna_DupliGenerator_description_get", "rna_DupliGenerator_description_length",
	                              "rna_DupliGenerator_description_set");
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	func = RNA_def_function(srna, "make_duplis", NULL);
	RNA_def_function_ui_description(func, "Create dupli instances");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER);
	parm = RNA_def_pointer(func, "container", "DupliContainer", "Dupli Container", "Container for dupli instances");
	RNA_def_property_flag(parm, PROP_NEVER_NULL);
	parm = RNA_def_pointer(func, "object", "Object", "Object", "");
	RNA_def_property_flag(parm, PROP_NEVER_NULL);
}

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
}

void RNA_def_object_dupli(BlenderRNA *brna)
{
	rna_def_dupli_generator(brna);
	rna_def_dupli_container(brna);
}

#endif

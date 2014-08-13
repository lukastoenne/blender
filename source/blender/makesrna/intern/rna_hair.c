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
 * The Original Code is Copyright (C) 2014 by the Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_hair.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "DNA_hair_types.h"

#include "BLI_utildefines.h"

#include "RNA_define.h"

#include "rna_internal.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "BKE_hair.h"

#include "RNA_access.h"

#include "WM_api.h"

#include "MEM_guardedalloc.h"

#if 0 /* unused */
static void rna_HairSystem_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	WM_main_add_notifier(NC_OBJECT|ND_DRAW, ptr->id.data);
}
#endif

static void rna_HairDisplaySettings_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	WM_main_add_notifier(NC_OBJECT|ND_DRAW, ptr->id.data);
}

static void rna_HairParams_render_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	WM_main_add_notifier(NC_OBJECT|ND_DRAW, ptr->id.data);
}

static PointerRNA rna_HairSystem_render_get(PointerRNA *ptr)
{
	HairSystem *hsys = ptr->data;
	PointerRNA r_ptr;
	
	if (!hsys->render_iter) {
		hsys->render_iter = MEM_callocN(sizeof(HairRenderIterator), "hair render iterator RNA instance");
		/* store the hsys here already, so callers
		 * don't have to pass it explicitly again in the init function */
		hsys->render_iter->hsys = hsys;
	}
	
	RNA_pointer_create(ptr->id.data, &RNA_HairRenderIterator, hsys->render_iter, &r_ptr);
	return r_ptr;
}

static void rna_HairRenderIterator_init(HairRenderIterator *iter)
{
	/* make sure the iterator is uninitialized first */
	if (BKE_hair_render_iter_initialized(iter))
		BKE_hair_render_iter_end(iter);
	
	BKE_hair_render_iter_init(iter, iter->hsys);
}

static void rna_HairRenderIterator_end(HairRenderIterator *iter)
{
	BKE_hair_render_iter_end(iter);
}

static int rna_HairRenderIterator_valid(HairRenderIterator *iter)
{
	return BKE_hair_render_iter_valid_hair(iter);
}

static void rna_HairRenderIterator_next(HairRenderIterator *iter)
{
	BKE_hair_render_iter_next_hair(iter);
}

static void rna_HairRenderIterator_count(HairRenderIterator *iter, int *tothairs, int *totsteps)
{
	BKE_hair_render_iter_count(iter, tothairs, totsteps);
}

static PointerRNA rna_HairRenderIterator_step_init(ID *id, HairRenderIterator *iter)
{
	PointerRNA r_ptr;
	
	if (!BKE_hair_render_iter_initialized(iter)) {
		RNA_pointer_create(id, &RNA_HairRenderStepIterator, NULL, &r_ptr);
		return r_ptr;
	}
	
	BKE_hair_render_iter_init_hair(iter);
	
	RNA_pointer_create(id, &RNA_HairRenderStepIterator, iter, &r_ptr);
	return r_ptr;
}

//static void rna_HairRenderStepIterator_init(HairRenderIterator *iter)
//{
//	BKE_hair_render_iter_init_hair(iter);
//}

static int rna_HairRenderStepIterator_valid(HairRenderIterator *iter)
{
	return BKE_hair_render_iter_valid_step(iter);
}

static void rna_HairRenderStepIterator_next(HairRenderIterator *iter)
{
	BKE_hair_render_iter_next_step(iter);
}

static int rna_HairRenderStepIterator_index_get(PointerRNA *ptr)
{
	HairRenderIterator *iter = ptr->data;
	return iter->step;
}

static int rna_HairRenderStepIterator_totsteps_get(PointerRNA *ptr)
{
	HairRenderIterator *iter = ptr->data;
	return iter->totsteps;
}

static void rna_HairRenderStepIterator_eval(HairRenderIterator *iter, float co[3], float *radius)
{
	BKE_hair_render_iter_get(iter, co, radius);
}

#else

static void rna_def_hair_params(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "HairParams", NULL);
	RNA_def_struct_ui_text(srna, "Hair Parameters", "Hair simulation parameters");

	prop = RNA_def_property(srna, "substeps_forces", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "substeps_forces");
	RNA_def_property_range(prop, 1, 1000);
	RNA_def_property_ui_range(prop, 1, 120, 1, 1);
	RNA_def_property_int_default(prop, 30);
	RNA_def_property_ui_text(prop, "Substeps Forces", "Substeps for force integration");

	prop = RNA_def_property(srna, "substeps_damping", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "substeps_damping");
	RNA_def_property_range(prop, 1, 100);
	RNA_def_property_ui_range(prop, 1, 30, 1, 1);
	RNA_def_property_int_default(prop, 10);
	RNA_def_property_ui_text(prop, "Substeps Damping", "Substeps for damping force integration (on top of force substeps)");

	prop = RNA_def_property(srna, "stretch_stiffness", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "stretch_stiffness");
	RNA_def_property_range(prop, 0.0f, 1.0e9f);
	RNA_def_property_ui_range(prop, 0.0f, 3000.0f, 0.1, 2);
	RNA_def_property_ui_text(prop, "Stretch Stiffness", "Resistance to stretching");

	prop = RNA_def_property(srna, "stretch_damping", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "stretch_damping");
	RNA_def_property_range(prop, 0.0f, 1.0e6f);
	RNA_def_property_ui_range(prop, 0.0f, 20.0f, 0.1, 2);
	RNA_def_property_ui_text(prop, "Stretch Damping", "Damping of stretch motion");

	prop = RNA_def_property(srna, "bend_stiffness", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "bend_stiffness");
	RNA_def_property_range(prop, 0.0f, 1.0e9f);
	RNA_def_property_ui_range(prop, 0.0f, 500.0f, 0.1, 2);
	RNA_def_property_ui_text(prop, "Bend Stiffness", "Resistance to bending");

	prop = RNA_def_property(srna, "bend_damping", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "bend_damping");
	RNA_def_property_range(prop, 0.0f, 1.0e6f);
	RNA_def_property_ui_range(prop, 0.0f, 20.0f, 0.1, 2);
	RNA_def_property_ui_text(prop, "Bend Damping", "Damping of bending motion");

	prop = RNA_def_property(srna, "bend_smoothing", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "bend_smoothing");
	RNA_def_property_range(prop, 0.0f, 256.0f);
	RNA_def_property_ui_range(prop, 0.0f, 8.0f, 0.1, 2);
	RNA_def_property_ui_text(prop, "Bend Smoothing", "Smoothing amount to avoid rotation of hair curls");

	prop = RNA_def_property(srna, "drag", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "drag");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.01f, 3);
	RNA_def_property_float_default(prop, 0.0f);
	RNA_def_property_ui_text(prop, "Drag", "Air drag factor");

	prop = RNA_def_property(srna, "friction", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "friction");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 1, 3);
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_ui_text(prop, "Friction", "Resistance of hair to sliding over objects");

	prop = RNA_def_property(srna, "restitution", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "restitution");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 1, 3);
	RNA_def_property_float_default(prop, 0.0f);
	RNA_def_property_ui_text(prop, "Restitution", "Amount of energy retained after collision");

	prop = RNA_def_property(srna, "margin", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "margin");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 100.0f, 0.01f, 3);
	RNA_def_property_float_default(prop, 0.02f);
	RNA_def_property_ui_text(prop, "Margin", "Collision margin to avoid penetration");

	/* Render Settings */

	prop = RNA_def_property(srna, "render_hairs", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "num_render_hairs");
	RNA_def_property_range(prop, 1, 10000);
	RNA_def_property_ui_range(prop, 1, 200, 1, 1);
	RNA_def_property_int_default(prop, 100);
	RNA_def_property_ui_text(prop, "Render Hairs", "Number of hairs rendered around each simulated hair");
	RNA_def_property_update(prop, 0, "rna_HairParams_render_update");

	prop = RNA_def_property(srna, "curl_smoothing", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "curl_smoothing");
	RNA_def_property_range(prop, 0.0f, 256.0f);
	RNA_def_property_ui_range(prop, 0.0f, 8.0f, 0.1, 2);
	RNA_def_property_ui_text(prop, "Curl Smoothing", "Smoothing amount to avoid rotation of hair curls");
	RNA_def_property_update(prop, 0, "rna_HairParams_render_update");
}

static void rna_def_hair_display_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem mode_items[] = {
	    {HAIR_DISPLAY_LINE,     "LINE",     0, "Line",      "Show center lines representing hair"},
	    {HAIR_DISPLAY_RENDER,   "RENDER",   0, "Render",    "Show render hairs"},
	    {HAIR_DISPLAY_HULL,     "HULL",     0, "Hull",      "Show symbolic hulls"},
	    {0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "HairDisplaySettings", NULL);
	RNA_def_struct_ui_text(srna, "Hair Display Settings", "");

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "mode");
	RNA_def_property_enum_items(prop, mode_items);
	RNA_def_property_ui_text(prop, "Mode", "Hair display mode");
	RNA_def_property_update(prop, 0, "rna_HairDisplaySettings_update");
}

static void rna_def_hair_system(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "HairSystem", NULL);
	RNA_def_struct_ui_text(srna, "Hair System", "Hair simulation and rendering");

	prop = RNA_def_property(srna, "params", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "params");
	RNA_def_property_struct_type(prop, "HairParams");
	RNA_def_property_ui_text(prop, "Parameters", "Parameters for the hair simulation");

	prop = RNA_def_property(srna, "display", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "display");
	RNA_def_property_struct_type(prop, "HairDisplaySettings");
	RNA_def_property_ui_text(prop, "Display Settings", "Display settings for the hair system");

	prop = RNA_def_property(srna, "render_iterator", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "HairRenderIterator");
	RNA_def_property_pointer_funcs(prop, "rna_HairSystem_render_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Render Iterator", "Access to render data");
}

static void rna_def_hair_render_iterator(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;

	srna = RNA_def_struct(brna, "HairRenderIterator", NULL);
	RNA_def_struct_sdna(srna, "HairRenderIterator");
	RNA_def_struct_ui_text(srna, "Hair Render Iterator", "Iterator over rendered hairs");

	func = RNA_def_function(srna, "init", "rna_HairRenderIterator_init");
	RNA_def_function_ui_description(func, "Reset the iterator to the start of the render data");

	func = RNA_def_function(srna, "end", "rna_HairRenderIterator_end");
	RNA_def_function_ui_description(func, "Clean up the iterator after finishing render data export");

	func = RNA_def_function(srna, "valid", "rna_HairRenderIterator_valid");
	RNA_def_function_ui_description(func, "Returns True if the iterator valid elements left");
	parm = RNA_def_boolean(func, "result", false, "Result", "");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "next", "rna_HairRenderIterator_next");
	RNA_def_function_ui_description(func, "Advance to the next hair");

	func = RNA_def_function(srna, "count", "rna_HairRenderIterator_count");
	RNA_def_function_ui_description(func, "Count total number of hairs and steps");
	parm = RNA_def_int(func, "tothairs", 0, INT_MIN, INT_MAX, "Hairs", "Total number of hair curves", INT_MIN, INT_MAX);
	RNA_def_function_output(func, parm);
	parm = RNA_def_int(func, "totsteps", 0, INT_MIN, INT_MAX, "Steps", "Total number of interpolation vertex steps", INT_MIN, INT_MAX);
	RNA_def_function_output(func, parm);

	func = RNA_def_function(srna, "step_init", "rna_HairRenderIterator_step_init");
	RNA_def_function_ui_description(func, "Iterator over interpolation steps");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	parm = RNA_def_pointer(func, "result", "HairRenderStepIterator", "Result", "");
	RNA_def_function_return(func, parm);
	RNA_def_property_flag(parm, PROP_RNAPTR);
}

static void rna_def_hair_render_step_iterator(BlenderRNA *brna)
{
	static const float default_co[3] = { 0.0f, 0.0f, 0.0f };
	
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	PropertyRNA *parm;

	srna = RNA_def_struct(brna, "HairRenderStepIterator", NULL);
	RNA_def_struct_sdna(srna, "HairRenderIterator");
	RNA_def_struct_ui_text(srna, "Hair Render Step Iterator", "Iterator over steps in a single hair's render data");

//	func = RNA_def_function(srna, "init", "rna_HairRenderStepIterator_init");
//	RNA_def_function_ui_description(func, "Reset the iterator to the start of the render data");

	func = RNA_def_function(srna, "valid", "rna_HairRenderStepIterator_valid");
	RNA_def_function_ui_description(func, "Returns True if the iterator valid elements left");
	parm = RNA_def_boolean(func, "result", false, "Result", "");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "next", "rna_HairRenderStepIterator_next");
	RNA_def_function_ui_description(func, "Advance to the next interpolation step");

	prop = RNA_def_property(srna, "index", PROP_INT, PROP_NONE);
	RNA_def_property_int_funcs(prop, "rna_HairRenderStepIterator_index_get", NULL, NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Steps", "Current Interpolation step on the hair curve");

	prop = RNA_def_property(srna, "totsteps", PROP_INT, PROP_NONE);
	RNA_def_property_int_funcs(prop, "rna_HairRenderStepIterator_totsteps_get", NULL, NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Steps", "Number of interpolation steps on the hair curve");

	func = RNA_def_function(srna, "eval", "rna_HairRenderStepIterator_eval");
	RNA_def_function_ui_description(func, "Evaluate the iterator to get hair data at the current step");
	parm = RNA_def_float_vector(func, "co", 3, default_co, -FLT_MAX, FLT_MAX, "Location", "Location of the hair strand", -FLT_MAX, FLT_MAX);
	RNA_def_function_output(func, parm);
	parm = RNA_def_float(func, "radius", 0.0f, -FLT_MAX, FLT_MAX, "Radius", "Thickness of the hair wisp", -FLT_MAX, FLT_MAX);
	RNA_def_function_output(func, parm);
}

void RNA_def_hair(BlenderRNA *brna)
{
	rna_def_hair_params(brna);
	rna_def_hair_display_settings(brna);
	rna_def_hair_render_iterator(brna);
	rna_def_hair_render_step_iterator(brna);
	rna_def_hair_system(brna);
}

#endif

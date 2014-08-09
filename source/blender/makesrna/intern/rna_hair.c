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

#include "WM_api.h"

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
}

void RNA_def_hair(BlenderRNA *brna)
{
	rna_def_hair_params(brna);
	rna_def_hair_display_settings(brna);
	rna_def_hair_system(brna);
}

#endif

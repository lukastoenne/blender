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

static void rna_HairSystem_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
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

	prop = RNA_def_property(srna, "stretch_stiffness", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "stretch_stiffness");
	RNA_def_property_range(prop, 0.0f, 1.0e9f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0e8f, 0.1, 2);
	RNA_def_property_ui_text(prop, "Stretch Stiffness", "");

	prop = RNA_def_property(srna, "stretch_damping", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "stretch_damping");
	RNA_def_property_range(prop, 0.0f, 1.0e6f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0e5f, 0.1, 2);
	RNA_def_property_ui_text(prop, "Stretch Damping", "");
}

static void rna_def_hair_system(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "HairSystem", NULL);
	RNA_def_struct_ui_text(srna, "Hair System", "Hair simulation and rendering");

	prop = RNA_def_property(srna, "smooth", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "smooth");
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1, 2);
	RNA_def_property_ui_text(prop, "Smoothing", "Amount of smoothing");
	RNA_def_property_update(prop, 0, "rna_HairSystem_update");

	prop = RNA_def_property(srna, "params", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "params");
	RNA_def_property_struct_type(prop, "HairParams");
	RNA_def_property_ui_text(prop, "Parameters", "Parameters for the hair simulation");
}

void RNA_def_hair(BlenderRNA *brna)
{
	rna_def_hair_params(brna);
	rna_def_hair_system(brna);
}

#endif

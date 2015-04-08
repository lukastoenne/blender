/*
 * Copyright 2015, Blender Foundation.
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
 */

/** \file blender/makesrna/intern/rna_strands.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "BLI_utildefines.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "rna_internal.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "BLI_math.h"

#include "BKE_strands.h"
#include "BKE_report.h"

static int rna_Strands_has_motion_state_get(PointerRNA *ptr)
{
	Strands *strands = ptr->data;
	return (bool)(strands->state != NULL);
}

#else

static void rna_def_strands_curve(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "StrandsCurve", NULL);
	RNA_def_struct_sdna(srna, "StrandsCurve");
	RNA_def_struct_ui_text(srna, "Strand Curve", "Strand curve");
	
	prop = RNA_def_property(srna, "size", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "numverts");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Size", "Number of vertices of the curve");
}

static void rna_def_strands_vertex(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "StrandsVertex", NULL);
	RNA_def_struct_sdna(srna, "StrandsVertex");
	RNA_def_struct_ui_text(srna, "Strand Vertex", "Strand vertex");
	
	prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "co");
	RNA_def_property_array(prop, 3);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Location", "");
}

static void rna_def_strands_motion_state(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "StrandsMotionState", NULL);
	RNA_def_struct_sdna(srna, "StrandsMotionState");
	RNA_def_struct_ui_text(srna, "Strand Vertex Motion State", "Physical motion state of a vertex");
	
	prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "co");
	RNA_def_property_array(prop, 3);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Location", "");
}

static void rna_def_strands(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "Strands", NULL);
	RNA_def_struct_sdna(srna, "Strands");
	RNA_def_struct_ui_text(srna, "Strands", "Strand geometry to represent hair and similar linear structures");
	
	prop = RNA_def_property(srna, "curves", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "curves", "totcurves");
	RNA_def_property_struct_type(prop, "StrandsCurve");
	RNA_def_property_ui_text(prop, "Strand Curves", "");
	
	prop = RNA_def_property(srna, "vertices", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "verts", "totverts");
	RNA_def_property_struct_type(prop, "StrandsVertex");
	RNA_def_property_ui_text(prop, "Strand Vertex", "");
	
	prop = RNA_def_property(srna, "has_motion_state", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_Strands_has_motion_state_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Has Motion State", "Strands have physical motion data associated with vertices");
	
	prop = RNA_def_property(srna, "motion_state", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "state", "totverts");
	RNA_def_property_struct_type(prop, "StrandsMotionState");
	RNA_def_property_ui_text(prop, "Strand Motion State", "");
}

void RNA_def_strands(BlenderRNA *brna)
{
	rna_def_strands_curve(brna);
	rna_def_strands_vertex(brna);
	rna_def_strands_motion_state(brna);
	rna_def_strands(brna);
}

#endif

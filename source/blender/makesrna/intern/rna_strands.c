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

static void rna_StrandsChildren_curve_uvs_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	StrandsChildren *strands = ptr->data;
	rna_iterator_array_begin(iter, strands->curve_uvs, sizeof(StrandsChildCurveUV), strands->totcurves * strands->numuv, false, NULL);
}

static int rna_StrandsChildren_curve_uvs_length(PointerRNA *ptr)
{
	StrandsChildren *strands = ptr->data;
	return strands->totcurves * strands->numuv;
}

static int rna_StrandsChildren_curve_uvs_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
	StrandsChildren *strands = ptr->data;
	if (index >= 0 && index < strands->totcurves * strands->numuv) {
		RNA_pointer_create(ptr->id.data, &RNA_StrandsChildCurveUV, strands->curve_uvs + index, r_ptr);
		return true;
	}
	else {
		RNA_pointer_create(ptr->id.data, &RNA_StrandsChildCurveUV, NULL, r_ptr);
		return false;
	}
}

static void rna_StrandsChildren_curve_vcols_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	StrandsChildren *strands = ptr->data;
	rna_iterator_array_begin(iter, strands->curve_vcols, sizeof(StrandsChildCurveVCol), strands->totcurves * strands->numvcol, false, NULL);
}

static int rna_StrandsChildren_curve_vcols_length(PointerRNA *ptr)
{
	StrandsChildren *strands = ptr->data;
	return strands->totcurves * strands->numvcol;
}

static int rna_StrandsChildren_curve_vcols_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
	StrandsChildren *strands = ptr->data;
	if (index >= 0 && index < strands->totcurves * strands->numvcol) {
		RNA_pointer_create(ptr->id.data, &RNA_StrandsChildCurveVCol, strands->curve_vcols + index, r_ptr);
		return true;
	}
	else {
		RNA_pointer_create(ptr->id.data, &RNA_StrandsChildCurveVCol, NULL, r_ptr);
		return false;
	}
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

static void rna_def_strands_child_curve(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "StrandsChildCurve", NULL);
	RNA_def_struct_sdna(srna, "StrandsChildCurve");
	RNA_def_struct_ui_text(srna, "Strand Child Curve", "Strand child curve");
	
	prop = RNA_def_property(srna, "size", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "numverts");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Size", "Number of vertices of the curve");
}

static void rna_def_strands_child_curve_uv(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "StrandsChildCurveUV", NULL);
	RNA_def_struct_sdna(srna, "StrandsChildCurveUV");
	RNA_def_struct_ui_text(srna, "Strand Child Curve UV", "UV data for child strand curves");
	
	prop = RNA_def_property(srna, "uv", PROP_FLOAT, PROP_NONE);
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_text(prop, "UV", "");
}

static void rna_def_strands_child_curve_vcol(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "StrandsChildCurveVCol", NULL);
	RNA_def_struct_sdna(srna, "StrandsChildCurveVCol");
	RNA_def_struct_ui_text(srna, "Strand Child Curve Vertex Color", "Vertex color data for child strand curves");
	
	prop = RNA_def_property(srna, "vcol", PROP_FLOAT, PROP_NONE);
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Vertex Color", "");
}

static void rna_def_strands_child_vertex(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "StrandsChildVertex", NULL);
	RNA_def_struct_sdna(srna, "StrandsChildVertex");
	RNA_def_struct_ui_text(srna, "Strand Child Vertex", "Strand child vertex");
	
	prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "co");
	RNA_def_property_array(prop, 3);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Location", "");
}

static void rna_def_strands_children(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "StrandsChildren", NULL);
	RNA_def_struct_sdna(srna, "StrandsChildren");
	RNA_def_struct_ui_text(srna, "Child Strands", "Strand geometry to represent hair and similar linear structures");
	
	prop = RNA_def_property(srna, "curves", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "curves", "totcurves");
	RNA_def_property_struct_type(prop, "StrandsChildCurve");
	RNA_def_property_ui_text(prop, "Strand Child Curves", "");
	
	prop = RNA_def_property(srna, "curve_uvs", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_StrandsChildren_curve_uvs_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_get",
	                                  "rna_StrandsChildren_curve_uvs_length", "rna_StrandsChildren_curve_uvs_lookup_int", NULL, NULL);
	RNA_def_property_struct_type(prop, "StrandsChildCurveUV");
	RNA_def_property_ui_text(prop, "Strand Child Curves UV", "");
	
	prop = RNA_def_property(srna, "num_curve_uv_layers", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "numuv");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "UV Layers", "Number of UV layers");
	
	prop = RNA_def_property(srna, "curve_vcols", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_StrandsChildren_curve_vcols_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_get",
	                                  "rna_StrandsChildren_curve_vcols_length", "rna_StrandsChildren_curve_vcols_lookup_int", NULL, NULL);
	RNA_def_property_struct_type(prop, "StrandsChildCurveVCol");
	RNA_def_property_ui_text(prop, "Strand Child Curves Vertex Colors", "");
	
	prop = RNA_def_property(srna, "num_curve_vcol_layers", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "numvcol");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Vertex Color Layers", "Number of Vertex Color layers");
	
	prop = RNA_def_property(srna, "vertices", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "verts", "totverts");
	RNA_def_property_struct_type(prop, "StrandsChildVertex");
	RNA_def_property_ui_text(prop, "Strand Child Vertex", "");
}

void RNA_def_strands(BlenderRNA *brna)
{
	rna_def_strands_curve(brna);
	rna_def_strands_vertex(brna);
	rna_def_strands_motion_state(brna);
	rna_def_strands(brna);
	rna_def_strands_child_curve(brna);
	rna_def_strands_child_curve_uv(brna);
	rna_def_strands_child_curve_vcol(brna);
	rna_def_strands_child_vertex(brna);
	rna_def_strands_children(brna);
}

#endif

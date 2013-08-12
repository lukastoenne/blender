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

/** \file blender/makesrna/intern/rna_nparticle.c
 *  \ingroup RNA
 */

#include "DNA_nparticle_types.h"

#include "RNA_define.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

#include "BKE_nparticle.h"
#include "BKE_report.h"

static void rna_NParticleAttribute_datatype_set(PointerRNA *ptr, int value)
{
//	NParticleAttribute *attr = ptr->data;
	/* XXX TODO */
	BLI_assert(false);
}

static NParticleBufferAttribute *rna_NParticleBuffer_attributes_new(NParticleBuffer *buf, ReportList *reports, const char *name, int datatype)
{
	if (BKE_nparticle_attribute_find(buf, name)) {
		BKE_reportf(reports, RPT_ERROR_INVALID_INPUT, "Particle attribute with name %s already exists", name);
		return NULL;
	}
	return BKE_nparticle_attribute_new(buf, name, datatype);
}

static void rna_NParticleBuffer_attributes_remove(NParticleBuffer *buf, NParticleBufferAttribute *attr)
{
	BKE_nparticle_attribute_remove(buf, attr);
}

static void rna_NParticleBuffer_attributes_clear(NParticleBuffer *buf)
{
	BKE_nparticle_attribute_remove_all(buf);
}

static void rna_NParticleBuffer_attributes_move(NParticleBuffer *buf, int from_index, int to_index)
{
	BKE_nparticle_attribute_move(buf, from_index, to_index);
}

#else

EnumPropertyItem nparticle_attribute_datatype_all[] = {
    {PAR_ATTR_DATATYPE_INTERNAL,    "INTERNAL",     0,  "Internal"      ""},
    {PAR_ATTR_DATATYPE_FLOAT,       "FLOAT",        0,  "Float"         ""},
    {PAR_ATTR_DATATYPE_INT,         "INT",          0,  "Int"           ""},
    {PAR_ATTR_DATATYPE_BOOL,        "BOOL",         0,  "Bool"          ""},
    {PAR_ATTR_DATATYPE_VECTOR,      "VECTOR",       0,  "Vector"        ""},
    {PAR_ATTR_DATATYPE_POINT,       "POINT",        0,  "Point"         ""},
    {PAR_ATTR_DATATYPE_NORMAL,      "NORMAL",       0,  "Normal"        ""},
    {PAR_ATTR_DATATYPE_COLOR,       "COLOR",        0,  "Color"         ""},
    {PAR_ATTR_DATATYPE_MATRIX,      "MATRIX",       0,  "Matrix"        ""},
    {0, NULL, 0, NULL, NULL}
};

EnumPropertyItem nparticle_attribute_datatype_user[] = {
    {PAR_ATTR_DATATYPE_FLOAT,       "FLOAT",        0,  "Float"         ""},
    {PAR_ATTR_DATATYPE_INT,         "INT",          0,  "Int"           ""},
    {PAR_ATTR_DATATYPE_BOOL,        "BOOL",         0,  "Bool"          ""},
    {PAR_ATTR_DATATYPE_VECTOR,      "VECTOR",       0,  "Vector"        ""},
    {PAR_ATTR_DATATYPE_POINT,       "POINT",        0,  "Point"         ""},
    {PAR_ATTR_DATATYPE_NORMAL,      "NORMAL",       0,  "Normal"        ""},
    {PAR_ATTR_DATATYPE_COLOR,       "COLOR",        0,  "Color"         ""},
    {PAR_ATTR_DATATYPE_MATRIX,      "MATRIX",       0,  "Matrix"        ""},
    {0, NULL, 0, NULL, NULL}
};

static void def_nparticle_attribute(StructRNA *srna)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Name", "Unique name");
	RNA_def_struct_name_property(srna, prop);

	prop = RNA_def_property(srna, "datatype", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "datatype");
	RNA_def_property_enum_items(prop, nparticle_attribute_datatype_all);
	RNA_def_property_enum_funcs(prop, NULL, "rna_NParticleAttribute_datatype_set", NULL);
	RNA_def_property_ui_text(prop, "Data Type", "Basic data type");
}

static void rna_def_nparticle_buffer_attribute(BlenderRNA *brna)
{
	StructRNA *srna;

	srna = RNA_def_struct(brna, "NParticleBufferAttribute", NULL);
	RNA_def_struct_sdna(srna, "NParticleBufferAttribute");
	RNA_def_struct_ui_text(srna, "Particle Buffer Attribute", "Attribute data associated to particles");

	RNA_def_struct_sdna_from(srna, "NParticleAttribute", "desc");
	def_nparticle_attribute(srna);
	RNA_def_struct_sdna_from(srna, "NParticleBufferAttribute", NULL); /* reset */
}

static void rna_def_nparticle_buffer_attributes_api(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *parm;
	FunctionRNA *func;

	RNA_def_property_srna(cprop, "NParticleBufferAttributes");
	srna = RNA_def_struct(brna, "NParticleBufferAttributes", NULL);
	RNA_def_struct_sdna(srna, "NParticleBuffer");
	RNA_def_struct_ui_text(srna, "Attributes", "Collection of particle attributes");

	func = RNA_def_function(srna, "new", "rna_NParticleBuffer_attributes_new");
	RNA_def_function_ui_description(func, "Add a particle attribute");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_string(func, "name", "", MAX_NAME, "Name", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_enum(func, "datatype", nparticle_attribute_datatype_user, PAR_ATTR_DATATYPE_FLOAT, "Data Type", "Base data type");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return value */
	parm = RNA_def_pointer(func, "attr", "NParticleBufferAttribute", "", "Attribute");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_NParticleBuffer_attributes_remove");
	RNA_def_function_ui_description(func, "Remove an attribute from the buffer");
	parm = RNA_def_pointer(func, "attr", "NParticleBufferAttribute", "", "The attribute to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "clear", "rna_NParticleBuffer_attributes_clear");
	RNA_def_function_ui_description(func, "Remove all attributes from the buffer");

	func = RNA_def_function(srna, "move", "rna_NParticleBuffer_attributes_move");
	RNA_def_function_ui_description(func, "Move an attribute to another position");
	parm = RNA_def_int(func, "from_index", -1, 0, INT_MAX, "From Index", "Index of the attribute to move", 0, 10000);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_int(func, "to_index", -1, 0, INT_MAX, "To Index", "Target index for the attribute", 0, 10000);
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

static void rna_def_nparticle_buffer(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "NParticleBuffer", NULL);
	RNA_def_struct_ui_text(srna, "Particle Buffer", "Container for particles");

	prop = RNA_def_property(srna, "attributes", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "attributes", NULL);
	RNA_def_property_ui_text(prop, "Attributes", "Data layers associated to particles");
	RNA_def_property_struct_type(prop, "NParticleBufferAttribute");
	rna_def_nparticle_buffer_attributes_api(brna, prop);
}

void RNA_def_nparticle(BlenderRNA *brna)
{
	rna_def_nparticle_buffer_attribute(brna);
	rna_def_nparticle_buffer(brna);
}

#endif

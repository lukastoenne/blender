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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_mesh_sample.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"

#include "BKE_mesh_sample.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "WM_types.h"


#ifdef RNA_RUNTIME

#include "WM_api.h"
#include "WM_types.h"



#else

static void rna_def_mesh_sample(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MeshSample", NULL);
	RNA_def_struct_sdna(srna, "MeshSample");
	RNA_def_struct_ui_text(srna, "Mesh Sample", "Point on a mesh that follows deformation");

	prop = RNA_def_property(srna, "vertex_indices", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "orig_verts");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Vertex Indices", "Index of the mesh vertices used for interpolation");
}

void RNA_def_mesh_sample(BlenderRNA *brna)
{
	rna_def_mesh_sample(brna);
}

#endif

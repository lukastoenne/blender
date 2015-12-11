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
 * Contributor(s): Blender Foundation (2014).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_depsgraph.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "BLI_utildefines.h"
#include "BLI_path_util.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "DEG_depsgraph.h"

#include "BKE_depsgraph.h"

#include "DEG_depsgraph_build.h"

#ifdef RNA_RUNTIME

#include "BKE_report.h"

#include "DEG_depsgraph_debug.h"

static void rna_DepsNode_add_scene_relation(struct DepsNodeHandle *node, struct Scene *scene,
                                            int component, const char *description)
{
	DEG_add_scene_relation(node, scene, component, description);
}

static void rna_DepsNode_add_object_relation(struct DepsNodeHandle *node, struct Object *ob,
                                             int component, const char *description)
{
	DEG_add_object_relation(node, ob, component, description);
}

static void rna_DepsNode_add_bone_relation(struct DepsNodeHandle *node, struct Object *ob, const char *bone_name,
                                           int component, const char *description)
{
	DEG_add_bone_relation(node, ob, bone_name, component, description);
}

static void rna_DepsNode_add_texture_relation(struct DepsNodeHandle *node, struct Tex *tex,
                                              int component, const char *description)
{
	DEG_add_texture_relation(node, tex, component, description);
}

/* ------------------------------------------------------------------------- */

static void rna_Depsgraph_debug_graphviz(Depsgraph *graph, const char *filename)
{
	FILE *f = fopen(filename, "w");
	if (f == NULL)
		return;
	
	DEG_debug_graphviz(graph, f, "Depsgraph", false);
	
	fclose(f);
}

static void rna_Depsgraph_debug_rebuild(Depsgraph *UNUSED(graph), Main *bmain)
{
	Scene *sce;
	DAG_relations_tag_update(bmain);
	for (sce = bmain->scene.first; sce; sce = sce->id.next) {
		DAG_scene_relations_rebuild(bmain, sce);
		DEG_graph_on_visible_update(bmain, sce);
	}
}

static void rna_Depsgraph_debug_stats(Depsgraph *graph, ReportList *reports)
{
	size_t outer, ops, rels;
	
	DEG_stats_simple(graph, &outer, &ops, &rels);
	
	// XXX: report doesn't seem to work
	printf("Approx %lu Operations, %lu Relations, %lu Outer Nodes\n",
	       ops, rels, outer);
		   
	BKE_reportf(reports, RPT_WARNING, "Approx. %lu Operations, %lu Relations, %lu Outer Nodes",
	            ops, rels, outer);
}

#else

static void rna_def_depsnode(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;
	
	static EnumPropertyItem scene_component_items[] = {
	    {DEG_SCENE_COMP_PARAMETERS, "PARAMETERS", ICON_NONE, "Parameters", ""},
	    {DEG_SCENE_COMP_ANIMATION, "ANIMATION", ICON_NONE, "Animation", ""},
	    {DEG_SCENE_COMP_SEQUENCER, "SEQUENCER", ICON_NONE, "Sequencer", ""},
	    {0, NULL, 0, NULL, NULL}
	};
	
	static EnumPropertyItem object_component_items[] = {
	    {DEG_OB_COMP_PARAMETERS, "PARAMETERS", ICON_NONE, "Parameters", ""},
	    {DEG_OB_COMP_PROXY, "PROXY", ICON_NONE, "Proxy", ""},
	    {DEG_OB_COMP_ANIMATION, "ANIMATION", ICON_NONE, "Animation", ""},
	    {DEG_OB_COMP_TRANSFORM, "TRANSFORM", ICON_NONE, "Transform", ""},
	    {DEG_OB_COMP_GEOMETRY, "GEOMETRY", ICON_NONE, "Geometry", ""},
	    {DEG_OB_COMP_EVAL_POSE, "EVAL_POSE", ICON_NONE, "Pose", ""},
	    {DEG_OB_COMP_BONE, "BONE", ICON_NONE, "Bone", ""},
	    {DEG_OB_COMP_EVAL_PARTICLES, "PARTICLES", ICON_NONE, "Particles", ""},
	    {DEG_OB_COMP_SHADING, "SHADING", ICON_NONE, "Shading", ""},
	    {0, NULL, 0, NULL, NULL}
	};
	
	static EnumPropertyItem texture_component_items[] = {
	    {DEG_OB_TEX_PARAMETERS, "PARAMETERS", ICON_NONE, "Parameters", ""},
	    {0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "DepsNode", NULL);
	RNA_def_struct_sdna(srna, "DepsNodeHandle");
	RNA_def_struct_ui_text(srna, "Depsgraph Node", "");
	
	func = RNA_def_function(srna, "add_scene_relation", "rna_DepsNode_add_scene_relation");
	parm = RNA_def_pointer(func, "scene", "Scene", "Scene", "Scene the node depends on");
	RNA_def_property_flag(parm, PROP_NEVER_NULL | PROP_REQUIRED);
	RNA_def_enum(func, "component", scene_component_items, DEG_SCENE_COMP_PARAMETERS, "Component",
	             "Component of data the node depends on");
	RNA_def_string(func, "description", NULL, 0, "Description", "Description of the relation");
	
	func = RNA_def_function(srna, "add_object_relation", "rna_DepsNode_add_object_relation");
	parm = RNA_def_pointer(func, "object", "Object", "Object", "Object the node depends on");
	RNA_def_property_flag(parm, PROP_NEVER_NULL | PROP_REQUIRED);
	RNA_def_enum(func, "component", object_component_items, DEG_OB_COMP_PARAMETERS, "Component",
	             "Component of data the node depends on");
	RNA_def_string(func, "description", NULL, 0, "Description", "Description of the relation");
	
	func = RNA_def_function(srna, "add_bone_relation", "rna_DepsNode_add_bone_relation");
	parm = RNA_def_pointer(func, "object", "Object", "Object", "Object the node depends on");
	RNA_def_property_flag(parm, PROP_NEVER_NULL | PROP_REQUIRED);
	RNA_def_string(func, "bone", NULL, 0, "Bone", "Name of the bone the node depends on");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_enum(func, "component", object_component_items, DEG_OB_COMP_PARAMETERS, "Component",
	             "Component of data the node depends on");
	RNA_def_string(func, "description", NULL, 0, "Description", "Description of the relation");
	
	func = RNA_def_function(srna, "add_texture_relation", "rna_DepsNode_add_texture_relation");
	parm = RNA_def_pointer(func, "texture", "Texture", "Texture", "Texture the node depends on");
	RNA_def_property_flag(parm, PROP_NEVER_NULL | PROP_REQUIRED);
	RNA_def_enum(func, "component", texture_component_items, DEG_OB_TEX_PARAMETERS, "Component",
	             "Component of data the node depends on");
	RNA_def_string(func, "description", NULL, 0, "Description", "Description of the relation");
}

static void rna_def_depsgraph(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;

	srna = RNA_def_struct(brna, "Depsgraph", NULL);
	RNA_def_struct_ui_text(srna, "Dependency Graph", "");
	
	func = RNA_def_function(srna, "debug_graphviz", "rna_Depsgraph_debug_graphviz");
	parm = RNA_def_string_file_path(func, "filename", NULL, FILE_MAX, "File Name",
	                                "File in which to store graphviz debug output");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "debug_rebuild", "rna_Depsgraph_debug_rebuild");
	RNA_def_function_flag(func, FUNC_USE_MAIN);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	
	func = RNA_def_function(srna, "debug_stats", "rna_Depsgraph_debug_stats");
	RNA_def_function_ui_description(func, "Report the number of elements in the Dependency Graph");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
}

void RNA_def_depsgraph(BlenderRNA *brna)
{
	rna_def_depsnode(brna);
	rna_def_depsgraph(brna);
}

#endif

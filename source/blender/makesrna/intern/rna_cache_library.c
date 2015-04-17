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
 * Contributor(s): Blender Foundation (2015).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_cache_library.c
 *  \ingroup RNA
 */

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "DNA_cache_library_types.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_types.h"

EnumPropertyItem cache_library_data_type_items[] = {
    {CACHE_TYPE_OBJECT,         "OBJECT",			ICON_OBJECT_DATA,       "Object", "Object base properties"},
    {CACHE_TYPE_DERIVED_MESH,   "DERIVED_MESH",     ICON_OUTLINER_OB_MESH,  "Derived Mesh", "Mesh result from modifiers"},
    {CACHE_TYPE_HAIR,           "HAIR",             ICON_PARTICLE_POINT,    "Hair", "Hair parent strands"},
    {CACHE_TYPE_HAIR_PATHS,     "HAIR_PATHS",       ICON_PARTICLE_PATH,     "Hair Paths", "Full hair paths"},
    {CACHE_TYPE_PARTICLES,      "PARTICLES",        ICON_PARTICLES,         "Particles", "Emitter particles"},
    {0, NULL, 0, NULL, NULL}
};

EnumPropertyItem cache_library_read_result_items[] = {
    {CACHE_READ_SAMPLE_INVALID,         "INVALID",      ICON_ERROR,             "Invalid",      "No valid sample found"},
    {CACHE_READ_SAMPLE_EXACT,           "EXACT",        ICON_SPACE3,            "Exact",        "Found sample for requested frame"},
    {CACHE_READ_SAMPLE_INTERPOLATED,    "INTERPOLATED", ICON_TRIA_DOWN_BAR,     "Interpolated", "Enclosing samples found for interpolation"},
    {CACHE_READ_SAMPLE_EARLY,           "EARLY",        ICON_TRIA_RIGHT_BAR,    "Early",        "Requested frame before the first sample"},
    {CACHE_READ_SAMPLE_LATE,            "LATE",         ICON_TRIA_LEFT_BAR,     "Late",         "Requested frame after the last sample"},
    {0, NULL, 0, NULL, NULL}
};

EnumPropertyItem cache_modifier_type_items[] = {
    {eCacheModifierType_HairSimulation, "HAIR_SIMULATION", ICON_HAIR, "Hair Simulation", ""},
    {eCacheModifierType_ForceField, "FORCE_FIELD", ICON_FORCE_FORCE, "Force Field", ""},
    {0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_object_types.h"
#include "DNA_particle_types.h"

#include "BKE_animsys.h"
#include "BKE_cache_library.h"
#include "BKE_depsgraph.h"
#include "BKE_main.h"

#include "RNA_access.h"

#include "WM_api.h"

/* ========================================================================= */

static void rna_CacheLibrary_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	CacheLibrary *cachelib = ptr->data;
	DAG_id_tag_update(&cachelib->id, OB_RECALC_DATA);
	WM_main_add_notifier(NC_WINDOW, NULL);
}

/* ========================================================================= */

static void rna_CacheModifier_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *UNUSED(ptr))
{
}

static void rna_CacheModifier_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	rna_CacheModifier_update(bmain, scene, ptr);
	DAG_relations_tag_update(bmain);
}


static StructRNA *rna_CacheModifier_refine(struct PointerRNA *ptr)
{
	CacheModifier *md = (CacheModifier *)ptr->data;

	switch ((eCacheModifier_Type)md->type) {
		case eCacheModifierType_HairSimulation:
			return &RNA_HairSimulationCacheModifier;
		case eCacheModifierType_ForceField:
			return &RNA_ForceFieldCacheModifier;
			
		/* Default */
		case eCacheModifierType_None:
		case NUM_CACHE_MODIFIER_TYPES:
			return &RNA_CacheLibraryModifier;
	}

	return &RNA_CacheLibraryModifier;
}

static void rna_CacheLibraryModifier_name_set(PointerRNA *ptr, const char *value)
{
	CacheModifier *md = ptr->data;
	char oldname[sizeof(md->name)];
	
	/* make a copy of the old name first */
	BLI_strncpy(oldname, md->name, sizeof(md->name));
	
	/* copy the new name into the name slot */
	BLI_strncpy_utf8(md->name, value, sizeof(md->name));
	
	/* make sure the name is truly unique */
	if (ptr->id.data) {
		CacheLibrary *cachelib = ptr->id.data;
		BKE_cache_modifier_unique_name(&cachelib->modifiers, md);
	}
	
	/* fix all the animation data which may link to this */
	BKE_animdata_fix_paths_rename_all(NULL, "modifiers", oldname, md->name);
}

static char *rna_CacheLibraryModifier_path(PointerRNA *ptr)
{
	CacheModifier *md = ptr->data;
	char name_esc[sizeof(md->name) * 2];

	BLI_strescape(name_esc, md->name, sizeof(name_esc));
	return BLI_sprintfN("modifiers[\"%s\"]", name_esc);
}

static CacheModifier *rna_CacheLibrary_modifier_new(CacheLibrary *cachelib, bContext *UNUSED(C), ReportList *UNUSED(reports),
                                                    const char *name, int type)
{
	return BKE_cache_modifier_add(cachelib, name, type);
}

static void rna_CacheLibrary_modifier_remove(CacheLibrary *cachelib, bContext *UNUSED(C), ReportList *UNUSED(reports), PointerRNA *md_ptr)
{
	CacheModifier *md = md_ptr->data;
	
	BKE_cache_modifier_remove(cachelib, md);

	RNA_POINTER_INVALIDATE(md_ptr);
}

static void rna_CacheLibrary_modifier_clear(CacheLibrary *cachelib, bContext *UNUSED(C))
{
	BKE_cache_modifier_clear(cachelib);
}

/* ------------------------------------------------------------------------- */

static int rna_CacheLibraryModifier_mesh_object_poll(PointerRNA *UNUSED(ptr), PointerRNA value)
{
	/*HairSimCacheModifier *hsmd = ptr->data;*/
	Object *ob = value.data;
	
	return ob->type == OB_MESH && ob->data != NULL;
}

static int rna_CacheLibraryModifier_hair_object_poll(PointerRNA *UNUSED(ptr), PointerRNA value)
{
	/*HairSimCacheModifier *hsmd = ptr->data;*/
	Object *ob = value.data;
	ParticleSystem *psys;
	bool has_hair_system = false;
	
	for (psys = ob->particlesystem.first; psys; psys = psys->next) {
		if (psys->part && psys->part->type == PART_HAIR) {
			has_hair_system = true;
			break;
		}
	}
	return has_hair_system;
}

static PointerRNA rna_HairSimulationCacheModifier_hair_system_get(PointerRNA *ptr)
{
	HairSimCacheModifier *hsmd = ptr->data;
	ParticleSystem *psys = hsmd->object ? BLI_findlink(&hsmd->object->particlesystem, hsmd->hair_system) : NULL;
	PointerRNA value;
	
	RNA_pointer_create(ptr->id.data, &RNA_ParticleSystem, psys, &value);
	return value;
}

static void rna_HairSimulationCacheModifier_hair_system_set(PointerRNA *ptr, PointerRNA value)
{
	HairSimCacheModifier *hsmd = ptr->data;
	ParticleSystem *psys = value.data;
	hsmd->hair_system = hsmd->object ? BLI_findindex(&hsmd->object->particlesystem, psys) : -1;
}

static int rna_HairSimulationCacheModifier_hair_system_poll(PointerRNA *ptr, PointerRNA value)
{
	HairSimCacheModifier *hsmd = ptr->data;
	ParticleSystem *psys = value.data;
	
	if (!hsmd->object)
		return false;
	if (BLI_findindex(&hsmd->object->particlesystem, psys) == -1)
		return false;
	if (!psys->part || psys->part->type != PART_HAIR)
		return false;
	return true;
}

#else

static void rna_def_hair_sim_params(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "HairSimulationParameters", NULL);
	RNA_def_struct_sdna(srna, "HairSimParams");
	RNA_def_struct_ui_text(srna, "Hair Simulation Parameters", "Simulation parameters for hair simulation");
	RNA_def_struct_ui_icon(srna, ICON_HAIR);
	
	prop = RNA_def_property(srna, "timescale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1f, 3);
	RNA_def_property_ui_text(prop, "Time Scale", "Simulation time scale relative to scene time");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "substeps", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 1, 80);
	RNA_def_property_ui_text(prop, "Substeps", "Simulation steps per frame");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "effector_weights", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "EffectorWeights");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Effector Weights", "");
	
	prop = RNA_def_property(srna, "mass", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Mass", "Mass of hair vertices");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "drag", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1f, 3);
	RNA_def_property_ui_text(prop, "Drag", "Drag simulating friction with surrounding air");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "goal_stiffness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1f, 3);
	RNA_def_property_float_default(prop, 0.0f);
	RNA_def_property_ui_text(prop, "Goal Strength", "Goal spring, pulling vertices toward their rest position");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "goal_damping", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1f, 3);
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_ui_text(prop, "Goal Damping", "Damping factor of goal springs");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "use_goal_stiffness_curve", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eHairSimParams_Flag_UseGoalStiffnessCurve);
	RNA_def_property_ui_text(prop, "Use Goal Stiffness Curve", "Use a curve to define goal stiffness along the strand");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "goal_stiffness_curve", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "goal_stiffness_mapping");
	RNA_def_property_struct_type(prop, "CurveMapping");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Goal Stiffness Curve", "Stiffness of goal springs along the strand curves");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "stretch_stiffness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 10000.0f, 0.1f, 3);
	RNA_def_property_float_default(prop, 10000.0f);
	RNA_def_property_ui_text(prop, "Stretch Stiffness", "Resistence to stretching");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "stretch_damping", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1f, 3);
	RNA_def_property_float_default(prop, 0.1f);
	RNA_def_property_ui_text(prop, "Stretch Damping", "Damping factor of stretch springs");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "bend_stiffness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 1000.0f, 0.1f, 3);
	RNA_def_property_float_default(prop, 100.0f);
	RNA_def_property_ui_text(prop, "Bend Stiffness", "Resistance to bending of the rest shape");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "bend_damping", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1f, 3);
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_ui_text(prop, "Bend Damping", "Damping factor of bending springs");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
}

static void rna_def_cache_modifier_hair_simulation(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	rna_def_hair_sim_params(brna);
	
	srna = RNA_def_struct(brna, "HairSimulationCacheModifier", "CacheLibraryModifier");
	RNA_def_struct_sdna(srna, "HairSimCacheModifier");
	RNA_def_struct_ui_text(srna, "Hair Simulation Cache Modifier", "Apply hair dynamics simulation to the cache");
	RNA_def_struct_ui_icon(srna, ICON_HAIR);
	
	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "object");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_funcs(prop, NULL, NULL, NULL, "rna_CacheLibraryModifier_hair_object_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object", "Object whose cache to simulate");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "hair_system_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "hair_system");
	RNA_def_property_ui_text(prop, "Hair System Index", "Hair system cache to simulate");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "hair_system", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_funcs(prop, "rna_HairSimulationCacheModifier_hair_system_get", "rna_HairSimulationCacheModifier_hair_system_set", NULL, "rna_HairSimulationCacheModifier_hair_system_poll");
	RNA_def_property_struct_type(prop, "ParticleSystem");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Hair System", "Hair system cache to simulate");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "parameters", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "sim_params");
	RNA_def_property_struct_type(prop, "HairSimulationParameters");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Simulation Parameters", "Parameters of the simulation");
}

static void rna_def_cache_modifier_force_field(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "ForceFieldCacheModifier", "CacheLibraryModifier");
	RNA_def_struct_sdna(srna, "ForceFieldCacheModifier");
	RNA_def_struct_ui_text(srna, "Force Field Cache Modifier", "Use an object as a force field");
	RNA_def_struct_ui_icon(srna, ICON_FORCE_FORCE);
	
	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "object");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_funcs(prop, NULL, NULL, NULL, "rna_CacheLibraryModifier_mesh_object_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object", "Object whose cache to simulate");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -10000.0f, 10000.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Strength", "");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "falloff", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1, 3);
	RNA_def_property_ui_text(prop, "Falloff", "");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "min_distance", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -100.0f, 100.0f, 0.1, 4);
	RNA_def_property_ui_text(prop, "Minimum Distance", "");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "max_distance", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -100.0f, 100.0f, 0.1, 4);
	RNA_def_property_ui_text(prop, "Maximum Distance", "");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
	
	prop = RNA_def_property(srna, "use_double_sided", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", eForceFieldCacheModifier_Flag_DoubleSided);
	RNA_def_property_ui_text(prop, "Use Double Sided", "");
	RNA_def_property_update(prop, 0, "rna_CacheModifier_update");
}

static void rna_def_cache_modifier(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "CacheLibraryModifier", NULL);
	RNA_def_struct_sdna(srna, "CacheModifier");
	RNA_def_struct_path_func(srna, "rna_CacheLibraryModifier_path");
	RNA_def_struct_refine_func(srna, "rna_CacheModifier_refine");
	RNA_def_struct_ui_text(srna, "Cache Modifier", "Cache Modifier");
	RNA_def_struct_ui_icon(srna, ICON_PHYSICS);
	
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, cache_modifier_type_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Type", "Type of the cache modifier");
	
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_CacheLibraryModifier_name_set");
	RNA_def_property_ui_text(prop, "Name", "Modifier name");
	RNA_def_property_update(prop, NC_ID | NA_RENAME, NULL);
	RNA_def_struct_name_property(srna, prop);
	
	rna_def_cache_modifier_hair_simulation(brna);
	rna_def_cache_modifier_force_field(brna);
}

static void rna_def_cache_library_modifiers(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;
	
	RNA_def_property_srna(cprop, "CacheLibraryModifiers");
	srna = RNA_def_struct(brna, "CacheLibraryModifiers", NULL);
	RNA_def_struct_sdna(srna, "CacheLibrary");
	RNA_def_struct_ui_text(srna, "Cache Modifiers", "Collection of cache modifiers");
	
	/* add modifier */
	func = RNA_def_function(srna, "new", "rna_CacheLibrary_modifier_new");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Add a new modifier");
	parm = RNA_def_string(func, "name", "Name", 0, "", "New name for the modifier");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* modifier to add */
	parm = RNA_def_enum(func, "type", cache_modifier_type_items, 1, "", "Modifier type to add");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return type */
	parm = RNA_def_pointer(func, "modifier", "CacheLibraryModifier", "", "Newly created modifier");
	RNA_def_function_return(func, parm);
	
	/* remove modifier */
	func = RNA_def_function(srna, "remove", "rna_CacheLibrary_modifier_remove");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func, "Remove an existing modifier");
	/* modifier to remove */
	parm = RNA_def_pointer(func, "modifier", "CacheLibraryModifier", "", "Modifier to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);
	
	/* clear all modifiers */
	func = RNA_def_function(srna, "clear", "rna_CacheLibrary_modifier_clear");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Remove all modifiers");
}

static void rna_def_cache_library(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem source_mode_items[] = {
	    {CACHE_LIBRARY_SOURCE_SCENE,    "SCENE",        0,      "Scene",        "Use generated scene data as source"},
	    {CACHE_LIBRARY_SOURCE_CACHE,    "CACHE",        0,      "Cache",        "Use cache data as source"},
	    {0, NULL, 0, NULL, NULL}
	};
	
	static EnumPropertyItem display_mode_items[] = {
	    {CACHE_LIBRARY_DISPLAY_SOURCE,  "SOURCE",       0,      "Source",       "Display source data unmodified"},
	    {CACHE_LIBRARY_DISPLAY_RESULT,  "RESULT",       0,      "Result",       "Display resulting data"},
	    {0, NULL, 0, NULL, NULL}
	};
	
	static EnumPropertyItem eval_mode_items[] = {
	    {CACHE_LIBRARY_EVAL_REALTIME,   "REALTIME",     ICON_RESTRICT_VIEW_OFF,     "Realtime",     "Evaluate data with realtime settings"},
	    {CACHE_LIBRARY_EVAL_RENDER,     "RENDER",       ICON_RESTRICT_RENDER_OFF,   "Render",       "Evaluate data with render settings"},
	    {0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "CacheLibrary", "ID");
	RNA_def_struct_ui_text(srna, "Cache Library", "Cache Library datablock for constructing an archive of caches");
	RNA_def_struct_ui_icon(srna, ICON_PHYSICS);
	
	prop = RNA_def_property(srna, "input_filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_sdna(prop, NULL, "input_filepath");
	RNA_def_property_ui_text(prop, "Input File Path", "Path to a cache archive for reading input");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
	prop = RNA_def_property(srna, "output_filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_sdna(prop, NULL, "output_filepath");
	RNA_def_property_ui_text(prop, "Output File Path", "Path where cache output is written");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
	prop = RNA_def_property(srna, "source_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "source_mode");
	RNA_def_property_enum_items(prop, source_mode_items);
	RNA_def_property_ui_text(prop, "Source Mode", "Source of the cache library data");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
	prop = RNA_def_property(srna, "display_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "display_mode");
	RNA_def_property_enum_items(prop, display_mode_items);
	RNA_def_property_ui_text(prop, "Display Mode", "What data to display in the viewport");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
	prop = RNA_def_property(srna, "display_motion", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "display_flag", CACHE_LIBRARY_DISPLAY_MOTION);
	RNA_def_property_ui_text(prop, "Display Motion", "Display motion state result from simulation, if available");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
	prop = RNA_def_property(srna, "display_children", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "display_flag", CACHE_LIBRARY_DISPLAY_CHILDREN);
	RNA_def_property_ui_text(prop, "Display Children", "Display child strands, if available");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
	prop = RNA_def_property(srna, "render_motion", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "render_flag", CACHE_LIBRARY_RENDER_MOTION);
	RNA_def_property_ui_text(prop, "Render Motion", "Render motion state result from simulation, if available");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
	prop = RNA_def_property(srna, "render_children", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "render_flag", CACHE_LIBRARY_RENDER_CHILDREN);
	RNA_def_property_ui_text(prop, "Render Children", "Render child strands, if available");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
	prop = RNA_def_property(srna, "eval_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "eval_mode");
	RNA_def_property_enum_items(prop, eval_mode_items);
	RNA_def_property_flag(prop, PROP_ENUM_FLAG);
	RNA_def_property_ui_text(prop, "Evaluation Mode", "Mode to use when evaluating data");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
	prop = RNA_def_property(srna, "data_types", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "data_types");
	RNA_def_property_enum_items(prop, cache_library_data_type_items);
	RNA_def_property_flag(prop, PROP_ENUM_FLAG);
	RNA_def_property_ui_text(prop, "Data Types", "Types of data to store in the cache");
	RNA_def_property_update(prop, 0, "rna_CacheLibrary_update");
	
	/* modifiers */
	prop = RNA_def_property(srna, "modifiers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "CacheLibraryModifier");
	RNA_def_property_ui_text(prop, "Modifiers", "Modifiers applying to the cached data");
	rna_def_cache_library_modifiers(brna, prop);
}

void RNA_def_cache_library(BlenderRNA *brna)
{
	rna_def_cache_modifier(brna);
	rna_def_cache_library(brna);
}

#endif

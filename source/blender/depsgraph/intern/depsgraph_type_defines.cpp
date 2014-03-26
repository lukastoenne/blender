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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Joshua Leung
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Defines and code for core node types
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_utildefines.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_types.h"
} /* extern "C" */

#include "depsgraph_types.h"
#include "depsgraph_intern.h"
#include "depsnode.h"
#include "depsnode_component.h"
#include "depsnode_operation.h"

#include "stubs.h" // XXX: THIS MUST BE REMOVED WHEN THE DEPSGRAPH REFACTOR IS DONE

void BKE_animsys_eval_driver(void *context, void *item) {}

void BKE_constraints_evaluate(void *context, void *item) {}
void BKE_pose_iktree_evaluate(void *context, void *item) {}
void BKE_pose_splineik_evaluate(void *context, void *item) {}
void BKE_pose_eval_bone(void *context, void *item) {}

void BKE_pose_rebuild_op(void *context, void *item) {}
void BKE_pose_eval_init(void *context, void *item) {}
void BKE_pose_eval_flush(void *context, void *item) {}

void BKE_particle_system_eval(void *context, void *item) {}

void BKE_rigidbody_rebuild_sim(void *context, void *item) {}
void BKE_rigidbody_eval_simulation(void *context, void *item) {}
void BKE_rigidbody_object_sync_transforms(void *context, void *item) {}

void BKE_object_eval_local_transform(void *context, void *item) {}
void BKE_object_eval_parent(void *context, void *item) {}

void BKE_mesh_eval_geometry(void *context, void *item) {}
void BKE_mball_eval_geometry(void *context, void *item) {}
void BKE_curve_eval_geometry(void *context, void *item) {}
void BKE_curve_eval_path(void *context, void *item) {}
void BKE_lattice_eval_geometry(void *context, void *item) {}

/* ******************************************************** */
/* External API */

/* Global type registry */

/* NOTE: For now, this is a hashtable not array, since the core node types
 * currently do not have contiguous ID values. Using a hash here gives us
 * more flexibility, albeit using more memory and also sacrificing a little
 * speed. Later on, when things stabilise we may turn this back to an array
 * since there are only just a few node types that an array would cope fine...
 */
static GHash *_depsnode_typeinfo_registry = NULL;

/* Registration ------------------------------------------- */

/* Register node type */
void DEG_register_node_typeinfo(DepsNodeFactory *factory)
{
	BLI_assert(factory != NULL);
	BLI_ghash_insert(_depsnode_typeinfo_registry, SET_INT_IN_POINTER(factory->type()), factory);
}

/* Register all node types */
void DEG_register_node_types(void)
{
	/* initialise registry */
	_depsnode_typeinfo_registry = BLI_ghash_int_new("Depsgraph Node Type Registry");
	
	/* register node types */
	DEG_register_base_depsnodes();
	DEG_register_component_depsnodes();
	DEG_register_operation_depsnodes();
}

/* Free registry on exit */
void DEG_free_node_types(void)
{
	BLI_ghash_free(_depsnode_typeinfo_registry, NULL, NULL);
}

/* Getters ------------------------------------------------- */

/* Get typeinfo for specified type */
DepsNodeFactory *DEG_get_node_factory(const eDepsNode_Type type)
{
	/* look up type - at worst, it doesn't exist in table yet, and we fail */
	return (DepsNodeFactory *)BLI_ghash_lookup(_depsnode_typeinfo_registry, SET_INT_IN_POINTER(type));
}

/* Get typeinfo for provided node */
DepsNodeFactory *DEG_node_get_factory(const DepsNode *node)
{
	if (!node)
		return NULL;
	
	return DEG_get_node_factory(node->type);
}

/* ******************************************************** */

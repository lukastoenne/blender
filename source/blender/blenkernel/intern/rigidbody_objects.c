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
 * The Original Code is Copyright (C) 2013 Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung, Sergej Reich, Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file rigidbody_objects.c
 *  \ingroup blenkernel
 *  \brief Rigid body object simulation
 */

#include "BLI_math.h"
#include "BLI_utildefines.h"

#ifdef WITH_BULLET
#  include "RBI_api.h"
#endif

#include "DNA_group_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_rigidbody.h"

/* ------------------------ */
/* build */

static void rigidbody_sync_object(Scene *scene, RigidBodyWorld *rbw, Object *ob, RigidBodyOb *rbo)
{
	float loc[3];
	float rot[4];
	float scale[3];

	/* only update if rigid body exists */
	if (rbo->physics_object == NULL)
		return;

	if (rbo->shape == RB_SHAPE_TRIMESH && rbo->flag & RBO_FLAG_USE_DEFORM) {
		DerivedMesh *dm = ob->derivedDeform;
		if (dm) {
			MVert *mvert = dm->getVertArray(dm);
			int totvert = dm->getNumVerts(dm);
			BoundBox *bb = BKE_object_boundbox_get(ob);

			RB_shape_trimesh_update(rbo->physics_shape, (float *)mvert, totvert, sizeof(MVert), bb->vec[0], bb->vec[6]);
		}
	}

	mat4_decompose(loc, rot, scale, ob->obmat);

	/* update scale for all objects */
	RB_body_set_scale(rbo->physics_object, scale);
	/* compensate for embedded convex hull collision margin */
	if (!(rbo->flag & RBO_FLAG_USE_MARGIN) && rbo->shape == RB_SHAPE_CONVEXH)
		RB_shape_set_margin(rbo->physics_shape, RBO_GET_MARGIN(rbo) * MIN3(scale[0], scale[1], scale[2]));

	/* make transformed objects temporarily kinmatic so that they can be moved by the user during simulation */
	if (ob->flag & SELECT && G.moving & G_TRANSFORM_OBJ) {
		RB_body_set_kinematic_state(rbo->physics_object, TRUE);
		RB_body_set_mass(rbo->physics_object, 0.0f);
	}

	/* update rigid body location and rotation for kinematic bodies */
	if (rbo->flag & RBO_FLAG_KINEMATIC || (ob->flag & SELECT && G.moving & G_TRANSFORM_OBJ)) {
		RB_body_activate(rbo->physics_object);
		RB_body_set_loc_rot(rbo->physics_object, loc, rot);
	}
	/* update influence of effectors - but don't do it on an effector */
	/* only dynamic bodies need effector update */
	else if (rbo->type == RBO_TYPE_ACTIVE && ((ob->pd == NULL) || (ob->pd->forcefield == PFIELD_NULL))) {
		EffectorWeights *effector_weights = rbw->effector_weights;
		EffectedPoint epoint;
		ListBase *effectors;

		/* get effectors present in the group specified by effector_weights */
		effectors = pdInitEffectors(scene, ob, NULL, effector_weights, true);
		if (effectors) {
			float eff_force[3] = {0.0f, 0.0f, 0.0f};
			float eff_loc[3], eff_vel[3];

			/* create dummy 'point' which represents last known position of object as result of sim */
			// XXX: this can create some inaccuracies with sim position, but is probably better than using unsimulated vals?
			RB_body_get_position(rbo->physics_object, eff_loc);
			RB_body_get_linear_velocity(rbo->physics_object, eff_vel);

			pd_point_from_loc(scene, eff_loc, eff_vel, 0, &epoint);

			/* calculate net force of effectors, and apply to sim object
			 *	- we use 'central force' since apply force requires a "relative position" which we don't have...
			 */
			pdDoEffectors(effectors, NULL, effector_weights, &epoint, eff_force, NULL);
			if (G.f & G_DEBUG)
				printf("\tapplying force (%f,%f,%f) to '%s'\n", eff_force[0], eff_force[1], eff_force[2], ob->id.name + 2);
			/* activate object in case it is deactivated */
			if (!is_zero_v3(eff_force))
				RB_body_activate(rbo->physics_object);
			RB_body_apply_central_force(rbo->physics_object, eff_force);
		}
		else if (G.f & G_DEBUG)
			printf("\tno forces to apply to '%s'\n", ob->id.name + 2);

		/* cleanup */
		pdEndEffectors(&effectors);
	}
	/* NOTE: passive objects don't need to be updated since they don't move */

	/* NOTE: no other settings need to be explicitly updated here,
	 * since RNA setters take care of the rest :)
	 */
}

/* Create physics sim representation of object given RigidBody settings
 * < rebuild: even if an instance already exists, replace it
 */
static void rigidbody_validate_sim_object(RigidBodyWorld *rbw, Object *ob, bool rebuild)
{
	RigidBodyOb *rbo = (ob) ? ob->rigidbody_object : NULL;
	rbRigidBody *body;
	float loc[3];
	float rot[4];

	/* sanity checks:
	 *	- object doesn't have RigidBody info already: then why is it here?
	 */
	if (rbo == NULL)
		return;

	/* make sure collision shape exists */
	/* FIXME we shouldn't always have to rebuild collision shapes when rebuilding objects, but it's needed for constraints to update correctly */
	if (rbo->physics_shape == NULL || rebuild)
		BKE_rigidbody_validate_sim_shape(ob, true);

	rbo->physics_object = BKE_rigidbody_body_ensure_alloc(rbw, rbo->physics_object, rebuild);
	body = rbo->physics_object; /* shortcut */

	mat4_to_loc_quat(loc, rot, ob->obmat);
	RB_body_init(body, rbo->physics_shape, loc, rot);
	
	RB_body_set_friction(body, rbo->friction);
	RB_body_set_restitution(body, rbo->restitution);
	
	RB_body_set_damping(body, rbo->lin_damping, rbo->ang_damping);
	RB_body_set_sleep_thresh(body, rbo->lin_sleep_thresh, rbo->ang_sleep_thresh);
	RB_body_set_activation_state(body, rbo->flag & RBO_FLAG_USE_DEACTIVATION);
	
	if (rbo->type == RBO_TYPE_PASSIVE || rbo->flag & RBO_FLAG_START_DEACTIVATED)
		RB_body_deactivate(body);
	
	
	RB_body_set_linear_factor(body,
	                          (ob->protectflag & OB_LOCK_LOCX) == 0,
	                          (ob->protectflag & OB_LOCK_LOCY) == 0,
	                          (ob->protectflag & OB_LOCK_LOCZ) == 0);
	RB_body_set_angular_factor(body,
	                           (ob->protectflag & OB_LOCK_ROTX) == 0,
	                           (ob->protectflag & OB_LOCK_ROTY) == 0,
	                           (ob->protectflag & OB_LOCK_ROTZ) == 0);
	
	RB_body_set_mass(body, RBO_GET_MASS(rbo));
	RB_body_set_kinematic_state(body, rbo->flag & RBO_FLAG_KINEMATIC || rbo->flag & RBO_FLAG_DISABLED);

	if (rbw && rbw->physics_world)
		RB_dworld_add_body(rbw->physics_world, rbo->physics_object, rbo->col_groups);
}

/* --------------------- */

void BKE_rigidbody_objects_build(Scene *scene, struct RigidBodyWorld *rbw, bool rebuild)
{
	GroupObject *go;
	for (go = rbw->group->gobject.first; go; go = go->next) {
		Object *ob = go->ob;

		if (!ob || ob->type != OB_MESH)
			continue;
		
		/* validate that we've got valid object set up here... */
		RigidBodyOb *rbo = ob->rigidbody_object;
		/* update transformation matrix of the object so we don't get a frame of lag for simple animations */
		BKE_object_where_is_calc(scene, ob);
		
		if (rbo == NULL) {
			/* Since this object is included in the sim group but doesn't have
				 * rigid body settings (perhaps it was added manually), add!
				 *	- assume object to be active? That is the default for newly added settings...
				 */
			ob->rigidbody_object = BKE_rigidbody_create_object(scene, ob, RBO_TYPE_ACTIVE);
			rigidbody_validate_sim_object(rbw, ob, true);
			
			rbo = ob->rigidbody_object;
		}
		else {
			/* perform simulation data updates as tagged */
			/* refresh object... */
			if (rebuild) {
				/* World has been rebuilt so rebuild object */
				rigidbody_validate_sim_object(rbw, ob, true);
			}
			else if (rbo->flag & RBO_FLAG_NEEDS_VALIDATE) {
				rigidbody_validate_sim_object(rbw, ob, false);
			}
			
			/* refresh shape... */
			if (rbo->flag & RBO_FLAG_NEEDS_RESHAPE) {
				/* mesh/shape data changed, so force shape refresh */
				BKE_rigidbody_validate_sim_shape(ob, true);
				/* now tell RB sim about it */
				// XXX: we assume that this can only get applied for active/passive shapes that will be included as rigidbodies
				RB_body_set_collision_shape(rbo->physics_object, rbo->physics_shape);
			}
			
			rbo->flag &= ~(RBO_FLAG_NEEDS_VALIDATE | RBO_FLAG_NEEDS_RESHAPE);
		}
		
		BKE_rigidbody_body_tag_used(rbo->physics_object);
		
		/* update simulation object... */
		rigidbody_sync_object(scene, rbw, ob, rbo);
	}
}

/* ------------------------ */
/* apply */

static void rigidbody_world_apply_object(Scene *UNUSED(scene), Object *ob)
{
	RigidBodyOb *rbo;

	rbo = ob->rigidbody_object;
	/* reset kinematic state for transformed objects */
	if (rbo && (ob->flag & SELECT) && (G.moving & G_TRANSFORM_OBJ)) {
		RB_body_set_kinematic_state(rbo->physics_object, rbo->flag & RBO_FLAG_KINEMATIC || rbo->flag & RBO_FLAG_DISABLED);
		RB_body_set_mass(rbo->physics_object, RBO_GET_MASS(rbo));
		/* deactivate passive objects so they don't interfere with deactivation of active objects */
		if (rbo->type == RBO_TYPE_PASSIVE)
			RB_body_deactivate(rbo->physics_object);
	}
}

void BKE_rigidbody_objects_apply(Scene *scene, RigidBodyWorld *rbw)
{
	GroupObject *go;
	
	for (go = rbw->group->gobject.first; go; go = go->next) {
		Object *ob = go->ob;
		
		if (!ob)
			continue;
		
		rigidbody_world_apply_object(scene, ob);
	}
}

/* ------------------------ */
/* transform utilities */

/* Sync rigid body and object transformations */
void BKE_rigidbody_object_apply_transforms(RigidBodyWorld *rbw, Object *ob, float ctime)
{
	RigidBodyOb *rbo = ob->rigidbody_object;

	/* keep original transform for kinematic and passive objects */
	if (ELEM(NULL, rbw, rbo) || rbo->flag & RBO_FLAG_KINEMATIC || rbo->type == RBO_TYPE_PASSIVE)
		return;

	/* use rigid body transform after cache start frame if objects is not being transformed */
	if (BKE_rigidbody_check_sim_running(rbw, ctime) && !(ob->flag & SELECT && G.moving & G_TRANSFORM_OBJ)) {
		float mat[4][4], size_mat[4][4], size[3];

		normalize_qt(rbo->orn); // RB_TODO investigate why quaternion isn't normalized at this point
		quat_to_mat4(mat, rbo->orn);
		copy_v3_v3(mat[3], rbo->pos);

		mat4_to_size(size, ob->obmat);
		size_to_mat4(size_mat, size);
		mul_m4_m4m4(mat, mat, size_mat);

		copy_m4_m4(ob->obmat, mat);
	}
	/* otherwise set rigid body transform to current obmat */
	else {
		mat4_to_loc_quat(rbo->pos, rbo->orn, ob->obmat);
	}
}

/* Used when canceling transforms - return rigidbody and object to initial states */
void BKE_rigidbody_object_aftertrans_update(Object *ob, float loc[3], float rot[3], float quat[4], float rotAxis[3], float rotAngle)
{
	RigidBodyOb *rbo = ob->rigidbody_object;

	/* return rigid body and object to their initial states */
	copy_v3_v3(rbo->pos, ob->loc);
	copy_v3_v3(ob->loc, loc);

	if (ob->rotmode > 0) {
		eulO_to_quat(rbo->orn, ob->rot, ob->rotmode);
		copy_v3_v3(ob->rot, rot);
	}
	else if (ob->rotmode == ROT_MODE_AXISANGLE) {
		axis_angle_to_quat(rbo->orn, ob->rotAxis, ob->rotAngle);
		copy_v3_v3(ob->rotAxis, rotAxis);
		ob->rotAngle = rotAngle;
	}
	else {
		copy_qt_qt(rbo->orn, ob->quat);
		copy_qt_qt(ob->quat, quat);
	}
	if (rbo->physics_object) {
		/* allow passive objects to return to original transform */
		if (rbo->type == RBO_TYPE_PASSIVE)
			RB_body_set_kinematic_state(rbo->physics_object, TRUE);
		RB_body_set_loc_rot(rbo->physics_object, rbo->pos, rbo->orn);
	}
	// RB_TODO update rigid body physics object's loc/rot for dynamic objects here as well (needs to be done outside bullet's update loop)
}

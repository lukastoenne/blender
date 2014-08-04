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

#include "MEM_guardedalloc.h"

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
/* Main Simulation Sync */

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
		RB_shape_set_margin(rbo->physics_shape, BKE_rigidbody_object_margin(rbo) * MIN3(scale[0], scale[1], scale[2]));

	/* make transformed objects temporarily kinmatic so that they can be moved by the user during simulation */
	if (ob->flag & SELECT && G.moving & G_TRANSFORM_OBJ) {
		RB_body_set_kinematic_state(rbo->physics_object, true);
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

/**
 * Create physics sim representation of object given RigidBody settings
 *
 * \param rebuild Even if an instance already exists, replace it
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
	
	RB_body_set_mass(body, BKE_rigidbody_object_mass(rbo));
	RB_body_set_kinematic_state(body, rbo->flag & RBO_FLAG_KINEMATIC || rbo->flag & RBO_FLAG_DISABLED);

	if (rbw && rbw->physics_world)
		RB_dworld_add_body(rbw->physics_world, rbo->physics_object, rbo->col_groups);
}

void BKE_rigidbody_objects_build(Scene *scene, struct RigidBodyWorld *rbw, bool rebuild)
{
	GroupObject *go;
	if (!rbw->group)
		return;
	for (go = rbw->group->gobject.first; go; go = go->next) {
		Object *ob = go->ob;
		RigidBodyOb *rbo;

		if (!ob || ob->type != OB_MESH)
			continue;
		
		/* validate that we've got valid object set up here... */
		rbo = ob->rigidbody_object;
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


/**
 * Create physics sim representation of constraint given rigid body constraint settings
 *
 * \param rebuild Even if an instance already exists, replace it
 */
static void rigidbody_validate_sim_constraint(RigidBodyWorld *rbw, Object *ob, bool rebuild)
{
	RigidBodyCon *rbc = (ob) ? ob->rigidbody_constraint : NULL;
	float loc[3];
	float rot[4];
	float lin_lower;
	float lin_upper;
	float ang_lower;
	float ang_upper;

	/* sanity checks:
	 *	- object should have a rigid body constraint
	 *  - rigid body constraint should have at least one constrained object
	 */
	if (rbc == NULL) {
		return;
	}

	if (ELEM(NULL, rbc->ob1, rbc->ob1->rigidbody_object, rbc->ob2, rbc->ob2->rigidbody_object)) {
		if (rbc->physics_constraint) {
			RB_dworld_remove_constraint(rbw->physics_world, rbc->physics_constraint);
			RB_constraint_delete(rbc->physics_constraint);
			rbc->physics_constraint = NULL;
		}
		return;
	}

	if (rbc->physics_constraint && rebuild == false) {
		RB_dworld_remove_constraint(rbw->physics_world, rbc->physics_constraint);
	}
	if (rbc->physics_constraint == NULL || rebuild) {
		rbRigidBody *rb1 = rbc->ob1->rigidbody_object->physics_object;
		rbRigidBody *rb2 = rbc->ob2->rigidbody_object->physics_object;

		/* remove constraint if it already exists before creating a new one */
		if (rbc->physics_constraint) {
			RB_constraint_delete(rbc->physics_constraint);
			rbc->physics_constraint = NULL;
		}

		mat4_to_loc_quat(loc, rot, ob->obmat);

		if (rb1 && rb2) {
			switch (rbc->type) {
				case RBC_TYPE_POINT:
					rbc->physics_constraint = RB_constraint_new_point(loc, rb1, rb2);
					break;
				case RBC_TYPE_FIXED:
					rbc->physics_constraint = RB_constraint_new_fixed(loc, rot, rb1, rb2);
					break;
				case RBC_TYPE_HINGE:
					rbc->physics_constraint = RB_constraint_new_hinge(loc, rot, rb1, rb2);
					if (rbc->flag & RBC_FLAG_USE_LIMIT_ANG_Z) {
						RB_constraint_set_limits_hinge(rbc->physics_constraint, rbc->limit_ang_z_lower, rbc->limit_ang_z_upper);
					}
					else
						RB_constraint_set_limits_hinge(rbc->physics_constraint, 0.0f, -1.0f);
					break;
				case RBC_TYPE_SLIDER:
					rbc->physics_constraint = RB_constraint_new_slider(loc, rot, rb1, rb2);
					if (rbc->flag & RBC_FLAG_USE_LIMIT_LIN_X)
						RB_constraint_set_limits_slider(rbc->physics_constraint, rbc->limit_lin_x_lower, rbc->limit_lin_x_upper);
					else
						RB_constraint_set_limits_slider(rbc->physics_constraint, 0.0f, -1.0f);
					break;
				case RBC_TYPE_PISTON:
					rbc->physics_constraint = RB_constraint_new_piston(loc, rot, rb1, rb2);
					if (rbc->flag & RBC_FLAG_USE_LIMIT_LIN_X) {
						lin_lower = rbc->limit_lin_x_lower;
						lin_upper = rbc->limit_lin_x_upper;
					}
					else {
						lin_lower = 0.0f;
						lin_upper = -1.0f;
					}
					if (rbc->flag & RBC_FLAG_USE_LIMIT_ANG_X) {
						ang_lower = rbc->limit_ang_x_lower;
						ang_upper = rbc->limit_ang_x_upper;
					}
					else {
						ang_lower = 0.0f;
						ang_upper = -1.0f;
					}
					RB_constraint_set_limits_piston(rbc->physics_constraint, lin_lower, lin_upper, ang_lower, ang_upper);
					break;
				case RBC_TYPE_6DOF_SPRING:
					rbc->physics_constraint = RB_constraint_new_6dof_spring(loc, rot, rb1, rb2);

					RB_constraint_set_spring_6dof_spring(rbc->physics_constraint, RB_LIMIT_LIN_X, rbc->flag & RBC_FLAG_USE_SPRING_X);
					RB_constraint_set_stiffness_6dof_spring(rbc->physics_constraint, RB_LIMIT_LIN_X, rbc->spring_stiffness_x);
					RB_constraint_set_damping_6dof_spring(rbc->physics_constraint, RB_LIMIT_LIN_X, rbc->spring_damping_x);

					RB_constraint_set_spring_6dof_spring(rbc->physics_constraint, RB_LIMIT_LIN_Y, rbc->flag & RBC_FLAG_USE_SPRING_Y);
					RB_constraint_set_stiffness_6dof_spring(rbc->physics_constraint, RB_LIMIT_LIN_Y, rbc->spring_stiffness_y);
					RB_constraint_set_damping_6dof_spring(rbc->physics_constraint, RB_LIMIT_LIN_Y, rbc->spring_damping_y);

					RB_constraint_set_spring_6dof_spring(rbc->physics_constraint, RB_LIMIT_LIN_Z, rbc->flag & RBC_FLAG_USE_SPRING_Z);
					RB_constraint_set_stiffness_6dof_spring(rbc->physics_constraint, RB_LIMIT_LIN_Z, rbc->spring_stiffness_z);
					RB_constraint_set_damping_6dof_spring(rbc->physics_constraint, RB_LIMIT_LIN_Z, rbc->spring_damping_z);

					RB_constraint_set_equilibrium_6dof_spring(rbc->physics_constraint);
					/* fall-through */
				case RBC_TYPE_6DOF:
					if (rbc->type == RBC_TYPE_6DOF) /* a litte awkward but avoids duplicate code for limits */
						rbc->physics_constraint = RB_constraint_new_6dof(loc, rot, rb1, rb2);

					if (rbc->flag & RBC_FLAG_USE_LIMIT_LIN_X)
						RB_constraint_set_limits_6dof(rbc->physics_constraint, RB_LIMIT_LIN_X, rbc->limit_lin_x_lower, rbc->limit_lin_x_upper);
					else
						RB_constraint_set_limits_6dof(rbc->physics_constraint, RB_LIMIT_LIN_X, 0.0f, -1.0f);

					if (rbc->flag & RBC_FLAG_USE_LIMIT_LIN_Y)
						RB_constraint_set_limits_6dof(rbc->physics_constraint, RB_LIMIT_LIN_Y, rbc->limit_lin_y_lower, rbc->limit_lin_y_upper);
					else
						RB_constraint_set_limits_6dof(rbc->physics_constraint, RB_LIMIT_LIN_Y, 0.0f, -1.0f);

					if (rbc->flag & RBC_FLAG_USE_LIMIT_LIN_Z)
						RB_constraint_set_limits_6dof(rbc->physics_constraint, RB_LIMIT_LIN_Z, rbc->limit_lin_z_lower, rbc->limit_lin_z_upper);
					else
						RB_constraint_set_limits_6dof(rbc->physics_constraint, RB_LIMIT_LIN_Z, 0.0f, -1.0f);

					if (rbc->flag & RBC_FLAG_USE_LIMIT_ANG_X)
						RB_constraint_set_limits_6dof(rbc->physics_constraint, RB_LIMIT_ANG_X, rbc->limit_ang_x_lower, rbc->limit_ang_x_upper);
					else
						RB_constraint_set_limits_6dof(rbc->physics_constraint, RB_LIMIT_ANG_X, 0.0f, -1.0f);

					if (rbc->flag & RBC_FLAG_USE_LIMIT_ANG_Y)
						RB_constraint_set_limits_6dof(rbc->physics_constraint, RB_LIMIT_ANG_Y, rbc->limit_ang_y_lower, rbc->limit_ang_y_upper);
					else
						RB_constraint_set_limits_6dof(rbc->physics_constraint, RB_LIMIT_ANG_Y, 0.0f, -1.0f);

					if (rbc->flag & RBC_FLAG_USE_LIMIT_ANG_Z)
						RB_constraint_set_limits_6dof(rbc->physics_constraint, RB_LIMIT_ANG_Z, rbc->limit_ang_z_lower, rbc->limit_ang_z_upper);
					else
						RB_constraint_set_limits_6dof(rbc->physics_constraint, RB_LIMIT_ANG_Z, 0.0f, -1.0f);
					break;
				case RBC_TYPE_MOTOR:
					rbc->physics_constraint = RB_constraint_new_motor(loc, rot, rb1, rb2);

					RB_constraint_set_enable_motor(rbc->physics_constraint, rbc->flag & RBC_FLAG_USE_MOTOR_LIN, rbc->flag & RBC_FLAG_USE_MOTOR_ANG);
					RB_constraint_set_max_impulse_motor(rbc->physics_constraint, rbc->motor_lin_max_impulse, rbc->motor_ang_max_impulse);
					RB_constraint_set_target_velocity_motor(rbc->physics_constraint, rbc->motor_lin_target_velocity, rbc->motor_ang_target_velocity);
					break;
			}
		}
		else { /* can't create constraint without both rigid bodies */
			return;
		}

		RB_constraint_set_enabled(rbc->physics_constraint, rbc->flag & RBC_FLAG_ENABLED);

		if (rbc->flag & RBC_FLAG_USE_BREAKING)
			RB_constraint_set_breaking_threshold(rbc->physics_constraint, rbc->breaking_threshold);
		else
			RB_constraint_set_breaking_threshold(rbc->physics_constraint, FLT_MAX);

		if (rbc->flag & RBC_FLAG_OVERRIDE_SOLVER_ITERATIONS)
			RB_constraint_set_solver_iterations(rbc->physics_constraint, rbc->num_solver_iterations);
		else
			RB_constraint_set_solver_iterations(rbc->physics_constraint, -1);
	}

	if (rbw && rbw->physics_world && rbc->physics_constraint) {
		RB_dworld_add_constraint(rbw->physics_world, rbc->physics_constraint, rbc->flag & RBC_FLAG_DISABLE_COLLISIONS);
	}
}

void BKE_rigidbody_constraints_build(Scene *scene, RigidBodyWorld *rbw, bool rebuild)
{
	GroupObject *go;
	
	if (!rbw->constraints)
		return;
	
	for (go = rbw->constraints->gobject.first; go; go = go->next) {
		Object *ob = go->ob;
		
		if (ob) {
			/* validate that we've got valid object set up here... */
			RigidBodyCon *rbc = ob->rigidbody_constraint;
			/* update transformation matrix of the object so we don't get a frame of lag for simple animations */
			BKE_object_where_is_calc(scene, ob);
			
			if (rbc == NULL) {
				/* Since this object is included in the group but doesn't have
				 * constraint settings (perhaps it was added manually), add!
				 */
				ob->rigidbody_constraint = BKE_rigidbody_create_constraint(scene, ob, RBC_TYPE_FIXED);
				rigidbody_validate_sim_constraint(rbw, ob, true);
				
				rbc = ob->rigidbody_constraint;
			}
			else {
				/* perform simulation data updates as tagged */
				if (rebuild) {
					/* World has been rebuilt so rebuild constraint */
					rigidbody_validate_sim_constraint(rbw, ob, true);
				}
				else if (rbc->flag & RBC_FLAG_NEEDS_VALIDATE) {
					rigidbody_validate_sim_constraint(rbw, ob, false);
				}
				rbc->flag &= ~RBC_FLAG_NEEDS_VALIDATE;
			}
		}
	}
}

void BKE_rigidbody_constraints_apply(Scene *UNUSED(scene), RigidBodyWorld *UNUSED(rbw))
{
	/* stub, just for consistency, might be useful later */
}

/* ------------------------ */

static void rigidbody_world_apply_object(Scene *UNUSED(scene), Object *ob)
{
	RigidBodyOb *rbo;

	rbo = ob->rigidbody_object;
	/* reset kinematic state for transformed objects */
	if (rbo && (ob->flag & SELECT) && (G.moving & G_TRANSFORM_OBJ)) {
		RB_body_set_kinematic_state(rbo->physics_object, rbo->flag & RBO_FLAG_KINEMATIC || rbo->flag & RBO_FLAG_DISABLED);
		RB_body_set_mass(rbo->physics_object, BKE_rigidbody_object_mass(rbo));
		/* deactivate passive objects so they don't interfere with deactivation of active objects */
		if (rbo->type == RBO_TYPE_PASSIVE)
			RB_body_deactivate(rbo->physics_object);
	}
}

void BKE_rigidbody_objects_apply(Scene *scene, RigidBodyWorld *rbw)
{
	GroupObject *go;
	if (!rbw->group)
		return;
	for (go = rbw->group->gobject.first; go; go = go->next) {
		Object *ob = go->ob;
		
		if (!ob)
			continue;
		
		rigidbody_world_apply_object(scene, ob);
	}
}

/* ------------------------ */
/* Transform Utils */

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
			RB_body_set_kinematic_state(rbo->physics_object, true);
		RB_body_set_loc_rot(rbo->physics_object, rbo->pos, rbo->orn);
	}
	// RB_TODO update rigid body physics object's loc/rot for dynamic objects here as well (needs to be done outside bullet's update loop)
}

/* ------------------------ */
/* Object Data Management */

/* Add rigid body settings to the specified object */
RigidBodyOb *BKE_rigidbody_create_object(Scene *scene, Object *ob, short type)
{
	RigidBodyOb *rbo;
	RigidBodyWorld *rbw = scene->rigidbody_world;

	/* sanity checks
	 *	- rigidbody world must exist
	 *	- object must exist
	 *	- cannot add rigid body if it already exists
	 */
	if (ob == NULL || (ob->rigidbody_object != NULL))
		return NULL;

	/* create new settings data, and link it up */
	rbo = MEM_callocN(sizeof(RigidBodyOb), "RigidBodyOb");

	/* set default settings */
	rbo->type = type;

	rbo->mass = 1.0f;

	rbo->friction = 0.5f; /* best when non-zero. 0.5 is Bullet default */
	rbo->restitution = 0.0f; /* best when zero. 0.0 is Bullet default */

	rbo->margin = 0.04f; /* 0.04 (in meters) is Bullet default */

	rbo->lin_sleep_thresh = 0.4f; /* 0.4 is half of Bullet default */
	rbo->ang_sleep_thresh = 0.5f; /* 0.5 is half of Bullet default */

	rbo->lin_damping = 0.04f; /* 0.04 is game engine default */
	rbo->ang_damping = 0.1f; /* 0.1 is game engine default */

	rbo->col_groups = 1;

	/* use triangle meshes for passive objects
	 * use convex hulls for active objects since dynamic triangle meshes are very unstable
	 */
	if (type == RBO_TYPE_ACTIVE)
		rbo->shape = RB_SHAPE_CONVEXH;
	else
		rbo->shape = RB_SHAPE_TRIMESH;

	rbo->mesh_source = RBO_MESH_DEFORM;

	/* set initial transform */
	mat4_to_loc_quat(rbo->pos, rbo->orn, ob->obmat);

	/* flag cache as outdated */
	BKE_rigidbody_cache_reset(rbw);

	/* return this object */
	return rbo;
}

void BKE_rigidbody_remove_object(Scene *scene, Object *ob)
{
	RigidBodyWorld *rbw = scene->rigidbody_world;
	RigidBodyOb *rbo = ob->rigidbody_object;
	RigidBodyCon *rbc;
	GroupObject *go;
	int i;

	if (rbw) {
		/* remove from rigidbody world, free object won't do this */
		if (rbw->physics_world && rbo->physics_object)
			RB_dworld_remove_body(rbw->physics_world, rbo->physics_object);

		/* remove object from array */
		if (rbw && rbw->objects) {
			for (i = 0; i < rbw->numbodies; i++) {
				if (rbw->objects[i] == ob) {
					rbw->objects[i] = NULL;
					break;
				}
			}
		}

		/* remove object from rigid body constraints */
		if (rbw->constraints) {
			for (go = rbw->constraints->gobject.first; go; go = go->next) {
				Object *obt = go->ob;
				if (obt && obt->rigidbody_constraint) {
					rbc = obt->rigidbody_constraint;
					if (ELEM(ob, rbc->ob1, rbc->ob2)) {
						BKE_rigidbody_remove_constraint(scene, obt);
					}
				}
			}
		}
	}

	/* remove object's settings */
	BKE_rigidbody_free_object(ob);

	/* flag cache as outdated */
	BKE_rigidbody_cache_reset(rbw);
}

/* Free RigidBody settings and sim instances */
void BKE_rigidbody_free_object(Object *ob)
{
	RigidBodyOb *rbo = (ob) ? ob->rigidbody_object : NULL;

	/* sanity check */
	if (rbo == NULL)
		return;

	/* free physics references */
	if (rbo->physics_object) {
		rbo->physics_object = NULL;
	}

	if (rbo->physics_shape) {
		RB_shape_delete(rbo->physics_shape);
		rbo->physics_shape = NULL;
	}

	/* free data itself */
	MEM_freeN(rbo);
	ob->rigidbody_object = NULL;
}

/* This just copies the data, clearing out references to physics objects.
 * Anything that uses them MUST verify that the copied object will
 * be added to relevant groups later...
 */
RigidBodyOb *BKE_rigidbody_copy_object(Object *ob)
{
	RigidBodyOb *rboN = NULL;

	if (ob->rigidbody_object) {
		/* just duplicate the whole struct first (to catch all the settings) */
		rboN = MEM_dupallocN(ob->rigidbody_object);

		/* tag object as needing to be verified */
		rboN->flag |= RBO_FLAG_NEEDS_VALIDATE;

		/* clear out all the fields which need to be revalidated later */
		rboN->physics_object = NULL;
		rboN->physics_shape = NULL;
	}

	/* return new copy of settings */
	return rboN;
}

/* ------------------------ */

/* Add rigid body constraint to the specified object */
RigidBodyCon *BKE_rigidbody_create_constraint(Scene *scene, Object *ob, short type)
{
	RigidBodyCon *rbc;
	RigidBodyWorld *rbw = scene->rigidbody_world;

	/* sanity checks
	 *	- rigidbody world must exist
	 *	- object must exist
	 *	- cannot add constraint if it already exists
	 */
	if (ob == NULL || (ob->rigidbody_constraint != NULL))
		return NULL;

	/* create new settings data, and link it up */
	rbc = MEM_callocN(sizeof(RigidBodyCon), "RigidBodyCon");

	/* set default settings */
	rbc->type = type;

	rbc->ob1 = NULL;
	rbc->ob2 = NULL;

	rbc->flag |= RBC_FLAG_ENABLED;
	rbc->flag |= RBC_FLAG_DISABLE_COLLISIONS;

	rbc->breaking_threshold = 10.0f; /* no good default here, just use 10 for now */
	rbc->num_solver_iterations = 10; /* 10 is Bullet default */

	rbc->limit_lin_x_lower = -1.0f;
	rbc->limit_lin_x_upper = 1.0f;
	rbc->limit_lin_y_lower = -1.0f;
	rbc->limit_lin_y_upper = 1.0f;
	rbc->limit_lin_z_lower = -1.0f;
	rbc->limit_lin_z_upper = 1.0f;
	rbc->limit_ang_x_lower = -M_PI_4;
	rbc->limit_ang_x_upper = M_PI_4;
	rbc->limit_ang_y_lower = -M_PI_4;
	rbc->limit_ang_y_upper = M_PI_4;
	rbc->limit_ang_z_lower = -M_PI_4;
	rbc->limit_ang_z_upper = M_PI_4;

	rbc->spring_damping_x = 0.5f;
	rbc->spring_damping_y = 0.5f;
	rbc->spring_damping_z = 0.5f;
	rbc->spring_stiffness_x = 10.0f;
	rbc->spring_stiffness_y = 10.0f;
	rbc->spring_stiffness_z = 10.0f;

	rbc->motor_lin_max_impulse = 1.0f;
	rbc->motor_lin_target_velocity = 1.0f;
	rbc->motor_ang_max_impulse = 1.0f;
	rbc->motor_ang_target_velocity = 1.0f;

	/* flag cache as outdated */
	BKE_rigidbody_cache_reset(rbw);

	/* return this object */
	return rbc;
}

void BKE_rigidbody_remove_constraint(Scene *scene, Object *ob)
{
	RigidBodyWorld *rbw = scene->rigidbody_world;
	RigidBodyCon *rbc = ob->rigidbody_constraint;

	/* remove from rigidbody world, free object won't do this */
	if (rbw && rbw->physics_world && rbc->physics_constraint) {
		RB_dworld_remove_constraint(rbw->physics_world, rbc->physics_constraint);
	}
	/* remove object's settings */
	BKE_rigidbody_free_constraint(ob);

	/* flag cache as outdated */
	BKE_rigidbody_cache_reset(rbw);
}

/* Free RigidBody constraint and sim instance */
void BKE_rigidbody_free_constraint(Object *ob)
{
	RigidBodyCon *rbc = (ob) ? ob->rigidbody_constraint : NULL;

	/* sanity check */
	if (rbc == NULL)
		return;

	/* free physics reference */
	if (rbc->physics_constraint) {
		RB_constraint_delete(rbc->physics_constraint);
		rbc->physics_constraint = NULL;
	}

	/* free data itself */
	MEM_freeN(rbc);
	ob->rigidbody_constraint = NULL;
}

/* This just copies the data, clearing out references to physics objects.
 * Anything that uses them MUST verify that the copied object will
 * be added to relevant groups later...
 */
RigidBodyCon *BKE_rigidbody_copy_constraint(Object *ob)
{
	RigidBodyCon *rbcN = NULL;

	if (ob->rigidbody_constraint) {
		/* just duplicate the whole struct first (to catch all the settings) */
		rbcN = MEM_dupallocN(ob->rigidbody_constraint);

		/* tag object as needing to be verified */
		rbcN->flag |= RBC_FLAG_NEEDS_VALIDATE;

		/* clear out all the fields which need to be revalidated later */
		rbcN->physics_constraint = NULL;
	}

	/* return new copy of settings */
	return rbcN;
}

/* preserve relationships between constraints and rigid bodies after duplication */
void BKE_rigidbody_relink_constraint(RigidBodyCon *rbc)
{
	ID_NEW(rbc->ob1);
	ID_NEW(rbc->ob2);
}

/* ------------------------ */
/* Utilities */

float BKE_rigidbody_object_mass(RigidBodyOb *rbo)
{
	if (!rbo)
		return 0.0f;
	if (rbo->type == RBO_TYPE_PASSIVE || rbo->flag & RBO_FLAG_KINEMATIC || rbo->flag & RBO_FLAG_DISABLED)
		return 0.0f;
	return rbo->mass;
}

float BKE_rigidbody_object_margin(RigidBodyOb *rbo)
{
	if (!rbo)
		return 0.0f;
	if (rbo->flag & RBO_FLAG_USE_MARGIN || ELEM(rbo->shape, RB_SHAPE_CONVEXH, RB_SHAPE_TRIMESH, RB_SHAPE_CONE))
		return rbo->margin;
	else
		return 0.04f;
}

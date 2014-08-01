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
 * Contributor(s): Joshua Leung, Sergej Reich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file rigidbody.c
 *  \ingroup blenkernel
 *  \brief Blender-side interface and methods for dealing with Rigid Body simulations
 */

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <float.h>
#include <math.h>
#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_mempool.h"

#ifdef WITH_BULLET
#  include "RBI_api.h"
#endif

#include "DNA_group_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_pointcache.h"
#include "BKE_rigidbody.h"

#include "RNA_access.h"

#ifdef WITH_BULLET

enum eRigidBodyFlag {
	RB_BODY_USED		= 1
};

/* ************************************** */
/* Memory Management */

/* Freeing Methods --------------------- */

static void rigidbody_world_clear_used_tags(RigidBodyWorld *rbw)
{
	BLI_mempool_iter iter;
	rbRigidBody *body;
	
	BLI_mempool_iternew(rbw->body_pool, &iter);
	for (body = BLI_mempool_iterstep(&iter); body; body = BLI_mempool_iterstep(&iter)) {
		RB_body_clear_flag(body, RB_BODY_USED);
	}
}

static void rigidbody_world_free_bodies(RigidBodyWorld *rbw, bool keep_used)
{
	BLI_mempool_iter iter;
	rbRigidBody *body, *body_next;
	
	/* XXX there is no formal guarantee that the next iterator is still valid
	 * after removing the current object, but with the current mempool implementation
	 * this should work fine.
	 */
	BLI_mempool_iternew(rbw->body_pool, &iter);
	for (body = BLI_mempool_iterstep(&iter); body; body = body_next) {
		body_next = BLI_mempool_iterstep(&iter);
		
		if (keep_used && (RB_body_get_flags(body) & RB_BODY_USED))
			continue;
		
		/* free physics references */
		if (rbw->physics_world)
			RB_dworld_remove_body(rbw->physics_world, body);
		RB_body_free(body);
		BLI_mempool_free(rbw->body_pool, body);
	}
}

/* Free rigidbody world */
void BKE_rigidbody_free_world(RigidBodyWorld *rbw)
{
	/* sanity check */
	if (!rbw)
		return;

	if (rbw->physics_world) {
		/* free physics references, we assume that all physics objects in will have been added to the world */
		GroupObject *go;
		if (rbw->constraints) {
			for (go = rbw->constraints->gobject.first; go; go = go->next) {
				if (go->ob && go->ob->rigidbody_constraint) {
					RigidBodyCon *rbc = go->ob->rigidbody_constraint;

					if (rbc->physics_constraint)
						RB_dworld_remove_constraint(rbw->physics_world, rbc->physics_constraint);
				}
			}
		}
		
		rigidbody_world_free_bodies(rbw, false);
		
		/* free dynamics world */
		RB_dworld_delete(rbw->physics_world);
	}
	if (rbw->objects)
		free(rbw->objects);

	/* free cache */
	BKE_ptcache_free_list(&(rbw->ptcaches));
	rbw->pointcache = NULL;

	/* free effector weights */
	if (rbw->effector_weights)
		MEM_freeN(rbw->effector_weights);

	BLI_mempool_destroy(rbw->body_pool);

	/* free rigidbody world itself */
	MEM_freeN(rbw);
}

/* ************************************** */
/* Setup Utilities - Validate Sim Instances */

rbRigidBody *BKE_rigidbody_body_ensure_alloc(RigidBodyWorld *rbw, rbRigidBody *body, bool rebuild)
{
	if (!body || rebuild) {
		if (body) {
			/* free the existing rigid body, memory reused below */
			RB_body_free(body);
		}
		else {
			/* only allocate if no rigid body exists yet,
			 * otherwise previous memory is reused
			 */
			body = BLI_mempool_alloc(rbw->body_pool);
		}
	}
	else
		RB_dworld_remove_body(rbw->physics_world, body);
	return body;
}

void BKE_rigidbody_body_tag_used(rbRigidBody *body)
{
	if (body)
		RB_body_set_flag(body, RB_BODY_USED);
}

/* get the appropriate DerivedMesh based on rigid body mesh source */
static DerivedMesh *rigidbody_get_mesh(Object *ob)
{
	if (ob->rigidbody_object->mesh_source == RBO_MESH_DEFORM) {
		return ob->derivedDeform;
	}
	else if (ob->rigidbody_object->mesh_source == RBO_MESH_FINAL) {
		return ob->derivedFinal;
	}
	else {
		return CDDM_from_mesh(ob->data);
	}
}

/* create collision shape of mesh - convex hull */
static rbCollisionShape *rigidbody_get_shape_convexhull_from_mesh(Object *ob, float margin, bool *can_embed)
{
	rbCollisionShape *shape = NULL;
	DerivedMesh *dm = NULL;
	MVert *mvert = NULL;
	int totvert = 0;

	if (ob->type == OB_MESH && ob->data) {
		dm = rigidbody_get_mesh(ob);
		mvert   = (dm) ? dm->getVertArray(dm) : NULL;
		totvert = (dm) ? dm->getNumVerts(dm) : 0;
	}
	else {
		printf("ERROR: cannot make Convex Hull collision shape for non-Mesh object\n");
	}

	if (totvert) {
		shape = RB_shape_new_convex_hull((float *)mvert, sizeof(MVert), totvert, margin, can_embed);
	}
	else {
		printf("ERROR: no vertices to define Convex Hull collision shape with\n");
	}

	if (dm && ob->rigidbody_object->mesh_source == RBO_MESH_BASE)
		dm->release(dm);

	return shape;
}

/* create collision shape of mesh - triangulated mesh
 * returns NULL if creation fails.
 */
static rbCollisionShape *rigidbody_get_shape_trimesh_from_mesh(Object *ob)
{
	rbCollisionShape *shape = NULL;

	if (ob->type == OB_MESH) {
		DerivedMesh *dm = NULL;
		MVert *mvert;
		MFace *mface;
		int totvert;
		int totface;
		int tottris = 0;
		int triangle_index = 0;

		dm = rigidbody_get_mesh(ob);

		/* ensure mesh validity, then grab data */
		if (dm == NULL)
			return NULL;

		DM_ensure_tessface(dm);

		mvert   = dm->getVertArray(dm);
		totvert = dm->getNumVerts(dm);
		mface   = dm->getTessFaceArray(dm);
		totface = dm->getNumTessFaces(dm);

		/* sanity checking - potential case when no data will be present */
		if ((totvert == 0) || (totface == 0)) {
			printf("WARNING: no geometry data converted for Mesh Collision Shape (ob = %s)\n", ob->id.name + 2);
		}
		else {
			rbMeshData *mdata;
			int i;
			
			/* count triangles */
			for (i = 0; i < totface; i++) {
				(mface[i].v4) ? (tottris += 2) : (tottris += 1);
			}

			/* init mesh data for collision shape */
			mdata = RB_trimesh_data_new(tottris, totvert);
			
			RB_trimesh_add_vertices(mdata, (float *)mvert, totvert, sizeof(MVert));

			/* loop over all faces, adding them as triangles to the collision shape
			 * (so for some faces, more than triangle will get added)
			 */
			for (i = 0; (i < totface) && (mface) && (mvert); i++, mface++) {
				/* add first triangle - verts 1,2,3 */
				RB_trimesh_add_triangle_indices(mdata, triangle_index, mface->v1, mface->v2, mface->v3);
				triangle_index++;

				/* add second triangle if needed - verts 1,3,4 */
				if (mface->v4) {
					RB_trimesh_add_triangle_indices(mdata, triangle_index, mface->v1, mface->v3, mface->v4);
					triangle_index++;
				}
			}
			RB_trimesh_finish(mdata);

			/* construct collision shape
			 *
			 * These have been chosen to get better speed/accuracy tradeoffs with regards
			 * to limitations of each:
			 *    - BVH-Triangle Mesh: for passive objects only. Despite having greater
			 *                         speed/accuracy, they cannot be used for moving objects.
			 *    - GImpact Mesh:      for active objects. These are slower and less stable,
			 *                         but are more flexible for general usage.
			 */
			if (ob->rigidbody_object->type == RBO_TYPE_PASSIVE) {
				shape = RB_shape_new_trimesh(mdata);
			}
			else {
				shape = RB_shape_new_gimpact_mesh(mdata);
			}
		}

		/* cleanup temp data */
		if (ob->rigidbody_object->mesh_source == RBO_MESH_BASE) {
			dm->release(dm);
		}
	}
	else {
		printf("ERROR: cannot make Triangular Mesh collision shape for non-Mesh object\n");
	}

	return shape;
}

/* Create new physics sim collision shape for object and store it,
 * or remove the existing one first and replace...
 */
void BKE_rigidbody_validate_sim_shape(Object *ob, bool rebuild)
{
	RigidBodyOb *rbo = ob->rigidbody_object;
	rbCollisionShape *new_shape = NULL;
	BoundBox *bb = NULL;
	float size[3] = {1.0f, 1.0f, 1.0f};
	float radius = 1.0f;
	float height = 1.0f;
	float capsule_height;
	float hull_margin = 0.0f;
	bool can_embed = true;
	bool has_volume;

	/* sanity check */
	if (rbo == NULL)
		return;

	/* don't create a new shape if we already have one and don't want to rebuild it */
	if (rbo->physics_shape && !rebuild)
		return;

	/* if automatically determining dimensions, use the Object's boundbox
	 *	- assume that all quadrics are standing upright on local z-axis
	 *	- assume even distribution of mass around the Object's pivot
	 *	  (i.e. Object pivot is centralized in boundbox)
	 */
	// XXX: all dimensions are auto-determined now... later can add stored settings for this
	/* get object dimensions without scaling */
	bb = BKE_object_boundbox_get(ob);
	if (bb) {
		size[0] = (bb->vec[4][0] - bb->vec[0][0]);
		size[1] = (bb->vec[2][1] - bb->vec[0][1]);
		size[2] = (bb->vec[1][2] - bb->vec[0][2]);
	}
	mul_v3_fl(size, 0.5f);

	if (ELEM(rbo->shape, RB_SHAPE_CAPSULE, RB_SHAPE_CYLINDER, RB_SHAPE_CONE)) {
		/* take radius as largest x/y dimension, and height as z-dimension */
		radius = MAX2(size[0], size[1]);
		height = size[2];
	}
	else if (rbo->shape == RB_SHAPE_SPHERE) {
		/* take radius to the the largest dimension to try and encompass everything */
		radius = MAX3(size[0], size[1], size[2]);
	}

	/* create new shape */
	switch (rbo->shape) {
		case RB_SHAPE_BOX:
			new_shape = RB_shape_new_box(size[0], size[1], size[2]);
			break;

		case RB_SHAPE_SPHERE:
			new_shape = RB_shape_new_sphere(radius);
			break;

		case RB_SHAPE_CAPSULE:
			capsule_height = (height - radius) * 2.0f;
			new_shape = RB_shape_new_capsule(radius, (capsule_height > 0.0f) ? capsule_height : 0.0f);
			break;
		case RB_SHAPE_CYLINDER:
			new_shape = RB_shape_new_cylinder(radius, height);
			break;
		case RB_SHAPE_CONE:
			new_shape = RB_shape_new_cone(radius, height * 2.0f);
			break;

		case RB_SHAPE_CONVEXH:
			/* try to emged collision margin */
			has_volume = (MIN3(size[0], size[1], size[2]) > 0.0f);

			if (!(rbo->flag & RBO_FLAG_USE_MARGIN) && has_volume)
				hull_margin = 0.04f;
			new_shape = rigidbody_get_shape_convexhull_from_mesh(ob, hull_margin, &can_embed);
			if (!(rbo->flag & RBO_FLAG_USE_MARGIN))
				rbo->margin = (can_embed && has_volume) ? 0.04f : 0.0f;  /* RB_TODO ideally we shouldn't directly change the margin here */
			break;
		case RB_SHAPE_TRIMESH:
			new_shape = rigidbody_get_shape_trimesh_from_mesh(ob);
			break;
	}
	/* assign new collision shape if creation was successful */
	if (new_shape) {
		if (rbo->physics_shape)
			RB_shape_delete(rbo->physics_shape);
		rbo->physics_shape = new_shape;
		RB_shape_set_margin(rbo->physics_shape, BKE_rigidbody_object_margin(rbo));
	}
	/* use box shape if we can't fall back to old shape */
	else if (rbo->physics_shape == NULL) {
		rbo->shape = RB_SHAPE_BOX;
		BKE_rigidbody_validate_sim_shape(ob, true);
	}
}

/* --------------------- */

/* helper function to calculate volume of rigidbody object */
// TODO: allow a parameter to specify method used to calculate this?
void BKE_rigidbody_calc_volume(Object *ob, float *r_vol)
{
	RigidBodyOb *rbo = ob->rigidbody_object;

	float size[3]  = {1.0f, 1.0f, 1.0f};
	float radius = 1.0f;
	float height = 1.0f;

	float volume = 0.0f;

	/* if automatically determining dimensions, use the Object's boundbox
	 *	- assume that all quadrics are standing upright on local z-axis
	 *	- assume even distribution of mass around the Object's pivot
	 *	  (i.e. Object pivot is centralized in boundbox)
	 *	- boundbox gives full width
	 */
	// XXX: all dimensions are auto-determined now... later can add stored settings for this
	BKE_object_dimensions_get(ob, size);

	if (ELEM(rbo->shape, RB_SHAPE_CAPSULE, RB_SHAPE_CYLINDER, RB_SHAPE_CONE)) {
		/* take radius as largest x/y dimension, and height as z-dimension */
		radius = MAX2(size[0], size[1]) * 0.5f;
		height = size[2];
	}
	else if (rbo->shape == RB_SHAPE_SPHERE) {
		/* take radius to the the largest dimension to try and encompass everything */
		radius = max_fff(size[0], size[1], size[2]) * 0.5f;
	}

	/* calculate volume as appropriate  */
	switch (rbo->shape) {
		case RB_SHAPE_BOX:
			volume = size[0] * size[1] * size[2];
			break;

		case RB_SHAPE_SPHERE:
			volume = 4.0f / 3.0f * (float)M_PI * radius * radius * radius;
			break;

		/* for now, assume that capsule is close enough to a cylinder... */
		case RB_SHAPE_CAPSULE:
		case RB_SHAPE_CYLINDER:
			volume = (float)M_PI * radius * radius * height;
			break;

		case RB_SHAPE_CONE:
			volume = (float)M_PI / 3.0f * radius * radius * height;
			break;

		case RB_SHAPE_CONVEXH:
		case RB_SHAPE_TRIMESH:
		{
			if (ob->type == OB_MESH) {
				DerivedMesh *dm = rigidbody_get_mesh(ob);
				MVert *mvert;
				MFace *mface;
				int totvert, totface;
				
				/* ensure mesh validity, then grab data */
				if (dm == NULL)
					return;
			
				DM_ensure_tessface(dm);
			
				mvert   = dm->getVertArray(dm);
				totvert = dm->getNumVerts(dm);
				mface   = dm->getTessFaceArray(dm);
				totface = dm->getNumTessFaces(dm);
				
				if (totvert > 0 && totface > 0) {
					BKE_mesh_calc_volume(mvert, totvert, mface, totface, &volume, NULL);
				}
				
				/* cleanup temp data */
				if (ob->rigidbody_object->mesh_source == RBO_MESH_BASE) {
					dm->release(dm);
				}
			}
			else {
				/* rough estimate from boundbox as fallback */
				/* XXX could implement other types of geometry here (curves, etc.) */
				volume = size[0] * size[1] * size[2];
			}
			break;
		}

#if 0 // XXX: not defined yet
		case RB_SHAPE_COMPOUND:
			volume = 0.0f;
			break;
#endif
	}

	/* return the volume calculated */
	if (r_vol) *r_vol = volume;
}

void BKE_rigidbody_calc_center_of_mass(Object *ob, float r_com[3])
{
	RigidBodyOb *rbo = ob->rigidbody_object;

	float size[3]  = {1.0f, 1.0f, 1.0f};
	float height = 1.0f;

	zero_v3(r_com);

	/* if automatically determining dimensions, use the Object's boundbox
	 *	- assume that all quadrics are standing upright on local z-axis
	 *	- assume even distribution of mass around the Object's pivot
	 *	  (i.e. Object pivot is centralized in boundbox)
	 *	- boundbox gives full width
	 */
	// XXX: all dimensions are auto-determined now... later can add stored settings for this
	BKE_object_dimensions_get(ob, size);

	/* calculate volume as appropriate  */
	switch (rbo->shape) {
		case RB_SHAPE_BOX:
		case RB_SHAPE_SPHERE:
		case RB_SHAPE_CAPSULE:
		case RB_SHAPE_CYLINDER:
			break;

		case RB_SHAPE_CONE:
			/* take radius as largest x/y dimension, and height as z-dimension */
			height = size[2];
			/* cone is geometrically centered on the median,
			 * center of mass is 1/4 up from the base
			 */
			r_com[2] = -0.25f * height;
			break;

		case RB_SHAPE_CONVEXH:
		case RB_SHAPE_TRIMESH:
		{
			if (ob->type == OB_MESH) {
				DerivedMesh *dm = rigidbody_get_mesh(ob);
				MVert *mvert;
				MFace *mface;
				int totvert, totface;
				
				/* ensure mesh validity, then grab data */
				if (dm == NULL)
					return;
			
				DM_ensure_tessface(dm);
			
				mvert   = dm->getVertArray(dm);
				totvert = dm->getNumVerts(dm);
				mface   = dm->getTessFaceArray(dm);
				totface = dm->getNumTessFaces(dm);
				
				if (totvert > 0 && totface > 0) {
					BKE_mesh_calc_volume(mvert, totvert, mface, totface, NULL, r_com);
				}
				
				/* cleanup temp data */
				if (ob->rigidbody_object->mesh_source == RBO_MESH_BASE) {
					dm->release(dm);
				}
			}
			break;
		}

#if 0 // XXX: not defined yet
		case RB_SHAPE_COMPOUND:
			volume = 0.0f;
			break;
#endif
	}
}

/* --------------------- */

/* Create physics sim world given RigidBody world settings */
// NOTE: this does NOT update object references that the scene uses, in case those aren't ready yet!
void BKE_rigidbody_validate_sim_world(Scene *scene, RigidBodyWorld *rbw, bool rebuild)
{
	/* sanity checks */
	if (rbw == NULL)
		return;

	/* create new sim world */
	if (rebuild || rbw->physics_world == NULL) {
		if (rbw->physics_world)
			RB_dworld_delete(rbw->physics_world);
		rbw->physics_world = RB_dworld_new(scene->physics_settings.gravity);
	}

	RB_dworld_set_solver_iterations(rbw->physics_world, rbw->num_solver_iterations);
	RB_dworld_set_split_impulse(rbw->physics_world, rbw->flag & RBW_FLAG_USE_SPLIT_IMPULSE);
}

/* ************************************** */
/* Setup Utilities - Create Settings Blocks */

/* Set up RigidBody world */
RigidBodyWorld *BKE_rigidbody_create_world(Scene *scene)
{
	/* try to get whatever RigidBody world that might be representing this already */
	RigidBodyWorld *rbw;

	/* sanity checks
	 *	- there must be a valid scene to add world to
	 *	- there mustn't be a sim world using this group already
	 */
	if (scene == NULL)
		return NULL;

	/* create a new sim world */
	rbw = MEM_callocN(sizeof(RigidBodyWorld), "RigidBodyWorld");
	BKE_rigidbody_world_init_mempool(rbw);

	/* set default settings */
	rbw->effector_weights = BKE_add_effector_weights(NULL);

	rbw->ltime = PSFRA;

	rbw->time_scale = 1.0f;

	rbw->steps_per_second = 60; /* Bullet default (60 Hz) */
	rbw->num_solver_iterations = 10; /* 10 is bullet default */

	rbw->pointcache = BKE_ptcache_add(&(rbw->ptcaches));
	rbw->pointcache->step = 1;

	/* return this sim world */
	return rbw;
}

void BKE_rigidbody_world_init_mempool(RigidBodyWorld *rbw)
{
	rbw->body_pool = BLI_mempool_create(rbRigidBodySize, 512, 512, BLI_MEMPOOL_ALLOW_ITER);
}

RigidBodyWorld *BKE_rigidbody_world_copy(RigidBodyWorld *rbw)
{
	RigidBodyWorld *rbwn = MEM_dupallocN(rbw);

	if (rbw->effector_weights)
		rbwn->effector_weights = MEM_dupallocN(rbw->effector_weights);
	if (rbwn->group)
		id_us_plus(&rbwn->group->id);
	if (rbwn->constraints)
		id_us_plus(&rbwn->constraints->id);

	rbwn->pointcache = BKE_ptcache_copy_list(&rbwn->ptcaches, &rbw->ptcaches, false);

	rbwn->objects = NULL;
	rbwn->physics_world = NULL;
	rbwn->numbodies = 0;

	return rbwn;
}

void BKE_rigidbody_world_groups_relink(RigidBodyWorld *rbw)
{
	if (rbw->group && rbw->group->id.newid)
		rbw->group = (Group *)rbw->group->id.newid;
	if (rbw->constraints && rbw->constraints->id.newid)
		rbw->constraints = (Group *)rbw->constraints->id.newid;
	if (rbw->effector_weights->group && rbw->effector_weights->group->id.newid)
		rbw->effector_weights->group = (Group *)rbw->effector_weights->group->id.newid;
}

/* ************************************** */
/* Utilities API */

/* Get RigidBody world for the given scene, creating one if needed
 *
 * \param scene Scene to find active Rigid Body world for
 */
RigidBodyWorld *BKE_rigidbody_get_world(Scene *scene)
{
	/* sanity check */
	if (scene == NULL)
		return NULL;

	return scene->rigidbody_world;
}

/* ************************************** */
/* Simulation Interface - Bullet */

/* Update object array and rigid body count so they're in sync with the rigid body group */
static void rigidbody_update_ob_array(RigidBodyWorld *rbw)
{
	GroupObject *go;
	int i, n;

	n = BLI_countlist(&rbw->group->gobject);

	if (rbw->numbodies != n) {
		rbw->numbodies = n;
		rbw->objects = realloc(rbw->objects, sizeof(Object *) * rbw->numbodies);
	}

	for (go = rbw->group->gobject.first, i = 0; go; go = go->next, i++) {
		Object *ob = go->ob;
		rbw->objects[i] = ob;
	}
}

static void rigidbody_sync_world(Scene *scene, RigidBodyWorld *rbw)
{
	float adj_gravity[3];

	/* adjust gravity to take effector weights into account */
	if (scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY) {
		copy_v3_v3(adj_gravity, scene->physics_settings.gravity);
		mul_v3_fl(adj_gravity, rbw->effector_weights->global_gravity * rbw->effector_weights->weight[0]);
	}
	else {
		zero_v3(adj_gravity);
	}

	/* update gravity, since this RNA setting is not part of RigidBody settings */
	RB_dworld_set_gravity(rbw->physics_world, adj_gravity);

	/* update object array in case there are changes */
	rigidbody_update_ob_array(rbw);
}

/**
 * Updates and validates world, bodies and shapes.
 *
 * \param rebuild Rebuild entire simulation
 */
static void rigidbody_world_build(Scene *scene, RigidBodyWorld *rbw, int rebuild)
{
	/* update world */
	if (rebuild)
		BKE_rigidbody_validate_sim_world(scene, rbw, true);
	rigidbody_sync_world(scene, rbw);

	/* clear sync tags */
	rigidbody_world_clear_used_tags(rbw);

	/* XXX TODO For rebuild: remove all constraints first.
	 * Otherwise we can end up deleting objects that are still
	 * referenced by constraints, corrupting bullet's internal list.
	 * XXX 2: this may have been solved by the new mempool implementation, revisit!
	 * 
	 * Memory management needs redesign here, this is just a dirty workaround.
	 */
	if (rebuild && rbw->constraints) {
		GroupObject *go;
		for (go = rbw->constraints->gobject.first; go; go = go->next) {
			Object *ob = go->ob;
			if (ob) {
				RigidBodyCon *rbc = ob->rigidbody_constraint;
				if (rbc && rbc->physics_constraint) {
					RB_dworld_remove_constraint(rbw->physics_world, rbc->physics_constraint);
					RB_constraint_delete(rbc->physics_constraint);
					rbc->physics_constraint = NULL;
				}
			}
		}
	}


	/* build objects */
	BKE_rigidbody_objects_build(scene, rbw, rebuild);
	
	/* build constraints */
	BKE_rigidbody_constraints_build(scene, rbw, rebuild);
	
	/* XXX it may be desirable to do this _before_ adding new bodies (world_build_*),
	 * otherwise could end up in situations where lots of unneeded
	 * data is allocated intermittently.
	 */
	/* remove orphaned rigid bodies */
	rigidbody_world_free_bodies(rbw, true);
}

static void rigidbody_world_apply(Scene *scene, RigidBodyWorld *rbw)
{
	BKE_rigidbody_objects_apply(scene, rbw);

	BKE_rigidbody_constraints_apply(scene, rbw);
}

bool BKE_rigidbody_check_sim_running(RigidBodyWorld *rbw, float ctime)
{
	return (rbw && (rbw->flag & RBW_FLAG_MUTED) == 0 && ctime > rbw->pointcache->startframe);
}

void BKE_rigidbody_cache_reset(RigidBodyWorld *rbw)
{
	if (rbw)
		rbw->pointcache->flag |= PTCACHE_OUTDATED;
}

/* ------------------ */

/* Rebuild rigid body world */
/* NOTE: this needs to be called before frame update to work correctly */
void BKE_rigidbody_rebuild_world(Scene *scene, float ctime)
{
	RigidBodyWorld *rbw = scene->rigidbody_world;
	PointCache *cache;
	PTCacheID pid;
	int startframe, endframe;

	BKE_ptcache_id_from_rigidbody(&pid, NULL, rbw);
	BKE_ptcache_id_time(&pid, scene, ctime, &startframe, &endframe, NULL);
	cache = rbw->pointcache;

	/* flag cache as outdated if we don't have a world or number of objects in the simulation has changed */
	if (rbw->physics_world == NULL || rbw->numbodies != BLI_countlist(&rbw->group->gobject)) {
		cache->flag |= PTCACHE_OUTDATED;
	}

	if (ctime == startframe + 1 && rbw->ltime == startframe) {
		if (cache->flag & PTCACHE_OUTDATED) {
			BKE_ptcache_id_reset(scene, &pid, PTCACHE_RESET_OUTDATED);
			rigidbody_world_build(scene, rbw, true);
			BKE_ptcache_validate(cache, (int)ctime);
			cache->last_exact = 0;
			cache->flag &= ~PTCACHE_REDO_NEEDED;
		}
	}
}

/* Run RigidBody simulation for the specified physics world */
bool BKE_rigidbody_do_simulation(Scene *scene, float ctime, void (*tickcb)(void *tickdata, float timestep), void *tickdata)
{
	float timestep;
	RigidBodyWorld *rbw = scene->rigidbody_world;
	PointCache *cache;
	PTCacheID pid;
	int startframe, endframe;

	BKE_ptcache_id_from_rigidbody(&pid, NULL, rbw);
	BKE_ptcache_id_time(&pid, scene, ctime, &startframe, &endframe, NULL);
	cache = rbw->pointcache;

	if (ctime <= startframe) {
		rbw->ltime = startframe;
		return true; /* is ok, just nothing to simulate before the start frame */
	}
	/* make sure we don't go out of cache frame range */
	else if (ctime > endframe) {
		ctime = endframe;
	}

	/* don't try to run the simulation if we don't have a world yet but allow reading baked cache */
	if (rbw->physics_world == NULL && !(cache->flag & PTCACHE_BAKED))
		return false;
	else if (rbw->objects == NULL)
		rigidbody_update_ob_array(rbw);

	/* try to read from cache */
	// RB_TODO deal with interpolated, old and baked results
	if (BKE_ptcache_read(&pid, ctime)) {
		BKE_ptcache_validate(cache, (int)ctime);
		rbw->ltime = ctime;
		/* XXX TODO run the stepping loop anyway, but then set all rigid bodies
		 * to animated and interpolate cached data!
		 * The collision info may still be needed for other solvers
		 */
		return true;
	}

	/* advance simulation, we can only step one frame forward */
	if (ctime == rbw->ltime + 1 && !(cache->flag & PTCACHE_BAKED)) {
		/* write cache for first frame when on second frame */
		if (rbw->ltime == startframe && (cache->flag & PTCACHE_OUTDATED || cache->last_exact == 0)) {
			BKE_ptcache_write(&pid, startframe);
		}

		/* update and validate simulation */
		rigidbody_world_build(scene, rbw, false);

		/* calculate how much time elapsed since last step in seconds */
		timestep = 1.0f / (float)FPS * (ctime - rbw->ltime) * rbw->time_scale;
		/* step simulation by the requested timestep, steps per second are adjusted to take time scale into account */
		RB_dworld_step_simulation(rbw->physics_world, timestep, INT_MAX, 1.0f / (float)rbw->steps_per_second * min_ff(rbw->time_scale, 1.0f),
		                          tickcb, tickdata, false);

		rigidbody_world_apply(scene, rbw);

		/* write cache for current frame */
		BKE_ptcache_validate(cache, (int)ctime);
		BKE_ptcache_write(&pid, (unsigned int)ctime);

		rbw->ltime = ctime;
	}
	
	return true;
}
/* ************************************** */

#else  /* WITH_BULLET */

/* stubs */
#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

void BKE_rigidbody_free_world(RigidBodyWorld *rbw) {}
void BKE_rigidbody_free_object(Object *ob) {}
void BKE_rigidbody_free_constraint(Object *ob) {}
struct RigidBodyOb *BKE_rigidbody_copy_object(Object *ob) { return NULL; }
struct RigidBodyCon *BKE_rigidbody_copy_constraint(Object *ob) { return NULL; }
void BKE_rigidbody_relink_constraint(RigidBodyCon *rbc) {}
void BKE_rigidbody_validate_sim_world(Scene *scene, RigidBodyWorld *rbw, bool rebuild) {}
void BKE_rigidbody_calc_volume(Object *ob, float *r_vol) { if (r_vol) *r_vol = 0.0f; }
void BKE_rigidbody_calc_center_of_mass(Object *ob, float r_com[3]) { zero_v3(r_com); }
struct RigidBodyWorld *BKE_rigidbody_create_world(Scene *scene) { return NULL; }
struct RigidBodyWorld *BKE_rigidbody_world_copy(RigidBodyWorld *rbw) { return NULL; }
void BKE_rigidbody_world_groups_relink(struct RigidBodyWorld *rbw) {}
struct RigidBodyOb *BKE_rigidbody_create_object(Scene *scene, Object *ob, short type) { return NULL; }
struct RigidBodyCon *BKE_rigidbody_create_constraint(Scene *scene, Object *ob, short type) { return NULL; }
struct RigidBodyWorld *BKE_rigidbody_get_world(Scene *scene) { return NULL; }
void BKE_rigidbody_remove_object(Scene *scene, Object *ob) {}
void BKE_rigidbody_remove_constraint(Scene *scene, Object *ob) {}
void BKE_rigidbody_sync_transforms(RigidBodyWorld *rbw, Object *ob, float ctime) {}
void BKE_rigidbody_aftertrans_update(Object *ob, float loc[3], float rot[3], float quat[4], float rotAxis[3], float rotAngle) {}
bool BKE_rigidbody_check_sim_running(RigidBodyWorld *rbw, float ctime) { return false; }
void BKE_rigidbody_cache_reset(RigidBodyWorld *rbw) {}
void BKE_rigidbody_rebuild_world(Scene *scene, float ctime) {}
void BKE_rigidbody_do_simulation(Scene *scene, float ctime) {}

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

#endif  /* WITH_BULLET */

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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/BPH_mass_spring.c
 *  \ingroup bph
 */

extern "C" {
#include "MEM_guardedalloc.h"

#include "DNA_cloth_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BLI_math.h"
#include "BLI_linklist.h"
#include "BLI_utildefines.h"

#include "BKE_cloth.h"
#include "BKE_collision.h"
#include "BKE_effect.h"
}

#include "BPH_mass_spring.h"
#include "implicit.h"

BLI_INLINE bool exclude_vertex(Cloth *cloth, int index)
{
	return cloth->verts[index].flags & CLOTH_VERT_FLAG_EXCLUDE;
}

BLI_INLINE bool exclude_spring(Cloth *cloth, ClothSpring *spring)
{
	switch (spring->type) {
		case CLOTH_SPRING_TYPE_BENDING_ANG:
			return exclude_vertex(cloth, spring->ij) || exclude_vertex(cloth, spring->kl) || exclude_vertex(cloth, spring->mn);
			
		default:
			return exclude_vertex(cloth, spring->ij) || exclude_vertex(cloth, spring->kl);
	}
}

/* Assign a running index to each cloth vertex for mapping to solver data,
 * or -1 if the vertex is to be ignored.
 * Returns the number of solver vertices required.
 */
static int assign_solver_indices(Cloth *cloth)
{
	ClothVertex *vert;
	int i, si;
	
	si = 0;
	vert = cloth->verts;
	for (i = 0; i < cloth->numverts; ++i, ++vert) {
		if (exclude_vertex(cloth, i))
			vert->solver_index = -1;
		else {
			vert->solver_index = si;
			++si;
		}
	}
	
	/* first element of solver data matrices is used to store numbers ...
	 * have to keep at least one
	 */
	if (si == 0)
		si = 1;
	
	return si;
}

/* Number of off-diagonal non-zero matrix blocks.
 * Basically there is one of these for each vertex-vertex interaction.
 */
static int cloth_count_nondiag_blocks(Cloth *cloth)
{
	LinkNode *link;
	int nondiag = 0;
	
	for (link = cloth->springs; link; link = link->next) {
		ClothSpring *spring = (ClothSpring *)link->link;
		if (exclude_spring(cloth, spring))
			continue;
		
		switch (spring->type) {
			case CLOTH_SPRING_TYPE_BENDING_ANG:
				/* angular bending combines 3 vertices */
				nondiag += 3;
				break;
				
			default:
				/* all other springs depend on 2 vertices only */
				nondiag += 1;
				break;
		}
	}
	
	return nondiag;
}

static struct Implicit_Data *cloth_solver_init_data(Cloth *cloth)
{
	int totvert = assign_solver_indices(cloth);
	
	if (cloth->implicit && totvert != BPH_mass_spring_solver_numvert(cloth->implicit)) {
		BPH_mass_spring_solver_free(cloth->implicit);
		cloth->implicit = NULL;
	}
	
	if (!cloth->implicit) {
		int nondiag = cloth_count_nondiag_blocks(cloth);
		cloth->implicit = BPH_mass_spring_solver_create(totvert, nondiag);
	}
	
	return cloth->implicit;
}

int BPH_cloth_solver_init(Object *UNUSED(ob), ClothModifierData *clmd)
{
	Cloth *cloth = clmd->clothObject;
	ClothVertex *vert;
	const float ZERO[3] = {0.0f, 0.0f, 0.0f};
	Implicit_Data *id = cloth_solver_init_data(cloth);
	unsigned int i;
	
	vert = cloth->verts;
	for (i = 0; i < cloth->numverts; ++i, ++vert) {
		if (vert->solver_index < 0)
			continue;
		BPH_mass_spring_set_vertex_mass(id, vert->solver_index, vert->mass);
		BPH_mass_spring_set_motion_state(id, vert->solver_index, vert->x, ZERO);
	}
	
	return 1;
}

void BPH_cloth_solver_free(ClothModifierData *clmd)
{
	Cloth *cloth = clmd->clothObject;
	
	if (cloth->implicit) {
		BPH_mass_spring_solver_free(cloth->implicit);
		cloth->implicit = NULL;
	}
}

void BPH_cloth_solver_set_positions(ClothModifierData *clmd)
{
	Cloth *cloth = clmd->clothObject;
	ClothVertex *vert;
	unsigned int numverts = cloth->numverts, i;
	ClothHairRoot *cloth_roots = clmd->roots;
	Implicit_Data *id = cloth_solver_init_data(cloth);
	
	vert = cloth->verts;
	for (i = 0; i < numverts; ++i, ++vert) {
		ClothHairRoot *root = &cloth_roots[i];
		
		if (vert->solver_index < 0)
			continue;
		
		BPH_mass_spring_set_rest_transform(id, vert->solver_index, root->rot);
		BPH_mass_spring_set_motion_state(id, vert->solver_index, vert->x, vert->v);
	}
}

static bool collision_response(ClothModifierData *clmd, CollisionModifierData *collmd, CollPair *collpair, float dt, float restitution, float r_impulse[3])
{
	Cloth *cloth = clmd->clothObject;
	int index = collpair->ap1;
	bool result = false;
	
	float v1[3], v2_old[3], v2_new[3], v_rel_old[3], v_rel_new[3];
	float epsilon2 = BLI_bvhtree_getepsilon(collmd->bvhtree);

	float margin_distance = collpair->distance - epsilon2;
	float mag_v_rel;
	
	zero_v3(r_impulse);
	
	if (margin_distance > 0.0f)
		return false; /* XXX tested before already? */
	
	/* only handle static collisions here */
	if ( collpair->flag & COLLISION_IN_FUTURE )
		return false;
	
	/* velocity */
	copy_v3_v3(v1, cloth->verts[index].v);
	collision_get_collider_velocity(v2_old, v2_new, collmd, collpair);
	/* relative velocity = velocity of the cloth point relative to the collider */
	sub_v3_v3v3(v_rel_old, v1, v2_old);
	sub_v3_v3v3(v_rel_new, v1, v2_new);
	/* normal component of the relative velocity */
	mag_v_rel = dot_v3v3(v_rel_old, collpair->normal);
	
	/* only valid when moving toward the collider */
	if (mag_v_rel < -ALMOST_ZERO) {
		float v_nor_old, v_nor_new;
		float v_tan_old[3], v_tan_new[3];
		float bounce, repulse;
		
		/* Collision response based on
		 * "Simulating Complex Hair with Robust Collision Handling" (Choe, Choi, Ko, ACM SIGGRAPH 2005)
		 * http://graphics.snu.ac.kr/publications/2005-choe-HairSim/Choe_2005_SCA.pdf
		 */
		
		v_nor_old = mag_v_rel;
		v_nor_new = dot_v3v3(v_rel_new, collpair->normal);
		
		madd_v3_v3v3fl(v_tan_old, v_rel_old, collpair->normal, -v_nor_old);
		madd_v3_v3v3fl(v_tan_new, v_rel_new, collpair->normal, -v_nor_new);
		
		bounce = -v_nor_old * restitution;
		
		repulse = -margin_distance / dt; /* base repulsion velocity in normal direction */
		/* XXX this clamping factor is quite arbitrary ...
		 * not sure if there is a more scientific approach, but seems to give good results
		 */
		CLAMP(repulse, 0.0f, 4.0f * bounce);
		
		if (margin_distance < -epsilon2) {
			mul_v3_v3fl(r_impulse, collpair->normal, max_ff(repulse, bounce) - v_nor_new);
		}
		else {
			bounce = 0.0f;
			mul_v3_v3fl(r_impulse, collpair->normal, repulse - v_nor_new);
		}
		
		result = true;
	}
	
	return result;
}

/* Init constraint matrix
 * This is part of the modified CG method suggested by Baraff/Witkin in
 * "Large Steps in Cloth Simulation" (Siggraph 1998)
 */
static void cloth_setup_constraints(ClothModifierData *clmd, ColliderContacts *contacts, int totcolliders, float dt)
{
	Cloth *cloth = clmd->clothObject;
	Implicit_Data *data = cloth->implicit;
	ClothVertex *vert;
	int numverts = cloth->numverts;
	int i, j, v;
	
	const float ZERO[3] = {0.0f, 0.0f, 0.0f};
	
	vert = cloth->verts;
	for (v = 0; v < numverts; ++v, ++vert) {
		if (vert->solver_index < 0)
			continue;
		if (vert->flags & CLOTH_VERT_FLAG_PINNED) {
			/* pinned vertex constraints */
			BPH_mass_spring_add_constraint_ndof0(data, vert->solver_index, ZERO); /* velocity is defined externally */
		}
		
		vert->impulse_count = 0;
	}

	for (i = 0; i < totcolliders; ++i) {
		ColliderContacts *ct = &contacts[i];
		for (j = 0; j < ct->totcollisions; ++j) {
			CollPair *collpair = &ct->collisions[j];
			int v = collpair->face1;
			float impulse[3];
			
//			float restitution = (1.0f - clmd->coll_parms->damping) * (1.0f - ct->ob->pd->pdef_sbdamp);
			float restitution = 0.0f;
			
			vert = &cloth->verts[v];
			if (vert->solver_index < 0)
				continue;
			/* pinned verts handled separately */
			if (vert->flags & CLOTH_VERT_FLAG_PINNED)
				continue;
			
			/* XXX cheap way of avoiding instability from multiple collisions in the same step
			 * this should eventually be supported ...
			 */
			if (vert->impulse_count > 0)
				continue;
			
			/* calculate collision response */
			if (!collision_response(clmd, ct->collmd, collpair, dt, restitution, impulse))
				continue;
			
			BPH_mass_spring_add_constraint_ndof2(data, vert->solver_index, collpair->normal, impulse);
			++vert->impulse_count;
			
			BKE_sim_debug_data_add_dot(clmd->debug_data, collpair->pa, 0, 1, 0, "collision", hash_collpair(936, collpair));
//			BKE_sim_debug_data_add_dot(clmd->debug_data, collpair->pb, 1, 0, 0, "collision", hash_collpair(937, collpair));
//			BKE_sim_debug_data_add_line(clmd->debug_data, collpair->pa, collpair->pb, 0.7, 0.7, 0.7, "collision", hash_collpair(938, collpair));
			
			{ /* DEBUG */
				float nor[3];
				mul_v3_v3fl(nor, collpair->normal, -collpair->distance);
				BKE_sim_debug_data_add_vector(clmd->debug_data, collpair->pa, nor, 1, 1, 0, "collision", hash_collpair(939, collpair));
//				BKE_sim_debug_data_add_vector(clmd->debug_data, collpair->pb, impulse, 1, 1, 0, "collision", hash_collpair(940, collpair));
//				BKE_sim_debug_data_add_vector(clmd->debug_data, collpair->pb, collpair->normal, 1, 1, 0, "collision", hash_collpair(941, collpair));
			}
		}
	}
}

/* computes where the cloth would be if it were subject to perfectly stiff edges
 * (edge distance constraints) in a lagrangian solver.  then add forces to help
 * guide the implicit solver to that state.  this function is called after
 * collisions*/
static int UNUSED_FUNCTION(cloth_calc_helper_forces)(Object *UNUSED(ob), ClothModifierData *clmd, float (*initial_cos)[3], float UNUSED(step), float dt)
{
	Cloth *cloth= clmd->clothObject;
	float (*cos)[3] = (float (*)[3])MEM_callocN(sizeof(float)*3*cloth->numverts, "cos cloth_calc_helper_forces");
	float *masses = (float *)MEM_callocN(sizeof(float)*cloth->numverts, "cos cloth_calc_helper_forces");
	LinkNode *node;
	ClothSpring *spring;
	ClothVertex *vert;
	int i, steps;
	
	vert = cloth->verts;
	for (i=0; i<cloth->numverts; i++, vert++) {
		if (vert->solver_index < 0)
			continue;
		
		copy_v3_v3(cos[i], vert->tx);
		
		if (vert->goal == 1.0f || len_squared_v3v3(initial_cos[i], vert->tx) != 0.0f) {
			masses[i] = 1e+10;
		}
		else {
			masses[i] = vert->mass;
		}
	}
	
	steps = 55;
	for (i=0; i<steps; i++) {
		for (node=cloth->springs; node; node=node->next) {
			/* ClothVertex *cv1, *cv2; */ /* UNUSED */
			int v1, v2;
			float len, c, l, vec[3];
			
			spring = (ClothSpring *)node->link;
			if (exclude_spring(cloth, spring))
				continue;
			if (spring->type != CLOTH_SPRING_TYPE_STRUCTURAL && spring->type != CLOTH_SPRING_TYPE_SHEAR) 
				continue;
			
			v1 = spring->ij; v2 = spring->kl;
			/* cv1 = cloth->verts + v1; */ /* UNUSED */
			/* cv2 = cloth->verts + v2; */ /* UNUSED */
			len = len_v3v3(cos[v1], cos[v2]);
			
			sub_v3_v3v3(vec, cos[v1], cos[v2]);
			normalize_v3(vec);
			
			c = (len - spring->restlen);
			if (c == 0.0f)
				continue;
			
			l = c / ((1.0f / masses[v1]) + (1.0f / masses[v2]));
			
			mul_v3_fl(vec, -(1.0f / masses[v1]) * l);
			add_v3_v3(cos[v1], vec);
	
			sub_v3_v3v3(vec, cos[v2], cos[v1]);
			normalize_v3(vec);
			
			mul_v3_fl(vec, -(1.0f / masses[v2]) * l);
			add_v3_v3(cos[v2], vec);
		}
	}
	
	vert = cloth->verts;
	for (i=0; i<cloth->numverts; i++, vert++) {
		float vec[3];
		
		if (vert->solver_index < 0)
			continue;
		
		/*compute forces*/
		sub_v3_v3v3(vec, cos[i], vert->tx);
		mul_v3_fl(vec, vert->mass*dt*20.0f);
		add_v3_v3(vert->tv, vec);
		//copy_v3_v3(cv->tx, cos[i]);
	}
	
	MEM_freeN(cos);
	MEM_freeN(masses);
	
	return 1;
}

BLI_INLINE void cloth_calc_spring_force(ClothModifierData *clmd, ClothSpring *s, float time)
{
	Cloth *cloth = clmd->clothObject;
	ClothSimSettings *parms = clmd->sim_parms;
	Implicit_Data *data = cloth->implicit;
	ClothVertex *verts = cloth->verts;
	int solver_ij = s->ij < 0 ? -1 : cloth->verts[s->ij].solver_index;
	int solver_kl = s->kl < 0 ? -1 : cloth->verts[s->kl].solver_index;
	int solver_mn = s->mn < 0 ? -1 : cloth->verts[s->mn].solver_index;
	
	bool no_compress = parms->flags & CLOTH_SIMSETTINGS_FLAG_NO_SPRING_COMPRESS;
	
	zero_v3(s->f);
	zero_m3(s->dfdx);
	zero_m3(s->dfdv);
	
	s->flags &= ~CLOTH_SPRING_FLAG_NEEDED;
	
	// calculate force of structural + shear springs
	if ((s->type & CLOTH_SPRING_TYPE_STRUCTURAL) || (s->type & CLOTH_SPRING_TYPE_SHEAR) || (s->type & CLOTH_SPRING_TYPE_SEWING) ) {
#ifdef CLOTH_FORCE_SPRING_STRUCTURAL
		float k, scaling;
		
		s->flags |= CLOTH_SPRING_FLAG_NEEDED;
		
		scaling = parms->structural + s->stiffness * fabsf(parms->max_struct - parms->structural);
		k = scaling / (parms->avg_spring_len + FLT_EPSILON);
		
		if (s->type & CLOTH_SPRING_TYPE_SEWING) {
			// TODO: verify, half verified (couldn't see error)
			// sewing springs usually have a large distance at first so clamp the force so we don't get tunnelling through colission objects
			BPH_mass_spring_force_spring_linear(data, solver_ij, solver_kl, s->restlen, k, parms->Cdis, no_compress, parms->max_sewing, s->f, s->dfdx, s->dfdv);
		}
		else {
			BPH_mass_spring_force_spring_linear(data, solver_ij, solver_kl, s->restlen, k, parms->Cdis, no_compress, 0.0f, s->f, s->dfdx, s->dfdv);
		}
#endif
	}
	else if (s->type & CLOTH_SPRING_TYPE_GOAL) {
#ifdef CLOTH_FORCE_SPRING_GOAL
		float goal_x[3], goal_v[3];
		float k, scaling;
		
		s->flags |= CLOTH_SPRING_FLAG_NEEDED;
		
		// current_position = xold + t * (newposition - xold)
		interp_v3_v3v3(goal_x, verts[s->ij].xold, verts[s->ij].xconst, time);
		sub_v3_v3v3(goal_v, verts[s->ij].xconst, verts[s->ij].xold); // distance covered over dt==1
		
		scaling = parms->goalspring + s->stiffness * fabsf(parms->max_struct - parms->goalspring);
		k = verts[s->ij].goal * scaling / (parms->avg_spring_len + FLT_EPSILON);
		
		BPH_mass_spring_force_spring_goal(data, solver_ij, goal_x, goal_v, k, parms->goalfrict * 0.01f, s->f, s->dfdx, s->dfdv);
#endif
	}
	else if (s->type & CLOTH_SPRING_TYPE_BENDING) {  /* calculate force of bending springs */
#ifdef CLOTH_FORCE_SPRING_BEND
		float kb, cb, scaling;
		
		s->flags |= CLOTH_SPRING_FLAG_NEEDED;
		
		scaling = parms->bending + s->stiffness * fabsf(parms->max_bend - parms->bending);
		kb = scaling / (20.0f * (parms->avg_spring_len + FLT_EPSILON));
		
		scaling = parms->bending_damping;
		cb = scaling / (20.0f * (parms->avg_spring_len + FLT_EPSILON));
		
		BPH_mass_spring_force_spring_bending(data, solver_ij, solver_kl, s->restlen, kb, cb, s->f, s->dfdx, s->dfdv);
#endif
	}
	else if (s->type & CLOTH_SPRING_TYPE_BENDING_ANG) {
#ifdef CLOTH_FORCE_SPRING_BEND
		float kb, cb, scaling;
		
		s->flags |= CLOTH_SPRING_FLAG_NEEDED;
		
		scaling = parms->bending + s->stiffness * fabsf(parms->max_bend - parms->bending);
		kb = scaling / (20.0f * (parms->avg_spring_len + FLT_EPSILON));
		
		scaling = parms->bending_damping;
		cb = scaling / (20.0f * (parms->avg_spring_len + FLT_EPSILON));
		
		/* XXX assuming same restlen for ij and jk segments here, this can be done correctly for hair later */
		BPH_mass_spring_force_spring_bending_angular(data, solver_ij, solver_kl, solver_mn, s->target, kb, cb);
		
		{
			float x_kl[3], x_mn[3], v[3], d[3];
			
			BPH_mass_spring_get_motion_state(data, solver_kl, x_kl, v);
			BPH_mass_spring_get_motion_state(data, solver_mn, x_mn, v);
			
			BKE_sim_debug_data_add_dot(clmd->debug_data, x_kl, 0.9, 0.9, 0.9, "target", hash_vertex(7980, s->kl));
			BKE_sim_debug_data_add_line(clmd->debug_data, x_kl, x_mn, 0.8, 0.8, 0.8, "target", hash_vertex(7981, s->kl));
			
			copy_v3_v3(d, s->target);
			BKE_sim_debug_data_add_vector(clmd->debug_data, x_kl, d, 0.8, 0.8, 0.2, "target", hash_vertex(7982, s->kl));
			
//			copy_v3_v3(d, s->target_ij);
//			BKE_sim_debug_data_add_vector(clmd->debug_data, x, d, 1, 0.4, 0.4, "target", hash_vertex(7983, s->kl));
		}
#endif
	}
}

static void hair_get_boundbox(ClothModifierData *clmd, float gmin[3], float gmax[3])
{
	Cloth *cloth = clmd->clothObject;
	Implicit_Data *data = cloth->implicit;
	unsigned int numverts = cloth->numverts;
	ClothVertex *vert;
	int i;
	
	INIT_MINMAX(gmin, gmax);
	vert = cloth->verts;
	for (i = 0; i < numverts; i++, vert++) {
		float x[3];
		if (vert->solver_index < 0)
			copy_v3_v3(x, vert->x); /* use input location as replacement for excluded verts */
		else
			BPH_mass_spring_get_motion_state(data, vert->solver_index, x, NULL);
		DO_MINMAX(x, gmin, gmax);
	}
}

static void cloth_calc_volume_force(ClothModifierData *clmd)
{
	ClothSimSettings *parms = clmd->sim_parms;
	Cloth *cloth = clmd->clothObject;
	Implicit_Data *data = cloth->implicit;
	int numverts = cloth->numverts;
	ClothVertex *vert;
	
	/* 2.0f is an experimental value that seems to give good results */
	float smoothfac = 2.0f * parms->velocity_smooth;
	float collfac = 2.0f * parms->collider_friction;
	float pressfac = parms->pressure;
	float minpress = parms->pressure_threshold;
	float gmin[3], gmax[3];
	int i;
	
	hair_get_boundbox(clmd, gmin, gmax);
	
	/* gather velocities & density */
	if (smoothfac > 0.0f || pressfac > 0.0f) {
		HairVertexGrid *vertex_grid = BPH_hair_volume_create_vertex_grid(clmd->sim_parms->voxel_res, gmin, gmax);
		
		vert = cloth->verts;
		for (i = 0; i < numverts; i++, vert++) {
			float x[3], v[3];
			
			if (vert->solver_index < 0) {
				copy_v3_v3(x, vert->x);
				copy_v3_v3(v, vert->v);
			}
			else {
				BPH_mass_spring_get_motion_state(data, vert->solver_index, x, v);
			}
			BPH_hair_volume_add_vertex(vertex_grid, x, v);
		}
		BPH_hair_volume_normalize_vertex_grid(vertex_grid);
		
#if 0
		/* apply velocity filter */
		BPH_hair_volume_vertex_grid_filter_box(vertex_grid, clmd->sim_parms->voxel_filter_size);
#endif
		
		vert = cloth->verts;
		for (i = 0; i < numverts; i++, vert++) {
			float x[3], v[3], f[3], dfdx[3][3], dfdv[3][3];
			
			if (vert->solver_index < 0)
				continue;
			
			/* calculate volumetric forces */
			BPH_mass_spring_get_motion_state(data, vert->solver_index, x, v);
			BPH_hair_volume_vertex_grid_forces(vertex_grid, x, v, smoothfac, pressfac, minpress, f, dfdx, dfdv);
			/* apply on hair data */
			BPH_mass_spring_force_extern(data, vert->solver_index, f, dfdx, dfdv);
		}
		
		BPH_hair_volume_free_vertex_grid(vertex_grid);
	}
}

static void cloth_calc_force(ClothModifierData *clmd, float UNUSED(frame), ListBase *effectors, float time)
{
	/* Collect forces and derivatives:  F, dFdX, dFdV */
	Cloth *cloth = clmd->clothObject;
	Implicit_Data *data = cloth->implicit;
	unsigned int i	= 0;
	float 		drag 	= clmd->sim_parms->Cvi * 0.01f; /* viscosity of air scaled in percent */
	float 		gravity[3] = {0.0f, 0.0f, 0.0f};
	MFace 		*mfaces 	= cloth->mfaces;
	unsigned int numverts = cloth->numverts;
	ClothVertex *vert;
	
#ifdef CLOTH_FORCE_GRAVITY
	/* global acceleration (gravitation) */
	if (clmd->scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY) {
		/* scale gravity force */
		mul_v3_v3fl(gravity, clmd->scene->physics_settings.gravity, 0.001f * clmd->sim_parms->effector_weights->global_gravity);
	}
	vert = cloth->verts;
	for (i = 0; i < cloth->numverts; i++, vert++) {
		BPH_mass_spring_force_gravity(data, i, vert->mass, gravity);
	}
#endif

	cloth_calc_volume_force(clmd);

#ifdef CLOTH_FORCE_DRAG
	BPH_mass_spring_force_drag(data, drag);
#endif
	
	/* handle external forces like wind */
	if (effectors) {
		/* cache per-vertex forces to avoid redundant calculation */
		float (*winvec)[3] = (float (*)[3])MEM_callocN(sizeof(float) * 3 * numverts, "effector forces");
		
		vert = cloth->verts;
		for (i = 0; i < cloth->numverts; i++, vert++) {
			float x[3], v[3];
			EffectedPoint epoint;
			
			if (vert->solver_index < 0)
				continue;
			
			BPH_mass_spring_get_motion_state(data, vert->solver_index, x, v);
			pd_point_from_loc(clmd->scene, x, v, i, &epoint);
			pdDoEffectors(effectors, NULL, clmd->sim_parms->effector_weights, &epoint, winvec[i], NULL);
		}
		
		for (i = 0; i < cloth->numfaces; i++) {
			MFace *mf = &mfaces[i];
			ClothVertex *verts = cloth->verts;
			int si1 = verts[mf->v1].solver_index;
			int si2 = verts[mf->v2].solver_index;
			int si3 = verts[mf->v3].solver_index;
			int si4 = mf->v4 ? verts[mf->v4].solver_index : -1;
			
			if (si1 < 0 || si2 < 0 || si3 < 0 || si4 < 0)
				continue;
			
			BPH_mass_spring_force_face_wind(data, si1, si2, si3, si4, winvec);
		}

		/* Hair has only edges */
		if (cloth->numfaces == 0) {
			for (LinkNode *link = cloth->springs; link; link = link->next) {
				ClothSpring *spring = (ClothSpring *)link->link;
				if (spring->type == CLOTH_SPRING_TYPE_STRUCTURAL) {
					ClothVertex *verts = cloth->verts;
					int si_ij = verts[spring->ij].solver_index;
					int si_kl = verts[spring->kl].solver_index;
					
					if (si_ij < 0 || si_kl < 0)
						continue;
					
					BPH_mass_spring_force_edge_wind(data, si_ij, si_kl, winvec);
				}
			}
		}

		MEM_freeN(winvec);
	}
	
	// calculate spring forces
	for (LinkNode *link = cloth->springs; link; link = link->next) {
		ClothSpring *spring = (ClothSpring *)link->link;
		if (exclude_spring(cloth, spring))
			continue;
		// only handle active springs
		if (!(spring->flags & CLOTH_SPRING_FLAG_DEACTIVATE))
			cloth_calc_spring_force(clmd, spring, time);
	}
}

static void cloth_clear_result(ClothModifierData *clmd)
{
	ClothSolverResult *sres = clmd->solver_result;
	
	sres->status = 0;
	sres->max_error = sres->min_error = sres->avg_error = 0.0f;
	sres->max_iterations = sres->min_iterations = 0;
	sres->avg_iterations = 0.0f;
}

static void cloth_record_result(ClothModifierData *clmd, ImplicitSolverResult *result, int steps)
{
	ClothSolverResult *sres = clmd->solver_result;
	
	if (sres->status) { /* already initialized ? */
		/* error only makes sense for successful iterations */
		if (result->status == BPH_SOLVER_SUCCESS) {
			sres->min_error = min_ff(sres->min_error, result->error);
			sres->max_error = max_ff(sres->max_error, result->error);
			sres->avg_error += result->error / (float)steps;
		}
		
		sres->min_iterations = min_ii(sres->min_iterations, result->iterations);
		sres->max_iterations = max_ii(sres->max_iterations, result->iterations);
		sres->avg_iterations += (float)result->iterations / (float)steps;
	}
	else {
		/* error only makes sense for successful iterations */
		if (result->status == BPH_SOLVER_SUCCESS) {
			sres->min_error = sres->max_error = result->error;
			sres->avg_error += result->error / (float)steps;
		}
		
		sres->min_iterations = sres->max_iterations  = result->iterations;
		sres->avg_iterations += (float)result->iterations / (float)steps;
	}
	
	sres->status |= result->status;
}

int BPH_cloth_solve(Object *ob, float frame, ClothModifierData *clmd, ListBase *effectors)
{
	unsigned int i=0;
	float step=0.0f, tf=clmd->sim_parms->timescale;
	Cloth *cloth = clmd->clothObject;
	ClothVertex *verts = cloth->verts, *vert;
	unsigned int numverts = cloth->numverts;
	float dt = clmd->sim_parms->timescale / clmd->sim_parms->stepsPerFrame;
	Implicit_Data *id = cloth->implicit;
	ColliderContacts *contacts = NULL;
	int totcolliders = 0;
	
	BPH_mass_spring_solver_debug_data(id, clmd->debug_data);
	
	BKE_sim_debug_data_clear_category(clmd->debug_data, "collision");
	
	if (!clmd->solver_result)
		clmd->solver_result = (ClothSolverResult *)MEM_callocN(sizeof(ClothSolverResult), "cloth solver result");
	cloth_clear_result(clmd);
	
	if (clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) { /* do goal stuff */
		vert = verts;
		for (i = 0; i < numverts; i++, vert++) {
			// update velocities with constrained velocities from pinned verts
			if (vert->flags & CLOTH_VERT_FLAG_PINNED) {
				float v[3];
				
				if (vert->solver_index < 0)
					continue;
				
				sub_v3_v3v3(v, verts[i].xconst, verts[i].xold);
				// mul_v3_fl(v, clmd->sim_parms->stepsPerFrame);
				BPH_mass_spring_set_velocity(id, vert->solver_index, v);
			}
		}
	}
	
	if (clmd->debug_data) {
		for (i = 0; i < numverts; i++) {
//			BKE_sim_debug_data_add_dot(clmd->debug_data, verts[i].x, 1.0f, 0.1f, 1.0f, "points", hash_vertex(583, i));
		}
	}
	
	while (step < tf) {
		ImplicitSolverResult result;
		
		/* initialize forces to zero */
		BPH_mass_spring_clear_forces(id);
		BPH_mass_spring_clear_constraints(id);
		
		/* copy velocities for collision */
		vert = verts;
		for (i = 0; i < numverts; i++, vert++) {
			if (vert->solver_index < 0)
				continue;
			BPH_mass_spring_get_motion_state(id, vert->solver_index, NULL, verts[i].tv);
			copy_v3_v3(verts[i].v, verts[i].tv);
		}
		
		/* determine contact points */
		if (clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_ENABLED) {
			if (clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_POINTS) {
				cloth_find_point_contacts(ob, clmd, 0.0f, tf, &contacts, &totcolliders);
			}
		}
		
		/* setup vertex constraints for pinned vertices and contacts */
		cloth_setup_constraints(clmd, contacts, totcolliders, dt);
		
		// damping velocity for artistic reasons
		// this is a bad way to do it, should be removed imo - lukas_t
		if (clmd->sim_parms->vel_damping != 1.0f) {
			vert = verts;
			for (i = 0; i < numverts; i++, vert++) {
				float v[3];
				if (vert->solver_index < 0)
					continue;
				BPH_mass_spring_get_motion_state(id, vert->solver_index, NULL, v);
				mul_v3_fl(v, clmd->sim_parms->vel_damping);
				BPH_mass_spring_set_velocity(id, vert->solver_index, v);
			}
		}
		
		// calculate forces
		cloth_calc_force(clmd, frame, effectors, step);
		
		// calculate new velocity and position
		BPH_mass_spring_solve(id, dt, &result);
		cloth_record_result(clmd, &result, clmd->sim_parms->stepsPerFrame);
		
		BPH_mass_spring_apply_result(id);
		
		/* move pinned verts to correct position */
		vert = verts;
		for (i = 0; i < numverts; i++, vert++) {
			if (vert->solver_index < 0)
				continue;
			if (clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) {
				if (vert->flags & CLOTH_VERT_FLAG_PINNED) {
					float x[3];
					interp_v3_v3v3(x, verts[i].xold, verts[i].xconst, step + dt);
					BPH_mass_spring_set_position(id, vert->solver_index, x);
				}
			}
			
			BPH_mass_spring_get_motion_state(id, vert->solver_index, verts[i].txold, NULL);
			
//			if (!(verts[i].flags & CLOTH_VERT_FLAG_PINNED) && i > 0) {
//				BKE_sim_debug_data_add_line(clmd->debug_data, id->X[i], id->X[i-1], 0.6, 0.3, 0.3, "hair", hash_vertex(4892, i));
//				BKE_sim_debug_data_add_line(clmd->debug_data, id->Xnew[i], id->Xnew[i-1], 1, 0.5, 0.5, "hair", hash_vertex(4893, i));
//			}
//			BKE_sim_debug_data_add_vector(clmd->debug_data, id->X[i], id->V[i], 0, 0, 1, "velocity", hash_vertex(3158, i));
		}
		
		/* free contact points */
		if (contacts) {
			cloth_free_contacts(contacts, totcolliders);
		}
		
		step += dt;
	}
	
	/* copy results back to cloth data */
	vert = verts;
	for (i = 0; i < numverts; i++, vert++) {
		if (vert->solver_index < 0)
			continue;
		BPH_mass_spring_get_motion_state(id, vert->solver_index, vert->x, vert->v);
		copy_v3_v3(vert->txold, vert->x);
	}
	
	BPH_mass_spring_solver_debug_data(id, NULL);
	
	return 1;
}

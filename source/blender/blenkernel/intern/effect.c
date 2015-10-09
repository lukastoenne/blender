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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/effect.c
 *  \ingroup bke
 */

#include <stddef.h>
#include <stdarg.h>

#include <math.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_group_types.h"
#include "DNA_listBase.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_particle_types.h"
#include "DNA_texture_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_noise.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "PIL_time.h"

#include "BKE_anim.h"		/* needed for where_on_path */
#include "BKE_collision.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_mesh_sample.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_scene.h"
#include "BKE_smoke.h"

#include "bmesh.h"

#include "BJIT_forcefield.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "GPU_draw.h" /* GPU_free_image */

#include "RE_render_ext.h"
#include "RE_shader_ext.h"

/* fluid sim particle import */
#ifdef WITH_MOD_FLUID
#include "LBM_fluidsim.h"
#include <zlib.h>
#include <string.h>
#endif // WITH_MOD_FLUID

EffectorWeights *BKE_add_effector_weights(Group *group)
{
	EffectorWeights *weights = MEM_callocN(sizeof(EffectorWeights), "EffectorWeights");
	int i;

	for (i=0; i<NUM_PFIELD_TYPES; i++)
		weights->weight[i] = 1.0f;

	weights->global_gravity = 1.0f;

	weights->group = group;

	return weights;
}
PartDeflect *object_add_collision_fields(int type)
{
	PartDeflect *pd;

	pd= MEM_callocN(sizeof(PartDeflect), "PartDeflect");

	pd->forcefield = type;
	pd->pdef_sbdamp = 0.1f;
	pd->pdef_sbift  = 0.2f;
	pd->pdef_sboft  = 0.02f;
	pd->seed = ((unsigned int)(ceil(PIL_check_seconds_timer()))+1) % 128;
	pd->f_strength = 1.0f;
	pd->f_damp = 1.0f;

	/* set sensible defaults based on type */
	switch (type) {
		case PFIELD_VORTEX:
			pd->shape = PFIELD_SHAPE_PLANE;
			break;
		case PFIELD_WIND:
			pd->shape = PFIELD_SHAPE_PLANE;
			pd->f_flow = 1.0f; /* realistic wind behavior */
			break;
		case PFIELD_TEXTURE:
			pd->f_size = 1.0f;
			break;
		case PFIELD_SMOKEFLOW:
			pd->f_flow = 1.0f;
			break;
	}
	pd->flag = PFIELD_DO_LOCATION|PFIELD_DO_ROTATION;

	return pd;
}

/* ***************** PARTICLES ***************** */

/* -------------------------- Effectors ------------------ */
void free_partdeflect(PartDeflect *pd)
{
	if (!pd)
		return;

	if (pd->tex)
		pd->tex->id.us--;

	if (pd->rng)
		BLI_rng_free(pd->rng);

	MEM_freeN(pd);
}

static EffectorCache *new_effector_cache(Scene *scene, Object *ob, ParticleSystem *psys, PartDeflect *pd)
{
	EffectorCache *eff = MEM_callocN(sizeof(EffectorCache), "EffectorCache");
	eff->scene = scene;
	eff->ob = ob;
	eff->psys = psys;
	eff->pd = pd;
	eff->frame = -1;
	return eff;
}
static void add_object_to_effectors(EffectorContext *effctx, Scene *scene, EffectorWeights *weights, Object *ob, Object *ob_src)
{
	EffectorCache *eff = NULL;

	if ( ob == ob_src || weights->weight[ob->pd->forcefield] == 0.0f )
		return;

	if (ob->pd->shape == PFIELD_SHAPE_POINTS && !ob->derivedFinal )
		return;

	eff = new_effector_cache(scene, ob, NULL, ob->pd);

	/* make sure imat is up to date */
	invert_m4_m4(ob->imat, ob->obmat);

	BLI_addtail(&effctx->effectors, eff);
}
static void add_particles_to_effectors(EffectorContext *effctx, Scene *scene, EffectorWeights *weights, Object *ob, ParticleSystem *psys, ParticleSystem *psys_src)
{
	ParticleSettings *part= psys->part;

	if ( !psys_check_enabled(ob, psys) )
		return;

	if ( psys == psys_src && (part->flag & PART_SELF_EFFECT) == 0)
		return;

	if ( part->pd && part->pd->forcefield && weights->weight[part->pd->forcefield] != 0.0f) {
		BLI_addtail(&effctx->effectors, new_effector_cache(scene, ob, psys, part->pd));
	}

	if (part->pd2 && part->pd2->forcefield && weights->weight[part->pd2->forcefield] != 0.0f) {
		BLI_addtail(&effctx->effectors, new_effector_cache(scene, ob, psys, part->pd2));
	}
}

/* returns ListBase handle with objects taking part in the effecting */
EffectorContext *pdInitEffectors(Scene *scene, Object *ob_src, ParticleSystem *psys_src,
                                 EffectorWeights *weights, bool precalc)
{
	EffectorContext *effctx = MEM_callocN(sizeof(EffectorContext), "effector context");
	Base *base;
	unsigned int layer= ob_src->lay;
	
	if (weights->group) {
		GroupObject *go;
		
		for (go= weights->group->gobject.first; go; go= go->next) {
			if ( (go->ob->lay & layer) ) {
				if ( go->ob->pd && go->ob->pd->forcefield )
					add_object_to_effectors(effctx, scene, weights, go->ob, ob_src);

				if ( go->ob->particlesystem.first ) {
					ParticleSystem *psys= go->ob->particlesystem.first;

					for ( ; psys; psys=psys->next )
						add_particles_to_effectors(effctx, scene, weights, go->ob, psys, psys_src);
				}
			}
		}
	}
	else {
		for (base = scene->base.first; base; base= base->next) {
			if ( (base->lay & layer) ) {
				if ( base->object->pd && base->object->pd->forcefield )
					add_object_to_effectors(effctx, scene, weights, base->object, ob_src);

				if ( base->object->particlesystem.first ) {
					ParticleSystem *psys= base->object->particlesystem.first;

					for ( ; psys; psys=psys->next )
						add_particles_to_effectors(effctx, scene, weights, base->object, psys, psys_src);
				}
			}
		}
	}
	
	if (precalc)
		pdPrecalculateEffectors(effctx);
	
	return effctx;
}

void pdEndEffectors(EffectorContext *effctx)
{
	if (effctx) {
		EffectorCache *eff = effctx->effectors.first;
		for (; eff; eff = eff->next) {
			if (eff->guide_data)
				MEM_freeN(eff->guide_data);
		}
		
		BLI_freelistN(&effctx->effectors);
		
		MEM_freeN(effctx);
	}
}

static void precalculate_effector(EffectorCache *eff)
{
	unsigned int cfra = (unsigned int)(eff->scene->r.cfra >= 0 ? eff->scene->r.cfra : -eff->scene->r.cfra);
	if (!eff->pd->rng)
		eff->pd->rng = BLI_rng_new(eff->pd->seed + cfra);
	else
		BLI_rng_srandom(eff->pd->rng, eff->pd->seed + cfra);

	if (eff->pd->forcefield == PFIELD_GUIDE && eff->ob->type==OB_CURVE) {
		Curve *cu= eff->ob->data;
		if (cu->flag & CU_PATH) {
			if (eff->ob->curve_cache == NULL || eff->ob->curve_cache->path==NULL || eff->ob->curve_cache->path->data==NULL)
				BKE_displist_make_curveTypes(eff->scene, eff->ob, 0);

			if (eff->ob->curve_cache->path && eff->ob->curve_cache->path->data) {
				where_on_path(eff->ob, 0.0, eff->guide_loc, eff->guide_dir, NULL, &eff->guide_radius, NULL);
				mul_m4_v3(eff->ob->obmat, eff->guide_loc);
				mul_mat3_m4_v3(eff->ob->obmat, eff->guide_dir);
			}
		}
	}
	else if (eff->pd->shape == PFIELD_SHAPE_SURFACE) {
		eff->surmd = (SurfaceModifierData *)modifiers_findByType( eff->ob, eModifierType_Surface );
		if (eff->ob->type == OB_CURVE)
			eff->flag |= PE_USE_NORMAL_DATA;
	}
	else if (eff->psys)
		psys_update_particle_tree(eff->psys, eff->scene->r.cfra);

	/* Store object velocity */
	if (eff->ob) {
		float old_vel[3];

		BKE_object_where_is_calc_time(eff->scene, eff->ob, cfra - 1.0f);
		copy_v3_v3(old_vel, eff->ob->obmat[3]);
		BKE_object_where_is_calc_time(eff->scene, eff->ob, cfra);
		sub_v3_v3v3(eff->velocity, eff->ob->obmat[3], old_vel);
	}
}

void pdPrecalculateEffectors(EffectorContext *effctx)
{
	EffectorCache *eff = effctx->effectors.first;
	for (; eff; eff = eff->next)
		precalculate_effector(eff);
}


void pd_point_from_particle(ParticleSimulationData *sim, ParticleData *pa, ParticleKey *state, EffectedPoint *point)
{
	ParticleSettings *part = sim->psys->part;
	point->loc = state->co;
	point->vel = state->vel;
	point->index = pa - sim->psys->particles;
	point->size = pa->size;
	point->charge = 0.0f;
	
	if (part->pd && part->pd->forcefield == PFIELD_CHARGE)
		point->charge += part->pd->f_strength;

	if (part->pd2 && part->pd2->forcefield == PFIELD_CHARGE)
		point->charge += part->pd2->f_strength;

	point->vel_to_sec = 1.0f;
	point->vel_to_frame = psys_get_timestep(sim);

	point->flag = 0;

	if (sim->psys->part->flag & PART_ROT_DYN) {
		point->ave = state->ave;
		point->rot = state->rot;
	}
	else
		point->ave = point->rot = NULL;

	point->psys = sim->psys;
}

void pd_point_from_loc(Scene *scene, float *loc, float *vel, int index, EffectedPoint *point)
{
	point->loc = loc;
	point->vel = vel;
	point->index = index;
	point->size = 0.0f;

	point->vel_to_sec = (float)scene->r.frs_sec;
	point->vel_to_frame = 1.0f;

	point->flag = 0;

	point->ave = point->rot = NULL;
	point->psys = NULL;
}
void pd_point_from_soft(Scene *scene, float *loc, float *vel, int index, EffectedPoint *point)
{
	point->loc = loc;
	point->vel = vel;
	point->index = index;
	point->size = 0.0f;

	point->vel_to_sec = (float)scene->r.frs_sec;
	point->vel_to_frame = 1.0f;

	point->flag = PE_WIND_AS_SPEED;

	point->ave = point->rot = NULL;

	point->psys = NULL;
}
/************************************************/
/*			Effectors		*/
/************************************************/

// triangle - ray callback function
static void eff_tri_ray_hit(void *UNUSED(userData), int UNUSED(index), const BVHTreeRay *UNUSED(ray), BVHTreeRayHit *hit)
{	
	/* whenever we hit a bounding box, we don't check further */
	hit->dist = -1;
	hit->index = 1;
}

// get visibility of a wind ray
static float eff_calc_visibility(ListBase *colliders, EffectorCache *eff, EffectorData *efd, EffectedPoint *point)
{
	const int raycast_flag = BVH_RAYCAST_DEFAULT & ~(BVH_RAYCAST_WATERTIGHT);
	ListBase *colls = colliders;
	ColliderCache *col;
	float norm[3], len = 0.0;
	float visibility = 1.0, absorption = 0.0;
	
	if (!(eff->pd->flag & PFIELD_VISIBILITY))
		return visibility;

	if (!colls)
		colls = get_collider_cache(eff->scene, eff->ob, NULL);

	if (!colls)
		return visibility;

	negate_v3_v3(norm, efd->vec_to_point);
	len = normalize_v3(norm);
	
	/* check all collision objects */
	for (col = colls->first; col; col = col->next) {
		CollisionModifierData *collmd = col->collmd;

		if (col->ob == eff->ob)
			continue;

		if (collmd->bvhtree) {
			BVHTreeRayHit hit;

			hit.index = -1;
			hit.dist = len + FLT_EPSILON;

			/* check if the way is blocked */
			if (BLI_bvhtree_ray_cast_ex(
			        collmd->bvhtree, point->loc, norm, 0.0f, &hit,
			        eff_tri_ray_hit, NULL, raycast_flag) != -1)
			{
				absorption= col->ob->pd->absorption;

				/* visibility is only between 0 and 1, calculated from 1-absorption */
				visibility *= CLAMPIS(1.0f-absorption, 0.0f, 1.0f);
				
				if (visibility <= 0.0f)
					break;
			}
		}
	}

	if (!colliders)
		free_collider_cache(&colls);
	
	return visibility;
}

// noise function for wind e.g.
static float wind_func(struct RNG *rng, float strength)
{
	int random = (BLI_rng_get_int(rng)+1) % 128; // max 2357
	float force = BLI_rng_get_float(rng) + 1.0f;
	float ret;
	float sign = 0;
	
	sign = ((float)random > 64.0f) ? 1.0f: -1.0f; // dividing by 2 is not giving equal sign distribution
	
	ret = sign*((float)random / force)*strength/128.0f;
	
	return ret;
}

/* maxdist: zero effect from this distance outwards (if usemax) */
/* mindist: full effect up to this distance (if usemin) */
/* power: falloff with formula 1/r^power */
static float falloff_func(float fac, int usemin, float mindist, int usemax, float maxdist, float power)
{
	/* first quick checks */
	if (usemax && fac > maxdist)
		return 0.0f;

	if (usemin && fac < mindist)
		return 1.0f;

	if (!usemin)
		mindist = 0.0;

	return pow((double)(1.0f+fac-mindist), (double)(-power));
}

static float falloff_func_dist(PartDeflect *pd, float fac)
{
	return falloff_func(fac, pd->flag&PFIELD_USEMIN, pd->mindist, pd->flag&PFIELD_USEMAX, pd->maxdist, pd->f_power);
}

static float falloff_func_rad(PartDeflect *pd, float fac)
{
	return falloff_func(fac, pd->flag&PFIELD_USEMINR, pd->minrad, pd->flag&PFIELD_USEMAXR, pd->maxrad, pd->f_power_r);
}

float effector_falloff(EffectorCache *eff, EffectorData *efd, EffectedPoint *UNUSED(point), EffectorWeights *weights)
{
	float temp[3];
	float falloff = weights ? weights->weight[0] * weights->weight[eff->pd->forcefield] : 1.0f;
	float fac, r_fac;

	fac = dot_v3v3(efd->nor, efd->vec_to_point2);

	if (eff->pd->zdir == PFIELD_Z_POS && fac < 0.0f)
		falloff=0.0f;
	else if (eff->pd->zdir == PFIELD_Z_NEG && fac > 0.0f)
		falloff=0.0f;
	else {
		switch (eff->pd->falloff) {
		case PFIELD_FALL_SPHERE:
			falloff*= falloff_func_dist(eff->pd, efd->distance);
			break;

		case PFIELD_FALL_TUBE:
			falloff*= falloff_func_dist(eff->pd, ABS(fac));
			if (falloff == 0.0f)
				break;

			madd_v3_v3v3fl(temp, efd->vec_to_point2, efd->nor, -fac);
			r_fac= len_v3(temp);
			falloff*= falloff_func_rad(eff->pd, r_fac);
			break;
		case PFIELD_FALL_CONE:
			falloff*= falloff_func_dist(eff->pd, ABS(fac));
			if (falloff == 0.0f)
				break;

			r_fac= RAD2DEGF(saacos(fac/len_v3(efd->vec_to_point)));
			falloff*= falloff_func_rad(eff->pd, r_fac);

			break;
		}
	}

	return falloff;
}

int closest_point_on_surface(SurfaceModifierData *surmd, const float co[3], float surface_co[3], float surface_nor[3], float surface_vel[3])
{
	BVHTreeNearest nearest;

	nearest.index = -1;
	nearest.dist_sq = FLT_MAX;

	BLI_bvhtree_find_nearest(surmd->bvhtree->tree, co, &nearest, surmd->bvhtree->nearest_callback, surmd->bvhtree);

	if (nearest.index != -1) {
		copy_v3_v3(surface_co, nearest.co);

		if (surface_nor) {
			copy_v3_v3(surface_nor, nearest.no);
		}

		if (surface_vel) {
			const MLoop *mloop = surmd->bvhtree->loop;
			const MLoopTri *lt = &surmd->bvhtree->looptri[nearest.index];
			
			copy_v3_v3(surface_vel, surmd->v[mloop[lt->tri[0]].v].co);
			add_v3_v3(surface_vel, surmd->v[mloop[lt->tri[1]].v].co);
			add_v3_v3(surface_vel, surmd->v[mloop[lt->tri[2]].v].co);

			mul_v3_fl(surface_vel, (1.0f / 3.0f));
		}
		return 1;
	}

	return 0;
}
int get_effector_data(EffectorCache *eff, EffectorData *efd, EffectedPoint *point, int real_velocity)
{
	float cfra = eff->scene->r.cfra;
	int ret = 0;

	if (eff->pd && eff->pd->shape==PFIELD_SHAPE_SURFACE && eff->surmd) {
		/* closest point in the object surface is an effector */
		float vec[3];

		/* using velocity corrected location allows for easier sliding over effector surface */
		copy_v3_v3(vec, point->vel);
		mul_v3_fl(vec, point->vel_to_frame);
		add_v3_v3(vec, point->loc);

		ret = closest_point_on_surface(eff->surmd, vec, efd->loc, efd->nor, real_velocity ? efd->vel : NULL);

		efd->size = 0.0f;
	}
	else if (eff->pd && eff->pd->shape==PFIELD_SHAPE_POINTS) {

		if (eff->ob->derivedFinal) {
			DerivedMesh *dm = eff->ob->derivedFinal;

			dm->getVertCo(dm, *efd->index, efd->loc);
			dm->getVertNo(dm, *efd->index, efd->nor);

			mul_m4_v3(eff->ob->obmat, efd->loc);
			mul_mat3_m4_v3(eff->ob->obmat, efd->nor);

			normalize_v3(efd->nor);

			efd->size = 0.0f;

			/**/
			ret = 1;
		}
	}
	else if (eff->psys) {
		ParticleData *pa = eff->psys->particles + *efd->index;
		ParticleKey state;

		/* exclude the particle itself for self effecting particles */
		if (eff->psys == point->psys && *efd->index == point->index) {
			/* pass */
		}
		else {
			ParticleSimulationData sim= {NULL};
			sim.scene= eff->scene;
			sim.ob= eff->ob;
			sim.psys= eff->psys;

			/* TODO: time from actual previous calculated frame (step might not be 1) */
			state.time = cfra - 1.0f;
			ret = psys_get_particle_state(&sim, *efd->index, &state, 0);

			/* TODO */
			//if (eff->pd->forcefiled == PFIELD_HARMONIC && ret==0) {
			//	if (pa->dietime < eff->psys->cfra)
			//		eff->flag |= PE_VELOCITY_TO_IMPULSE;
			//}

			copy_v3_v3(efd->loc, state.co);

			/* rather than use the velocity use rotated x-axis (defaults to velocity) */
			efd->nor[0] = 1.f;
			efd->nor[1] = efd->nor[2] = 0.f;
			mul_qt_v3(state.rot, efd->nor);
		
			if (real_velocity)
				copy_v3_v3(efd->vel, state.vel);

			efd->size = pa->size;
		}
	}
	else {
		/* use center of object for distance calculus */
		const Object *ob = eff->ob;

		/* use z-axis as normal*/
		normalize_v3_v3(efd->nor, ob->obmat[2]);

		if (eff->pd && eff->pd->shape == PFIELD_SHAPE_PLANE) {
			float temp[3], translate[3];
			sub_v3_v3v3(temp, point->loc, ob->obmat[3]);
			project_v3_v3v3(translate, temp, efd->nor);

			/* for vortex the shape chooses between old / new force */
			if (eff->pd->forcefield == PFIELD_VORTEX)
				add_v3_v3v3(efd->loc, ob->obmat[3], translate);
			else /* normally efd->loc is closest point on effector xy-plane */
				sub_v3_v3v3(efd->loc, point->loc, translate);
		}
		else {
			copy_v3_v3(efd->loc, ob->obmat[3]);
		}

		if (real_velocity)
			copy_v3_v3(efd->vel, eff->velocity);

		efd->size = 0.0f;

		ret = 1;
	}

	if (ret) {
		sub_v3_v3v3(efd->vec_to_point, point->loc, efd->loc);
		efd->distance = len_v3(efd->vec_to_point);

		/* rest length for harmonic effector, will have to see later if this could be extended to other effectors */
		if (eff->pd && eff->pd->forcefield == PFIELD_HARMONIC && eff->pd->f_size)
			mul_v3_fl(efd->vec_to_point, (efd->distance-eff->pd->f_size)/efd->distance);

		if (eff->flag & PE_USE_NORMAL_DATA) {
			copy_v3_v3(efd->vec_to_point2, efd->vec_to_point);
			copy_v3_v3(efd->nor2, efd->nor);
		}
		else {
			/* for some effectors we need the object center every time */
			sub_v3_v3v3(efd->vec_to_point2, point->loc, eff->ob->obmat[3]);
			normalize_v3_v3(efd->nor2, eff->ob->obmat[2]);
		}
	}

	return ret;
}
static void get_effector_tot(EffectorCache *eff, EffectorData *efd, EffectedPoint *point, int *tot, int *p, int *step)
{
	*p = 0;
	efd->index = p;

	if (eff->pd->shape == PFIELD_SHAPE_POINTS) {
		*tot = eff->ob->derivedFinal ? eff->ob->derivedFinal->numVertData : 1;

		if (*tot && eff->pd->forcefield == PFIELD_HARMONIC && point->index >= 0) {
			*p = point->index % *tot;
			*tot = *p+1;
		}
	}
	else if (eff->psys) {
		*tot = eff->psys->totpart;
		
		if (eff->pd->forcefield == PFIELD_CHARGE) {
			/* Only the charge of the effected particle is used for 
			 * interaction, not fall-offs. If the fall-offs aren't the
			 * same this will be unphysical, but for animation this
			 * could be the wanted behavior. If you want physical
			 * correctness the fall-off should be spherical 2.0 anyways.
			 */
			efd->charge = eff->pd->f_strength;
		}
		else if (eff->pd->forcefield == PFIELD_HARMONIC && (eff->pd->flag & PFIELD_MULTIPLE_SPRINGS)==0) {
			/* every particle is mapped to only one harmonic effector particle */
			*p= point->index % eff->psys->totpart;
			*tot= *p + 1;
		}

		if (eff->psys->part->effector_amount) {
			int totpart = eff->psys->totpart;
			int amount = eff->psys->part->effector_amount;

			*step = (totpart > amount) ? totpart/amount : 1;
		}
	}
	else {
		*tot = 1;
	}
}
static void do_texture_effector(EffectorCache *eff, EffectorData *efd, EffectedPoint *point, float *total_force)
{
	TexResult result[4];
	float tex_co[3], strength, force[3];
	float nabla = eff->pd->tex_nabla;
	int hasrgb;
	short mode = eff->pd->tex_mode;
	bool scene_color_manage;

	if (!eff->pd->tex)
		return;

	result[0].nor = result[1].nor = result[2].nor = result[3].nor = NULL;

	strength= eff->pd->f_strength * efd->falloff;

	copy_v3_v3(tex_co, point->loc);

	if (eff->pd->flag & PFIELD_TEX_2D) {
		float fac=-dot_v3v3(tex_co, efd->nor);
		madd_v3_v3fl(tex_co, efd->nor, fac);
	}

	if (eff->pd->flag & PFIELD_TEX_OBJECT) {
		mul_m4_v3(eff->ob->imat, tex_co);
	}

	scene_color_manage = BKE_scene_check_color_management_enabled(eff->scene);

	hasrgb = multitex_ext(eff->pd->tex, tex_co, NULL, NULL, 0, result, NULL, scene_color_manage, false);

	if (hasrgb && mode==PFIELD_TEX_RGB) {
		force[0] = (0.5f - result->tr) * strength;
		force[1] = (0.5f - result->tg) * strength;
		force[2] = (0.5f - result->tb) * strength;
	}
	else {
		strength/=nabla;

		tex_co[0] += nabla;
		multitex_ext(eff->pd->tex, tex_co, NULL, NULL, 0, result+1, NULL, scene_color_manage, false);

		tex_co[0] -= nabla;
		tex_co[1] += nabla;
		multitex_ext(eff->pd->tex, tex_co, NULL, NULL, 0, result+2, NULL, scene_color_manage, false);

		tex_co[1] -= nabla;
		tex_co[2] += nabla;
		multitex_ext(eff->pd->tex, tex_co, NULL, NULL, 0, result+3, NULL, scene_color_manage, false);

		if (mode == PFIELD_TEX_GRAD || !hasrgb) { /* if we don't have rgb fall back to grad */
			/* generate intensity if texture only has rgb value */
			if (hasrgb & TEX_RGB) {
				int i;
				for (i=0; i<4; i++)
					result[i].tin = (1.0f / 3.0f) * (result[i].tr + result[i].tg + result[i].tb);
			}
			force[0] = (result[0].tin - result[1].tin) * strength;
			force[1] = (result[0].tin - result[2].tin) * strength;
			force[2] = (result[0].tin - result[3].tin) * strength;
		}
		else { /*PFIELD_TEX_CURL*/
			float dbdy, dgdz, drdz, dbdx, dgdx, drdy;

			dbdy = result[2].tb - result[0].tb;
			dgdz = result[3].tg - result[0].tg;
			drdz = result[3].tr - result[0].tr;
			dbdx = result[1].tb - result[0].tb;
			dgdx = result[1].tg - result[0].tg;
			drdy = result[2].tr - result[0].tr;

			force[0] = (dbdy - dgdz) * strength;
			force[1] = (drdz - dbdx) * strength;
			force[2] = (dgdx - drdy) * strength;
		}
	}

	if (eff->pd->flag & PFIELD_TEX_2D) {
		float fac = -dot_v3v3(force, efd->nor);
		madd_v3_v3fl(force, efd->nor, fac);
	}

	add_v3_v3(total_force, force);
}
static void do_physical_effector(EffectorCache *eff, EffectorData *efd, EffectedPoint *point, float *total_force)
{
	PartDeflect *pd = eff->pd;
	RNG *rng = pd->rng;
	float force[3] = {0, 0, 0};
	float temp[3];
	float fac;
	float strength = pd->f_strength;
	float damp = pd->f_damp;
	float noise_factor = pd->f_noise;

	if (noise_factor > 0.0f) {
		strength += wind_func(rng, noise_factor);

		if (ELEM(pd->forcefield, PFIELD_HARMONIC, PFIELD_DRAG))
			damp += wind_func(rng, noise_factor);
	}

	copy_v3_v3(force, efd->vec_to_point);

	switch (pd->forcefield) {
		case PFIELD_WIND:
			copy_v3_v3(force, efd->nor);
			mul_v3_fl(force, strength * efd->falloff);
			break;
		case PFIELD_FORCE:
			normalize_v3(force);
			mul_v3_fl(force, strength * efd->falloff);
			break;
		case PFIELD_VORTEX:
			/* old vortex force */
			if (pd->shape == PFIELD_SHAPE_POINT) {
				cross_v3_v3v3(force, efd->nor, efd->vec_to_point);
				normalize_v3(force);
				mul_v3_fl(force, strength * efd->distance * efd->falloff);
			}
			else {
				/* new vortex force */
				cross_v3_v3v3(temp, efd->nor2, efd->vec_to_point2);
				mul_v3_fl(temp, strength * efd->falloff);
				
				cross_v3_v3v3(force, efd->nor2, temp);
				mul_v3_fl(force, strength * efd->falloff);
				
				madd_v3_v3fl(temp, point->vel, -point->vel_to_sec);
				add_v3_v3(force, temp);
			}
			break;
		case PFIELD_MAGNET:
			if (eff->pd->shape == PFIELD_SHAPE_POINT)
				/* magnetic field of a moving charge */
				cross_v3_v3v3(temp, efd->nor, efd->vec_to_point);
			else
				copy_v3_v3(temp, efd->nor);

			normalize_v3(temp);
			mul_v3_fl(temp, strength * efd->falloff);
			cross_v3_v3v3(force, point->vel, temp);
			mul_v3_fl(force, point->vel_to_sec);
			break;
		case PFIELD_HARMONIC:
			mul_v3_fl(force, -strength * efd->falloff);
			copy_v3_v3(temp, point->vel);
			mul_v3_fl(temp, -damp * 2.0f * sqrtf(fabsf(strength)) * point->vel_to_sec);
			add_v3_v3(force, temp);
			break;
		case PFIELD_CHARGE:
			mul_v3_fl(force, point->charge * strength * efd->falloff);
			break;
		case PFIELD_LENNARDJ:
			fac = pow((efd->size + point->size) / efd->distance, 6.0);
			
			fac = - fac * (1.0f - fac) / efd->distance;

			/* limit the repulsive term drastically to avoid huge forces */
			fac = ((fac>2.0f) ? 2.0f : fac);

			mul_v3_fl(force, strength * fac);
			break;
		case PFIELD_BOID:
			/* Boid field is handled completely in boids code. */
			return;
		case PFIELD_TURBULENCE:
			if (pd->flag & PFIELD_GLOBAL_CO) {
				copy_v3_v3(temp, point->loc);
			}
			else {
				add_v3_v3v3(temp, efd->vec_to_point2, efd->nor2);
			}
			force[0] = -1.0f + 2.0f * BLI_gTurbulence(pd->f_size, temp[0], temp[1], temp[2], 2, 0, 2);
			force[1] = -1.0f + 2.0f * BLI_gTurbulence(pd->f_size, temp[1], temp[2], temp[0], 2, 0, 2);
			force[2] = -1.0f + 2.0f * BLI_gTurbulence(pd->f_size, temp[2], temp[0], temp[1], 2, 0, 2);
			mul_v3_fl(force, strength * efd->falloff);
			break;
		case PFIELD_DRAG:
			copy_v3_v3(force, point->vel);
			fac = normalize_v3(force) * point->vel_to_sec;

			strength = MIN2(strength, 2.0f);
			damp = MIN2(damp, 2.0f);

			mul_v3_fl(force, -efd->falloff * fac * (strength * fac + damp));
			break;
		case PFIELD_SMOKEFLOW:
			zero_v3(force);
			if (pd->f_source) {
				float density;
				if ((density = smoke_get_velocity_at(pd->f_source, point->loc, force)) >= 0.0f) {
					float influence = strength * efd->falloff;
					if (pd->flag & PFIELD_SMOKE_DENSITY)
						influence *= density;
					mul_v3_fl(force, influence);
					/* apply flow */
					madd_v3_v3fl(total_force, point->vel, -pd->f_flow * influence);
				}
			}
			break;

	}

	if (pd->flag & PFIELD_DO_LOCATION) {
		madd_v3_v3fl(total_force, force, 1.0f/point->vel_to_sec);

		if (ELEM(pd->forcefield, PFIELD_HARMONIC, PFIELD_DRAG, PFIELD_SMOKEFLOW)==0 && pd->f_flow != 0.0f) {
			madd_v3_v3fl(total_force, point->vel, -pd->f_flow * efd->falloff);
		}
	}

	if (point->ave)
		zero_v3(point->ave);
	if (pd->flag & PFIELD_DO_ROTATION && point->ave && point->rot) {
		float xvec[3] = {1.0f, 0.0f, 0.0f};
		float dave[3];
		mul_qt_v3(point->rot, xvec);
		cross_v3_v3v3(dave, xvec, force);
		if (pd->f_flow != 0.0f) {
			madd_v3_v3fl(dave, point->ave, -pd->f_flow * efd->falloff);
		}
		add_v3_v3(point->ave, dave);
	}
}

/*  -------- pdDoEffectors() --------
 * generic force/speed system, now used for particles and softbodies
 * effctx      = compiled effector setup
 * scene       = scene where it runs in, for time and stuff
 * opco		= global coord, as input
 * force		= force accumulator
 * speed		= actual current speed which can be altered
 * cur_time	= "external" time in frames, is constant for static particles
 * loc_time	= "local" time in frames, range <0-1> for the lifetime of particle
 * par_layer	= layer the caller is in
 * flags		= only used for softbody wind now
 * guide		= old speed of particle
 */
void pdDoEffectors(struct EffectorContext *effctx, ListBase *colliders, EffectorWeights *weights, EffectedPoint *point, float *force, float *impulse)
{
/*
 * Modifies the force on a particle according to its
 * relation with the effector object
 * Different kind of effectors include:
 *     Forcefields: Gravity-like attractor
 *     (force power is related to the inverse of distance to the power of a falloff value)
 *     Vortex fields: swirling effectors
 *     (particles rotate around Z-axis of the object. otherwise, same relation as)
 *     (Forcefields, but this is not done through a force/acceleration)
 *     Guide: particles on a path
 *     (particles are guided along a curve bezier or old nurbs)
 *     (is independent of other effectors)
 */
	EffectorCache *eff;
	EffectorData efd;
	int p=0, tot = 1, step = 1;

	/* Cycle through collected objects, get total of (1/(gravity_strength * dist^gravity_power)) */
	/* Check for min distance here? (yes would be cool to add that, ton) */
	
	if (effctx) for (eff = effctx->effectors.first; eff; eff=eff->next) {
		/* object effectors were fully checked to be OK to evaluate! */

		get_effector_tot(eff, &efd, point, &tot, &p, &step);

		for (; p<tot; p+=step) {
			if (get_effector_data(eff, &efd, point, 0)) {
				efd.falloff= effector_falloff(eff, &efd, point, weights);
				
				if (efd.falloff > 0.0f)
					efd.falloff *= eff_calc_visibility(colliders, eff, &efd, point);

				if (efd.falloff <= 0.0f) {
					/* don't do anything */
				}
				else if (eff->pd->forcefield == PFIELD_TEXTURE) {
					do_texture_effector(eff, &efd, point, force);
				}
				else {
					float temp1[3] = {0, 0, 0}, temp2[3];
					copy_v3_v3(temp1, force);

					do_physical_effector(eff, &efd, point, force);
					
					/* for softbody backward compatibility */
					if (point->flag & PE_WIND_AS_SPEED && impulse) {
						sub_v3_v3v3(temp2, force, temp1);
						sub_v3_v3v3(impulse, impulse, temp2);
					}
				}
			}
			else if (eff->flag & PE_VELOCITY_TO_IMPULSE && impulse) {
				/* special case for harmonic effector */
				add_v3_v3v3(impulse, impulse, efd.vel);
			}
		}
	}
}

/* ======== JIT-compiled Effectors ======== */

EffectorContext *pdInitJITEffectors(Scene *scene, Object *ob_src, ParticleSystem *psys_src,
                                    EffectorWeights *weights, bool precalc)
{
	EffectorContext *effctx = pdInitEffectors(scene, ob_src, psys_src, weights, precalc);
	
	BJIT_build_effector_function(effctx);
	
	return effctx;
}

void pdEndJITEffectors(EffectorContext *effctx)
{
	BJIT_free_effector_function(effctx);
	
	pdEndEffectors(effctx);
}

void pdDoJITEffectors(struct EffectorContext *effctx, ListBase *UNUSED(colliders), EffectorWeights *weights,
                      EffectedPoint *point, float *force, float *impulse)
{
	BJIT_effector_eval(effctx, weights, point, force, impulse);
}


/* ======== Force Field Visualization ======== */

typedef void (*ForceVizFieldEvalFunc)(float R[3], float t, const float co[3], void *calldata);

typedef struct ForceVizInput {
	float co[3];
	float nor[3];
	float vel[3];
	
	float force[3];
	float dforce[3][3];
	float impulse[3];
} ForceVizInput;

typedef float (*ForceVizScalarFunc)(ForceVizModifierData *fmd, const ForceVizInput *input);
typedef void (*ForceVizVectorFunc)(float res[3], ForceVizModifierData *fmd, const ForceVizInput *input);
typedef void (*ForceVizColorFunc)(float res[4], ForceVizModifierData *fmd, const ForceVizInput *input);

static void forceviz_eval_field(ForceVizModifierData *fmd, EffectorContext *effectors,
                                const float loc[3], const float nor[3], const float vel[3],
                                ForceVizInput *result)
{
	EffectedPoint point;
	
	copy_v3_v3(result->co, loc);
	copy_v3_v3(result->nor, nor);
	copy_v3_v3(result->vel, vel);
	pd_point_from_loc(fmd->modifier.scene, (float *)loc, (float *)vel, 0, &point);
	
	zero_v3(result->force);
	zero_v3(result->impulse);
	zero_m3(result->dforce); // TODO
	pdDoEffectors(effectors, NULL, fmd->effector_weights, &point, result->force, result->impulse);
}

BLI_INLINE void forceviz_eval_field_loc(ForceVizModifierData *fmd, EffectorContext *effectors,
                                    const float loc[3],
	                                ForceVizInput *result)
{
	static const float vel[3] = {0.0f, 0.0f, 0.0f};
	static const float nor[3] = {0.0f, 0.0f, 1.0f};
	forceviz_eval_field(fmd, effectors, loc, nor, vel, result);
}

BLI_INLINE void forceviz_eval_field_loc_nor(ForceVizModifierData *fmd, EffectorContext *effectors,
                                    const float loc[3], const float nor[3],
	                                ForceVizInput *result)
{
	static const float vel[3] = {0.0f, 0.0f, 0.0f};
	forceviz_eval_field(fmd, effectors, loc, nor, vel, result);
}

/* ------------------------------------------------------------------------- */

static BMVert *forceviz_create_vertex(BMesh *bm, int cd_strength_layer, const float loc[3], const float offset[3], float size, const float strength[3])
{
	BMVert *vert;
	float co[3];
	MFloat3Property *s;
	
	madd_v3_v3v3fl(co, loc, offset, size);
	vert = BM_vert_create(bm, co, NULL, BM_CREATE_NOP);
	
	s = CustomData_bmesh_get_layer_n(&bm->vdata, vert->head.data, cd_strength_layer);
	if (s)
		copy_v3_v3(s->f, strength);
	
	return vert;
}

static BMEdge *forceviz_create_edge(BMesh *bm, BMVert *v1, BMVert *v2)
{
	BMEdge *edge = BM_edge_create(bm, v1, v2, NULL, BM_CREATE_NOP);
	
	sub_v3_v3v3(v2->no, v2->co, v1->co);
	normalize_v3(v2->no);
	
	return edge;
}

static BMFace *forceviz_create_face(BMesh *bm, int cd_loopuv_layer, int mat,
                                    BMVert *v1, BMVert *v2, BMVert *v3, BMVert *v4,
                                    float u0, float u1)
{
	BMFace *face;
	
	face = BM_face_create_quad_tri(bm, v1, v2, v3, v4, NULL, BM_CREATE_NOP);
	BM_elem_flag_set(face, BM_ELEM_SMOOTH, true);
	face->mat_nr = (short)mat;
	
	if (cd_loopuv_layer >= 0) {
		BMLoop *loop;
		BMIter iter;
		BM_ITER_ELEM(loop, &iter, face, BM_LOOPS_OF_FACE) {
			MLoopUV *uv = CustomData_bmesh_get_layer_n(&bm->ldata, loop->head.data, cd_loopuv_layer);
			if (loop->v == v1) {
				uv->uv[0] = u0;
				uv->uv[1] = 0.0f;
			}
			else if (loop->v == v2) {
				uv->uv[0] = u0;
				uv->uv[1] = 1.0f;
			}
			else if (loop->v == v3) {
				uv->uv[0] = u1;
				uv->uv[1] = 1.0f;
			}
			else if (loop->v == v4) {
				uv->uv[0] = u1;
				uv->uv[1] = 0.0f;
			}
			else
				BLI_assert(false);
		}
	}
	
	return face;
}

typedef struct ForceVizLine {
	float loc_prev[3];
	BMVert *vert_prev;
	float strength_prev[3];
	int index;
} ForceVizLine;

static void forceviz_line_add(ForceVizModifierData *fmd, BMesh *bm, ForceVizLine *line,
                              const float loc[3], const float strength[3])
{
	/* create vertex */
	const int cd_strength_layer = CustomData_get_named_layer_index(&bm->vdata, CD_PROP_FLT3, fmd->fieldlines.strength_layer);
	BMVert *vert_prev = line->vert_prev;
	static const float offset[3] = {0.0f, 0.0f, 0.0f};
	const int index = line->index;
	BMVert *vert;
	
	vert = forceviz_create_vertex(bm, cd_strength_layer, loc, offset, 0.0f, strength);
	
	/* create edge */
	if (index > 0) {
		forceviz_create_edge(bm, vert_prev, vert);
		
		if (index == 1) {
			copy_v3_v3(vert_prev->no, vert->no);
		}
	}
	
	line->index += 1;
	line->vert_prev = vert;
	copy_v3_v3(line->loc_prev, loc);
	copy_v3_v3(line->strength_prev, strength);
}

typedef struct ForceVizRibbon {
	float loc_prev[3];
	BMVert *verts_prev[2];
	float strength_prev[3];
	float length_prev;
	int index;
} ForceVizRibbon;

static void forceviz_ribbon_add(ForceVizModifierData *fmd, BMesh *bm, ForceVizRibbon *ribbon,
								float size, const float view_target[3], int mat,
								const float loc[3], float length, const float strength[3])
{
	const int cd_loopuv_layer = CustomData_get_active_layer_index(&bm->ldata, CD_MLOOPUV);
	const int cd_strength_layer = CustomData_get_named_layer_index(&bm->vdata, CD_PROP_FLT3, fmd->fieldlines.strength_layer);
	BMVert **verts_prev = ribbon->verts_prev;
	const float *loc_prev = ribbon->loc_prev;
	BMVert *verts[2] = {0};
	int index = ribbon->index;
	
	if (index > 0) {
		float edge[3], dir[3], view[3], offset[2][3];
		float co[3];
		
		sub_v3_v3v3(edge, loc, loc_prev);
		normalize_v3_v3(dir, edge);
		
		sub_v3_v3v3(view, view_target, loc);
		normalize_v3(view);
		
		cross_v3_v3v3(offset[0], dir, view);
		normalize_v3(offset[0]);
		negate_v3_v3(offset[1], offset[0]);
		
		if (index == 1) {
			/* create first vertex pair */
			verts_prev[0] = forceviz_create_vertex(bm, cd_strength_layer, loc_prev, offset[0], size * 0.5f, ribbon->strength_prev);
			verts_prev[1] = forceviz_create_vertex(bm, cd_strength_layer, loc_prev, offset[1], size * 0.5f, ribbon->strength_prev);
		}
		else {
			/* average orientation of previous segment */
			madd_v3_v3v3fl(co, loc_prev, offset[0], size * 0.5f);
			add_v3_v3(verts_prev[0]->co, co);
			mul_v3_fl(verts_prev[0]->co, 0.5f);
			
			madd_v3_v3v3fl(co, loc_prev, offset[1], size * 0.5f);
			add_v3_v3(verts_prev[1]->co, co);
			mul_v3_fl(verts_prev[1]->co, 0.5f);
		}
		
		/* create new vertex pair */
		verts[0] = forceviz_create_vertex(bm, cd_strength_layer, loc, offset[0], size * 0.5f, strength);
		verts[1] = forceviz_create_vertex(bm, cd_strength_layer, loc, offset[1], size * 0.5f, strength);
		
		/* create a quad */
		forceviz_create_face(bm, cd_loopuv_layer, mat,
		                     verts_prev[1], verts_prev[0], verts[0], verts[1],
		                     ribbon->length_prev, length);
	}
	
	ribbon->index += 1;
	ribbon->verts_prev[0] = verts[0];
	ribbon->verts_prev[1] = verts[1];
	copy_v3_v3(ribbon->loc_prev, loc);
	ribbon->length_prev = length;
	copy_v3_v3(ribbon->strength_prev, strength);
}

typedef struct ForceVizTube {
	float loc_prev[3], dir_prev[3];
	float size_prev;
	float strength_prev[3];
	float length_prev;
	BMVert **verts_prev;
	int index;
	float (*ring)[3];
	
	/* temporary arrays
	 * no data is stored here, but keeping them avoid realloc */
	BMVert **verts; 
} ForceVizTube;

static void forceviz_tube_init(ForceVizTube *tube, int numradial)
{
	float dalpha = 2.0f*M_PI / (float)numradial;
	int k;
	
	tube->verts = MEM_callocN(sizeof(MVert *) * numradial, "forceviz tube verts");
	tube->verts_prev = MEM_callocN(sizeof(MVert *) * numradial, "forceviz tube verts_prev");
	tube->ring = MEM_mallocN(sizeof(float) * 3 * numradial, "forceviz tube ring");
	
	for (k = 0; k < numradial; ++k) {
		float alpha = dalpha * (float)k;
		tube->ring[k][0] = sin(alpha);
		tube->ring[k][1] = cos(alpha);
		tube->ring[k][2] = 0.0f;
	}
	tube->dir_prev[0] = 0.0f;
	tube->dir_prev[1] = 0.0f;
	tube->dir_prev[2] = 1.0f;
}

static void forceviz_tube_clear(ForceVizTube *tube)
{
	if (tube->ring)
		MEM_freeN(tube->ring);
	if (tube->verts)
		MEM_freeN(tube->verts);
	if (tube->verts_prev)
		MEM_freeN(tube->verts_prev);
}

static void forceviz_tube_add(ForceVizModifierData *fmd, BMesh *bm, ForceVizTube *tube,
                              int numradial, float size, int mat,
                              const float loc[3], float length, const float strength[3])
{
	const int cd_loopuv_layer = CustomData_get_active_layer_index(&bm->ldata, CD_MLOOPUV);
	const int cd_strength_layer = CustomData_get_named_layer_index(&bm->vdata, CD_PROP_FLT3, fmd->fieldlines.strength_layer);
	
	BMVert **verts_prev = tube->verts_prev;
	const float *loc_prev = tube->loc_prev;
	float (*ring)[3] = tube->ring;
	BMVert **verts = tube->verts;
	int index = tube->index;
	float dir[3];
	int k;
	
	if (index > 0) {
		float edge[3];
		float rot[4], rot2[4];
		
		sub_v3_v3v3(edge, loc, loc_prev);
		normalize_v3_v3(dir, edge);
		
		rotation_between_vecs_to_quat(rot2, tube->dir_prev, dir);
		copy_qt_qt(rot, rot2);
		mul_fac_qt_fl(rot, 0.5f);
		
		if (index == 1) {
			/* create first vertex ring */
			for (k = 0; k < numradial; ++k) {
				mul_qt_v3(rot2, ring[k]);
				verts_prev[k] = forceviz_create_vertex(bm, cd_strength_layer, loc_prev, ring[k], tube->size_prev * 0.5f, tube->strength_prev);
			}
		}
		else {
			/* set orientation of previous segment */
			for (k = 0; k < numradial; ++k) {
				mul_qt_v3(rot, ring[k]);
				copy_v3_v3(verts_prev[k]->co, loc_prev);
				madd_v3_v3fl(verts_prev[k]->co, ring[k], tube->size_prev * 0.5f);
				
				/* orientation for the current segment */
				mul_qt_v3(rot, ring[k]);
			}
		}
		
		/* create new vertex ring */
		for (k = 0; k < numradial; ++k) {
			verts[k] = forceviz_create_vertex(bm, cd_strength_layer, loc, ring[k], size * 0.5f, strength);
		}
		
		/* create quads */
		for (k = 0; k < numradial; ++k) {
			forceviz_create_face(bm, cd_loopuv_layer, mat,
			                     verts_prev[(k+1) % numradial], verts_prev[k], verts[k], verts[(k+1) % numradial],
			                     tube->length_prev, length);
		}
	}
	else {
		copy_v3_v3(dir, tube->dir_prev);
	}
	
	tube->index += 1;
	for (k = 0; k < numradial; ++k) {
		tube->verts_prev[k] = verts[k];
	}
	copy_v3_v3(tube->loc_prev, loc);
	copy_v3_v3(tube->dir_prev, dir);
	tube->size_prev = size;
	tube->length_prev = length;
	copy_v3_v3(tube->strength_prev, strength);
}

static void forceviz_integrate_rk4(float res[3], const float co1[3], float t1, float h, ForceVizFieldEvalFunc func, void *calldata)
{
	float k1[3], k2[3], k3[3], k4[3];
	float t2, t3, t4;
	float co2[3], co3[3], co4[3];
	float delta[3];
	
	func(k1, t1, co1, calldata);
	
	t2 = t1 + 0.5f*h;
	madd_v3_v3v3fl(co2, co1, k1, 0.5f*h);
	func(k2, t2, co2, calldata);
	
	t3 = t1 + 0.5f*h;
	madd_v3_v3v3fl(co3, co1, k2, 0.5f*h);
	func(k3, t3, co3, calldata);
	
	t4 = t1 + h;
	madd_v3_v3v3fl(co4, co1, k3, h);
	func(k4, t4, co4, calldata);
	
	zero_v3(delta);
	add_v3_v3(delta, k1);
	madd_v3_v3fl(delta, k2, 2.0f);
	madd_v3_v3fl(delta, k3, 2.0f);
	add_v3_v3(delta, k4);
	
	madd_v3_v3v3fl(res, co1, delta, h/6.0f);
}

typedef struct ForceVizEffectorData {
	Scene *scene;
	Object *object;
	float mat[4][4];
	float imat[4][4];
	EffectorContext *effectors;
	EffectorWeights *weights;
	bool use_blenjit;
} ForceVizEffectorData;

static void forceviz_get_field_vector(float R[3], float UNUSED(t), const float co[3], ForceVizEffectorData *data)
{
	Scene *scene = data->scene;
	PhysicsSettings *phys = &scene->physics_settings;
	EffectedPoint point;
	float loc[3], vel[3];
	float force[3], impulse[3];
	
	/* transform to world space for effectors */
	mul_v3_m4v3(loc, data->mat, co);
	zero_v3(vel);
	pd_point_from_loc(data->scene, loc, vel, 0, &point);
	
	zero_v3(force);
	zero_v3(impulse);
	if (data->use_blenjit)
		pdDoJITEffectors(data->effectors, NULL, data->weights, &point, force, impulse);
	else
		pdDoEffectors(data->effectors, NULL, data->weights, &point, force, impulse);
	
	/* add gravity */
	if (phys->flag & PHYS_GLOBAL_GRAVITY)
		madd_v3_v3fl(force, phys->gravity, data->weights->global_gravity);
	
	/* transform back to object space */
	mul_mat3_m4_v3(data->imat, force);
	
	copy_v3_v3(R, force);
}

static void forceviz_get_field_direction(float R[3], float t, const float co[3], ForceVizEffectorData *data)
{
	forceviz_get_field_vector(R, t, co, data);
	normalize_v3(R);
}

static void forceviz_integrate_field_line(ForceVizModifierData *fmd, BMesh *bm, ForceVizEffectorData *funcdata, const float start[3])
{
	Scene *scene = fmd->modifier.scene;
	const int res = fmd->fieldlines.res;
	const int substeps = fmd->fieldlines.substeps;
	const float length = fmd->fieldlines.length;
	const float inv_length = length != 0.0f ? 1.0f / length : 0.0f;
	const float segment = length / (float)(res - 1);
	const float stepsize = segment / (float)substeps;
	const int mat = CLAMPIS(fmd->fieldlines.material, 0, funcdata->object->totcol - 1);
	
	ForceVizLine line = {{0}};
	ForceVizRibbon ribbon = {{0}};
	ForceVizTube tube = {{0}};
	float target[3] = {0.0f, 0.0f, 0.0f};
	
	float t = 0.0f;
	float loc[3];
	int k;
	
	if (fmd->fieldlines.drawtype == MOD_FORCEVIZ_FIELDLINE_TUBE) {
		forceviz_tube_init(&tube, fmd->fieldlines.radial_res);
	}
	
	if (BKE_forceviz_needs_camera(fmd) && scene->camera) {
		mul_v3_m4v3(target, funcdata->imat, scene->camera->obmat[3]);
	}
	
	/* initialize loc */
	copy_v3_v3(loc, start);
	
	for (k = 0; k < res; ++k) {
		float strength[3];
		float nloc[3];
		int step;
		
		forceviz_get_field_vector(strength, t, loc, funcdata);
		
		switch (fmd->fieldlines.drawtype) {
			case MOD_FORCEVIZ_FIELDLINE_LINE:
				forceviz_line_add(fmd, bm, &line, loc, strength);
				break;
			case MOD_FORCEVIZ_FIELDLINE_RIBBON:
				forceviz_ribbon_add(fmd, bm, &ribbon, fmd->fieldlines.drawsize, target, mat,
				                    loc, t * inv_length, strength);
				break;
			case MOD_FORCEVIZ_FIELDLINE_TUBE:
				forceviz_tube_add(fmd, bm, &tube,
				                  fmd->fieldlines.radial_res, fmd->fieldlines.drawsize, mat,
				                  loc, t * inv_length, strength);
				break;
		}
		
		/* integrate to the next vertex location */
		copy_v3_v3(nloc, loc);
		for (step = 0; step < substeps; ++step) {
			forceviz_integrate_rk4(nloc, nloc, t, stepsize, (ForceVizFieldEvalFunc)forceviz_get_field_direction, funcdata);
			t += stepsize;
		}
		
		copy_v3_v3(loc, nloc);
	}
	
	forceviz_tube_clear(&tube);
}

/* XXX first variant gives fieldline density proportional to flux density,
 * but then the field lines pop in and out when fields change, which is undesirable.
 */
#if 0
static float forceviz_field_vertex_weight(DerivedMesh *UNUSED(dm), MVert *mvert, unsigned int UNUSED(index), void *userdata)
{
	ForceVizEffectorData *data = userdata;
	float R[3], nor[3];
	float weight;
	
	forceviz_get_field_vector(R, 0.0f, mvert->co, data);
	
	normal_short_to_float_v3(nor, mvert->no);
	weight = max_ff(dot_v3v3(R, nor), 0.0f);
	return weight;
}
#else
static float forceviz_field_vertex_weight(DerivedMesh *UNUSED(dm), MVert *UNUSED(mvert), unsigned int UNUSED(index), void *UNUSED(userdata))
{
	return 1.0f;
}
#endif

static void forceviz_generate_field_lines(ForceVizModifierData *fmd, EffectorContext *effectors, Object *ob, DerivedMesh *dm, BMesh *bm)
{
	const int totlines = fmd->fieldlines.num;
	const int res = fmd->fieldlines.res;
	const int substeps = fmd->fieldlines.substeps;
	
	MeshSampleGenerator *gen;
	ForceVizEffectorData funcdata;
	int i;
	
	if (totlines <= 0 || res < 2 || substeps < 1)
		return;
	
	funcdata.scene = fmd->modifier.scene;
	funcdata.object = ob;
	funcdata.weights = fmd->effector_weights;
	funcdata.effectors = effectors;
	copy_m4_m4(funcdata.mat, ob->obmat);
	invert_m4_m4(funcdata.imat, funcdata.mat);
	funcdata.use_blenjit = fmd->flag & MOD_FORCEVIZ_USE_BLENJIT;
	
//	gen = BKE_mesh_sample_gen_surface_random_ex(dm, fmd->seed, forceviz_field_vertex_weight, &funcdata, true);
	gen = BKE_mesh_sample_gen_volume_random_bbray(dm, fmd->seed, 10.0f);
	if (!gen)
		return;
	
	BM_data_layer_add_named(bm, &bm->vdata, CD_PROP_FLT3, fmd->fieldlines.strength_layer);
	
	for (i = 0; i < totlines; ++i) {
		MeshSample sample;
		float loc[3], nor[3], tang[3];
		
		/* generate a starting point on the mesh surface */
		BKE_mesh_sample_generate(gen, &sample);
		BKE_mesh_sample_eval(dm, &sample, loc, nor, tang);
		
		forceviz_integrate_field_line(fmd, bm, &funcdata, loc);
	}
//	bm->elem_index_dirty &= ~(BM_VERT | BM_EDGE);
	bm->elem_index_dirty |= BM_VERT | BM_EDGE;
	
	BKE_mesh_sample_free_generator(gen);
}

/* ------------------------------------------------------------------------- */

static void forceviz_set_vertex_attribute_float(ForceVizModifierData *fmd, Object *ob, DerivedMesh *dm, EffectorContext *effectors,
                                                const char *name, ForceVizScalarFunc fn)
{
	const int numverts = dm->getNumVerts(dm);
	MVert *mverts = dm->getVertArray(dm);
	CustomData *vdata = dm->getVertDataLayout(dm);
	float *data = CustomData_get_layer_named(vdata, CD_PROP_FLT, name);
	int i;
	
	if (!data) {
		/* create new layer */
		data = CustomData_add_layer_named(vdata, CD_PROP_FLT, CD_CALLOC, NULL, numverts, name);
	}
	
	for (i = 0; i < numverts; ++i) {
		ForceVizInput input;
		float wco[3], wnor[3];
		float value = 0.0f;
		
		mul_v3_m4v3(wco, ob->obmat, mverts[i].co);
		normal_short_to_float_v3(wnor, mverts[i].no);
		mul_mat3_m4_v3(ob->obmat, wnor);
		
		forceviz_eval_field_loc_nor(fmd, effectors, wco, wnor, &input);
		value = fn(fmd, &input);
		
		data[i] = value;
	}
}

static void forceviz_set_vertex_attribute_float3(ForceVizModifierData *fmd, Object *ob, DerivedMesh *dm, ListBase *effectors,
                                                 const char *name, ForceVizVectorFunc fn)
{
	const int numverts = dm->getNumVerts(dm);
	MVert *mverts = dm->getVertArray(dm);
	CustomData *vdata = dm->getVertDataLayout(dm);
	float (*data)[3] = CustomData_get_layer_named(vdata, CD_PROP_FLT3, name);
	int i;
	
	if (!data) {
		/* create new layer */
		data = CustomData_add_layer_named(vdata, CD_PROP_FLT3, CD_CALLOC, NULL, numverts, name);
	}
	
	for (i = 0; i < numverts; ++i) {
		ForceVizInput input;
		float wco[3], wnor[3];
		float value[3] = {0.0f, 0.0f, 0.0f};
		
		mul_v3_m4v3(wco, ob->obmat, mverts[i].co);
		normal_short_to_float_v3(wnor, mverts[i].no);
		mul_mat3_m4_v3(ob->obmat, wnor);
		
		forceviz_eval_field_loc_nor(fmd, effectors, wco, wnor, &input);
		fn(value, fmd, &input);
		
		copy_v3_v3(data[i], value);
	}
}

static void forceviz_vertex_attribute_force(float res[3], ForceVizModifierData *UNUSED(fmd), const ForceVizInput *input)
{
	copy_v3_v3(res, input->force);
}

static float forceviz_vertex_attribute_flux(ForceVizModifierData *UNUSED(fmd), const ForceVizInput *input)
{
	return dot_v3v3(input->force, input->nor);
}

static void forceviz_set_vertex_attribute(ForceVizModifierData *fmd, Object *ob, DerivedMesh *dm, EffectorContext *effectors)
{
	ForceVizVertexAttributeSettings *vattr = &fmd->vertex_attribute;
	const char *name = vattr->attribute_name;
	
	if (name == NULL || name[0] == '\0')
		return;
	
	switch (vattr->type) {
		case MOD_FORCEVIZ_ATTR_FORCE:
			forceviz_set_vertex_attribute_float3(fmd, ob, dm, effectors, name, forceviz_vertex_attribute_force);
			break;
		case MOD_FORCEVIZ_ATTR_FLUX:
			forceviz_set_vertex_attribute_float(fmd, ob, dm, effectors, name, forceviz_vertex_attribute_flux);
			break;
	}
}

/* ------------------------------------------------------------------------- */

/* Modifier call. Processes dynamic paint modifier step. */
DerivedMesh *BKE_forceviz_do(ForceVizModifierData *fmd, Scene *scene, Object *ob, DerivedMesh *dm, float (*tex_co)[3])
{
	BMesh *bm = NULL;
	EffectorContext *effectors;
	bool use_blenjit = fmd->flag & MOD_FORCEVIZ_USE_BLENJIT;
	UNUSED_VARS(tex_co);
	
	if (use_blenjit)
		effectors = pdInitJITEffectors(scene, ob, NULL, fmd->effector_weights, false);
	else
		effectors = pdInitEffectors(scene, ob, NULL, fmd->effector_weights, false);
	
	switch (fmd->mode) {
		case MOD_FORCEVIZ_MODE_FIELDLINES: {
			/* allocate output dm */
			bm = DM_to_bmesh(dm, true);
			forceviz_generate_field_lines(fmd, effectors, ob, dm, bm);
			break;
		}
		case MOD_FORCEVIZ_MODE_VERTEX_ATTRIBUTE: {
			forceviz_set_vertex_attribute(fmd, ob, dm, effectors);
			break;
		}
	}
	
	if (use_blenjit)
		pdEndJITEffectors(effectors);
	else
		pdEndEffectors(effectors);
	
	if (bm) {
		DerivedMesh *result = CDDM_from_bmesh(bm, true);
		BM_mesh_free(bm);
		
		result->dirty |= DM_DIRTY_NORMALS;
		
//		DM_is_valid(result);
		
		return result;
	}
	else
		return dm;
}

bool BKE_forceviz_needs_camera(ForceVizModifierData *fmd)
{
	bool needs_camera = false;
	
	if (fmd->mode == MOD_FORCEVIZ_MODE_FIELDLINES) {
		if (ELEM(fmd->fieldlines.drawtype, MOD_FORCEVIZ_FIELDLINE_RIBBON))
			needs_camera = true;
	}
	
	return needs_camera;
}


/* ======== Simulation Debugging ======== */

SimDebugData *_sim_debug_data = NULL;

unsigned int BKE_sim_debug_data_hash(int i)
{
	return BLI_ghashutil_uinthash((unsigned int)i);
}

unsigned int BKE_sim_debug_data_hash_combine(unsigned int kx, unsigned int ky)
{
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

	unsigned int a, b, c;

	a = b = c = 0xdeadbeef + (2 << 2) + 13;
	a += kx;
	b += ky;

	c ^= b; c -= rot(b,14);
	a ^= c; a -= rot(c,11);
	b ^= a; b -= rot(a,25);
	c ^= b; c -= rot(b,16);
	a ^= c; a -= rot(c,4);
	b ^= a; b -= rot(a,14);
	c ^= b; c -= rot(b,24);

	return c;

#undef rot
}

static unsigned int debug_element_hash(const void *key)
{
	const SimDebugElement *elem = key;
	return elem->hash;
}

static bool debug_element_compare(const void *a, const void *b)
{
	const SimDebugElement *elem1 = a;
	const SimDebugElement *elem2 = b;

	if (elem1->hash == elem2->hash) {
		return 0;
	}
	return 1;
}

static void debug_element_free(void *val)
{
	SimDebugElement *elem = val;
	MEM_freeN(elem);
}

void BKE_sim_debug_data_set_enabled(bool enable)
{
	if (enable) {
		if (!_sim_debug_data) {
			_sim_debug_data = MEM_callocN(sizeof(SimDebugData), "sim debug data");
			_sim_debug_data->gh = BLI_ghash_new(debug_element_hash, debug_element_compare, "sim debug element hash");
		}
	}
	else {
		BKE_sim_debug_data_free();
	}
}

bool BKE_sim_debug_data_get_enabled(void)
{
	return _sim_debug_data != NULL;
}

void BKE_sim_debug_data_free(void)
{
	if (_sim_debug_data) {
		if (_sim_debug_data->gh)
			BLI_ghash_free(_sim_debug_data->gh, NULL, debug_element_free);
		MEM_freeN(_sim_debug_data);
	}
}

static void debug_data_insert(SimDebugData *debug_data, SimDebugElement *elem)
{
	SimDebugElement *old_elem = BLI_ghash_lookup(debug_data->gh, elem);
	if (old_elem) {
		*old_elem = *elem;
		MEM_freeN(elem);
	}
	else
		BLI_ghash_insert(debug_data->gh, elem, elem);
}

void BKE_sim_debug_data_add_element(int type, const float v1[3], const float v2[3], float r, float g, float b, const char *category, unsigned int hash)
{
	unsigned int category_hash = BLI_ghashutil_strhash_p(category);
	SimDebugElement *elem;
	
	if (!_sim_debug_data) {
		if (G.debug & G_DEBUG_SIMDATA)
			BKE_sim_debug_data_set_enabled(true);
		else
			return;
	}
	
	elem = MEM_callocN(sizeof(SimDebugElement), "sim debug data element");
	elem->type = type;
	elem->category_hash = category_hash;
	elem->hash = hash;
	elem->color[0] = r;
	elem->color[1] = g;
	elem->color[2] = b;
	copy_v3_v3(elem->v1, v1);
	copy_v3_v3(elem->v2, v2);
	
	debug_data_insert(_sim_debug_data, elem);
}

void BKE_sim_debug_data_remove_element(unsigned int hash)
{
	SimDebugElement dummy;
	if (!_sim_debug_data)
		return;
	
	dummy.hash = hash;
	BLI_ghash_remove(_sim_debug_data->gh, &dummy, NULL, debug_element_free);
}

void BKE_sim_debug_data_clear(void)
{
	if (!_sim_debug_data)
		return;
	
	if (_sim_debug_data->gh)
		BLI_ghash_clear(_sim_debug_data->gh, NULL, debug_element_free);
}

void BKE_sim_debug_data_clear_category(const char *category)
{
	int category_hash = (int)BLI_ghashutil_strhash_p(category);
	
	if (!_sim_debug_data)
		return;
	
	if (_sim_debug_data->gh) {
		GHashIterator iter;
		BLI_ghashIterator_init(&iter, _sim_debug_data->gh);
		while (!BLI_ghashIterator_done(&iter)) {
			const SimDebugElement *elem = BLI_ghashIterator_getValue(&iter);
			BLI_ghashIterator_step(&iter); /* removing invalidates the current iterator, so step before removing */
			
			if (elem->category_hash == category_hash)
				BLI_ghash_remove(_sim_debug_data->gh, elem, NULL, debug_element_free);
		}
	}
}

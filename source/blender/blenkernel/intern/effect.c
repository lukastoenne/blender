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
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_scene.h"
#include "BKE_smoke.h"

#include "bmesh.h"

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
static void add_object_to_effectors(ListBase **effectors, Scene *scene, EffectorWeights *weights, Object *ob, Object *ob_src)
{
	EffectorCache *eff = NULL;

	if ( ob == ob_src || weights->weight[ob->pd->forcefield] == 0.0f )
		return;

	if (ob->pd->shape == PFIELD_SHAPE_POINTS && !ob->derivedFinal )
		return;

	if (*effectors == NULL)
		*effectors = MEM_callocN(sizeof(ListBase), "effectors list");

	eff = new_effector_cache(scene, ob, NULL, ob->pd);

	/* make sure imat is up to date */
	invert_m4_m4(ob->imat, ob->obmat);

	BLI_addtail(*effectors, eff);
}
static void add_particles_to_effectors(ListBase **effectors, Scene *scene, EffectorWeights *weights, Object *ob, ParticleSystem *psys, ParticleSystem *psys_src)
{
	ParticleSettings *part= psys->part;

	if ( !psys_check_enabled(ob, psys) )
		return;

	if ( psys == psys_src && (part->flag & PART_SELF_EFFECT) == 0)
		return;

	if ( part->pd && part->pd->forcefield && weights->weight[part->pd->forcefield] != 0.0f) {
		if (*effectors == NULL)
			*effectors = MEM_callocN(sizeof(ListBase), "effectors list");

		BLI_addtail(*effectors, new_effector_cache(scene, ob, psys, part->pd));
	}

	if (part->pd2 && part->pd2->forcefield && weights->weight[part->pd2->forcefield] != 0.0f) {
		if (*effectors == NULL)
			*effectors = MEM_callocN(sizeof(ListBase), "effectors list");

		BLI_addtail(*effectors, new_effector_cache(scene, ob, psys, part->pd2));
	}
}

/* returns ListBase handle with objects taking part in the effecting */
ListBase *pdInitEffectors(Scene *scene, Object *ob_src, ParticleSystem *psys_src,
                          EffectorWeights *weights, bool precalc)
{
	Base *base;
	unsigned int layer= ob_src->lay;
	ListBase *effectors = NULL;
	
	if (weights->group) {
		GroupObject *go;
		
		for (go= weights->group->gobject.first; go; go= go->next) {
			if ( (go->ob->lay & layer) ) {
				if ( go->ob->pd && go->ob->pd->forcefield )
					add_object_to_effectors(&effectors, scene, weights, go->ob, ob_src);

				if ( go->ob->particlesystem.first ) {
					ParticleSystem *psys= go->ob->particlesystem.first;

					for ( ; psys; psys=psys->next )
						add_particles_to_effectors(&effectors, scene, weights, go->ob, psys, psys_src);
				}
			}
		}
	}
	else {
		for (base = scene->base.first; base; base= base->next) {
			if ( (base->lay & layer) ) {
				if ( base->object->pd && base->object->pd->forcefield )
					add_object_to_effectors(&effectors, scene, weights, base->object, ob_src);

				if ( base->object->particlesystem.first ) {
					ParticleSystem *psys= base->object->particlesystem.first;

					for ( ; psys; psys=psys->next )
						add_particles_to_effectors(&effectors, scene, weights, base->object, psys, psys_src);
				}
			}
		}
	}
	
	if (precalc)
		pdPrecalculateEffectors(effectors);
	
	return effectors;
}

void pdEndEffectors(ListBase **effectors)
{
	if (*effectors) {
		EffectorCache *eff = (*effectors)->first;

		for (; eff; eff=eff->next) {
			if (eff->guide_data)
				MEM_freeN(eff->guide_data);
		}

		BLI_freelistN(*effectors);
		MEM_freeN(*effectors);
		*effectors = NULL;
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

void pdPrecalculateEffectors(ListBase *effectors)
{
	if (effectors) {
		EffectorCache *eff = effectors->first;
		for (; eff; eff=eff->next)
			precalculate_effector(eff);
	}
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
			if (BLI_bvhtree_ray_cast(collmd->bvhtree, point->loc, norm, 0.0f, &hit, eff_tri_ray_hit, NULL)>=0) {
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

			madd_v3_v3v3fl(temp, efd->vec_to_point, efd->nor, -fac);
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
			MFace *mface = CDDM_get_tessface(surmd->dm, nearest.index);
			
			copy_v3_v3(surface_vel, surmd->v[mface->v1].co);
			add_v3_v3(surface_vel, surmd->v[mface->v2].co);
			add_v3_v3(surface_vel, surmd->v[mface->v3].co);
			if (mface->v4)
				add_v3_v3(surface_vel, surmd->v[mface->v4].co);

			mul_v3_fl(surface_vel, mface->v4 ? 0.25f : (1.0f / 3.0f));
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
	if (eff->pd->shape == PFIELD_SHAPE_POINTS) {
		efd->index = p;

		*p = 0;
		*tot = eff->ob->derivedFinal ? eff->ob->derivedFinal->numVertData : 1;

		if (*tot && eff->pd->forcefield == PFIELD_HARMONIC && point->index >= 0) {
			*p = point->index % *tot;
			*tot = *p+1;
		}
	}
	else if (eff->psys) {
		efd->index = p;

		*p = 0;
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
		*p = 0;
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
 * scene       = scene where it runs in, for time and stuff
 * lb			= listbase with objects that take part in effecting
 * opco		= global coord, as input
 * force		= force accumulator
 * speed		= actual current speed which can be altered
 * cur_time	= "external" time in frames, is constant for static particles
 * loc_time	= "local" time in frames, range <0-1> for the lifetime of particle
 * par_layer	= layer the caller is in
 * flags		= only used for softbody wind now
 * guide		= old speed of particle
 */
void pdDoEffectors(ListBase *effectors, ListBase *colliders, EffectorWeights *weights, EffectedPoint *point, float *force, float *impulse)
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
	
	if (effectors) for (eff = effectors->first; eff; eff=eff->next) {
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


/* ======== Force Field Visualization ======== */

typedef struct ForceVizInput {
	float co[3];
	float vel[3];
	
	float force[3];
	float dforce[3][3];
	float impulse[3];
} ForceVizInput;

typedef void (*ForceVizImageGenerator)(float col[4], ForceVizModifierData *fmd, const ForceVizInput *input);

#if 0
/* rasterize triangle with uv2[1] == uv3[1] and uv1[1] != uv2[1] */
static void forceviz_rasterize_halftri(ForceVizModifierData *fmd, ListBase *effectors, ImBuf *ibuf, ForceVizImageGenerator cb, const rcti *rect,
                                       const float co1[3], const float co2[3], const float co3[3],
                                       const float uv1[2], const float uv2[2], const float uv3[2])
{
	const bool is_upper = uv1[1] > uv2[1];
	
	const bool use_float_buffer = (ibuf->rect_float != NULL);
	const int channels = ibuf->channels;
	const int xstride = channels, ystride = channels * ibuf->x;
	float *buf_float;
	unsigned char *buf_char;
	
	float vmin, vmax;
	float umin, umax;
	float H, Hinv;
	int y0, y1;
	float u0, du0, u1, du1;
	int i, j;
	
	if (is_upper) {
		vmin = uv2[1];
		vmax = uv1[1];
	}
	else {
		vmin = uv1[1];
		vmax = uv2[1];
	}
	y0 = max_ii((int)vmin, rect->ymin);
	y1 = min_ii((int)vmax, rect->ymax-1);
	H = vmax - vmin;
	Hinv = H != 0.0f ? 1.0f/H : 0.0f;
	
	if (uv2[0] < uv3[0]) {
		umin = uv2[0];
		umax = uv3[0];
	}
	else {
		umin = uv3[0];
		umax = uv2[0];
	}
	
	if (H != 0.0f) {
		du0 = (umin - uv1[0]) * Hinv;
		du1 = (umax - uv1[0]) * Hinv;
	}
	else {
		du0 = 0.0f;
		du1 = 0.0f;
	}
	
	{
		float v0 = roundf(vmax);
		u0 = umin + (v0 - vmin) * du0;
		u1 = uv1[0] + (v0 - vmin) * du1;
	}
	
	if (use_float_buffer)
		buf_float = ibuf->rect_float + ystride * y0;
	else
		buf_char = (unsigned char *)ibuf->rect + ystride * y0;
	
	for (j = y0; j <= y1; ++j) {
		float v = (float)j + 0.5f;
		int x0 = max_ii((int)u0, rect->xmin);
		int x1 = min_ii((int)u1, rect->xmax-1);
		float L = u1 - u0;
		float Linv = L != 0.0f ? 1.0f/L : 0.0f;
		
		float *row_float = NULL;
		unsigned char *row_char = NULL;
		if (use_float_buffer)
			row_float = buf_float + xstride * x0;
		else
			row_char = buf_char + xstride * x0;
		
		for (i = x0; i <= x1; ++i) {
			float u = (float)i + 0.5f;
			float facx = u * Linv;
			float facy = v * Hinv;
			float col[4] = {0.0f, 0.0f, 0.0f, 1.0f};
			
			{
				EffectedPoint point;
				ForceVizInput input;
				float co23[3];
				
				interp_v3_v3v3(co23, co2, co3, facx);
				interp_v3_v3v3(input.co, co1, co23, facy);
				zero_v3(input.vel);
				
				pd_point_from_loc(fmd->modifier.scene, input.co, input.vel, 0, &point);
				zero_v3(input.force);
				zero_v3(input.impulse);
				zero_m3(input.dforce); // TODO
				pdDoEffectors(effectors, NULL, fmd->effector_weights, &point, input.force, input.impulse);
				
				cb(col, fmd, &input);
			}
			
			if (use_float_buffer) {
				if (channels == 3)
					copy_v3_v3(row_float, col);
				else
					copy_v4_v4(row_float, col);
			}
			else {
				if (channels == 3)
					rgb_float_to_uchar(row_char, col);
				else
					rgba_float_to_uchar(row_char, col);
			}
			
			if (use_float_buffer)
				row_float += xstride;
			else
				row_char += xstride;
		}
		
		if (use_float_buffer)
			buf_float += ystride;
		else
			buf_char += ystride;
		u0 += du0;
		u1 += du1;
	}
}
#endif

/* rasterize triangle with uv2[1] == uv3[1] and uv1[1] >= uv2[1] */
static void forceviz_rasterize_halftri_lower(ForceVizModifierData *fmd, ListBase *effectors, ImBuf *ibuf, ForceVizImageGenerator cb, const rcti *rect,
                                             const float co1[3], const float co2[3], const float co3[3],
                                             const float uv1[2], const float uv2[2], const float uv3[2])
{
	const bool use_float_buffer = (ibuf->rect_float != NULL);
	const int channels = ibuf->channels;
	const int xstride = channels, ystride = channels * ibuf->x;
	float *buf_float;
	unsigned char *buf_char;
	
	float H, Hinv;
	int y0, y1;
	float u0, du0, u1, du1;
	int i, j;
	
	y0 = max_ii((int)uv1[1], rect->ymin);
	y1 = min_ii((int)uv2[1], rect->ymax-1);
	
	if (use_float_buffer)
		buf_float = ibuf->rect_float + ystride * y0;
	else
		buf_char = (unsigned char *)ibuf->rect + ystride * y0;
	
	{
		float umin, umax;
		if (uv2[0] < uv3[0]) {
			umin = uv2[0];
			umax = uv3[0];
		}
		else {
			umin = uv3[0];
			umax = uv2[0];
		}
		
		H = uv2[1] - uv1[1];
		Hinv = H > 0.0f ? 1.0f/H : 0.0f;
		if (H > 0.0f) {
			du0 = (umin - uv1[0]) / H;
			du1 = (umax - uv1[0]) / H;
		}
		else {
			du0 = 0.0f;
			du1 = 0.0f;
		}
	}
	
	{
		float v0 = ceilf(uv1[1]);
		u0 = uv1[0] + (v0 - uv1[1]) * du0;
		u1 = uv1[0] + (v0 - uv1[1]) * du1;
	}
	for (j = y0; j <= y1; ++j) {
		float v = (float)j + 0.5f;
		int x0 = max_ii((int)u0, rect->xmin);
		int x1 = min_ii((int)u1, rect->xmax-1);
		float L = u1 - u0;
		float Linv = L > 0.0f ? 1.0f/L : 0.0f;
		
		float *row_float = NULL;
		unsigned char *row_char = NULL;
		if (use_float_buffer)
			row_float = buf_float + xstride * x0;
		else
			row_char = buf_char + xstride * x0;
		
		for (i = x0; i <= x1; ++i) {
			float u = (float)i + 0.5f;
			float facx = u * Linv;
			float facy = v * Hinv;
			float col[4] = {0.0f, 0.0f, 0.0f, 1.0f};
			
			{
				EffectedPoint point;
				ForceVizInput input;
				float co23[3];
				
				interp_v3_v3v3(co23, co2, co3, facx);
				interp_v3_v3v3(input.co, co1, co23, facy);
				zero_v3(input.vel);
				
				pd_point_from_loc(fmd->modifier.scene, input.co, input.vel, 0, &point);
				zero_v3(input.force);
				zero_v3(input.impulse);
				zero_m3(input.dforce); // TODO
				pdDoEffectors(effectors, NULL, fmd->effector_weights, &point, input.force, input.impulse);
				
				cb(col, fmd, &input);
			}
			
			if (use_float_buffer) {
				if (channels == 3)
					copy_v3_v3(row_float, col);
				else
					copy_v4_v4(row_float, col);
			}
			else {
				if (channels == 3)
					rgb_float_to_uchar(row_char, col);
				else
					rgba_float_to_uchar(row_char, col);
			}
			
			if (use_float_buffer)
				row_float += xstride;
			else
				row_char += xstride;
		}
		
		if (use_float_buffer)
			buf_float += ystride;
		else
			buf_char += ystride;
		u0 += du0;
		u1 += du1;
	}
}

BLI_INLINE void forceviz_reorder_tri(const float *uv[3], const float *co[3], int i1, int i2, int i3)
{
	const float *nuv[3];
	const float *nco[3];
	
	nuv[0] = uv[i1]; nuv[1] = uv[i2]; nuv[2] = uv[i3];
	uv[0] = nuv[0]; uv[1] = nuv[1]; uv[2] = nuv[2];
	
	nco[0] = co[i1]; nco[1] = co[i2]; nco[2] = co[i3];
	co[0] = nco[0]; co[1] = nco[1]; co[2] = nco[2];
}

static void forceviz_sort_tri(const float *uv[3], const float *co[3])
{
	float v1 = uv[0][1], v2 = uv[1][1], v3 = uv[2][1];
	
	if (v1 < v2) {
		// ABC, CAB, ACB
		if (v1 < v3) {
			// ABC, ACB
			if (v2 < v3) {
				// ABC
				forceviz_reorder_tri(uv, co, 0, 1, 2);
			}
			else {
				// ACB
				forceviz_reorder_tri(uv, co, 0, 2, 1);
			}
		}
		else {
			// CAB
			forceviz_reorder_tri(uv, co, 2, 0, 1);
		}
	}
	else if (v1 > v2) {
		// BCA, CBA, BAC
		if (v1 > v3) {
			// BCA, CBA
			if (v2 > v3) {
				// CBA
				forceviz_reorder_tri(uv, co, 2, 1, 0);
			}
			else {
				// BCA
				forceviz_reorder_tri(uv, co, 1, 2, 0);
			}
		}
		else {
			// BAC
			forceviz_reorder_tri(uv, co, 1, 0, 2);
		}
	}
	else {
		// v1 == v2
		if (v1 < v3) {
			// ABC, BAC (equivalent)
			forceviz_reorder_tri(uv, co, 0, 1, 2);
		}
		else if (v1 > v3) {
			// CAB, CBA (equivalent)
			forceviz_reorder_tri(uv, co, 2, 0, 1);
		}
		else {
			// trivial case, v1==v2==v3
			//forceviz_reorder_tri(uv, co, 0, 1, 2);
		}
	}
}

static void forceviz_rasterize_tri(ForceVizModifierData *fmd, ListBase *effectors, ImBuf *ibuf, ForceVizImageGenerator cb, const rcti *rect,
                                   const float co1[3], const float co2[3], const float co3[3],
                                   const float uv1[2], const float uv2[2], const float uv3[2])
{
	const float *uv_sort[3] = {uv1, uv2, uv3};
	const float *co_sort[3] = {co1, co2, co3};
	forceviz_sort_tri(uv_sort, co_sort);
	
	{
		float t = (uv_sort[2][1] > uv_sort[0][1])? (uv_sort[1][1] - uv_sort[0][1]) / (uv_sort[2][1] - uv_sort[0][1]): 0.0f;
		float co_mid[3];
		float uv_mid[2];
		interp_v3_v3v3(co_mid, co_sort[0], co_sort[2], t);
		uv_mid[0] = interpf(uv_sort[2][1], uv_sort[0][1], t);
		uv_mid[1] = uv2[1];
//		forceviz_rasterize_halftri(fmd, effectors, ibuf, cb, rect, co_sort[0], co_sort[1], co_mid, uv_sort[0], uv_sort[1], uv_mid);
//		forceviz_rasterize_halftri(fmd, effectors, ibuf, cb, rect, co_sort[2], co_mid, co_sort[1], uv_sort[2], uv_mid, uv_sort[1]);
		forceviz_rasterize_halftri_lower(fmd, effectors, ibuf, cb, rect, co_sort[0], co_sort[1], co_mid, uv_sort[0], uv_sort[1], uv_mid);
	}
}

static void forceviz_rasterize_face(ForceVizModifierData *fmd, ListBase *effectors, ImBuf *ibuf, ForceVizImageGenerator cb, const rcti *rect,
                                    MVert *verts, MFace *mf, float (*tex_co)[3], float obmat[4][4])
{
	float co1[3], co2[3], co3[3];
	float uv1[2], uv2[2], uv3[2];
	mul_v3_m4v3(co1, obmat, verts[mf->v1].co);
	mul_v3_m4v3(co2, obmat, verts[mf->v2].co);
	mul_v3_m4v3(co3, obmat, verts[mf->v3].co);
	copy_v2_v2(uv1, tex_co[mf->v1]);
	copy_v2_v2(uv2, tex_co[mf->v2]);
	copy_v2_v2(uv3, tex_co[mf->v3]);
	/* scale to pixel coordinates */
	uv1[0] = (uv1[0] + 1.0f) * 0.5f * ibuf->x;
	uv1[1] = (uv1[1] + 1.0f) * 0.5f * ibuf->y;
	uv2[0] = (uv2[0] + 1.0f) * 0.5f * ibuf->x;
	uv2[1] = (uv2[1] + 1.0f) * 0.5f * ibuf->y;
	uv3[0] = (uv3[0] + 1.0f) * 0.5f * ibuf->x;
	uv3[1] = (uv3[1] + 1.0f) * 0.5f * ibuf->y;
	
	forceviz_rasterize_tri(fmd, effectors, ibuf, cb, rect, co1, co2, co3, uv1, uv2, uv3);
	
	if (mf->v4) {
		float co4[3];
		float uv4[2];
		mul_v3_m4v3(co4, obmat, verts[mf->v4].co);
		copy_v2_v2(uv4, tex_co[mf->v4]);
		uv4[0] = (uv4[0] + 1.0f) * 0.5f * ibuf->x;
		uv4[1] = (uv4[1] + 1.0f) * 0.5f * ibuf->y;
		
		forceviz_rasterize_tri(fmd, effectors, ibuf, cb, rect, co1, co3, co4, uv1, uv3, uv4);
	}
}

static void forceviz_rasterize_mesh(ForceVizModifierData *fmd, ListBase *effectors, ImBuf *ibuf, ForceVizImageGenerator cb,
                                    DerivedMesh *dm, float (*tex_co)[3], float obmat[4][4])
{
	int numfaces = dm->getNumTessFaces(dm);
	MFace *faces = dm->getTessFaceArray(dm);
	MVert *verts = dm->getVertArray(dm);
	int i;
	rcti rect;
	
	if (!faces || !verts)
		return;
	
	rect.xmin = 0;
	rect.ymin = 0;
	rect.xmax = ibuf->x;
	rect.ymax = ibuf->y;
	
	for (i = 0; i < numfaces; ++i) {
		forceviz_rasterize_face(fmd, effectors, ibuf, cb, &rect, verts, &faces[i], tex_co, obmat);
	}
	
	ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID | IB_BITMAPDIRTY;
}

static void forceviz_generate_image(ForceVizModifierData *fmd, ListBase *effectors, Image *image, ForceVizImageGenerator cb, Object *ob, DerivedMesh *dm, float (*tex_co)[3])
{
	void *lock;
	ImBuf *ibuf;
	
	if (!image)
		return;
	
	ibuf = BKE_image_acquire_ibuf(image, &fmd->iuser, &lock);
	if (!ibuf || !ELEM(ibuf->channels, 3, 4)) {
		BKE_image_release_ibuf(image, ibuf, lock);
		return;
	}
	
	if (ibuf->rect_float)
		memset(ibuf->rect_float, 0, sizeof(float) * ibuf->channels * ibuf->x * ibuf->y);
	
	forceviz_rasterize_mesh(fmd, effectors, ibuf, cb, dm, tex_co, ob->obmat);
	
	/* force OpenGL reload and mipmap recalc */
	/* some of the images could have been changed during bake,
	 * so recreate mipmaps regardless bake result status
	 */
	if (image->ok == IMA_OK_LOADED) {
		if (ibuf) {
			if (ibuf->userflags & IB_BITMAPDIRTY) {
				GPU_free_image(image);
				imb_freemipmapImBuf(ibuf);
			}
			
			/* invalidate display buffers for changed images */
			if (ibuf->userflags & IB_BITMAPDIRTY)
				ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
			
			BKE_image_release_ibuf(image, ibuf, lock);
			DAG_id_tag_update(&image->id, 0);
		}
	}
}

/* ------------------------------------------------------------------------- */

static void forceviz_generate_field_lines(ForceVizModifierData *fmd, ListBase *effectors, BMesh *bm)
{
	BMVert *vert, *vert_prev = NULL;
	BMEdge *edge;
	int ivert = bm->totvert, iedge = bm->totedge;
	int i, k;
	
	if (fmd->fieldlines_num <= 0 || fmd->fieldlines_res < 2)
		return;
	
	for (i = 0; i < fmd->fieldlines_num; ++i) {
		for (k = 0; k < fmd->fieldlines_res; ++k) {
			float co[3];
//			co[0] = (float)i * 0.33333f;
//			co[1] = (float)k * 0.33333f;
//			co[2] = (float)k * 0.33333f;
			const float a[3] = {0.64, 0.3652, 0.155};
			mul_v3_v3fl(co, a, k);
			
			vert_prev = vert;
			vert = BM_vert_create(bm, co, NULL, BM_CREATE_NOP);
			BM_elem_index_set(vert, ivert++); /* set_inline */
			
			if (k > 0) {
				edge = BM_edge_create(bm, vert_prev, vert, NULL, BM_CREATE_NOP);
				BM_elem_index_set(edge, iedge++); /* set_inline */
				
				sub_v3_v3v3(vert->no, vert->co, vert_prev->co);
				normalize_v3(vert->no);
				if (k == 1)
					copy_v3_v3(vert_prev->no, vert->no);
			}
		}
	}
	bm->elem_index_dirty &= ~(BM_VERT | BM_EDGE);
}

/* ------------------------------------------------------------------------- */

static void forceviz_image_test(float col[4], ForceVizModifierData *UNUSED(fmd), const ForceVizInput *input)
{
	copy_v3_v3(col, input->co);
}

static void forceviz_image_vectors(float col[4], ForceVizModifierData *UNUSED(fmd), const ForceVizInput *input)
{
	copy_v3_v3(col, input->force);
	mul_v3_fl(col, 0.5f * 0.01f); // TODO add custom scaling factor or max force
	add_v3_fl(col, 0.5f);
	CLAMP3(col, 0.0f, 1.0f);
}

/* Modifier call. Processes dynamic paint modifier step. */
DerivedMesh *BKE_forceviz_do(ForceVizModifierData *fmd, Scene *scene, Object *ob, DerivedMesh *dm, float (*tex_co)[3])
{
	BMesh *bm = NULL;
	ListBase *effectors;
	
	/* allocate output dm */
	if (fmd->flag & MOD_FORCEVIZ_USE_FIELD_LINES) {
		bm = DM_to_bmesh(dm, true);
//		bm = BM_mesh_create(&bm_mesh_allocsize_default);
	}
	
	effectors = pdInitEffectors(scene, ob, NULL, fmd->effector_weights, false);
	
	if (fmd->flag & MOD_FORCEVIZ_USE_IMG_VEC)
		forceviz_generate_image(fmd, effectors, fmd->image_vec, forceviz_image_vectors, ob, dm, tex_co);
	
	if (fmd->flag & MOD_FORCEVIZ_USE_FIELD_LINES) {
		forceviz_generate_field_lines(fmd, effectors, bm);
	}
	
	pdEndEffectors(&effectors);
	
	if (bm) {
		DerivedMesh *result = CDDM_from_bmesh(bm, true);
		BM_mesh_free(bm);
		
		/* we actually calculate normals ourselves */
//		result->dirty |= DM_DIRTY_NORMALS;
		
		DM_is_valid(result);
		
		return result;
	}
	else
		return dm;
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

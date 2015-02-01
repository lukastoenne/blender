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
 * The Original Code is Copyright (C) 2007 by Janne Karhu.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/particle_interpolate.c
 *  \ingroup bke
 */


#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_key_types.h"
#include "DNA_particle_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_key.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"

void psys_interpolate_particle(short type, ParticleKey keys[4], float dt, ParticleKey *result, bool velocity)
{
	float t[4];

	if (type < 0) {
		interp_cubic_v3(result->co, result->vel, keys[1].co, keys[1].vel, keys[2].co, keys[2].vel, dt);
	}
	else {
		key_curve_position_weights(dt, t, type);

		interp_v3_v3v3v3v3(result->co, keys[0].co, keys[1].co, keys[2].co, keys[3].co, t);

		if (velocity) {
			float temp[3];

			if (dt > 0.999f) {
				key_curve_position_weights(dt - 0.001f, t, type);
				interp_v3_v3v3v3v3(temp, keys[0].co, keys[1].co, keys[2].co, keys[3].co, t);
				sub_v3_v3v3(result->vel, result->co, temp);
			}
			else {
				key_curve_position_weights(dt + 0.001f, t, type);
				interp_v3_v3v3v3v3(temp, keys[0].co, keys[1].co, keys[2].co, keys[3].co, t);
				sub_v3_v3v3(result->vel, temp, result->co);
			}
		}
	}
}

float psys_get_dietime_from_cache(PointCache *cache, int index)
{
	PTCacheMem *pm;
	int dietime = 10000000; /* some max value so that we can default to pa->time+lifetime */

	for (pm = cache->mem_cache.last; pm; pm = pm->prev) {
		if (BKE_ptcache_mem_index_find(pm, index) >= 0)
			return (float)pm->frame;
	}

	return (float)dietime;
}

/* Assumes pointcache->mem_cache exists, so for disk cached particles call psys_make_temp_pointcache() before use */
/* It uses ParticleInterpolationData->pm to store the current memory cache frame so it's thread safe. */
static void get_pointcache_keys_for_time(Object *UNUSED(ob), PointCache *cache, PTCacheMem **cur, int index, float t, ParticleKey *key1, ParticleKey *key2)
{
	static PTCacheMem *pm = NULL;
	int index1, index2;

	if (index < 0) { /* initialize */
		*cur = cache->mem_cache.first;

		if (*cur)
			*cur = (*cur)->next;
	}
	else {
		if (*cur) {
			while (*cur && (*cur)->next && (float)(*cur)->frame < t)
				*cur = (*cur)->next;

			pm = *cur;

			index2 = BKE_ptcache_mem_index_find(pm, index);
			index1 = BKE_ptcache_mem_index_find(pm->prev, index);

			BKE_ptcache_make_particle_key(key2, index2, pm->data, (float)pm->frame);
			if (index1 < 0)
				copy_particle_key(key1, key2, 1);
			else
				BKE_ptcache_make_particle_key(key1, index1, pm->prev->data, (float)pm->prev->frame);
		}
		else if (cache->mem_cache.first) {
			pm = cache->mem_cache.first;
			index2 = BKE_ptcache_mem_index_find(pm, index);
			BKE_ptcache_make_particle_key(key2, index2, pm->data, (float)pm->frame);
			copy_particle_key(key1, key2, 1);
		}
	}
}

static int get_pointcache_times_for_particle(PointCache *cache, int index, float *start, float *end)
{
	PTCacheMem *pm;
	int ret = 0;

	for (pm = cache->mem_cache.first; pm; pm = pm->next) {
		if (BKE_ptcache_mem_index_find(pm, index) >= 0) {
			*start = pm->frame;
			ret++;
			break;
		}
	}

	for (pm = cache->mem_cache.last; pm; pm = pm->prev) {
		if (BKE_ptcache_mem_index_find(pm, index) >= 0) {
			*end = pm->frame;
			ret++;
			break;
		}
	}

	return ret == 2;
}

static void edit_to_particle(ParticleKey *key, PTCacheEditKey *ekey)
{
	copy_v3_v3(key->co, ekey->co);
	if (ekey->vel) {
		copy_v3_v3(key->vel, ekey->vel);
	}
	key->time = *(ekey->time);
}

static void hair_to_particle(ParticleKey *key, HairKey *hkey)
{
	copy_v3_v3(key->co, hkey->co);
	key->time = hkey->time;
}

static void mvert_to_particle(ParticleKey *key, MVert *mvert, HairKey *hkey)
{
	copy_v3_v3(key->co, mvert->co);
	key->time = hkey->time;
}

void init_particle_interpolation(Object *ob, ParticleSystem *psys, ParticleData *pa, ParticleInterpolationData *pind)
{

	if (pind->epoint) {
		PTCacheEditPoint *point = pind->epoint;

		pind->ekey[0] = point->keys;
		pind->ekey[1] = point->totkey > 1 ? point->keys + 1 : NULL;

		pind->birthtime = *(point->keys->time);
		pind->dietime = *((point->keys + point->totkey - 1)->time);
	}
	else if (pind->keyed) {
		ParticleKey *key = pa->keys;
		pind->kkey[0] = key;
		pind->kkey[1] = pa->totkey > 1 ? key + 1 : NULL;

		pind->birthtime = key->time;
		pind->dietime = (key + pa->totkey - 1)->time;
	}
	else if (pind->cache) {
		float start = 0.0f, end = 0.0f;
		get_pointcache_keys_for_time(ob, pind->cache, &pind->pm, -1, 0.0f, NULL, NULL);
		pind->birthtime = pa ? pa->time : pind->cache->startframe;
		pind->dietime = pa ? pa->dietime : pind->cache->endframe;

		if (get_pointcache_times_for_particle(pind->cache, pa - psys->particles, &start, &end)) {
			pind->birthtime = MAX2(pind->birthtime, start);
			pind->dietime = MIN2(pind->dietime, end);
		}
	}
	else {
		HairKey *key = pa->hair;
		pind->hkey[0] = key;
		pind->hkey[1] = key + 1;

		pind->birthtime = key->time;
		pind->dietime = (key + pa->totkey - 1)->time;

		if (pind->dm) {
			pind->mvert[0] = CDDM_get_vert(pind->dm, pa->hair_index);
			pind->mvert[1] = pind->mvert[0] + 1;
		}
	}
}

void do_particle_interpolation(ParticleSystem *psys, int p, ParticleData *pa, float t, ParticleInterpolationData *pind, ParticleKey *result)
{
	PTCacheEditPoint *point = pind->epoint;
	ParticleKey keys[4];
	int point_vel = (point && point->keys->vel);
	float real_t, dfra, keytime, invdt = 1.f;

	/* billboards wont fill in all of these, so start cleared */
	memset(keys, 0, sizeof(keys));

	/* interpret timing and find keys */
	if (point) {
		if (result->time < 0.0f)
			real_t = -result->time;
		else
			real_t = *(pind->ekey[0]->time) + t * (*(pind->ekey[0][point->totkey - 1].time) - *(pind->ekey[0]->time));

		while (*(pind->ekey[1]->time) < real_t)
			pind->ekey[1]++;

		pind->ekey[0] = pind->ekey[1] - 1;
	}
	else if (pind->keyed) {
		/* we have only one key, so let's use that */
		if (pind->kkey[1] == NULL) {
			copy_particle_key(result, pind->kkey[0], 1);
			return;
		}

		if (result->time < 0.0f)
			real_t = -result->time;
		else
			real_t = pind->kkey[0]->time + t * (pind->kkey[0][pa->totkey - 1].time - pind->kkey[0]->time);

		if (psys->part->phystype == PART_PHYS_KEYED && psys->flag & PSYS_KEYED_TIMING) {
			ParticleTarget *pt = psys->targets.first;

			pt = pt->next;

			while (pt && pa->time + pt->time < real_t)
				pt = pt->next;

			if (pt) {
				pt = pt->prev;

				if (pa->time + pt->time + pt->duration > real_t)
					real_t = pa->time + pt->time;
			}
			else
				real_t = pa->time + ((ParticleTarget *)psys->targets.last)->time;
		}

		CLAMP(real_t, pa->time, pa->dietime);

		while (pind->kkey[1]->time < real_t)
			pind->kkey[1]++;
		
		pind->kkey[0] = pind->kkey[1] - 1;
	}
	else if (pind->cache) {
		if (result->time < 0.0f) /* flag for time in frames */
			real_t = -result->time;
		else
			real_t = pa->time + t * (pa->dietime - pa->time);
	}
	else {
		if (result->time < 0.0f)
			real_t = -result->time;
		else
			real_t = pind->hkey[0]->time + t * (pind->hkey[0][pa->totkey - 1].time - pind->hkey[0]->time);

		while (pind->hkey[1]->time < real_t) {
			pind->hkey[1]++;
			pind->mvert[1]++;
		}

		pind->hkey[0] = pind->hkey[1] - 1;
	}

	/* set actual interpolation keys */
	if (point) {
		edit_to_particle(keys + 1, pind->ekey[0]);
		edit_to_particle(keys + 2, pind->ekey[1]);
	}
	else if (pind->dm) {
		pind->mvert[0] = pind->mvert[1] - 1;
		mvert_to_particle(keys + 1, pind->mvert[0], pind->hkey[0]);
		mvert_to_particle(keys + 2, pind->mvert[1], pind->hkey[1]);
	}
	else if (pind->keyed) {
		memcpy(keys + 1, pind->kkey[0], sizeof(ParticleKey));
		memcpy(keys + 2, pind->kkey[1], sizeof(ParticleKey));
	}
	else if (pind->cache) {
		get_pointcache_keys_for_time(NULL, pind->cache, &pind->pm, p, real_t, keys + 1, keys + 2);
	}
	else {
		hair_to_particle(keys + 1, pind->hkey[0]);
		hair_to_particle(keys + 2, pind->hkey[1]);
	}

	/* set secondary interpolation keys for hair */
	if (!pind->keyed && !pind->cache && !point_vel) {
		if (point) {
			if (pind->ekey[0] != point->keys)
				edit_to_particle(keys, pind->ekey[0] - 1);
			else
				edit_to_particle(keys, pind->ekey[0]);
		}
		else if (pind->dm) {
			if (pind->hkey[0] != pa->hair)
				mvert_to_particle(keys, pind->mvert[0] - 1, pind->hkey[0] - 1);
			else
				mvert_to_particle(keys, pind->mvert[0], pind->hkey[0]);
		}
		else {
			if (pind->hkey[0] != pa->hair)
				hair_to_particle(keys, pind->hkey[0] - 1);
			else
				hair_to_particle(keys, pind->hkey[0]);
		}

		if (point) {
			if (pind->ekey[1] != point->keys + point->totkey - 1)
				edit_to_particle(keys + 3, pind->ekey[1] + 1);
			else
				edit_to_particle(keys + 3, pind->ekey[1]);
		}
		else if (pind->dm) {
			if (pind->hkey[1] != pa->hair + pa->totkey - 1)
				mvert_to_particle(keys + 3, pind->mvert[1] + 1, pind->hkey[1] + 1);
			else
				mvert_to_particle(keys + 3, pind->mvert[1], pind->hkey[1]);
		}
		else {
			if (pind->hkey[1] != pa->hair + pa->totkey - 1)
				hair_to_particle(keys + 3, pind->hkey[1] + 1);
			else
				hair_to_particle(keys + 3, pind->hkey[1]);
		}
	}

	dfra = keys[2].time - keys[1].time;
	keytime = (real_t - keys[1].time) / dfra;

	/* convert velocity to timestep size */
	if (pind->keyed || pind->cache || point_vel) {
		invdt = dfra * 0.04f * (psys ? psys->part->timetweak : 1.f);
		mul_v3_fl(keys[1].vel, invdt);
		mul_v3_fl(keys[2].vel, invdt);
		interp_qt_qtqt(result->rot, keys[1].rot, keys[2].rot, keytime);
	}

	/* now we should have in chronologiacl order k1<=k2<=t<=k3<=k4 with keytime between [0, 1]->[k2, k3] (k1 & k4 used for cardinal & bspline interpolation)*/
	psys_interpolate_particle((pind->keyed || pind->cache || point_vel) ? -1 /* signal for cubic interpolation */
	                          : (pind->bspline ? KEY_BSPLINE : KEY_CARDINAL),
	                          keys, keytime, result, 1);

	/* the velocity needs to be converted back from cubic interpolation */
	if (pind->keyed || pind->cache || point_vel)
		mul_v3_fl(result->vel, 1.f / invdt);
}

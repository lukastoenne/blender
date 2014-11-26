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

/** \file blender/editors/hair/hair_particles.c
 *  \ingroup edhair
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "DNA_object_types.h"
#include "DNA_particle_types.h"

#include "BKE_edithair.h"
#include "BKE_particle.h"

/* ==== convert particle data to hair edit ==== */

static void copy_edit_curve(HairEditData *hedit, HairEditCurve *curve, ParticleData *pa)
{
	int totverts = pa->totkey;
	HairEditVertex *vert;
	HairKey *hkey;
	int k;
	
	vert = BKE_edithair_curve_extend(hedit, curve, NULL, totverts);
	for (k = 0, hkey = pa->hair;
	     k < totverts;
	     ++k, ++hkey, vert = vert->next) {
		
		copy_v3_v3(vert->co, hkey->co);
		
		// TODO define other key stuff ...
	}
}

void BKE_edithair_from_particles(HairEditData *hedit, Object *UNUSED(ob), ParticleSystem *psys)
{
	HairEditCurve *curve;
	ParticleData *pa;
	int p;
	
	BKE_edithair_clear(hedit);
	
	for (p = 0, pa = psys->particles;
	     p < psys->totpart;
	     ++p, ++pa) {
		
		curve = BKE_edithair_curve_create(hedit, NULL);
		
		copy_edit_curve(hedit, curve, pa);
		
		// TODO copy particle stuff ...
	}
}

/* ==== convert hair edit to particle data ==== */

static void free_particle_data(ParticleSystem *psys)
{
	if (psys->particles) {
		ParticleData *pa;
		int p;
		
		for (p = 0, pa = psys->particles; p < psys->totpart; ++p, ++pa) {
			if (pa->hair)
				MEM_freeN(pa->hair);
		}
		
		MEM_freeN(psys->particles);
		psys->particles = NULL;
		psys->totpart = 0;
	}
}

static void create_particle_curve(ParticleSystem *psys, ParticleData *pa, HairEditData *hedit, HairEditCurve *curve)
{
	int ntotkey = BKE_edithair_curve_vertex_count(hedit, curve);
	HairKey *nhair = MEM_callocN(sizeof(HairKey) * ntotkey, "hair keys");
	
	HairEditIter iter;
	HairEditVertex *vert;
	HairKey *hkey;
	int k;
	
	pa->alive = PARS_ALIVE;
	pa->flag = 0;
	
	pa->time = 0.0f;
	pa->lifetime = 100.0f;
	pa->dietime = 100.0f;
	
	pa->fuv[0] = 1.0f;
	pa->fuv[1] = 0.0f;
	pa->fuv[2] = 0.0f;
	pa->fuv[3] = 0.0f;
	
	pa->size = psys->part->size;
	
	k = 0;
	hkey = nhair;
	HAIREDIT_ITER_ELEM(vert, &iter, curve, HAIREDIT_VERTS_OF_CURVE) {
		copy_v3_v3(hkey->co, vert->co);
		hkey->time = ntotkey > 0 ? (float)k / (float)(ntotkey - 1) : 0.0f;
		hkey->weight = 1.0f;
		// TODO define other key stuff ...
		
		++k;
		++hkey;
	}
	
	pa->totkey = ntotkey;
	pa->hair = nhair;
}

static void create_particle_data(ParticleSystem *psys, HairEditData *hedit)
{
	int ntotpart = hedit->totcurves;
	ParticleData *nparticles = MEM_callocN(sizeof(ParticleData) * ntotpart, "particle data");
	
	HairEditCurve *curve;
	HairEditIter iter;
	ParticleData *pa;
	int p;
	
	p = 0;
	pa = nparticles;
	HAIREDIT_ITER(curve, &iter, hedit, HAIREDIT_CURVES_OF_MESH) {
		
		// TODO copy particle stuff ...
		
		create_particle_curve(psys, pa, hedit, curve);
		
		++p;
		++pa;
	}
	
	psys->particles = nparticles;
	psys->totpart = hedit->totcurves;
}

void BKE_edithair_to_particles(HairEditData *hedit, Object *UNUSED(ob), ParticleSystem *psys)
{
	free_particle_data(psys);
	create_particle_data(psys, hedit);
}

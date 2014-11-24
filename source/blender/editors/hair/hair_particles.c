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

#include "BKE_particle.h"

#include "hair_intern.h"

/* ==== convert particle data to hair edit ==== */

static int particle_totverts(ParticleSystem *psys)
{
	ParticleData *pa;
	int p;
	int totverts = 0;
	
	for (p = 0, pa = psys->particles; p < psys->totpart; ++p, ++pa)
		totverts += pa->totkey;
	
	return totverts;
}

static void copy_edit_curve(HairEditData *hedit, HairEditCurve *curve, ParticleData *pa, int start)
{
	int totverts = pa->totkey;
	HairKey *hair = pa->hair;
	HairEditVertex *vert;
	HairKey *hkey;
	int k;
	
	BLI_assert(start + totverts <= hedit->alloc_verts);
	
	curve->start = start;
	curve->numverts = totverts;
	
	for (k = 0, vert = hedit->verts + start, hkey = hair;
	     k < totverts;
	     ++k, ++vert, ++hkey) {
		
		copy_v3_v3(vert->co, hkey->co);
		// TODO define other key stuff ...
	}
}

void hair_edit_from_particles(HairEditData *hedit, Object *UNUSED(ob), ParticleSystem *psys)
{
	int totverts = particle_totverts(psys);
	
	HairEditCurve *curve;
	int i;
	ParticleData *pa;
	int p;
	int vert_start;
	
	ED_hair_edit_clear(hedit);
	
	ED_hair_edit_reserve(hedit, psys->totpart, totverts, true);
	
	/* TODO we should have a clean input stream API for hair edit data
	 * to avoid implicit size and index calculations here and make the code
	 * as fool proof as possible.
	 */
	
	hedit->totcurves = psys->totpart;
	hedit->totverts = totverts;
	
	vert_start = 0;
	for (i = 0, curve = hedit->curves, p = 0, pa = psys->particles;
	     i < hedit->totcurves;
	     ++i, ++curve, ++p, ++pa) {
		
		copy_edit_curve(hedit, curve, pa, vert_start);
		
		// TODO copy particle stuff ...
		
		vert_start += curve->numverts;
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

static void create_particle_curve(ParticleData *pa, HairEditData *hedit, HairEditCurve *curve)
{
	int ntotkey = curve->numverts;
	HairKey *nhair = MEM_callocN(sizeof(HairKey) * ntotkey, "hair keys");
	HairEditVertex *vert;
	HairKey *hkey;
	int k;
	
	for (k = 0, vert = hedit->verts + curve->start, hkey = nhair;
	     k < curve->numverts;
	     ++k, ++vert, ++hkey) {
		
		copy_v3_v3(hkey->co, vert->co);
		// TODO define other key stuff ...
	}
	
	pa->totkey = ntotkey;
	pa->hair = nhair;
}

static void create_particle_data(ParticleSystem *psys, HairEditData *hedit)
{
	int ntotpart = hedit->totcurves;
	ParticleData *nparticles = MEM_callocN(sizeof(ParticleData) * ntotpart, "particle data");
	HairEditCurve *curve;
	int i;
	ParticleData *pa;
	int p;
	
	for (i = 0, curve = hedit->curves, p = 0, pa = nparticles;
	     i < hedit->totcurves;
	     ++i, ++curve, ++p, ++pa) {
		
		// TODO copy particle stuff ...
		
		create_particle_curve(pa, hedit, curve);
	}
	
	psys->particles = nparticles;
	psys->totpart = hedit->totcurves;
}

void hair_edit_to_particles(HairEditData *hedit, Object *UNUSED(ob), ParticleSystem *psys)
{
	psys->flag |= PSYS_EDITED;
	
	free_particle_data(psys);
	
	create_particle_data(psys, hedit);
}

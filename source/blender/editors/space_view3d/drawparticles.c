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
 * The Original Code is Copyright (C) 2013 by the Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/drawparticles.c
 *  \ingroup spview3d
 */

#include "DNA_nparticle_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_nparticle.h"

#include "BIF_gl.h"

#include "view3d_intern.h"

static void draw_particles(NParticleSystem *psys, const char *attr_pos)
{
	NParticleIterator it;
	float pos[3];
	
	if (!psys->state)
		return;
	
	glPointSize(2.0);
	
	glBegin(GL_POINTS);
	for (BKE_nparticle_iter_init(psys->state, &it); BKE_nparticle_iter_valid(&it); BKE_nparticle_iter_next(&it)) {
		BKE_nparticle_iter_get_vector(&it, attr_pos, pos);
		glVertex3fv(pos);
	}
	glEnd();
	
	glPointSize(1.0);
}

void draw_nparticles(NParticleSystem *psys, NParticleDisplay *display)
{
	switch (display->type) {
		case PAR_DISPLAY_PARTICLE:
			draw_particles(psys, display->attribute);
			break;
	}
}

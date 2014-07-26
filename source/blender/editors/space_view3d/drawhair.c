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
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/drawarmature.c
 *  \ingroup spview3d
 */

#include "DNA_hair_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_hair.h"
#include "BKE_modifier.h"

#include "view3d_intern.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

/* ******** Hair Drawing ******** */

/* TODO vertex/index buffers, etc. etc., avoid direct mode ... */

static void draw_hair_curve(HairSystem *UNUSED(hsys), HairCurve *hair)
{
	HairPoint *point;
	int k;
	
	glBegin(GL_LINE_STRIP);
	for (point = hair->points, k = 0; k < hair->totpoints; ++point, ++k) {
		glVertex3fv(point->co);
	}
	glEnd();
}

/* called from drawobject.c, return true if nothing was drawn */
bool draw_hair_system(Scene *UNUSED(scene), View3D *UNUSED(v3d), ARegion *ar, Base *base, HairSystem *hsys)
{
	RegionView3D *rv3d = ar->regiondata;
	Object *ob = base->object;
	HairCurve *hair;
	int i;
	bool retval = true;
	
	glLoadMatrixf(rv3d->viewmat);
	glMultMatrixf(ob->obmat);
	
	for (hair = hsys->curves, i = 0; i < hsys->totcurves; ++hair, ++i) {
		draw_hair_curve(hsys, hair);
	}
	
	return retval;
}

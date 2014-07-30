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
#include "BKE_mesh_sample.h"
#include "BKE_modifier.h"

#include "view3d_intern.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_resources.h"

#include "HAIR_capi.h"

/* ******** Hair Drawing ******** */

/* TODO vertex/index buffers, etc. etc., avoid direct mode ... */

static void draw_hair_curve(HairSystem *hsys, HairCurve *hair)
{
	HairPoint *point;
	int k;
	
	glColor3f(0.4f, 0.7f, 1.0f);
	
	glBegin(GL_LINE_STRIP);
	for (point = hair->points, k = 0; k < hair->totpoints; ++point, ++k) {
		glVertex3fv(point->co);
	}
	glEnd();
	
#if 0
	/* frames */
	{
		struct HAIR_FrameIterator *iter;
		float co[3], nor[3], tan[3], cotan[3];
		int k = 0;
		const float scale = 0.1f;
		
		glBegin(GL_LINES);
		iter = HAIR_frame_iter_new(hair, 1.0f / hair->totpoints, hsys->smooth, nor, tan, cotan);
		copy_v3_v3(co, hair->points[0].co);
		mul_v3_fl(nor, scale);
		mul_v3_fl(tan, scale);
		mul_v3_fl(cotan, scale);
		add_v3_v3(nor, co);
		add_v3_v3(tan, co);
		add_v3_v3(cotan, co);
		++k;
		
		glColor3f(1.0f, 0.0f, 0.0f);
		glVertex3fv(co);
		glVertex3fv(nor);
		glColor3f(0.0f, 1.0f, 0.0f);
		glVertex3fv(co);
		glVertex3fv(tan);
		glColor3f(0.0f, 0.0f, 1.0f);
		glVertex3fv(co);
		glVertex3fv(cotan);
		
		while (HAIR_frame_iter_valid(hair, iter)) {
			HAIR_frame_iter_next(hair, iter, nor, tan, cotan);
			copy_v3_v3(co, hair->points[k].co);
			mul_v3_fl(nor, scale);
			mul_v3_fl(tan, scale);
			mul_v3_fl(cotan, scale);
			add_v3_v3(nor, co);
			add_v3_v3(tan, co);
			add_v3_v3(cotan, co);
			++k;
			
			glColor3f(1.0f, 0.0f, 0.0f);
			glVertex3fv(co);
			glVertex3fv(nor);
			glColor3f(0.0f, 1.0f, 0.0f);
			glVertex3fv(co);
			glVertex3fv(tan);
			glColor3f(0.0f, 0.0f, 1.0f);
			glVertex3fv(co);
			glVertex3fv(cotan);
		}
		HAIR_frame_iter_free(iter);
		glEnd();
	}
#endif
	
#if 0
	/* smoothed curve */
	if (hair->totpoints >= 2) {
		struct HAIR_SmoothingIteratorFloat3 *iter;
		float smooth_co[3];
		
		glColor3f(0.5f, 1.0f, 0.1f);
		
		glBegin(GL_LINE_STRIP);
		iter = HAIR_smoothing_iter_new(hair, 1.0f / hair->totpoints, hsys->smooth, smooth_co);
		glVertex3fv(smooth_co);
		while (HAIR_smoothing_iter_valid(hair, iter)) {
			HAIR_smoothing_iter_next(hair, iter, smooth_co);
			glVertex3fv(smooth_co);
		}
		HAIR_smoothing_iter_end(hair, iter, smooth_co);
		glVertex3fv(smooth_co);
		HAIR_smoothing_iter_free(iter);
		glEnd();
		
		glPointSize(2.5f);
		glBegin(GL_POINTS);
		iter = HAIR_smoothing_iter_new(hair, 1.0f / hair->totpoints, hsys->smooth, smooth_co);
		glVertex3fv(smooth_co);
		while (HAIR_smoothing_iter_valid(hair, iter)) {
			HAIR_smoothing_iter_next(hair, iter, smooth_co);
			glVertex3fv(smooth_co);
		}
		HAIR_smoothing_iter_end(hair, iter, smooth_co);
		glVertex3fv(smooth_co);
		HAIR_smoothing_iter_free(iter);
		glEnd();
		glPointSize(1.0f);
	}
#endif
}

/* called from drawobject.c, return true if nothing was drawn */
bool draw_hair_system(Scene *UNUSED(scene), View3D *UNUSED(v3d), ARegion *ar, Base *base, HairSystem *hsys)
{
	RegionView3D *rv3d = ar->regiondata;
	Object *ob = base->object;
	struct DerivedMesh *dm = ob->derivedFinal;
	HairCurve *hair;
	int i;
	bool retval = true;
	
	glLoadMatrixf(rv3d->viewmat);
	glMultMatrixf(ob->obmat);
	
	/* hair roots */
	if (dm) {
		glPointSize(3.0f);
		glColor3f(1.0f, 1.0f, 0.0f);
		glBegin(GL_LINES);
		for (hair = hsys->curves, i = 0; i < hsys->totcurves; ++hair, ++i) {
			float loc[3], nor[3];
			if (BKE_mesh_sample_eval(dm, &hair->root, loc, nor)) {
				glVertex3f(loc[0], loc[1], loc[2]);
				madd_v3_v3fl(loc, nor, 0.1f);
				glVertex3f(loc[0], loc[1], loc[2]);
			}
		}
		glEnd();
	}
	
	for (hair = hsys->curves, i = 0; i < hsys->totcurves; ++hair, ++i) {
		draw_hair_curve(hsys, hair);
	}
	
	return retval;
}

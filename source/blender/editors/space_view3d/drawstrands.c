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
 * The Original Code is Copyright (C) 2014 by the Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/drawsimdebug.c
 *  \ingroup spview3d
 */

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_strands.h"

#include "view3d_intern.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_resources.h"

typedef struct StrandsDrawGLState {
	GLint polygonmode[2];
} StrandsDrawGLState;

static void draw_strands_begin(StrandsDrawGLState *state, short dflag)
{
	glGetIntegerv(GL_POLYGON_MODE, state->polygonmode);
	glEnableClientState(GL_VERTEX_ARRAY);
	
	/* setup gl flags */
	glEnableClientState(GL_NORMAL_ARRAY);
	
	if ((dflag & DRAW_CONSTCOLOR) == 0) {
//		if (part->draw_col == PART_DRAW_COL_MAT)
//			glEnableClientState(GL_COLOR_ARRAY);
	}
	
	glColor3f(1,1,1);
	glEnable(GL_LIGHTING);
//	glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
//	glEnable(GL_COLOR_MATERIAL);
}

static void draw_strands_end(StrandsDrawGLState *state)
{
	/* restore & clean up */
//	if (part->draw_col == PART_DRAW_COL_MAT)
//		glDisableClientState(GL_COLOR_ARRAY);
	glDisable(GL_LIGHTING);
	glDisable(GL_COLOR_MATERIAL);
	
	glLineWidth(1.0f);
	
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	
	glPolygonMode(GL_FRONT, state->polygonmode[0]);
	glPolygonMode(GL_BACK, state->polygonmode[1]);
}

static void draw_strand_lines(Strands *strands, short dflag)
{
	const bool has_motion_state = strands->state;
	StrandsDrawGLState gl_state;
	StrandIterator it_strand;
	
	draw_strands_begin(&gl_state, dflag);
	
	for (BKE_strand_iter_init(&it_strand, strands); BKE_strand_iter_valid(&it_strand); BKE_strand_iter_next(&it_strand)) {
		if (it_strand.tot <= 0)
			continue;
		
		if (has_motion_state) {
			glVertexPointer(3, GL_FLOAT, sizeof(StrandsMotionState), it_strand.state->co);
			glNormalPointer(GL_FLOAT, sizeof(StrandsMotionState), it_strand.state->nor);
		}
		else {
			glVertexPointer(3, GL_FLOAT, sizeof(StrandsVertex), it_strand.verts->co);
			glNormalPointer(GL_FLOAT, sizeof(StrandsVertex), it_strand.verts->nor);
		}
		if ((dflag & DRAW_CONSTCOLOR) == 0) {
//			if (part->draw_col == PART_DRAW_COL_MAT) {
//				glColorPointer(3, GL_FLOAT, sizeof(ParticleCacheKey), path->col);
//			}
		}
		
		glDrawArrays(GL_LINE_STRIP, 0, it_strand.curve->numverts);
	}
	
	draw_strands_end(&gl_state);
}

static void draw_strand_child_lines(StrandsChildren *children, short dflag)
{
	StrandsDrawGLState gl_state;
	StrandChildIterator it_strand;
	
	draw_strands_begin(&gl_state, dflag);
	
	for (BKE_strand_child_iter_init(&it_strand, children); BKE_strand_child_iter_valid(&it_strand); BKE_strand_child_iter_next(&it_strand)) {
		if (it_strand.tot <= 0)
			continue;
		
		glVertexPointer(3, GL_FLOAT, sizeof(StrandsChildVertex), it_strand.verts->co);
		glNormalPointer(GL_FLOAT, sizeof(StrandsChildVertex), it_strand.verts->nor);
		
		if ((dflag & DRAW_CONSTCOLOR) == 0) {
//			if (part->draw_col == PART_DRAW_COL_MAT) {
//				glColorPointer(3, GL_FLOAT, sizeof(ParticleCacheKey), path->col);
//			}
		}
		
		glDrawArrays(GL_LINE_STRIP, 0, it_strand.curve->numverts);
	}
	
	draw_strands_end(&gl_state);
}

void draw_strands(Scene *UNUSED(scene), View3D *UNUSED(v3d), ARegion *ar, Object *ob, Strands *strands, StrandsChildren *children, short dflag)
{
	RegionView3D *rv3d = ar->regiondata;
	float imat[4][4];
	
	invert_m4_m4(imat, rv3d->viewmatob);
	
//	glDepthMask(GL_FALSE);
//	glEnable(GL_BLEND);
	
	glPushMatrix();
	
	glLoadMatrixf(rv3d->viewmat);
	glMultMatrixf(ob->obmat);
	
	if (children)
		draw_strand_child_lines(children, dflag);
	else
		draw_strand_lines(strands, dflag);
	
	glPopMatrix();
	
//	glDepthMask(GL_TRUE);
//	glDisable(GL_BLEND);
}

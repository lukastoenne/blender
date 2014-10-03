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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/interface_generic_widgets.c
 *  \ingroup edinterface
 */

#include "WM_types.h"

#include "DNA_screen_types.h"
#include "DNA_lamp_types.h"
#include "DNA_object_types.h"
#include "DNA_lamp_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_utildefines.h"
#include "BLI_math_matrix.h"
#include "BLI_math.h"

#include "BKE_context.h"

#include "ED_view3d.h"

#include "WM_types.h"

#include "GL/glew.h"
#include "GPU_select.h"

#include "BIF_glutil.h"

#include "UI_interface.h"

int WIDGET_lamp_handler(struct bContext *C, const struct wmEvent *event, struct wmWidget *widget, int active)
{	
	return OPERATOR_FINISHED;
}

static void intern_lamp_draw(const struct bContext *C, int selectoffset, wmWidget *widget)
{
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;
	float size = 0.30f;
	int highlight = widget->active_handle;

	Object *ob = CTX_data_active_object(C);	
	Lamp *la = ob->data;

	float widgetmat[4][4];	

	copy_m4_m4(widgetmat, ob->obmat);	
	normalize_m4(widgetmat);
	glPushMatrix();
	glMultMatrixf(&widgetmat[0][0]);

	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);

	if (selectoffset != -1)
		GPU_select_load_id(selectoffset + 1);
	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	
	if (highlight == 1)
		glColor4f(1.0, 1.0, 0.4, 1.0);
	else
		glColor4f(1.0, 1.0, 1.0, 1.0);
	glVertex3f(0.0, 0.0, 0.0);
	if (highlight == 1)
		glColor4f(1.0, 1.0, 2.0, 1.0);
	else
		glColor4f(0.0, 0.0, 0.0, 0.0);
	glVertex3f(0.0, 0.0, -la->dist);
	glEnd();	

	glDisable(GL_BLEND);	
	
	glPopMatrix();
	mul_mat3_m4_fl(widgetmat, ED_view3d_pixel_size(rv3d, widgetmat[3]) * U.tw_size);	
	
	glPushMatrix();
	glMultMatrixf(&widgetmat[0][0]);

	if (highlight == 2)
		glColor4f(1.0, 1.0, 1.0, 1.0);
	else
		glColor4f(0.8, 0.8, 0.8, 1.0);

	if (selectoffset != -1)
		GPU_select_load_id(selectoffset + 2);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glBegin(GL_QUAD_STRIP);
	glVertex3f(size, size, 3.0 * size);
	glVertex3f(-size, size, 3.0 * size);
	glVertex3f(size, -size, 3.0 * size);
	glVertex3f(-size, -size, 3.0 * size);
	glVertex3f(size, -size, 0.0);
	glVertex3f(-size, -size, 0.0);
	glVertex3f(size, size, 0.0);
	glVertex3f(-size, size, 0.0);
	glVertex3f(size, size, 3.0 * size);
	glVertex3f(-size, size, 3.0 * size);
	glEnd();
	
	glPushMatrix();
	glTranslatef(size, 0.0f, 0.0f);	
	glRotatef(-90.0f * la->spotsize / M_PI, 0.0f, 1.0f, 0.0f);
	
	glBegin(GL_QUADS);
	glVertex3f(0.0f, -size, 0.0);
	glVertex3f(0.0f, size, 0.0);
	glVertex3f(0.0f, size, -size);
	glVertex3f(0.0f, -size, -size);	
	glEnd();
	glPopMatrix();	

	glPushMatrix();
	glTranslatef(-size, 0.0f, 0.0f);
	glRotatef(90.0f * la->spotsize / M_PI, 0.0f, 1.0f, 0.0f);
	
	glBegin(GL_QUADS);
	glVertex3f(0.0f, -size, 0.0);
	glVertex3f(0.0f, size, 0.0);
	glVertex3f(0.0f, size, -size);
	glVertex3f(0.0f, -size, -size);	
	glEnd();
	glPopMatrix();	

	glPushMatrix();
	glTranslatef(0.0f, size, 0.0f);
	glRotatef(90.0f * la->spotsize / M_PI, 1.0f, 0.0f, 0.0f);
	
	glBegin(GL_QUADS);
	glVertex3f(-size, 0.0f, 0.0f);
	glVertex3f(size, 0.0f, 0.0f);
	glVertex3f(size, 0.0f, -size);
	glVertex3f(-size, 0.0f, -size);	
	glEnd();
	glPopMatrix();	

	glPushMatrix();
	glTranslatef(0.0f, -size, 0.0f);
	glRotatef(-90.0f * la->spotsize / M_PI, 1.0f, 0.0f, 0.0f);
	
	glBegin(GL_QUADS);
	glVertex3f(-size, 0.0f, 0.0f);
	glVertex3f(size, 0.0f, 0.0f);
	glVertex3f(size, 0.0f, -size);
	glVertex3f(-size, 0.0f, -size);	
	glEnd();
	glPopMatrix();	
	
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	
	glPopMatrix();	
}

void WIDGET_lamp_render_3d_intersect(const struct bContext *C, struct wmWidget *widget, int selectionbase)
{
	intern_lamp_draw(C, selectionbase, widget);	
}

void WIDGET_lamp_draw(const struct bContext *C, wmWidget *widget)
{
	intern_lamp_draw(C, -1, widget);
}

bool WIDGET_lamp_poll(const struct bContext *C, struct wmWidget *UNUSED(widget))
{
	Object *ob = CTX_data_active_object(C);
	
	if (ob && ob->type == OB_LAMP) {
		Lamp *la = ob->data;
		return (la->type == LA_SPOT);      
	}
	return false;
}

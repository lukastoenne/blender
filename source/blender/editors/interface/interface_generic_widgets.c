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

#include "BLI_utildefines.h"
#include "BLI_math_matrix.h"

#include "BKE_context.h"

#include "ED_view3d.h"

#include "WM_types.h"

#include "GL/glew.h"

#include "UI_interface.h"

int WIDGET_lamp_handler(struct bContext *C, const struct wmEvent *event, struct wmWidget *widget, int active)
{
	return 0;
}

void WIDGET_lamp_render_3d_intersect(const struct bContext *C, struct wmWidget *widget, int selectionbase)
{
	
}

void WIDGET_lamp_draw(const struct bContext *C, struct wmWidget *widget)
{
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;

	Object *ob = CTX_data_active_object(C);	
	Lamp *la = ob->data;

	float widgetmat[4][4];	

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	copy_m4_m4(widgetmat, ob->obmat);	
	normalize_m4(widgetmat);
	mul_mat3_m4_fl(widgetmat, ED_view3d_pixel_size(rv3d, widgetmat[3]) * U.tw_size);	
	
	glMultMatrixf(&widgetmat[0][0]);
	
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glShadeModel(GL_SMOOTH);
	glBegin(GL_TRIANGLES);
	glColor3f(1.0, 0.0, 0.0);
	glVertex3f(0.0, 0.0, 1.0);
	glColor3f(0.0, 1.0, 0.0);
	glVertex3f(-1.0, 0.0, -1.0);
	glColor3f(0.0, 0.0, 1.0);
	glVertex3f(1.0, 0.0, -1.0);
	glEnd();	
	
	glPopMatrix();
}

bool WIDGET_lamp_poll(const struct bContext *C, struct wmWidget *UNUSED(widget))
{
	Object *ob = CTX_data_active_object(C);
	
	if (ob->type == OB_LAMP) {
		Lamp *la = ob->data;
		return (la->type == LA_SPOT);      
	}
	return false;
}

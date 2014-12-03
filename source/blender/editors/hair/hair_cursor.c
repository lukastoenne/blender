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

/** \file blender/editors/hair/hair_cursor.c
 *  \ingroup edhair
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "DNA_brush_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_view3d.h"

#include "hair_intern.h"

static void hair_draw_cursor(bContext *C, int x, int y, void *UNUSED(customdata))
{
	Scene *scene = CTX_data_scene(C);
	UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
	HairEditSettings *settings = &scene->toolsettings->hair_edit;
	Brush *brush = settings->brush;
	
	float final_radius;
	float translation[2];
	float outline_alpha, *outline_col;
	
	if (!brush)
		return;
	
	final_radius = BKE_brush_size_get(scene, brush);
	
	/* set various defaults */
	translation[0] = x;
	translation[1] = y;
	outline_alpha = 0.5;
	outline_col = brush->add_col;
	
	/* make lines pretty */
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);

	/* set brush color */
	glColor4f(outline_col[0], outline_col[1], outline_col[2], outline_alpha);

	/* draw brush outline */
	glTranslatef(translation[0], translation[1], 0);

	/* draw an inner brush */
	if (ups->stroke_active && BKE_brush_use_size_pressure(scene, brush)) {
		/* inner at full alpha */
		glutil_draw_lined_arc(0.0, M_PI*2.0f, final_radius * ups->size_pressure_value, 40);
		/* outer at half alpha */
		glColor4f(outline_col[0], outline_col[1], outline_col[2], outline_alpha * 0.5f);
	}
	glutil_draw_lined_arc(0.0, M_PI*2.0f, final_radius, 40);
	glTranslatef(-translation[0], -translation[1], 0);

	/* restore GL state */
	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}

void hair_edit_cursor_start(bContext *C, int (*poll)(bContext *C))
{
	Scene *scene = CTX_data_scene(C);
	HairEditSettings *settings = &scene->toolsettings->hair_edit;
	
	if (!settings->paint_cursor)
		settings->paint_cursor = WM_paint_cursor_activate(CTX_wm_manager(C), poll, hair_draw_cursor, NULL);
}

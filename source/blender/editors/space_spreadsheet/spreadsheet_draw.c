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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_node/spreadsheet_draw.c
 *  \ingroup spspreadsheet
 */

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_math.h"
#include "BLI_rect.h"

#include "BLT_translation.h"

#include "BKE_context.h"

#include "BLF_api.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_gpencil.h"
#include "ED_space_api.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"

#include "spreadsheet_intern.h"  /* own include */


int spreadsheet_row_height(void)
{
	return U.widget_unit + 3;
}

void spreadsheet_get_size(const bContext *UNUSED(C), int *width, int *height)
{
	// TODO
	if (width) *width = 32000;
	if (height) *height = 32000;
}

static int y_to_row(float y)
{
	const int H = spreadsheet_row_height();
	return (int)ceilf(-y / (float)H);
}

static float row_to_y(int row)
{
	const int H = spreadsheet_row_height();
	return -row * (float)H;
}

void spreadsheet_draw_grease_pencil(const bContext *C, bool onlyv2d)
{
	ED_gpencil_draw_view2d(C, onlyv2d);
}

void spreadsheet_draw_main(const bContext *C, SpaceSpreadsheet *ssheet, ARegion *ar)
{
	const int H = spreadsheet_row_height();
	
	View2D *v2d = &ar->v2d;
	const rctf *rect = &v2d->cur;
	int row_first = y_to_row(rect->ymax);
	int row_last = y_to_row(rect->ymin);
	int row;
	
	glColor3f(1.0f, 1.0f, 0.0f);
	glBegin(GL_LINES);
	for (row = row_first; row <= row_last; ++row) {
		float y = row_to_y(row);
		
		glVertex2f(10.0f * (row % 5), y);
		glVertex2f(10.0f * (row % 5), y + H);
	}
	glEnd();
	
//	UI_block_begin()
//	UI_block_layout()
}

void spreadsheet_set_cursor(wmWindow *win, SpaceSpreadsheet *ssheet, const float cursor[2])
{
	int wmcursor = CURSOR_STD;
	
	WM_cursor_set(win, wmcursor);
}

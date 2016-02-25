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
#include "BLI_string.h"

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

typedef void (*CellDrawFn)(uiLayout *layout, int row);

static void draw_rows(uiBlock *block, int row_first, int row_last, float x, float width, CellDrawFn cell_draw)
{
	const int H = spreadsheet_row_height();
	
	int i;
	
	glBegin(GL_QUADS);
	for (i = row_first; i <= row_last; ++i) {
		float y = row_to_y(i);
		
		{
			bool even = (i % 2 == 0);
			if (even)
				glColor3f(1.0f, 1.0f, 0.0f);
			else
				glColor3f(1.0f, 0.4f, 8.0f);
		
			glVertex2f(x, y);
			glVertex2f(x, y + H);
			glVertex2f(x + width, y + H);
			glVertex2f(x + width, y);
		}
		
		{
			uiLayout *layout;
			
			layout = UI_block_layout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL,
			                         x, y, width, H, 0, UI_style_get());
			cell_draw(layout, i);
		}
	}
	glEnd();
}

static void cell_draw_index(uiLayout *layout, int row)
{
	char buf[256];
	
	uiLayoutSetAlignment(layout, UI_LAYOUT_ALIGN_RIGHT);
	
	BLI_snprintf(buf, sizeof(buf), "%d", row+1);
	uiItemL(layout, buf, ICON_NONE);
}

#include "BLI_ghash.h"
static void cell_draw_fake(uiLayout *layout, int row, int col)
{
	char buf[256];
	unsigned int a = BLI_ghashutil_uinthash(col + 1);
	unsigned int b = row + a*a;
	unsigned int c = BLI_ghashutil_uinthash(b);
	unsigned int d = c % 16;
	unsigned int e = c % (1 << d);
	BLI_snprintf(buf, sizeof(buf), "%d", (int)e);
	uiItemL(layout, buf, ICON_NONE);
}

static void cell_draw_fake1(uiLayout *layout, int row) { cell_draw_fake(layout, row, 0); }
static void cell_draw_fake2(uiLayout *layout, int row) { cell_draw_fake(layout, row, 1); }
static void cell_draw_fake3(uiLayout *layout, int row) { cell_draw_fake(layout, row, 2); }
static void cell_draw_fake4(uiLayout *layout, int row) { cell_draw_fake(layout, row, 3); }
static void cell_draw_fake5(uiLayout *layout, int row) { cell_draw_fake(layout, row, 4); }

void spreadsheet_draw_main(const bContext *C, SpaceSpreadsheet *UNUSED(ssheet), ARegion *ar)
{
	const float index_width = 50.0f;
	const float column_width = 100.0f;
	
	View2D *v2d = &ar->v2d;
	const rctf *rect = &v2d->cur;
	int row_first = y_to_row(rect->ymax);
	int row_last = y_to_row(rect->ymin);
	uiBlock *index_block, *data_block;
	
	/* block for data column */
	data_block = UI_block_begin(C, CTX_wm_region(C), "spreadsheet data table", UI_EMBOSS);
	/* block for indexing column */
	index_block = UI_block_begin(C, CTX_wm_region(C), "spreadsheet index column", UI_EMBOSS);
	/* block ui events on the index block: hides data fields behind it */
	UI_block_flag_enable(index_block, UI_BLOCK_CLIP_EVENTS);
	
	draw_rows(data_block, row_first, row_last, index_width, column_width, cell_draw_fake1);
	
	UI_block_layout_resolve(data_block, NULL, NULL);
	UI_block_end(C, data_block);
	UI_block_draw(C, data_block);
	
	draw_rows(index_block, row_first, row_last, rect->xmin, index_width, cell_draw_index);
	
	UI_block_layout_resolve(index_block, NULL, NULL);
	UI_block_end(C, index_block);
	UI_block_draw(C, index_block);
}

void spreadsheet_set_cursor(wmWindow *win, SpaceSpreadsheet *UNUSED(ssheet), const float UNUSED(cursor[2]))
{
	int wmcursor = CURSOR_STD;
	
	WM_cursor_set(win, wmcursor);
}

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

static int y_to_row(int y)
{
	const int H = spreadsheet_row_height();
	return (-y + H - 1) / H;
}

static int row_to_y(int row)
{
	const int H = spreadsheet_row_height();
	return -(row * H);
}

void spreadsheet_draw_grease_pencil(const bContext *C, bool onlyv2d)
{
	ED_gpencil_draw_view2d(C, onlyv2d);
}

static void draw_background_quad(int x, int y, int width, int shade_offset)
{
	const int H = spreadsheet_row_height();
	
	UI_ThemeColorShade(TH_BACK, shade_offset);
	
	glVertex2i(x, y);
	glVertex2i(x, y - H);
	glVertex2i(x + width, y - H);
	glVertex2i(x + width, y);
}

static void draw_background_rows(int row_first, int row_last, int x, int width)
{
	int i;
	
	glBegin(GL_QUADS);
	for (i = row_first; i <= row_last; ++i) {
		int y = row_to_y(i);
		
		bool even = (i % 2 == 0);
		draw_background_quad(x, y, width, even? 0: -20);
	}
	glEnd();
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

static void draw_data_columns(const bContext *C, SpaceSpreadsheet *UNUSED(ssheet),
                              SpreadsheetDataField *UNUSED(fields), int num_fields,
                              View2D *UNUSED(v2d), int row_first, int row_last)
{
	const int H = spreadsheet_row_height();
	const int index_width = 50;
	const int column_width = 100;
	const int x = index_width;
	int width; /* calculated from data fields below */
	
	uiBlock *block;
	int r, c;
	
	width = 0.0f;
	for (c = 0; c < num_fields; ++c) {
		/* TODO this will make more sense with variable field width */
		width += column_width;
	}
	
	/* block for data column */
	block = UI_block_begin(C, CTX_wm_region(C), "spreadsheet data table", UI_EMBOSS);
	
	for (r = row_first; r <= row_last; ++r) {
		const int y = row_to_y(r);
		uiLayout *layout, *row;
		
		layout = UI_block_layout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL,
		                         x, y, width, H, 0, UI_style_get());
		
		row = uiLayoutRow(layout, false);
		
		for (c = 0; c < num_fields; ++c) {
			cell_draw_fake(row, r, c);
		}
	}
	
	UI_block_layout_resolve(block, NULL, NULL);
	UI_block_end(C, block);
	
	/* background drawing */
	draw_background_rows(row_first, row_last, x, width);
	/* buttons drawing */
	UI_block_draw(C, block);
}

static void draw_header_row(const bContext *C, SpaceSpreadsheet *UNUSED(ssheet),
                            SpreadsheetDataField *fields, int num_fields,
                            View2D *v2d)
{
	const rctf *rect = &v2d->cur;
	const int H = spreadsheet_row_height();
	const int index_width = 50.0f;
	const int column_width = 100.0f;
	const int x = index_width;
	const int y = rect->ymax;
	int width; /* calculated from data fields below */
	
	uiBlock *block;
	uiLayout *layout, *row;
	int c;
	
	width = 0.0f;
	for (c = 0; c < num_fields; ++c) {
		/* TODO this will make more sense with variable field width */
		width += column_width;
	}
	
	/* block for fields */
	block = UI_block_begin(C, CTX_wm_region(C), "spreadsheet header row", UI_EMBOSS);
	
	layout = UI_block_layout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL,
	                         x, y, width, H, 0, UI_style_get());
	
	row = uiLayoutRow(layout, false);
	
	for (c = 0; c < num_fields; ++c) {
		const char *name = RNA_property_ui_name(fields[c].prop);
		uiItemL(row, name, ICON_NONE);
	}
	
	UI_block_layout_resolve(block, NULL, NULL);
	UI_block_end(C, block);
	
	/* background drawing */
	glBegin(GL_QUADS);
	draw_background_quad(x, y, width, 0);
	glEnd();
	/* buttons drawing */
	UI_block_draw(C, block);
}

static void draw_index_column(const bContext *C, SpaceSpreadsheet *UNUSED(ssheet), View2D *v2d, int row_first, int row_last)
{
	const rctf *rect = &v2d->cur;
	const int H = spreadsheet_row_height();
	const int index_width = 50.0f;
	const int x = rect->xmin;
	const int width = index_width;
	
	uiBlock *block;
	int i;
	
	/* block for indexing column */
	block = UI_block_begin(C, CTX_wm_region(C), "spreadsheet index column", UI_EMBOSS);
	/* block ui events on the index block: hides data fields behind it */
	UI_block_flag_enable(block, UI_BLOCK_CLIP_EVENTS);
	
	for (i = row_first; i <= row_last; ++i) {
		const int y = row_to_y(i);
		uiLayout *layout;
		char buf[64];
		
		layout = UI_block_layout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL,
		                         x, y, width, H, 0, UI_style_get());
		uiLayoutSetAlignment(layout, UI_LAYOUT_ALIGN_RIGHT);
		
		BLI_snprintf(buf, sizeof(buf), "%d", i+1);
		uiItemL(layout, buf, ICON_NONE);
	}
	
	UI_block_layout_resolve(block, NULL, NULL);
	UI_block_end(C, block);
	
	/* background drawing */
	draw_background_rows(row_first, row_last, x, width);
	/* buttons drawing */
	UI_block_draw(C, block);
}

void spreadsheet_draw_main(const bContext *C, SpaceSpreadsheet *ssheet, ARegion *ar)
{
	View2D *v2d = &ar->v2d;
	int row_first = y_to_row(v2d->cur.ymax);
	int row_last = y_to_row(v2d->cur.ymin);
	
	PointerRNA ptr;
	PropertyRNA *prop;
	SpreadsheetDataField fields[SPREADSHEET_MAX_FIELDS];
	int num_fields;
	
	if (!spreadsheet_get_data(C, ssheet, &ptr, &prop))
		return;
	num_fields = spreadsheet_get_data_fields(ssheet, &ptr, prop, fields, SPREADSHEET_MAX_FIELDS);
	
	draw_data_columns(C, ssheet, fields, num_fields, v2d, row_first, row_last);
	draw_header_row(C, ssheet, fields, num_fields, v2d);
	draw_index_column(C, ssheet, v2d, row_first, row_last);
}

void spreadsheet_set_cursor(wmWindow *win, SpaceSpreadsheet *UNUSED(ssheet), const float UNUSED(cursor[2]))
{
	int wmcursor = CURSOR_STD;
	
	WM_cursor_set(win, wmcursor);
}

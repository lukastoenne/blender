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

// XXX placeholders
static const int index_width = 50;
static int column_width(const SpreadsheetDataField *field)
{
	PropertyType type = RNA_property_type(field->prop);
	/* Note: RNA_property_array_length works with null ptr,
	 * will just return static array length then
	 */
	PointerRNA dummyptr;
	int array_length;
	int w;
	
	RNA_pointer_create(NULL, NULL, NULL, &dummyptr);
	array_length = RNA_property_array_length(&dummyptr, field->prop);
	
	switch (type) {
		case PROP_BOOLEAN:
			w = 50;
			break;
		case PROP_INT:
		case PROP_FLOAT:
			w = 80;
			break;
		case PROP_ENUM:
		case PROP_STRING:
			w = 120;
			break;
		
		default:
			w = 0;
			break;
	}
	
	if (array_length > 1)
		w *= array_length;
	
	return w;
}

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
	return (-y) / H;
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

static void draw_background_rows(int row_begin, int row_end, int x, int width)
{
	int i;
	
	glBegin(GL_QUADS);
	for (i = row_begin; i < row_end; ++i) {
		int y = row_to_y(i);
		
		bool even = (i % 2 == 0);
		draw_background_quad(x, y, width, even? 0: -20);
	}
	glEnd();
}

static void draw_data_columns(const bContext *C, SpaceSpreadsheet *UNUSED(ssheet),
                              PointerRNA *ptr, PropertyRNA *prop,
                              SpreadsheetDataField *fields, int num_fields,
                              View2D *UNUSED(v2d), int row_begin, int row_end)
{
	const int H = spreadsheet_row_height();
	const int x0 = index_width;
	int x, width; /* calculated from data fields below */
	
	uiBlock *block;
	int r, c;
	
	width = 0.0f;
	for (c = 0; c < num_fields; ++c) {
		/* TODO this will make more sense with variable field width */
		width += column_width(&fields[c]);
	}
	
	/* block for data column */
	block = UI_block_begin(C, CTX_wm_region(C), "spreadsheet data table", UI_EMBOSS);
	
	for (r = row_begin; r < row_end; ++r) {
		const int y = row_to_y(r);
		
		x = x0;
		for (c = 0; c < num_fields; ++c) {
			const int colw = column_width(&fields[c]);
			uiLayout *layout = UI_block_layout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL,
			                                   x, y, colw, H, 0, UI_style_get());
			uiLayout *row = uiLayoutRow(layout, false);
			PointerRNA dataptr;
			
			RNA_property_collection_lookup_int(ptr, prop, r, &dataptr);
//			uiItemFullR(row, &dataptr, fields[c].prop, -1, 0, UI_ITEM_R_EXPAND | UI_ITEM_R_NO_BG, "", ICON_NONE);
			uiItemFullR(row, &dataptr, fields[c].prop, -1, 0, UI_ITEM_R_EXPAND, "", ICON_NONE);
			
			x += colw;
		}
	}
	
	UI_block_layout_resolve(block, NULL, NULL);
	UI_block_end(C, block);
	
	/* background drawing */
	draw_background_rows(row_begin, row_end, x0, width);
	/* buttons drawing */
	UI_block_draw(C, block);
}

static void draw_header_row(const bContext *C, SpaceSpreadsheet *UNUSED(ssheet),
                            SpreadsheetDataField *fields, int num_fields,
                            View2D *v2d)
{
	const rctf *rect = &v2d->cur;
	const int H = spreadsheet_row_height();
	const int x0 = index_width;
	const int y0 = rect->ymax;
	int x, width; /* calculated from data fields below */
	
	uiBlock *block;
	int c;
	
	/* block for fields */
	block = UI_block_begin(C, CTX_wm_region(C), "spreadsheet header row", UI_EMBOSS);
	
	x = x0;
	width = 0;
	for (c = 0; c < num_fields; ++c) {
		const int colw = column_width(&fields[c]);
		uiLayout *layout = UI_block_layout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL,
		                         x, y0, colw, H, 0, UI_style_get());
		uiLayout *row = uiLayoutRow(layout, false);
		const char *name = RNA_property_ui_name(fields[c].prop);
		
		uiItemL(row, name, ICON_NONE);
		
		x += colw;
		/* TODO this will make more sense with variable field width */
		width += colw;
	}
	
	UI_block_layout_resolve(block, NULL, NULL);
	UI_block_end(C, block);
	
	/* background drawing */
	glBegin(GL_QUADS);
	draw_background_quad(x0, y0, width, 0);
	glEnd();
	/* buttons drawing */
	UI_block_draw(C, block);
}

static void draw_index_column(const bContext *C, SpaceSpreadsheet *UNUSED(ssheet),
                              View2D *v2d, int row_begin, int row_end)
{
	const rctf *rect = &v2d->cur;
	const int H = spreadsheet_row_height();
	const int x = rect->xmin;
	const int width = index_width;
	
	uiBlock *block;
	int i;
	
	/* block for indexing column */
	block = UI_block_begin(C, CTX_wm_region(C), "spreadsheet index column", UI_EMBOSS);
	/* block ui events on the index block: hides data fields behind it */
	UI_block_flag_enable(block, UI_BLOCK_CLIP_EVENTS);
	
	for (i = row_begin; i < row_end; ++i) {
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
	draw_background_rows(row_begin, row_end, x, width);
	/* buttons drawing */
	UI_block_draw(C, block);
}

void spreadsheet_draw_main(const bContext *C, SpaceSpreadsheet *ssheet, ARegion *ar)
{
	View2D *v2d = &ar->v2d;
	int row_begin, row_end;
	
	PointerRNA ptr;
	PropertyRNA *prop;
	SpreadsheetDataField fields[SPREADSHEET_MAX_FIELDS];
	int num_fields, length;
	
	if (!spreadsheet_get_data(C, ssheet, &ptr, &prop))
		return;
	num_fields = spreadsheet_get_data_fields(ssheet, &ptr, prop, fields, SPREADSHEET_MAX_FIELDS);
	length = spreadsheet_get_data_length(&ptr, prop);
	
	row_begin = max_ii(y_to_row(v2d->cur.ymax), 0);
	row_end = min_ii(y_to_row(v2d->cur.ymin) + 1, length);
	
	draw_data_columns(C, ssheet, &ptr, prop, fields, num_fields, v2d, row_begin, row_end);
	draw_header_row(C, ssheet, fields, num_fields, v2d);
	draw_index_column(C, ssheet, v2d, row_begin, row_end);
}

void spreadsheet_set_cursor(wmWindow *win, SpaceSpreadsheet *UNUSED(ssheet), const float UNUSED(cursor[2]))
{
	int wmcursor = CURSOR_STD;
	
	WM_cursor_set(win, wmcursor);
}

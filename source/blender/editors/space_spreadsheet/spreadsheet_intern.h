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

/** \file blender/editors/space_node/spreadsheet_intern.h
 *  \ingroup spspreadsheet
 */

#ifndef __SPREADSHEET_INTERN_H__
#define __SPREADSHEET_INTERN_H__

struct bContext;
struct ID;
struct ListBase;
struct SpaceSpreadsheet;
struct ARegion;
struct wmWindow;
struct PointerRNA;
struct PropertyRNA;

/* spreadsheet_data.c */
typedef struct SpreadsheetDataField {
	PropertyRNA *prop;
} SpreadsheetDataField;

#define SPREADSHEET_MAX_FIELDS 64

bool spreadsheet_is_supported_context_id(int id_type);
struct ID *spreadsheet_get_context_id(const struct bContext *C, int id_type);
struct ID *spreadsheet_get_id(const struct bContext *C, struct SpaceSpreadsheet *ssheet);
bool spreadsheet_get_data(const struct bContext *C, struct SpaceSpreadsheet *ssheet,
                          struct PointerRNA *ptr, struct PropertyRNA **prop);
int spreadsheet_get_data_fields(struct SpaceSpreadsheet *ssheet, struct PointerRNA *ptr, struct PropertyRNA *prop,
                                struct SpreadsheetDataField *fields, int max_fields);
int spreadsheet_get_data_length(struct PointerRNA *ptr, struct PropertyRNA *prop);

/* spreadsheet_draw.c */
int spreadsheet_row_height(void);
void spreadsheet_get_size(const bContext *C, int *width, int *height);

void spreadsheet_draw_grease_pencil(const struct bContext *C, bool onlyv2d);
void spreadsheet_draw_main(const struct bContext *C, struct SpaceSpreadsheet *ssheet, struct ARegion *ar);
void spreadsheet_set_cursor(struct wmWindow *win, struct SpaceSpreadsheet *ssheet, const float cursor[2]);

#endif /* __SPREADSHEET_INTERN_H__ */

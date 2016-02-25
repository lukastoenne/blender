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

/** \file blender/editors/space_node/space_spreadsheet.c
 *  \ingroup spspreadsheet
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_space_api.h"
#include "ED_screen.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"

#include "spreadsheet_intern.h"

/* ******************** default callbacks for space ***************** */

static SpaceLink *spreadsheet_new(const bContext *UNUSED(C))
{
	ARegion *ar;
	SpaceSpreadsheet *ssheet;

	ssheet = MEM_callocN(sizeof(SpaceSpreadsheet), "initspreadsheet");
	ssheet->spacetype = SPACE_SPREADSHEET;

	ssheet->id_type = ID_OB;

	/* header */
	ar = MEM_callocN(sizeof(ARegion), "header for spreadsheet");

	BLI_addtail(&ssheet->regionbase, ar);
	ar->regiontype = RGN_TYPE_HEADER;
	ar->alignment = RGN_ALIGN_BOTTOM;

	/* main region */
	ar = MEM_callocN(sizeof(ARegion), "main region for spreadsheet");

	BLI_addtail(&ssheet->regionbase, ar);
	ar->regiontype = RGN_TYPE_WINDOW;

	ar->v2d.tot.xmin =  -12.8f * U.widget_unit;
	ar->v2d.tot.ymin =  -12.8f * U.widget_unit;
	ar->v2d.tot.xmax = 38.4f * U.widget_unit;
	ar->v2d.tot.ymax = 38.4f * U.widget_unit;

	ar->v2d.cur =  ar->v2d.tot;

	ar->v2d.min[0] = 1.0f;
	ar->v2d.min[1] = 1.0f;

	ar->v2d.max[0] = 32000.0f;
	ar->v2d.max[1] = 32000.0f;

	ar->v2d.minzoom = 1.0f;
	ar->v2d.maxzoom = 1.0f;

	ar->v2d.scroll = (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM);
	ar->v2d.keepzoom = V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y | V2D_KEEPASPECT;
	ar->v2d.keeptot = 0;

	return (SpaceLink *)ssheet;
}

static void spreadsheet_free(SpaceLink *sl)
{
	SpaceSpreadsheet *ssheet = (SpaceSpreadsheet *)sl;
	UNUSED_VARS(ssheet);
}

static void spreadsheet_init(struct wmWindowManager *UNUSED(wm), ScrArea *UNUSED(sa))
{
}

static SpaceLink *spreadsheet_duplicate(SpaceLink *sl)
{
	SpaceSpreadsheet *ssheet = (SpaceSpreadsheet *)sl;
	SpaceSpreadsheet *ssheetn = MEM_dupallocN(ssheet);

	return (SpaceLink *)ssheetn;
}

static void spreadsheet_operatortypes(void)
{
}

static void spreadsheet_keymap(struct wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;

	/* ******** Global hotkeys avalaible for all regions ******** */

	keymap = WM_keymap_find(keyconf, "Spreadsheet", SPACE_SPREADSHEET, 0);
}


static void spreadsheet_area_listener(bScreen *UNUSED(sc), ScrArea *sa, wmNotifier *wmn)
{
	SpaceSpreadsheet *ssheet = sa->spacedata.first;
	UNUSED_VARS(ssheet);

	/* preview renders */
	switch (wmn->category) {
		case NC_SPACE:
			if (wmn->data == ND_SPACE_SPREADSHEET)
				ED_area_tag_refresh(sa);
			break;
		case NC_SCREEN:
			switch (wmn->data) {
				case ND_ANIMPLAY:
					ED_area_tag_refresh(sa);
					break;
			}
			break;
		case NC_WM:
			if (wmn->data == ND_UNDO) {
				ED_area_tag_refresh(sa);
			}
			break;
		case NC_GPENCIL:
			if (ELEM(wmn->action, NA_EDITED, NA_SELECTED)) {
				ED_area_tag_redraw(sa);
			}
			break;
	}
}

static void spreadsheet_area_refresh(const struct bContext *UNUSED(C), ScrArea *sa)
{
	/* default now: refresh node is starting preview */
	SpaceSpreadsheet *ssheet = sa->spacedata.first;
	UNUSED_VARS(ssheet);
}

const char *spreadsheet_context_dir[] = {NULL};

static int spreadsheet_context(const bContext *C, const char *member, bContextDataResult *result)
{
	SpaceSpreadsheet *ssheet = CTX_wm_space_spreadsheet(C);
	UNUSED_VARS(ssheet);

	if (CTX_data_dir(member)) {
		CTX_data_dir_set(result, spreadsheet_context_dir);
		return 1;
	}

	return 0;
}

/* ************* dropboxes ************* */

/* this region dropbox definition */
static void spreadsheet_dropboxes(void)
{
	ListBase *lb = WM_dropboxmap_find("Spreadsheet Editor", SPACE_SPREADSHEET, RGN_TYPE_WINDOW);
	UNUSED_VARS(lb);
}

/* ************* end drop *********** */


/* setup View2D from offset */
static void spreadsheet_main_area_set_view2d(const bContext *C, ARegion *ar)
{
	int w, h, winx, winy;

	spreadsheet_get_size(C, &w, &h);

	winx = BLI_rcti_size_x(&ar->winrct) + 1;
	winy = BLI_rcti_size_y(&ar->winrct) + 1;

	ar->v2d.tot.xmin = 0;
	ar->v2d.tot.ymin = 0;
	ar->v2d.tot.xmax = w;
	ar->v2d.tot.ymax = h;

	ar->v2d.mask.xmin = 0;
	ar->v2d.mask.ymin = 0;
	ar->v2d.mask.xmax = winx;
	ar->v2d.mask.ymax = winy;

	CLAMP_MAX(ar->v2d.cur.xmin, w - winx);
	CLAMP_MIN(ar->v2d.cur.xmin, 0);
	CLAMP_MAX(ar->v2d.cur.ymin, -winy);
	CLAMP_MIN(ar->v2d.cur.ymin, -winy - h);
	
	CLAMP_MAX(ar->v2d.cur.xmax, w);
	CLAMP_MIN(ar->v2d.cur.xmax, winx);
	CLAMP_MAX(ar->v2d.cur.ymax, 0);
	CLAMP_MIN(ar->v2d.cur.ymax, -h);
}

/* Initialize main region, setting handlers. */
static void spreadsheet_main_region_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	ListBase *lb;

	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);

	/* own keymaps */
	keymap = WM_keymap_find(wm->defaultconf, "Spreadsheet", SPACE_SPREADSHEET, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);

	keymap = WM_keymap_find(wm->defaultconf, "Spreadsheet Editor", SPACE_SPREADSHEET, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);

	/* add drop boxes */
	lb = WM_dropboxmap_find("Spreadsheet Editor", SPACE_SPREADSHEET, RGN_TYPE_WINDOW);

	WM_event_add_dropbox_handler(&ar->handlers, lb);
}

static void spreadsheet_main_region_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	SpaceSpreadsheet *ssheet = CTX_wm_space_spreadsheet(C);
	View2D *v2d = &ar->v2d;

	/* clear and setup matrix */
	UI_ThemeClearColor(TH_BACK);
	glClear(GL_COLOR_BUFFER_BIT);

	spreadsheet_main_area_set_view2d(C, ar);

	UI_view2d_view_ortho(v2d);

	spreadsheet_draw_main(C, ssheet, ar);

	if (ssheet->flag & SPREADSHEET_SHOW_GPENCIL) {
		/* Grease Pencil */
		spreadsheet_draw_grease_pencil(C, true);
	}

	/* reset view matrix */
	UI_view2d_view_restore(C);

	if (ssheet->flag & SPREADSHEET_SHOW_GPENCIL) {
		/* draw Grease Pencil - screen space only */
		spreadsheet_draw_grease_pencil(C, false);
	}
}

static void spreadsheet_main_region_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_GPENCIL:
			if (wmn->action == NA_EDITED)
				ED_region_tag_redraw(ar);
			else if (wmn->data & ND_GPENCIL_EDITMODE)
				ED_region_tag_redraw(ar);
			break;
	}
}

static void spreadsheet_main_region_cursor(wmWindow *win, ScrArea *sa, ARegion *ar)
{
	SpaceSpreadsheet *ssheet = sa->spacedata.first;
	
	float mouse_view[2];
	
	/* convert mouse coordinates to v2d space */
	UI_view2d_region_to_view(&ar->v2d, win->eventstate->x - ar->winrct.xmin, win->eventstate->y - ar->winrct.ymin,
	                         &mouse_view[0], &mouse_view[1]);
	
	spreadsheet_set_cursor(win, ssheet, mouse_view);
}


/* add handlers, stuff you only do once or on area/region changes */
static void spreadsheet_header_region_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
	ED_region_header_init(ar);
}

static void spreadsheet_header_region_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
}

static void spreadsheet_header_region_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *ar, wmNotifier *wmn)
{
	UNUSED_VARS(ar);
	/* context changes */
	switch (wmn->category) {
	}
}


/* only called once, from space/spacetypes.c */
void ED_spacetype_spreadsheet(void)
{
	SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype spreadsheet");
	ARegionType *art;

	st->spaceid = SPACE_SPREADSHEET;
	strncpy(st->name, "Node", BKE_ST_MAXNAME);

	st->new = spreadsheet_new;
	st->free = spreadsheet_free;
	st->init = spreadsheet_init;
	st->duplicate = spreadsheet_duplicate;
	st->operatortypes = spreadsheet_operatortypes;
	st->keymap = spreadsheet_keymap;
	st->listener = spreadsheet_area_listener;
	st->refresh = spreadsheet_area_refresh;
	st->context = spreadsheet_context;
	st->dropboxes = spreadsheet_dropboxes;

	/* regions: main window */
	art = MEM_callocN(sizeof(ARegionType), "spacetype spreadsheet region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init = spreadsheet_main_region_init;
	art->draw = spreadsheet_main_region_draw;
	art->listener = spreadsheet_main_region_listener;
	art->cursor = spreadsheet_main_region_cursor;
	art->event_cursor = true;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_GPENCIL;

	BLI_addhead(&st->regiontypes, art);

	/* regions: header */
	art = MEM_callocN(sizeof(ARegionType), "spacetype spreadsheet region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey = HEADERY;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
	art->listener = spreadsheet_header_region_listener;
	art->init = spreadsheet_header_region_init;
	art->draw = spreadsheet_header_region_draw;

	BLI_addhead(&st->regiontypes, art);

	BKE_spacetype_register(st);
}

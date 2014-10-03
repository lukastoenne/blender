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
 * The Original Code is Copyright (C) 2007 Blender Foundation but based 
 * on ghostwinlay.c (C) 2001-2002 by NaN Holding BV
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2008
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/wm_widgets.c
 *  \ingroup wm
 *
 * Window management, widget API.
 */

#include <stdlib.h>
#include <string.h>

#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "ED_view3d.h"
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"
#include "wm_window.h"
#include "wm_event_system.h"
#include "wm_event_types.h"
#include "wm_draw.h"

#include "GL/glew.h"
#include "GPU_select.h"

#ifndef NDEBUG
#  include "RNA_enum_types.h"
#endif

typedef struct wmWidgetMap {
	struct wmWidgetMap *next, *prev;
	
	ListBase widgets;
	short spaceid, regionid;
	char idname[KMAP_MAX_NAME];
	/* check if widgetmap does 3D drawing */
	bool is_3d;
	
	/* active widget for this map. We redraw the widgetmap when this changes  */
	wmWidget *active_widget;
} wmWidgetMap;

/* store all widgetboxmaps here. Anyone who wants to register a widget for a certain 
 * area type can query the widgetbox to do so */
static ListBase widgetmaps = {NULL, NULL};


wmWidget *WM_widget_new(bool (*poll)(const struct bContext *C, struct wmWidget *customdata),
                        void (*draw)(const struct bContext *C, struct wmWidget *customdata),
                        void (*render_3d_intersection)(const struct bContext *C, struct wmWidget *customdata, int selectionbase),
						int  (*intersect)(struct bContext *C, const struct wmEvent *event, struct wmWidget *customdata),
                        int  (*handler)(struct bContext *C, const struct wmEvent *event, struct wmWidget *customdata, int active),
                        void *customdata, bool free_data)
{
	wmWidget *widget;
	
	widget = MEM_callocN(sizeof(wmWidget), "widget");
	
	widget->poll = poll;
	widget->draw = draw;
	widget->handler = handler;
	widget->intersect = intersect;
	widget->render_3d_intersection = render_3d_intersection;
	widget->customdata = customdata;
	
	if (free_data)
		widget->flag |= WM_WIDGET_FREE_DATA;

	return widget;
}

static void WM_widgets_delete(ListBase *widgetlist, wmWidget *widget)
{
	if (widget->flag & WM_WIDGET_FREE_DATA)
		MEM_freeN(widget->customdata);
	
	BLI_freelinkN(widgetlist, widget);
}


void WM_widgets_draw(const struct bContext *C, struct ARegion *ar)
{
	wmWidgetMap *wmap = ar->widgetmap;
	if (wmap->widgets.first) {
		wmWidget *widget;
		
		for (widget = wmap->widgets.first; widget; widget = widget->next) {
			if ((widget->draw) &&
				(widget->poll == NULL || widget->poll(C, widget))) 
			{
				widget->draw(C, widget);			
			}
		}
	}
}

void WM_event_add_widget_handler(ARegion *ar)
{
	wmEventHandler *handler;
	
	for (handler = ar->handlers.first; handler; handler = handler->next)
		if (handler->widgetmap == ar->widgetmap)
			return;
	
	handler = MEM_callocN(sizeof(wmEventHandler), "widget handler");
	
	handler->widgetmap = ar->widgetmap;
	BLI_addhead(&ar->handlers, handler);
}


bool WM_widget_register(struct wmWidgetMap *wmap, wmWidget *widget)
{
	wmWidget *widget_iter;
	/* search list, might already be registered */	
	for (widget_iter = wmap->widgets.first; widget_iter; widget_iter = widget_iter->next) {
		if (widget_iter == widget)
			return false;
	}
	
	BLI_addtail(&wmap->widgets, widget);
	return true;
}

void WM_widget_unregister(struct wmWidgetMap *wmap, wmWidget *widget)
{
	BLI_remlink(&wmap->widgets, widget);
}

wmWidgetMap *WM_widgetmap_find(const char *idname, int spaceid, int regionid, bool is_3d)
{
	wmWidgetMap *wmap;
	
	for (wmap = widgetmaps.first; wmap; wmap = wmap->next)
		if (wmap->spaceid == spaceid && wmap->regionid == regionid && wmap->is_3d == is_3d)
			if (0 == strncmp(idname, wmap->idname, KMAP_MAX_NAME))
				return wmap;
	
	wmap = MEM_callocN(sizeof(struct wmWidgetMap), "widget list");
	BLI_strncpy(wmap->idname, idname, KMAP_MAX_NAME);
	wmap->spaceid = spaceid;
	wmap->regionid = regionid;
	wmap->is_3d = is_3d;
	BLI_addhead(&widgetmaps, wmap);
	
	return wmap;
}

void WM_widgetmaps_free(void)
{
	wmWidgetMap *wmap;
	
	for (wmap = widgetmaps.first; wmap; wmap = wmap->next) {
		wmWidget *widget;
		
		for (widget = wmap->widgets.first; widget;) {
			wmWidget *widget_next = widget->next;
			WM_widgets_delete(&wmap->widgets, widget);
			widget = widget_next;
		}
		BLI_freelistN(&wmap->widgets);
	}
	
	BLI_freelistN(&widgetmaps);
}

ListBase *wm_widgetmap_widget_list(struct wmWidgetMap *wmap)
{
	return &wmap->widgets;
}

bool wm_widgetmap_is_3d(struct wmWidgetMap *wmap)
{
	return wmap->is_3d;
}

static void widget_find_active_3D_loop(bContext *C, ListBase *widgetlist)
{
	int selectionbase = 0;
	wmWidget *widget;
	
	for (widget = widgetlist->first; widget; widget = widget->next) {
		if (widget->render_3d_intersection && (!widget->poll || widget->poll(C, widget))) {
			/* we use only 8 bits as free ids for per widget handles */
			widget->render_3d_intersection(C, widget, selectionbase << 8);
		}
		selectionbase++;		
	}
}

static int WM_widget_find_active_3D_intern (ListBase *widgetlist, bContext *C, const struct wmEvent *event, float hotspot)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d = sa->spacedata.first;
	RegionView3D *rv3d = ar->regiondata;
	rctf rect, selrect;
	GLuint buffer[64];      // max 4 items per select, so large enuf
	short hits;
	const bool do_passes = GPU_select_query_check_active();

	extern void view3d_winmatrix_set(ARegion *ar, View3D *v3d, rctf *rect);
	
	rect.xmin = event->mval[0] - hotspot;
	rect.xmax = event->mval[0] + hotspot;
	rect.ymin = event->mval[1] - hotspot;
	rect.ymax = event->mval[1] + hotspot;
	
	selrect = rect;
	
	view3d_winmatrix_set(ar, v3d, &rect);
	mul_m4_m4m4(rv3d->persmat, rv3d->winmat, rv3d->viewmat);
	
	if (do_passes)
		GPU_select_begin(buffer, 64, &selrect, GPU_SELECT_NEAREST_FIRST_PASS, 0);
	else
		GPU_select_begin(buffer, 64, &selrect, GPU_SELECT_ALL, 0);
	/* do the drawing */
	widget_find_active_3D_loop(C, widgetlist);
	
	hits = GPU_select_end();
	
	if (do_passes) {
		GPU_select_begin(buffer, 64, &selrect, GPU_SELECT_NEAREST_SECOND_PASS, hits);
		widget_find_active_3D_loop(C, widgetlist);		
		GPU_select_end();
	}
	
	view3d_winmatrix_set(ar, v3d, NULL);
	mul_m4_m4m4(rv3d->persmat, rv3d->winmat, rv3d->viewmat);

	if (hits == 1) {
		return buffer[3];

		/* find the widget the value belongs to */		
	}
	else if (hits > 1) {
		GLuint val, dep, mindep = 0, minval = -1;
		int a;

		/* we compare the hits in buffer, but value centers highest */
		/* we also store the rotation hits separate (because of arcs) and return hits on other widgets if there are */

		for (a = 0; a < hits; a++) {
			dep = buffer[4 * a + 1];
			val = buffer[4 * a + 3];

			if (minval == -1 || dep < mindep) {
				mindep = dep;
				minval = val;
			}
		}

		return minval;
	}
	return -1;
}


int wm_widget_find_active_3D (struct wmWidgetMap *wmap, bContext *C, const struct wmEvent *event)
{
	int ret, retsec;
	
	/* set up view matrices */	
	view3d_operator_needs_opengl(C);
	
	ret = WM_widget_find_active_3D_intern(&wmap->widgets, C, event, 0.5f * (float)U.tw_hotspot);
	
	if (ret != -1) {
		retsec = WM_widget_find_active_3D_intern(&wmap->widgets, C, event, 0.2f * (float)U.tw_hotspot);
		
		if (retsec == -1)
			return ret;
		else
			return retsec;
	}
	
	return -1;
}

void wm_widgetmap_set_active_widget(struct wmWidgetMap *wmap, struct bContext *C, struct wmWidget *widget, int handle)
{
	ARegion *ar = CTX_wm_region(C);
		
	if (widget != wmap->active_widget) {
		if (wmap->active_widget) {
			wmap->active_widget->active_handle = -1;
			wmap->active_widget->flag &= ~WM_WIDGET_HIGHLIGHT;
		}
		wmap->active_widget = widget;
		
		if (widget) {
			widget->active_handle = handle;
			widget->flag |= WM_WIDGET_HIGHLIGHT;
		}
		
		/* tag the region for redraw */		
		ED_region_tag_redraw(ar);
	}
	else if (widget && widget->active_handle != handle) {
		ED_region_tag_redraw(ar);
		widget->active_handle = handle;
	}
}

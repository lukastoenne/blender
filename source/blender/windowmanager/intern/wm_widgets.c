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

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"


#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"
#include "wm_window.h"
#include "wm_event_system.h"
#include "wm_event_types.h"
#include "wm_draw.h"

#ifndef NDEBUG
#  include "RNA_enum_types.h"
#endif

typedef struct wmWidgetMap {
	struct wmWidgetMap *next, *prev;
	
	ListBase widgets;
	short spaceid, regionid;
	char idname[KMAP_MAX_NAME];
	
} wmWidgetMap;

/* store all widgetboxmaps here. Anyone who wants to register a widget for a certain 
 * area type can query the widgetbox to do so */
static ListBase widgetmaps = {NULL, NULL};


wmWidget *WM_widget_new(bool (*poll)(const struct bContext *C, struct wmWidget *customdata),
                        void (*draw)(const struct bContext *C, struct wmWidget *customdata),
                        void (*draw_highlighted)(const struct bContext *C, struct wmWidget *customdata),
                        int  (*handler)(struct bContext *C, struct wmEvent *event, struct wmWidget *customdata),
                        void *customdata, bool free_data, bool requires_ogl)
{
	wmWidget *widget = MEM_callocN(sizeof(wmWidget), "widget");
	
	widget->poll = poll;
	widget->draw = draw;
	widget->draw_highlighted = draw_highlighted;
	widget->handler = handler;
	widget->customdata = customdata;
	
	if (free_data)
		widget->flag |= WM_WIDGET_FREE_DATA;

	if (requires_ogl)
		widget->flag |= WM_WIDGET_REQUIRES_OGL;
	
	return widget;
	
	return NULL;
}

void WM_widgets_delete(ListBase *widgetlist, wmWidget *widget)
{
	if (widget->flag & WM_WIDGET_FREE_DATA)
		MEM_freeN(widget->customdata);
	
	BLI_freelinkN(widgetlist, widget);
}


void WM_widgets_draw(const struct bContext *C, struct ARegion *ar)
{
	if (ar->widgets->first) {
		wmWidget *widget;
		
		for (widget = ar->widgets->first; widget; widget = widget->next) {
			if ((widget->draw || widget->draw_highlighted) &&
				(widget->poll == NULL || widget->poll(C, widget->customdata))) 
			{
				if (widget->draw_highlighted && (widget->flag & WM_WIDGET_HIGHLIGHT)) {
					widget->draw_highlighted(C, widget->customdata);
				}
				else if (widget->draw) {
					widget->draw(C, widget->customdata);
				}				
			}
		}
	}
}

void WM_event_add_widget_handler(ARegion *ar)
{
	wmEventHandler *handler;
	
	for (handler = ar->handlers.first; handler; handler = handler->next)
		if (handler->widgets == ar->widgets)
			return;
	
	handler = MEM_callocN(sizeof(wmEventHandler), "widget handler");
	
	handler->widgets = ar->widgets;
	BLI_addhead(&ar->handlers, handler);
}


bool WM_widget_register(ListBase *widgetlist, wmWidget *widget)
{
	wmWidget *widget_iter;
	/* search list, might already be registered */	
	for (widget_iter = widgetlist->first; widget_iter; widget_iter = widget_iter->next) {
		if (widget_iter == widget)
			return false;
	}
	
	BLI_addtail(widgetlist, widget);
	return true;
}

void WM_widget_unregister(ListBase *widgetlist, wmWidget *widget)
{
	BLI_remlink(widgetlist, widget);
}

ListBase *WM_widgetmap_find(const char *idname, int spaceid, int regionid)
{
	wmWidgetMap *wmap;
	
	for (wmap = widgetmaps.first; wmap; wmap = wmap->next)
		if (wmap->spaceid == spaceid && wmap->regionid == regionid)
			if (0 == strncmp(idname, wmap->idname, KMAP_MAX_NAME))
				return &wmap->widgets;
	
	wmap = MEM_callocN(sizeof(struct wmWidgetMap), "widget list");
	BLI_strncpy(wmap->idname, idname, KMAP_MAX_NAME);
	wmap->spaceid = spaceid;
	wmap->regionid = regionid;
	BLI_addhead(&widgetmaps, wmap);
	
	return &wmap->widgets;
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


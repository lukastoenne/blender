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

typedef struct wmWidget {
	struct wmWidget *next, *prev;

	void *customdata;
	
	/* poll if widget should be active */
	int (*poll)(const struct bContext *C, void *customdata);
	/* draw widget in screen space */
	void (*draw)(const struct bContext *C, void *customdata);
	/* draw widget highlight */
	void (*draw_highlighted)(const struct bContext *C, void *customdata);
	/* draw widget in 3d space */
	void (*interact)(struct bContext *C, struct wmEvent *event, void *customdata);
	void (*handler)(struct bContext *C, struct wmEvent *event, void *customdata);
	int flag; /* flags set by drawing and interaction, such as highlighting */
} wmWidget;


typedef struct wmWidgetMap {
	struct wmWidgetMap *next, *prev;
	
	ListBase widgets;
	short spaceid, regionid;
	char idname[KMAP_MAX_NAME];
	
} wmWidgetMap;

/* store all widgetboxmaps here. Anyone who wants to register a widget for a certain 
 * area type can query the widgetbox to do so */
static ListBase widgetmaps = {NULL, NULL};


wmWidget *WM_widget_new(int (*poll)(const struct bContext *C, void *customdata),
						void (*draw)(const struct bContext *C, void *customdata),
						void (*draw_highlighted)(const struct bContext *C, void *customdata),
						void (*interact)(struct bContext *C, struct wmEvent *event, void *customdata),
						void (*handler)(struct bContext *C, struct wmEvent *event, void *customdata),
						void *customdata, bool free_data, bool requires_ogl)
{
	wmWidget *widget = MEM_callocN(sizeof(wmWidget), "widget");
	
	widget->poll = poll;
	widget->draw = draw;
	widget->draw_highlighted = draw_highlighted;
	widget->interact = interact;
	widget->handler = handler;
	widget->customdata = customdata;
	
	if (free_data)
		widget->flag |= WM_WIDGET_FREE_DATA;

	if (requires_ogl)
		widget->flag |= WM_WIDGET_REQUIRES_OGL;
	
	return widget;
	
	return NULL;
}

void WM_widgets_delete(wmWidget *widget)
{
	if (widget->flag & WM_WIDGET_FREE_DATA)
		MEM_freeN(widget->customdata);
	
	MEM_freeN(widget);
}


void WM_widgets_draw(const struct bContext *C, struct ARegion *ar)
{
	if (ar->widgets.first) {
		wmWidget *widget;
		
		for (widget = ar->widgets.first; widget; widget = widget->next) {
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

void WM_widget_handler_register(ARegion *ar)
{
	wmEventHandler *handler;
	
	for (handler = ar->handlers.first; handler; handler = handler->next)
		if (handler->widgets == &ar->widgets)
			return;
	
	handler = MEM_callocN(sizeof(wmEventHandler), "widget handler");
	
	handler->widgets = &ar->widgets;
	BLI_addtail(&ar->handlers, handler);
}


void WM_widget_register(ARegion *ar, wmWidget *widget)
{
	BLI_addtail(&ar->widgets, widget);
}

void WM_widget_unregister(ARegion *ar, wmWidget *widget)
{
	wmEventHandler *handler;
	wmWidget *widget_iter;
	
	for (handler = ar->handlers.first; handler; handler = handler->next) {
		if (handler->widgets == &ar->widgets) {
			BLI_remlink(&ar->handlers, handler);
			break;
		}
	}
	
	for (widget_iter = ar->widgets.first; widget_iter; widget_iter = widget_iter->next) {
		if (widget_iter == widget) {
			BLI_remlink(&ar->widgets, widget);
		}
	}
}

ListBase *WM_widgetmap_find(const char *idname, int spaceid, int regionid)
{
	wmWidgetMap *widgetm;
	
	for (widgetm = widgetmaps.first; widgetm; widgetm = widgetm->next)
		if (widgetm->spaceid == spaceid && widgetm->regionid == regionid)
			if (0 == strncmp(idname, widgetm->idname, KMAP_MAX_NAME))
				return &widgetm->widgets;
	
	widgetm = MEM_callocN(sizeof(struct wmWidgetMap), "widget list");
	BLI_strncpy(widgetm->idname, idname, KMAP_MAX_NAME);
	widgetm->spaceid = spaceid;
	widgetm->regionid = regionid;
	BLI_addtail(&widgetmaps, widgetm);
	
	return &widgetm->widgets;
}

void wm_widgetmap_free(void)
{
	wmWidgetMap *widgetm;
	
	for (widgetm = widgetmaps.first; widgetm; widgetm = widgetm->next) {
		wmWidget *widget;
		
		for (widget = widgetm->widgets.first; widget; widget = widget->next) {
			WM_widgets_delete(widget);
		}
		BLI_freelistN(&widgetm->widgets);
	}
	
	BLI_freelistN(&widgetmaps);
}


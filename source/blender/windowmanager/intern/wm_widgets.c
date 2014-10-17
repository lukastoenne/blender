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
	
	ListBase widgetgroups;
	short spaceid, regionid;
	char idname[KMAP_MAX_NAME];
	/* check if widgetmap does 3D drawing */
	bool is_3d;
	
	/* highlighted widget for this map. We redraw the widgetmap when this changes  */
	wmWidget *highlighted_widget;

	/* active widget for this map. User has clicked and is currently this widget  */
	wmWidget *active_widget;
} wmWidgetMap;


struct wmWidgetGroup {
	struct wmWidgetGroup *next, *prev;
	ListBase widgets;
		
	void *customdata;

	/* free the widgetmap. Should take care of any customdata too */
	void (*free)(struct wmWidgetGroup *wgroup);
	
	/* poll if widgetmap should be active */
	bool (*poll)(struct wmWidgetGroup *wgroup, const struct bContext *C);

	/* update widgets, called right before drawing */
	void (*update)(struct wmWidgetGroup *wgroup, const struct bContext *C);
};


/* store all widgetboxmaps here. Anyone who wants to register a widget for a certain 
 * area type can query the widgetbox to do so */
static ListBase widgetmaps = {NULL, NULL};

struct wmWidgetGroup *WM_widgetgroup_new(bool (*poll)(struct wmWidgetGroup *, const struct bContext *C),
                                         void (*update)(struct wmWidgetGroup *, const struct bContext *),
                                         void (*free)(struct wmWidgetGroup *wgroup), void *customdata)
{
	wmWidgetGroup *wgroup;
	
	wgroup = MEM_callocN(sizeof(wmWidgetGroup), "widgetgroup");
	
	wgroup->poll = poll;
	wgroup->update = update;
	wgroup->free = free;
	wgroup->customdata = customdata;
	
	return wgroup;
}

void *WM_widgetgroup_customdata(struct wmWidgetGroup *wgroup)
{
	return wgroup->customdata;
}

ListBase *WM_widgetgroup_widgets(struct wmWidgetGroup *wgroup)
{
	return &wgroup->widgets;
}

wmWidget *WM_widget_new(void (*draw)(struct wmWidget *customdata, const struct bContext *C, float scale),
                        void (*render_3d_intersection)(const struct bContext *C, struct wmWidget *customdata, float scale, int selectionbase),
						int  (*intersect)(struct bContext *C, const struct wmEvent *event, struct wmWidget *widget),
                        int  (*initialize_op)(struct bContext *C, const struct wmEvent *event, struct wmWidget *widget, struct PointerRNA *),
                        int  (*handler)(struct bContext *C, const struct wmEvent *event, struct wmWidget *widget, struct wmOperator *op),
                        void *customdata, bool free_data, char *opname, char *prop)
{
	wmWidget *widget;
	
	widget = MEM_callocN(sizeof(wmWidget), "widget");
	
	widget->draw = draw;
	widget->initialize_op = initialize_op;
	widget->handler = handler;
	widget->intersect = intersect;
	widget->render_3d_intersection = render_3d_intersection;
	widget->customdata = customdata;
	
	widget->opname = opname;
	widget->prop = prop;

	if (free_data)
		widget->flag |= WM_WIDGET_FREE_DATA;

	return widget;
}

static void wm_widgets_delete(ListBase *widgetlist, wmWidget *widget)
{
	if (widget->flag & WM_WIDGET_FREE_DATA)
		MEM_freeN(widget->customdata);
	
	BLI_freelinkN(widgetlist, widget);
}


void WM_widgets_draw(const struct bContext *C, struct ARegion *ar)
{
	RegionView3D *rv3d = ar->regiondata;
	wmWidgetMap *wmap = ar->widgetmap;
	wmWidget *widget = wmap->active_widget;
	bool use_lighting = (U.tw_flag & V3D_SHADED_WIDGETS) != 0;

	if (use_lighting) {
		float lightpos[4] = {0.0, 0.0, 1.0, 0.0};
		float diffuse[4] = {1.0, 1.0, 1.0, 0.0};

		glPushAttrib(GL_LIGHTING_BIT | GL_ENABLE_BIT);
		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		glEnable(GL_COLOR_MATERIAL);
		glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
		glPushMatrix();
		glLoadIdentity();
		glLightfv(GL_LIGHT0, GL_POSITION, lightpos);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
		glPopMatrix();
	}

	if (widget) {
		float scale = 1.0;

		if (!(U.tw_flag & V3D_3D_WIDGETS))
			scale = ED_view3d_pixel_size(rv3d, widget->origin) * U.tw_size;

		/* notice that we don't update the widgetgroup, widget is now on its own, it should have all
		 * relevant data to update itself */
		widget->draw(widget, C, scale);
	}
	else if (wmap->widgetgroups.first) {
		wmWidgetGroup *wgroup;
		
		for (wgroup = wmap->widgetgroups.first; wgroup; wgroup = wgroup->next) {
			if (!wgroup->poll || wgroup->poll(wgroup, C)) 
			{
				wmWidget *widget_iter;

				if (wgroup->update)
					wgroup->update(wgroup, C);

				for (widget_iter = wgroup->widgets.first; widget_iter; widget_iter = widget_iter->next) {
					if (!(widget_iter->flag & WM_WIDGET_SKIP_DRAW)) {
						float scale = 1.0;

						if (!(U.tw_flag & V3D_3D_WIDGETS))
							scale = ED_view3d_pixel_size(rv3d, widget_iter->origin) * U.tw_size;

						widget_iter->draw(widget_iter, C, scale);
					}
				}
			}
		}
	}

	if (use_lighting)
		glPopAttrib();
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


bool WM_widget_register(struct wmWidgetGroup *wgroup, wmWidget *widget)
{
	wmWidget *widget_iter;
	
	/* search list, might already be registered */	
	for (widget_iter = wgroup->widgets.first; widget_iter; widget_iter = widget_iter->next) {
		if (widget_iter == widget)
			return false;
	}
	
	BLI_addtail(&wgroup->widgets, widget);
	return true;
}

bool WM_widgetgroup_register(struct wmWidgetMap *wmap, wmWidgetGroup *wgroup)
{
	wmWidgetGroup *wgroup_iter;
	/* search list, might already be registered */	
	for (wgroup_iter = wmap->widgetgroups.first; wgroup_iter; wgroup_iter = wgroup_iter->next) {
		if (wgroup_iter == wgroup)
			return false;
	}
	
	BLI_addtail(&wmap->widgetgroups, wgroup);
	return true;
}

void WM_widgetgroup_unregister(struct wmWidgetMap *wmap, wmWidgetGroup *wgroup)
{
	BLI_remlink(&wmap->widgetgroups, wgroup);
	wmap->prev = wmap->next = NULL;
}

void WM_widget_unregister(struct wmWidgetGroup *wgroup, wmWidget *widget)
{
	BLI_remlink(&wgroup->widgets, widget);
	widget->prev = widget->next = NULL;
}

void *WM_widget_customdata(struct wmWidget *widget)
{
	return widget->customdata;
}

void WM_widget_set_origin(struct wmWidget *widget, float origin[3])
{
	copy_v3_v3(widget->origin, origin);
}

void WM_widget_set_draw(struct wmWidget *widget, bool draw)
{
	if (draw) {
		widget->flag &= ~WM_WIDGET_SKIP_DRAW;
	}
	else {
		widget->flag |= WM_WIDGET_SKIP_DRAW;
	}
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
		wmWidgetGroup *wgroup;
		
		for (wgroup = wmap->widgetgroups.first; wgroup; wgroup = wgroup->next) {
			wmWidget *widget;
			
			if (wgroup->free)
				wgroup->free(wgroup);
			
			for (widget = wgroup->widgets.first; widget;) {
				wmWidget *widget_next = widget->next;
				wm_widgets_delete(&wgroup->widgets, widget);
				widget = widget_next;
			}
			BLI_freelistN(&wgroup->widgets);
		}
		BLI_freelistN(&wmap->widgetgroups);
	}
	BLI_freelistN(&widgetmaps);

	fix_linking_widget_lib();
}

bool wm_widgetmap_is_3d(struct wmWidgetMap *wmap)
{
	return wmap->is_3d;
}

static void widget_find_active_3D_loop(bContext *C, ListBase *visible_widgets)
{
	int selectionbase = 0;
	LinkData *link;
	wmWidget *widget;
	RegionView3D *rv3d = CTX_wm_region(C)->regiondata;

	for (link = visible_widgets->first; link; link = link->next) {
		float scale = 1.0;

		widget = link->data;

		if (!(U.tw_flag & V3D_3D_WIDGETS))
			scale = ED_view3d_pixel_size(rv3d, widget->origin) * U.tw_size;
		/* we use only 8 bits as free ids for per widget handles */
		widget->render_3d_intersection(C, widget, scale, selectionbase);

		selectionbase++;
	}
}

static int wm_widget_find_highlighted_3D_intern (ListBase *visible_widgets, bContext *C, const struct wmEvent *event, float hotspot)
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
	widget_find_active_3D_loop(C, visible_widgets);
	
	hits = GPU_select_end();
	
	if (do_passes) {
		GPU_select_begin(buffer, 64, &selrect, GPU_SELECT_NEAREST_SECOND_PASS, hits);
		widget_find_active_3D_loop(C, visible_widgets);
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

static void wm_prepare_visible_widgets(struct wmWidgetMap *wmap, ListBase *visible_widgets, bContext *C)
{
	wmWidget *widget;
	wmWidgetGroup *wgroup;

	for (wgroup = wmap->widgetgroups.first; wgroup; wgroup = wgroup->next) {
		if (!wgroup->poll || wgroup->poll(wgroup, C)) {
			for (widget = wgroup->widgets.first; widget; widget = widget->next) {
				if (!(widget->flag & WM_WIDGET_SKIP_DRAW) && widget->render_3d_intersection) {
					BLI_addhead(visible_widgets, BLI_genericNodeN(widget));
				}
			}
		}
	}
}

wmWidget *wm_widget_find_highlighted_3D(struct wmWidgetMap *wmap, bContext *C, const struct wmEvent *event)
{
	int ret, retsec;
	wmWidget *result = NULL;
	
	ListBase visible_widgets = {0};

	wm_prepare_visible_widgets(wmap, &visible_widgets, C);

	/* set up view matrices */	
	view3d_operator_needs_opengl(C);
	
	ret = wm_widget_find_highlighted_3D_intern(&visible_widgets, C, event, 0.5f * (float)U.tw_hotspot);
	
	if (ret != -1) {
		LinkData *link;
		int retfinal;
		retsec = wm_widget_find_highlighted_3D_intern(&visible_widgets, C, event, 0.2f * (float)U.tw_hotspot);
		
		if (retsec == -1)
			retfinal = ret;
		else
			retfinal = retsec;
		
		link = BLI_findlink(&visible_widgets, retfinal);
		result = link->data;
	}

	BLI_freelistN(&visible_widgets);
	
	return result;
}

void wm_widgetmap_set_highlighted_widget(struct wmWidgetMap *wmap, struct bContext *C, struct wmWidget *widget)
{
	if (widget != wmap->highlighted_widget) {
		ARegion *ar = CTX_wm_region(C);

		if (wmap->highlighted_widget) {
			wmap->highlighted_widget->flag &= ~WM_WIDGET_HIGHLIGHT;
		}
		wmap->highlighted_widget = widget;
		
		if (widget) {
			widget->flag |= WM_WIDGET_HIGHLIGHT;
		}
		
		/* tag the region for redraw */
		ED_region_tag_redraw(ar);
	}
}

struct wmWidget *wm_widgetmap_get_highlighted_widget(struct wmWidgetMap *wmap)
{
	return wmap->highlighted_widget;
}

void wm_widgetmap_set_active_widget(struct wmWidgetMap *wmap, struct bContext *C, struct wmEvent *event, struct wmWidget *widget)
{
	if (widget) {
		if (widget->opname && widget->handler) {
			wmOperatorType *ot = widget->ot = WM_operatortype_find(widget->opname, 0);

			if (ot) {
				widget->flag |= WM_WIDGET_ACTIVE;
				/* first activate the widget itself */
				if (widget->activate_state) {
					widget->activate_state(C, event, widget, WIDGET_ACTIVATE);
				}

				WM_operator_properties_create_ptr(&widget->propptr, ot);

				/* time to initialize those properties now */
				if (widget->initialize_op) {
					widget->initialize_op(C, event, widget, &widget->propptr);
				}

				CTX_wm_widget_set(C, widget);
				WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &widget->propptr);
				wmap->active_widget = widget;
				return;
			}
			else {
				printf("Widget error: operator not found");
				wmap->active_widget = NULL;
				return;
			}
		}
		else {
			/* widget does nothing, pass */
			wmap->active_widget = NULL;
		}
	}
	else {
		ARegion *ar = CTX_wm_region(C);
		widget = wmap->active_widget;

		/* deactivate, widget but first take care of some stuff */
		if (widget) {
			widget->flag &= ~WM_WIDGET_ACTIVE;
			/* first activate the widget itself */
			if (widget->activate_state) {
				widget->activate_state(C, event, widget, WIDGET_DEACTIVATE);
			}

			WM_operator_properties_free(&widget->propptr);
		}

		CTX_wm_widget_set(C, NULL);

		wmap->active_widget = NULL;
		ED_region_tag_redraw(ar);
	}
}

struct wmWidget *wm_widgetmap_get_active_widget(struct wmWidgetMap *wmap)
{
	return wmap->active_widget;
}

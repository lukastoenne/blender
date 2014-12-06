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
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"
#include "wm_window.h"
#include "wm_event_system.h"
#include "wm_event_types.h"
#include "wm_draw.h"

#include "GL/glew.h"
#include "GPU_select.h"

#include "RNA_access.h"
#include "BPY_extern.h"

typedef struct wmWidgetMap {
	struct wmWidgetMap *next, *prev;
	
	struct wmWidgetMapType *type;
	ListBase widgetgroups;
	
	/* highlighted widget for this map. We redraw the widgetmap when this changes  */
	wmWidget *highlighted_widget;
	/* active widget for this map. User has clicked and is currently this widget  */
	wmWidget *active_widget;
} wmWidgetMap;

/**
 * This is a container for all widget types that can be instantiated in a region.
 * (similar to dropboxes).
 *
 * \note There is only ever one of these for every (area, region) combination.
 */
typedef struct wmWidgetMapType {
	struct wmWidgetMapType *next, *prev;
	short spaceid, regionid;
	/**
	 * Check if widgetmap does 3D drawing
	 * (uses a different kind of interaction),
	 * - 3d: use glSelect buffer.
	 * - 2d: use simple cursor position intersection test. */
	bool is_3d;
	/* types of widgetgroups for this widgetmap type */
	ListBase widgetgrouptypes;
} wmWidgetMapType;


/* store all widgetboxmaps here. Anyone who wants to register a widget for a certain
 * area type can query the widgetbox to do so */
static ListBase widgetmaptypes = {NULL, NULL};

struct wmWidgetGroupType *WM_widgetgrouptype_new(
        int (*poll)(const struct bContext *C, struct wmWidgetGroupType *),
        void (*draw)(const struct bContext *, struct wmWidgetGroup *))
{
	wmWidgetGroupType *wgrouptype;
	
	wgrouptype = MEM_callocN(sizeof(wmWidgetGroupType), "widgetgroup");
	
	wgrouptype->poll = poll;
	wgrouptype->draw = draw;
	
	return wgrouptype;
}

wmWidget *WM_widget_new(void (*draw)(struct wmWidget *customdata, const struct bContext *C),
                        void (*render_3d_intersection)(const struct bContext *C, struct wmWidget *customdata, int selectionbase),
                        int  (*intersect)(struct bContext *C, const struct wmEvent *event, struct wmWidget *widget),
                        int  (*handler)(struct bContext *C, const struct wmEvent *event, struct wmWidget *widget, struct wmOperator *op),
                        void *customdata, bool free_data)
{
	wmWidget *widget;
	
	widget = MEM_callocN(sizeof(wmWidget), "widget");
	
	widget->draw = draw;
	widget->handler = handler;
	widget->intersect = intersect;
	widget->render_3d_intersection = render_3d_intersection;
	widget->customdata = customdata;

	if (free_data)
		widget->flag |= WM_WIDGET_FREE_DATA;

	return widget;
}

void WM_widget_property(struct wmWidget *widget, struct PointerRNA *ptr, const char *propname)
{
	/* if widget evokes an operator we cannot use it for property manipulation */
	widget->opname = NULL;
	widget->ptr = *ptr;
	widget->propname = propname;
	widget->prop = RNA_struct_find_property(ptr, propname);

	if (widget->bind_to_prop)
		widget->bind_to_prop(widget);
}

void WM_widget_operator(struct wmWidget *widget,
                        const char *opname,
                        const char *propname)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0);
	
	if (ot) {
		widget->opname = opname;
		widget->propname = propname;
		
		WM_operator_properties_create_ptr(&widget->opptr, ot);
		
		if (widget->bind_to_prop)
			widget->bind_to_prop(widget);
	}
}


static void wm_widget_delete(ListBase *widgetlist, wmWidget *widget)
{
	if (widget->flag & WM_WIDGET_FREE_DATA)
		MEM_freeN(widget->customdata);
	
	if (widget->opptr.data) {
		WM_operator_properties_free(&widget->opptr);
	}
	
	BLI_freelinkN(widgetlist, widget);
}


static void widget_calculate_scale(wmWidget *widget, const bContext *C)
{
	float scale = 1.0f;
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	if (rv3d && !(U.tw_flag & V3D_3D_WIDGETS) && (widget->flag & WM_WIDGET_SCALE_3D)) {
		if (widget->get_final_position) {
			float position[3];

			widget->get_final_position(widget, position);
			scale = ED_view3d_pixel_size(rv3d, position) * U.tw_size;
		}
		else {
			scale = ED_view3d_pixel_size(rv3d, widget->origin) * U.tw_size;
		}
	}

	widget->scale = scale * widget->user_scale;
}

void WM_widgets_draw(const bContext *C, struct ARegion *ar)
{
	wmWidgetMap *wmap = ar->widgetmap;
	wmWidget *widget;
	bool use_lighting;

	if (!wmap)
		return;

	use_lighting = (U.tw_flag & V3D_SHADED_WIDGETS) != 0;

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

	widget = wmap->active_widget;

	if (widget) {
		widget_calculate_scale(widget, C);
		/* notice that we don't update the widgetgroup, widget is now on its own, it should have all
		 * relevant data to update itself */
		widget->draw(widget, C);
	}
	else if (wmap->widgetgroups.first) {
		wmWidgetGroup *wgroup;
		
		for (wgroup = wmap->widgetgroups.first; wgroup; wgroup = wgroup->next) {
			if (!wgroup->type->poll || wgroup->type->poll(C, wgroup->type))
			{
				wmWidget *widget_iter;
				wmWidget *highlighted = NULL;
				
				/* first delete and recreate the widgets */
				for (widget_iter = wgroup->widgets.first; widget_iter;) {
					wmWidget *widget_next = widget_iter->next;
					
					/* do not delete the highlighted widget, instead keep it to compare with the new one */
					if (widget_iter->flag & WM_WIDGET_HIGHLIGHT) {
						highlighted = widget_iter;
						BLI_remlink(&wgroup->widgets, widget_iter);
					}
					else {
						wm_widget_delete(&wgroup->widgets, widget_iter);
					}
					widget_iter = widget_next;
				}
				
				if (wgroup->type->draw) {
					wgroup->type->draw(C, wgroup);
				}
				
				if (highlighted) {
					for (widget_iter = wgroup->widgets.first; widget_iter; widget_iter = widget_iter->next) {
						if (widget_iter->ptr.data == highlighted->ptr.data &&
						    widget_iter->opptr.data == highlighted->opptr.data &&
						    widget_iter->prop == highlighted->prop) 
						{
							widget_iter->flag |= WM_WIDGET_HIGHLIGHT;
							wmap->highlighted_widget = widget_iter;
							wm_widget_delete(&wgroup->widgets, highlighted);
							highlighted = NULL;
						}
					}
				}
				
				/* if we don't find a highlighted widget, delete the old one here */
				if (highlighted) {
					if (highlighted->flag & WM_WIDGET_FREE_DATA)
						MEM_freeN(highlighted->customdata);
					MEM_freeN(highlighted);
					highlighted = NULL;
					wmap->highlighted_widget = NULL;
				}
				

				for (widget_iter = wgroup->widgets.first; widget_iter; widget_iter = widget_iter->next) {
					widget_calculate_scale(widget_iter, C);
					/* scale must be calculated still for hover widgets, we just avoid drawing */
					if (!(widget_iter->flag & WM_WIDGET_DRAW_HOVER) || (widget_iter->flag & WM_WIDGET_HIGHLIGHT))
						widget_iter->draw(widget_iter, C);
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

bool WM_widgetgrouptype_register(struct wmWidgetMapType *wmaptype, wmWidgetGroupType *wgrouptype)
{
	wmWidgetGroupType *wgrouptype_iter;
	/* search list, might already be registered */	
	for (wgrouptype_iter = wmaptype->widgetgrouptypes.first; wgrouptype_iter; wgrouptype_iter = wgrouptype_iter->next) {
		if (wgrouptype_iter == wgrouptype)
			return false;
	}
	
	BLI_addtail(&wmaptype->widgetgrouptypes, wgrouptype);
	return true;
}

void WM_widgetgrouptype_unregister(struct wmWidgetMapType *wmaptype, wmWidgetGroupType *wgrouptype)
{
	BLI_remlink(&wmaptype->widgetgrouptypes, wgrouptype);
	wmaptype->prev = wmaptype->next = NULL;
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

void WM_widget_set_3d_scale(struct wmWidget *widget, bool scale)
{
	if (scale) {
		widget->flag |= WM_WIDGET_SCALE_3D;
	}
	else {
		widget->flag &= ~WM_WIDGET_SCALE_3D;
	}
}


void WM_widget_set_draw_on_hover_only(struct wmWidget *widget, bool draw)
{
	if (draw) {
		widget->flag |= WM_WIDGET_DRAW_HOVER;
	}
	else {
		widget->flag &= ~WM_WIDGET_DRAW_HOVER;
	}
}

void WM_widget_set_scale(struct wmWidget *widget, float scale)
{
	widget->user_scale = scale;
}


wmWidgetMapType *WM_widgetmaptype_find(int spaceid, int regionid, bool is_3d, bool create)
{
	wmWidgetMapType *wmaptype;

	for (wmaptype = widgetmaptypes.first; wmaptype; wmaptype = wmaptype->next) {
		if (wmaptype->spaceid == spaceid && wmaptype->regionid == regionid) {
			if (wmaptype->is_3d == is_3d) {
				return wmaptype;
			}
		}
	}

	if (!create) return NULL;

	wmaptype = MEM_callocN(sizeof(wmWidgetMapType), "widgettype list");
	wmaptype->spaceid = spaceid;
	wmaptype->regionid = regionid;
	wmaptype->is_3d = is_3d;
	BLI_addhead(&widgetmaptypes, wmaptype);
	
	return wmaptype;
}

void WM_widgetmaptypes_free(void)
{
	wmWidgetMapType *wmaptype;
	
	for (wmaptype = widgetmaptypes.first; wmaptype; wmaptype = wmaptype->next) {
		BLI_freelistN(&wmaptype->widgetgrouptypes);
	}
	BLI_freelistN(&widgetmaptypes);

	fix_linking_widget_lib();
}

bool wm_widgetmap_is_3d(struct wmWidgetMap *wmap)
{
	return wmap->type->is_3d;
}

static void widget_find_active_3D_loop(bContext *C, ListBase *visible_widgets)
{
	int selectionbase = 0;
	LinkData *link;
	wmWidget *widget;

	for (link = visible_widgets->first; link; link = link->next) {
		widget = link->data;
		/* pass the selection id shifted by 8 bits. Last 8 bits are used for selected widget part id */
		widget->render_3d_intersection(C, widget, selectionbase << 8);

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

static void wm_prepare_visible_widgets_3D(struct wmWidgetMap *wmap, ListBase *visible_widgets, bContext *C)
{
	wmWidget *widget;
	wmWidgetGroup *wgroup;

	for (wgroup = wmap->widgetgroups.first; wgroup; wgroup = wgroup->next) {
		if (!wgroup->type->poll || wgroup->type->poll(C, wgroup->type)) {
			for (widget = wgroup->widgets.first; widget; widget = widget->next) {
				if (widget->render_3d_intersection) {
					BLI_addhead(visible_widgets, BLI_genericNodeN(widget));
				}
			}
		}
	}
}

wmWidget *wm_widget_find_highlighted_3D(struct wmWidgetMap *wmap, bContext *C, const struct wmEvent *event, unsigned char *part)
{
	int ret;
	wmWidget *result = NULL;
	
	ListBase visible_widgets = {0};

	wm_prepare_visible_widgets_3D(wmap, &visible_widgets, C);

	*part = 0;
	/* set up view matrices */	
	view3d_operator_needs_opengl(C);
	
	ret = wm_widget_find_highlighted_3D_intern(&visible_widgets, C, event, 0.5f * (float)U.tw_hotspot);
	
	if (ret != -1) {
		LinkData *link;
		int retsec;
		retsec = wm_widget_find_highlighted_3D_intern(&visible_widgets, C, event, 0.2f * (float)U.tw_hotspot);
		
		if (retsec != -1)
			ret = retsec;
		
		link = BLI_findlink(&visible_widgets, ret >> 8);
		*part = ret & 255;
		result = link->data;
	}

	BLI_freelistN(&visible_widgets);
	
	return result;
}

wmWidget *wm_widget_find_highlighted(struct wmWidgetMap *wmap, bContext *C, const struct wmEvent *event, unsigned char *part)
{
	wmWidget *widget;
	wmWidgetGroup *wgroup;

	for (wgroup = wmap->widgetgroups.first; wgroup; wgroup = wgroup->next) {
		if (!wgroup->type->poll || wgroup->type->poll(C, wgroup->type)) {
			for (widget = wgroup->widgets.first; widget; widget = widget->next) {
				if (widget->intersect) {
					if ((*part = widget->intersect(C, event, widget)))
						return widget;
				}
			}
		}
	}
	
	return NULL;
}


void wm_widgetmap_set_highlighted_widget(struct wmWidgetMap *wmap, struct bContext *C, struct wmWidget *widget, unsigned char part)
{
	if (widget != wmap->highlighted_widget) {
		ARegion *ar = CTX_wm_region(C);

		if (wmap->highlighted_widget) {
			wmap->highlighted_widget->flag &= ~WM_WIDGET_HIGHLIGHT;
			wmap->highlighted_widget->highlighted_part = 0;
		}
		
		wmap->highlighted_widget = widget;
		
		if (widget) {
			widget->flag |= WM_WIDGET_HIGHLIGHT;
			widget->highlighted_part = part;
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
		if (!widget->handler) {
			/* widget does nothing, pass */
			wmap->active_widget = NULL;
		}
		else if (widget->opptr.data || widget->ptr.data) {
			wmOperatorType *ot;
			const char *opname = (widget->opname) ? widget->opname : "WM_OT_widget_tweak";
			
			ot = WM_operatortype_find(opname, 0);
			
			if (ot) {
				widget->flag |= WM_WIDGET_ACTIVE;
				/* first activate the widget itself */
				if (widget->activate_state) {
					widget->activate_state(C, event, widget, WIDGET_ACTIVATE);
				}

				CTX_wm_widget_set(C, widget);
				WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &widget->opptr);
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
		}

		CTX_wm_widget_set(C, NULL);

		wmap->active_widget = NULL;
		ED_region_tag_redraw(ar);
		WM_event_add_mousemove(C);
	}
}

struct wmWidget *wm_widgetmap_get_active_widget(struct wmWidgetMap *wmap)
{
	return wmap->active_widget;
}


struct wmWidgetMap *WM_widgetmap_from_type(int spaceid, int regionid, bool is_3d)
{
	wmWidgetMapType *wmaptype = WM_widgetmaptype_find(spaceid, regionid, is_3d, true);
	wmWidgetGroupType *wgrouptype = wmaptype->widgetgrouptypes.first;
	wmWidgetMap *wmap;

	/* no need to create a widgetmap if no types have been registered */
	if (!wgrouptype)
		return NULL;

	wmap = MEM_callocN(sizeof(wmWidgetMap), "WidgetMap");
	wmap->type = wmaptype;

	/* create all widgetgroups for this widgetmap */
	for (; wgrouptype; wgrouptype = wgrouptype->next) {
		wmWidgetGroup *wgroup = MEM_callocN(sizeof(wmWidgetGroup), "widgetgroup");
		wgroup->type = wgrouptype;
		BLI_addtail(&wmap->widgetgroups, wgroup);
	}

	return wmap;
}

void WM_widgetmap_delete(struct wmWidgetMap *wmap)
{
	wmWidgetGroup *wgroup;

	if (!wmap)
		return;

	for (wgroup = wmap->widgetgroups.first; wgroup; wgroup = wgroup->next) {
		wmWidget *widget;
		
		for (widget = wgroup->widgets.first; widget;) {
			wmWidget *widget_next = widget->next;
			wm_widget_delete(&wgroup->widgets, widget);
			widget = widget_next;
		}
	}
	BLI_freelistN(&wmap->widgetgroups);

	MEM_freeN(wmap);
}

static void wm_widgetgroup_free(wmWidgetMap *wmap, wmWidgetGroup *wgroup)
{
	wmWidget *widget;

	for (widget = wgroup->widgets.first; widget;) {
		wmWidget *widget_next = widget->next;
		wm_widget_delete(&wgroup->widgets, widget);
		widget = widget_next;
	}

#ifdef WITH_PYTHON
	if (wgroup->py_instance) {
		/* do this first in case there are any __del__ functions or
		 * similar that use properties */
		BPY_DECREF_RNA_INVALIDATE(wgroup->py_instance);
	}
#endif

	if (wgroup->reports && (wgroup->reports->flag & RPT_FREE)) {
		BKE_reports_clear(wgroup->reports);
		MEM_freeN(wgroup->reports);
	}

	BLI_remlink(&wmap->widgetgroups, wgroup);
	MEM_freeN(wgroup);
}


void WM_widgetgrouptype_free(Main *bmain, wmWidgetGroupType *wgrouptype)
{
	bScreen *sc;
	wmWidgetMapType *wmaptype;

	for (sc = bmain->screen.first; sc; sc = sc->id.next) {
		ScrArea *sa;
		for (sa = sc->areabase.first; sa; sa = sa->next) {
			SpaceLink *sl;

			for (sl = sa->spacedata.first; sl; sl = sl->next) {
				ARegion *ar;
				ListBase *lb = (sl == sa->spacedata.first) ? &sa->regionbase : &sl->regionbase;

				for (ar = lb->first; ar; ar = ar->next) {
					if (ar->widgetmap) {
						wmWidgetMap *wmap = ar->widgetmap;
						wmWidgetGroup *wgroup;
						for (wgroup = wmap->widgetgroups.first; wgroup; wgroup = wgroup->next) {
							if (wgroup->type == wgrouptype) {
								wm_widgetgroup_free(wmap, wgroup);
							}
						}
					}
				}
			}
		}
	}

	wmaptype = WM_widgetmaptype_find(wgrouptype->spaceid, wgrouptype->regionid, wgrouptype->is_3d, false);
	BLI_remlink(&wmaptype->widgetgrouptypes, wgrouptype);
	MEM_freeN(wgrouptype);
}


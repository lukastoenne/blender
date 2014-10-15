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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/wm.h
 *  \ingroup wm
 */

#ifndef __WM_H__
#define __WM_H__

struct wmWindow;
struct ReportList;
struct wmEvent;
struct wmWidgetMap;
struct wmOperatorType;

typedef struct wmPaintCursor {
	struct wmPaintCursor *next, *prev;

	void *customdata;
	
	int (*poll)(struct bContext *C);
	void (*draw)(bContext *C, int, int, void *customdata);
} wmPaintCursor;

/* widgets are set per screen/area/region by registering them on widgetmaps */
typedef struct wmWidget {
	struct wmWidget *next, *prev;

	void *customdata;

	/* draw widget */
	void (*draw)(struct wmWidget *widget, const struct bContext *C, float scale);
	/* determine if the mouse intersects with the widget. The calculation should be done in the callback itself */
	int  (*intersect)(struct bContext *C, const struct wmEvent *event, struct wmWidget *widget);

	/* determines 3d intersetion by rendering the widget in a selection routine. */
	void (*render_3d_intersection)(const struct bContext *C, struct wmWidget *widget, float scale, int selectionbase);

	/* handler used by the widget. Usually handles interaction tied to a widget type */
	int  (*handler)(struct bContext *C, const struct wmEvent *event, struct wmWidget *widget);

	int  flag; /* flags set by drawing and interaction, such as highlighting */

	/* position in space, 2d or 3d */
	float origin[3];

	/* data used during interaction */
	void *interaction_data;

	/* name of operator to spawn when activating the widget */
	char *opname;
	/* property name of the operator that the widget controls */
	char *oppropname;
	/* operator type that will be called */
	wmOperatorType *ot;

	/* operator properties, stored if widget spawns and controls an operator */
	struct PointerRNA opptr;
} wmWidget;

/* wmWidget->flag */
#define WM_WIDGET_HIGHLIGHT    (1 << 0)
#define WM_WIDGET_FREE_DATA    (1 << 1)
#define WM_WIDGET_SKIP_DRAW    (1 << 2)

extern void wm_close_and_free(bContext *C, wmWindowManager *);
extern void wm_close_and_free_all(bContext *C, ListBase *);

extern void wm_add_default(bContext *C);
extern void wm_clear_default_size(bContext *C);
			
			/* register to windowmanager for redo or macro */
void		wm_operator_register(bContext *C, wmOperator *op);

/* wm_operator.c, for init/exit */
void wm_operatortype_free(void);
void wm_operatortype_init(void);
void wm_window_keymap(wmKeyConfig *keyconf);

void wm_tweakevent_test(bContext *C, wmEvent *event, int action);

/* wm_gesture.c */
#define WM_LASSO_MIN_POINTS		1024
void wm_gesture_draw(struct wmWindow *win);
int wm_gesture_evaluate(wmGesture *gesture);
void wm_gesture_tag_redraw(bContext *C);

/* wm_jobs.c */
void wm_jobs_timer(const bContext *C, wmWindowManager *wm, wmTimer *wt);
void wm_jobs_timer_ended(wmWindowManager *wm, wmTimer *wt);

/* wm_files.c */
void wm_autosave_timer(const bContext *C, wmWindowManager *wm, wmTimer *wt);
void wm_autosave_timer_ended(wmWindowManager *wm);
void wm_autosave_delete(void);
void wm_autosave_read(bContext *C, struct ReportList *reports);
void wm_autosave_location(char *filepath);

/* init operator properties */
void wm_open_init_load_ui(wmOperator *op, bool use_prefs);
void wm_open_init_use_scripts(wmOperator *op, bool use_prefs);

/* wm_widgets.c */
bool wm_widgetmap_is_3d(struct wmWidgetMap *wmap);

/* hack to store circle select size - campbell, must replace with nice operator memory */
#define GESTURE_MEMORY

#ifdef GESTURE_MEMORY
extern int circle_select_size;
#endif

#endif /* __WM_H__ */


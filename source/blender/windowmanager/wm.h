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
struct PointerRNA;
struct PropertyRNA;
struct wmOperator;

typedef struct wmPaintCursor {
	struct wmPaintCursor *next, *prev;

	void *customdata;
	
	int (*poll)(struct bContext *C);
	void (*draw)(bContext *C, int, int, void *customdata);
} wmPaintCursor;

/* widgets are set per screen/area/region by registering them on widgetmaps */
typedef struct wmWidget {
	struct wmWidget *next, *prev;
	
	char idname[64];

	/* draw widget */
	void (*draw)(struct wmWidget *widget, const struct bContext *C);
	/* determine if the mouse intersects with the widget. The calculation should be done in the callback itself */
	int  (*intersect)(struct bContext *C, const struct wmEvent *event, struct wmWidget *widget);

	/* determines 3d intersection by rendering the widget in a selection routine. */
	void (*render_3d_intersection)(const struct bContext *C, struct wmWidget *widget, int selectionbase);

	/* handler used by the widget. Usually handles interaction tied to a widget type */
	int  (*handler)(struct bContext *C, const struct wmEvent *event, struct wmWidget *widget);

	/* widget-specific handler to update widget attributes when a property is bound */
	void (*bind_to_prop)(struct wmWidget *widget, int slot);

	/* returns the final position which may be different from the origin, depending on the widget.
	 * used in calculations of scale */
	void (*get_final_position)(struct wmWidget *widget, float vec[3]);

	/* activate a widget state when the user clicks on it */
	int (*invoke)(struct bContext *C, const struct wmEvent *event, struct wmWidget *widget);

	int (*get_cursor)(struct wmWidget *widget);
	
	int  flag; /* flags set by drawing and interaction, such as highlighting */

	unsigned char highlighted_part;

	/* center of widget in space, 2d or 3d */
	float origin[3];

	/* runtime property, set the scale while drawing on the viewport */
	float scale;

	/* user defined scale, in addition to the original one */
	float user_scale;

	/* data used during interaction */
	void *interaction_data;

	/* name of operator to spawn when activating the widget */
	const char *opname;

	/* operator properties if widget spawns and controls an operator, or owner pointer if widget spawns and controls a property */
	struct PointerRNA opptr;

	/* maximum number of properties attached to the widget */
	int max_prop;
	
	/* arrays of properties attached to various widget parameters. As the widget is interacted with, those properties get updated */
	struct PointerRNA *ptr;
	struct PropertyRNA **props;
} wmWidget;

/* wmWidget->flag */
enum widgetflags {
	/* states */
	WM_WIDGET_HIGHLIGHT   = (1 << 0),
	WM_WIDGET_ACTIVE      = (1 << 1),

	WM_WIDGET_DRAW_HOVER  = (1 << 2),

	WM_WIDGET_SCALE_3D    = (1 << 3),
	WM_WIDGET_SCENE_DEPTH = (1 << 4) /* widget is depth culled with scene objects*/
};

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

/* wm_stereo.c */
void wm_method_draw_stereo3d(const bContext *C, wmWindow *win);
int wm_stereo3d_set_exec(bContext *C, wmOperator *op);
int wm_stereo3d_set_invoke(bContext *C, wmOperator *op, const wmEvent *event);
void wm_stereo3d_set_draw(bContext *C, wmOperator *op);
bool wm_stereo3d_set_check(bContext *C, wmOperator *op);
void wm_stereo3d_set_cancel(bContext *C, wmOperator *op);

/* init operator properties */
void wm_open_init_load_ui(wmOperator *op, bool use_prefs);
void wm_open_init_use_scripts(wmOperator *op, bool use_prefs);

/* wm_widgets.c */
bool wm_widgetmap_is_3d(struct wmWidgetMap *wmap);
bool wm_widget_register(struct wmWidgetGroup *wgroup, struct wmWidget *widget);


/* hack to store circle select size - campbell, must replace with nice operator memory */
#define GESTURE_MEMORY

#ifdef GESTURE_MEMORY
extern int circle_select_size;
#endif

void fix_linking_widget_lib(void);

#endif /* __WM_H__ */


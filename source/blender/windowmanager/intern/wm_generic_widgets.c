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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/interface_generic_widgets.c
 *  \ingroup edinterface
 */

#include "RNA_types.h"
#include "RNA_access.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_object_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_userdef_types.h"

#include "BLI_utildefines.h"
#include "BLI_math_matrix.h"
#include "BLI_math.h"
#include "BLI_rect.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_object.h"

#include "ED_view3d.h"
#include "ED_screen.h"

#include "WM_types.h"
#include "WM_api.h"

#include "GL/glew.h"
#include "GPU_select.h"

#include "BIF_glutil.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.h"

#include "3d_widgets/ui_widget_library.h"

#include "wm.h"
#include "WM_types.h"


/******************************************************
 *            GENERIC WIDGET LIBRARY                  *
 ******************************************************/

typedef struct WidgetDrawInfo {
	int nverts;
	int ntris;
	float (*verts)[3];
	float (*normals)[3];
	unsigned short *indices;
	bool init;
} WidgetDrawInfo;


WidgetDrawInfo arraw_draw_info = {0};
WidgetDrawInfo dial_draw_info = {0};

static void widget_draw_intern(WidgetDrawInfo *info, bool select)
{
	GLuint buf[3];

	bool use_lighting = !select && ((U.tw_flag & V3D_SHADED_WIDGETS) != 0);

	if (use_lighting)
		glGenBuffers(3, buf);
	else
		glGenBuffers(2, buf);

	glEnableClientState(GL_VERTEX_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, buf[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * info->nverts, info->verts, GL_STATIC_DRAW);
	glVertexPointer(3, GL_FLOAT, 0, NULL);

	if (use_lighting) {
		glEnableClientState(GL_NORMAL_ARRAY);
		glBindBuffer(GL_ARRAY_BUFFER, buf[2]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * info->nverts, info->normals, GL_STATIC_DRAW);
		glNormalPointer(GL_FLOAT, 0, NULL);
		glShadeModel(GL_SMOOTH);
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf[1]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short) * (3 * info->ntris), info->indices, GL_STATIC_DRAW);

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	glDrawElements(GL_TRIANGLES, info->ntris * 3, GL_UNSIGNED_SHORT, NULL);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glDisableClientState(GL_VERTEX_ARRAY);

	if (use_lighting) {
		glDisableClientState(GL_NORMAL_ARRAY);
		glShadeModel(GL_FLAT);
		glDeleteBuffers(3, buf);
	}
	else {
		glDeleteBuffers(2, buf);
	}
}

/* Arrow widget */

typedef struct ArrowWidget {
	wmWidget widget;
	int style;
	float direction[3];
	float color[4];
} ArrowWidget;

typedef struct ArrowInteraction {
	float orig_origin[3];
	float orig_mouse[2];

	/* direction vector, projected in screen space */
	float proj_direction[2];
} ArrowInteraction;

static void arrow_draw_intern(ArrowWidget *arrow, bool select, bool highlight, float scale)
{
	float rot[3][3];
	float mat[4][4];
	float up[3] = {0.0f, 0.0f, 1.0f};

	rotation_between_vecs_to_mat3(rot, up, arrow->direction);
	copy_m4_m3(mat, rot);
	copy_v3_v3(mat[3], arrow->widget.origin);
	mul_mat3_m4_fl(mat, scale);

	glPushMatrix();
	glMultMatrixf(&mat[0][0]);

	if (highlight)
		glColor4f(1.0, 1.0, 0.0, 1.0);
	else
		glColor4fv(arrow->color);

	widget_draw_intern(&arraw_draw_info, select);

	glPopMatrix();

	if (arrow->widget.interaction_data) {
		ArrowInteraction *data = arrow->widget.interaction_data;

		copy_m4_m3(mat, rot);
		copy_v3_v3(mat[3], data->orig_origin);
		mul_mat3_m4_fl(mat, scale);

		glPushMatrix();
		glMultMatrixf(&mat[0][0]);

		glEnable(GL_BLEND);
		glColor4f(0.5f, 0.5f, 0.5f, 0.5f);
		widget_draw_intern(&arraw_draw_info, select);
		glDisable(GL_BLEND);

		glPopMatrix();
	}
}

static void widget_arrow_render_3d_intersect(const struct bContext *UNUSED(C), struct wmWidget *widget, float scale, int selectionbase)
{
	GPU_select_load_id(selectionbase);
	arrow_draw_intern((ArrowWidget *)widget, true, false, scale);
}

static void widget_arrow_draw(struct wmWidget *widget, const struct bContext *UNUSED(C), float scale)
{
	arrow_draw_intern((ArrowWidget *)widget, false, (widget->flag & WM_WIDGET_HIGHLIGHT) != 0, scale);
}

static int widget_arrow_handler(struct bContext *C, const struct wmEvent *event, struct wmWidget *widget, struct wmOperator *op)
{
	ArrowWidget *arrow = (ArrowWidget *)widget;
	ArrowInteraction *data = widget->interaction_data;
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;

	float orig_origin[4];
	float offset[4];
	float m_diff[2];
	float dir_2d[2], dir2d_final[2];
	float fac;

	copy_v3_v3(orig_origin, data->orig_origin);
	orig_origin[3] = 1.0f;
	add_v3_v3v3(offset, orig_origin, arrow->direction);
	offset[3] = 1.0f;

	/* multiply to projection space */
	mul_m4_v4(rv3d->persmat, orig_origin);
	mul_m4_v4(rv3d->persmat, offset);

	mul_v4_fl(orig_origin, 1.0f/orig_origin[3]);
	mul_v4_fl(offset, 1.0f/offset[3]);
	sub_v2_v2v2(dir_2d, offset, orig_origin);
	normalize_v2(dir_2d);

	dir_2d[0] *= BLI_rcti_size_x(&ar->winrct) * 0.5f;
	dir_2d[1] *= BLI_rcti_size_y(&ar->winrct) * 0.5f;

	/* find mouse difference */
	m_diff[0] = event->mval[0] - data->orig_mouse[0];
	m_diff[1] = event->mval[1] - data->orig_mouse[1];

	/* project the displacement on the screen space arrow direction */
	project_v2_v2v2(dir2d_final, m_diff, dir_2d);

	dir2d_final[0] /= (BLI_rcti_size_x(&ar->winrct) * 0.5f);
	dir2d_final[1] /= (BLI_rcti_size_y(&ar->winrct) * 0.5f);

	add_v2_v2(orig_origin, dir2d_final);

	/* project back to world space and find world space displacement direction */
	mul_m4_v4(rv3d->persinv, orig_origin);
	mul_v4_fl(orig_origin, 1.0f/orig_origin[3]);

	sub_v3_v3(orig_origin, data->orig_origin);

	/* reuse offset, project direction to displacement */
	project_v3_v3v3(offset, arrow->direction, orig_origin);
	fac = len_v3(orig_origin) / len_v3(offset);
	if (dot_v3v3(offset, orig_origin) < 0.0f)
		fac *= -1.0;
	mul_v3_v3fl(widget->origin, offset, fac);
	add_v3_v3(widget->origin, data->orig_origin);

	/* set the property for the operator and call its modal function */
	if (op && widget->prop) {
		RNA_float_set_array(op->ptr, widget->prop, widget->origin);
	}

	/* tag the region for redraw */
	ED_region_tag_redraw(ar);

	return OPERATOR_PASS_THROUGH;
}


static int widget_arrow_activate(struct bContext *UNUSED(C), const struct wmEvent *event, struct wmWidget *widget, int state)
{
	if (state == WIDGET_ACTIVATE) {
		ArrowInteraction *data = MEM_callocN(sizeof (ArrowInteraction), "arrow_interaction");
		data->orig_mouse[0] = event->mval[0];
		data->orig_mouse[1] = event->mval[1];

		copy_v3_v3(data->orig_origin, widget->origin);

		widget->interaction_data = data;
	}
	else if (state == WIDGET_DEACTIVATE) {
		MEM_freeN(widget->interaction_data);
		widget->interaction_data = NULL;
	}
	return OPERATOR_FINISHED;
}


wmWidget *WIDGET_arrow_new(int style,
                           int (*initialize_op)(struct bContext *C, const struct wmEvent *event, struct wmWidget *widget, struct PointerRNA *ptr),
                           const char *opname, const char *prop, void *customdata)
{
	float dir_default[3] = {0.0f, 0.0f, 1.0f};
	ArrowWidget *arrow;

	if (!arraw_draw_info.init) {
		arraw_draw_info.nverts = _WIDGET_nverts_arrow,
		arraw_draw_info.ntris = _WIDGET_ntris_arrow,
		arraw_draw_info.verts = _WIDGET_verts_arrow,
		arraw_draw_info.normals = _WIDGET_normals_arrow,
		arraw_draw_info.indices = _WIDGET_indices_arrow,
		arraw_draw_info.init = true;
	}
	
	arrow = MEM_callocN(sizeof(ArrowWidget), "arrowwidget");
	
	arrow->widget.draw = widget_arrow_draw;
	arrow->widget.initialize_op = initialize_op;
	arrow->widget.intersect = NULL;
	arrow->widget.handler = widget_arrow_handler;
	arrow->widget.activate_state = widget_arrow_activate;
	arrow->widget.render_3d_intersection = widget_arrow_render_3d_intersect;
	arrow->widget.opname = opname;
	arrow->widget.prop = prop;
	arrow->widget.customdata = customdata;

	arrow->style = style;
	copy_v3_v3(arrow->direction, dir_default);
	
	return (wmWidget *)arrow;
}

void WIDGET_arrow_set_color(struct wmWidget *widget, float color[4])
{
	ArrowWidget *arrow = (ArrowWidget *)widget;
	
	copy_v4_v4(arrow->color, color);
}

void WIDGET_arrow_set_direction(struct wmWidget *widget, float direction[3])
{
	ArrowWidget *arrow = (ArrowWidget *)widget;
	
	copy_v3_v3(arrow->direction, direction);
	normalize_v3(arrow->direction);
}

/* Dial widget */

typedef struct DialWidget {
	wmWidget widget;
	int style;
	float direction[3];
	float color[4];
} DialWidget;

static void dial_draw_intern(DialWidget *dial, bool select, bool highlight, float scale)
{
	float rot[3][3];
	float mat[4][4];
	float up[3] = {0.0f, 0.0f, 1.0f};

	rotation_between_vecs_to_mat3(rot, up, dial->direction);
	copy_m4_m3(mat, rot);
	copy_v3_v3(mat[3], dial->widget.origin);
	mul_mat3_m4_fl(mat, scale);

	glPushMatrix();
	glMultMatrixf(&mat[0][0]);

	if (highlight)
		glColor4f(1.0, 1.0, 0.0, 1.0);
	else
		glColor4fv(dial->color);

	widget_draw_intern(&dial_draw_info, select);

	glPopMatrix();

}

static void widget_dial_render_3d_intersect(const struct bContext *C, struct wmWidget *widget, float scale, int selectionbase)
{
	DialWidget *dial = (DialWidget *)widget;
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;

	/* enable clipping if needed */
	if (dial->style == UI_DIAL_STYLE_RING_CLIPPED) {
		double plane[4];
		copy_v3db_v3fl(plane, rv3d->viewinv[2]);
		plane[3] = -dot_v3v3(rv3d->viewinv[2], widget->origin);
		glClipPlane(GL_CLIP_PLANE0, plane);
		glEnable(GL_CLIP_PLANE0);
	}

	GPU_select_load_id(selectionbase);
	dial_draw_intern(dial, true, false, scale);

	if (dial->style == UI_DIAL_STYLE_RING_CLIPPED) {
		glDisable(GL_CLIP_PLANE0);
	}
}

static void widget_dial_draw(struct wmWidget *widget, const struct bContext *C, float scale)
{
	DialWidget *dial = (DialWidget *)widget;
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;

	/* enable clipping if needed */
	if (dial->style == UI_DIAL_STYLE_RING_CLIPPED) {
		double plane[4];
		copy_v3db_v3fl(plane, rv3d->viewinv[2]);
		plane[3] = -dot_v3v3(rv3d->viewinv[2], widget->origin);
		glClipPlane(GL_CLIP_PLANE0, plane);
		glEnable(GL_CLIP_PLANE0);
	}

	dial_draw_intern(dial, false, (widget->flag & WM_WIDGET_HIGHLIGHT) != 0, scale);

	if (dial->style == UI_DIAL_STYLE_RING_CLIPPED) {
		glDisable(GL_CLIP_PLANE0);
	}
}

wmWidget *WIDGET_dial_new(int style,
                          int (*initialize_op)(struct bContext *C, const struct wmEvent *event, struct wmWidget *widget, struct PointerRNA *ptr),
                          const char *opname, const char *prop, void *customdata)
{
	float dir_default[3] = {0.0f, 0.0f, 1.0f};
	DialWidget *dial;

	if (!dial_draw_info.init) {
		dial_draw_info.nverts = _WIDGET_nverts_dial,
		dial_draw_info.ntris = _WIDGET_ntris_dial,
		dial_draw_info.verts = _WIDGET_verts_dial,
		dial_draw_info.normals = _WIDGET_normals_dial,
		dial_draw_info.indices = _WIDGET_indices_dial,
		dial_draw_info.init = true;
	}

	dial = MEM_callocN(sizeof(ArrowWidget), "arrowwidget");

	dial->widget.draw = widget_dial_draw;
	dial->widget.initialize_op = initialize_op;
	dial->widget.intersect = NULL;
	dial->widget.render_3d_intersection = widget_dial_render_3d_intersect;
	dial->widget.opname = opname;
	dial->widget.prop = prop;
	dial->widget.customdata = customdata;

	dial->style = style;
	copy_v3_v3(dial->direction, dir_default);

	return (wmWidget *)dial;
}

void WIDGET_dial_set_color(struct wmWidget *widget, float color[4])
{
	DialWidget *arrow = (DialWidget *)widget;

	copy_v4_v4(arrow->color, color);
}

void WIDGET_dial_set_direction(struct wmWidget *widget, float direction[3])
{
	DialWidget *arrow = (DialWidget *)widget;

	copy_v3_v3(arrow->direction, direction);
	normalize_v3(arrow->direction);
}

void fix_linking_widget_lib(void)
{
	(void) 0;
}

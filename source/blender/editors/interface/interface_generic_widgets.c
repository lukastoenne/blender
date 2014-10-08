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

#include "WM_types.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_lamp_types.h"
#include "DNA_object_types.h"
#include "DNA_lamp_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_utildefines.h"
#include "BLI_math_matrix.h"
#include "BLI_math.h"

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
#include "interface_intern.h"

#include "3d_widgets/ui_widget_library.h"

typedef struct LampPositionData {
	int pos[2];
	float quat[4];
	float lvec[3];
} LampPositionData;

/* Modal Operator init */
static int lamp_position_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Object *ob = CTX_data_active_object(C);	
	LampPositionData *data;
	data = op->customdata = MEM_mallocN(sizeof (LampPositionData), "lamp_position_data");
	
	copy_v2_v2_int(data->pos, event->mval);

	mat4_to_quat(data->quat, ob->obmat);
	copy_v3_v3(data->lvec, ob->obmat[2]);
	negate_v3(data->lvec);
	normalize_v3(data->lvec);
	
	WM_event_add_modal_handler(C, op);
	
	return OPERATOR_RUNNING_MODAL;
}

/* Repeat operator */
static int lamp_position_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	
	LampPositionData *data = op->customdata;
	
	switch (event->type) {
		case MOUSEMOVE:
		{
			Object *ob = CTX_data_active_object(C);	
			Lamp *la = ob->data;			
			Scene *scene = CTX_data_scene(C);
			ARegion *ar = CTX_wm_region(C);
			View3D *v3d = CTX_wm_view3d(C);
			float world_pos[3];
			int flag = v3d->flag2;
			
			v3d->flag2 |= V3D_RENDER_OVERRIDE;

			view3d_operator_needs_opengl(C);
			if (ED_view3d_autodist(scene, ar, v3d, event->mval, world_pos, true, NULL)) {
				float axis[3];

				/* restore the floag here */				
				v3d->flag2 = flag;
				
				sub_v3_v3(world_pos, ob->obmat[3]);
				la->dist = normalize_v3(world_pos);
				
				cross_v3_v3v3(axis, data->lvec, world_pos);
				if (normalize_v3(axis) > 0.0001) {
					float mat[4][4];
					float quat[4], qfinal[4];
					float angle = saacos(dot_v3v3(world_pos, data->lvec));

					/* transform the initial rotation quaternion to the new position and set the matrix to the lamp */
					axis_angle_to_quat(quat, axis, angle);
					mul_qt_qtqt(qfinal, quat, data->quat);
					quat_to_mat4(mat, qfinal);
					copy_v3_v3(mat[3], ob->obmat[3]);
					
					BKE_object_apply_mat4(ob, mat, true, false);
				}
				
				DAG_id_tag_update(&ob->id, OB_RECALC_OB);
				
				ED_region_tag_redraw(ar);
			}
			
			v3d->flag2 = flag;
			
			break;
		}
			
		case LEFTMOUSE:
			if (event->val == KM_RELEASE) {
				MEM_freeN(op->customdata);
				return OPERATOR_FINISHED;
			}
	}

	return OPERATOR_RUNNING_MODAL;
}

static int lamp_position_poll(bContext *C)
{
	return CTX_wm_region_view3d(C) != NULL;
}


void UI_OT_lamp_position(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Lamp Position";
	ot->idname = "UI_OT_lamp_position";
	ot->description = "Sample a color from the Blender Window to store in a property";

	/* api callbacks */
	ot->invoke = lamp_position_invoke;
	ot->modal = lamp_position_modal;
	ot->poll = lamp_position_poll;

	/* flags */
	ot->flag = OPTYPE_BLOCKING | OPTYPE_UNDO;

	/* properties */	
}

int WIDGET_lamp_handler(struct bContext *C, const struct wmEvent *event, struct wmWidget *widget)
{	
	
	if (event->type == LEFTMOUSE && event->val == KM_PRESS && (widget->flag & WM_WIDGET_HIGHLIGHT)) {
		struct PointerRNA *ptr = NULL;			/* rna pointer to access properties */
		struct IDProperty *properties = NULL;	/* operator properties, assigned to ptr->data and can be written to a file */

		WM_operator_properties_alloc(&ptr, &properties, "UI_OT_lamp_position");
		WM_operator_name_call(C, "UI_OT_lamp_position", WM_OP_INVOKE_DEFAULT, ptr);
		WM_operator_properties_free(ptr);
		MEM_freeN(ptr);
	}

	return OPERATOR_FINISHED;
}

static void intern_lamp_draw(const struct bContext *C, int selectoffset, wmWidget *widget)
{
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;
	float size = 0.30f;

	Object *ob = CTX_data_active_object(C);
	Lamp *la = ob->data;
	
	float widgetmat[4][4];	

	copy_m4_m4(widgetmat, ob->obmat);	
	normalize_m4(widgetmat);
	glPushMatrix();
	glMultMatrixf(&widgetmat[0][0]);

	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);

	if (selectoffset != -1)
		GPU_select_load_id(selectoffset);
	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	
	if (widget->flag & WM_WIDGET_HIGHLIGHT)
		glColor4f(1.0, 1.0, 0.4, 1.0);
	else
		glColor4f(1.0, 1.0, 1.0, 1.0);
	glVertex3f(0.0, 0.0, 0.0);
	if (widget->flag & WM_WIDGET_HIGHLIGHT)
		glColor4f(1.0, 1.0, 2.0, 1.0);
	else
		glColor4f(0.0, 0.0, 0.0, 0.0);
	glVertex3f(0.0, 0.0, -la->dist);
	glEnd();	

	glDisable(GL_BLEND);	
	
	glPopMatrix();
	mul_mat3_m4_fl(widgetmat, ED_view3d_pixel_size(rv3d, widgetmat[3]) * U.tw_size);	
	
	glPushMatrix();
	glMultMatrixf(&widgetmat[0][0]);

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glBegin(GL_QUAD_STRIP);
	glVertex3f(size, size, 3.0 * size);
	glVertex3f(-size, size, 3.0 * size);
	glVertex3f(size, -size, 3.0 * size);
	glVertex3f(-size, -size, 3.0 * size);
	glVertex3f(size, -size, 0.0);
	glVertex3f(-size, -size, 0.0);
	glVertex3f(size, size, 0.0);
	glVertex3f(-size, size, 0.0);
	glVertex3f(size, size, 3.0 * size);
	glVertex3f(-size, size, 3.0 * size);
	glEnd();
	
	glPushMatrix();
	glTranslatef(size, 0.0f, 0.0f);	
	glRotatef(-90.0f * la->spotsize / M_PI, 0.0f, 1.0f, 0.0f);
	
	glBegin(GL_QUADS);
	glVertex3f(0.0f, -size, 0.0);
	glVertex3f(0.0f, size, 0.0);
	glVertex3f(0.0f, size, -size);
	glVertex3f(0.0f, -size, -size);	
	glEnd();
	glPopMatrix();	

	glPushMatrix();
	glTranslatef(-size, 0.0f, 0.0f);
	glRotatef(90.0f * la->spotsize / M_PI, 0.0f, 1.0f, 0.0f);
	
	glBegin(GL_QUADS);
	glVertex3f(0.0f, -size, 0.0);
	glVertex3f(0.0f, size, 0.0);
	glVertex3f(0.0f, size, -size);
	glVertex3f(0.0f, -size, -size);	
	glEnd();
	glPopMatrix();	

	glPushMatrix();
	glTranslatef(0.0f, size, 0.0f);
	glRotatef(90.0f * la->spotsize / M_PI, 1.0f, 0.0f, 0.0f);
	
	glBegin(GL_QUADS);
	glVertex3f(-size, 0.0f, 0.0f);
	glVertex3f(size, 0.0f, 0.0f);
	glVertex3f(size, 0.0f, -size);
	glVertex3f(-size, 0.0f, -size);	
	glEnd();
	glPopMatrix();	

	glPushMatrix();
	glTranslatef(0.0f, -size, 0.0f);
	glRotatef(-90.0f * la->spotsize / M_PI, 1.0f, 0.0f, 0.0f);
	
	glBegin(GL_QUADS);
	glVertex3f(-size, 0.0f, 0.0f);
	glVertex3f(size, 0.0f, 0.0f);
	glVertex3f(size, 0.0f, -size);
	glVertex3f(-size, 0.0f, -size);	
	glEnd();
	glPopMatrix();	
	
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	
	glPopMatrix();	
}

void WIDGET_lamp_render_3d_intersect(const struct bContext *C, struct wmWidget *widget, int selectionbase)
{
	intern_lamp_draw(C, selectionbase, widget);	
}

void WIDGET_lamp_draw(wmWidget *widget, const struct bContext *C)
{
	intern_lamp_draw(C, -1, widget);
}

bool WIDGETGROUP_lamp_poll(struct wmWidgetGroup *UNUSED(widget), const struct bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	
	if (ob && ob->type == OB_LAMP) {
		Lamp *la = ob->data;
		return (la->type == LA_SPOT);      
	}
	return false;
}



/******************************************************
 *            GENERIC WIDGET LIBRARY                  *
 ******************************************************/

typedef struct ArrowWidget {
	wmWidget widget;
	int style;
	float origin[3];
	float direction[3];
	float color[4];
} ArrowWidget;

static void widget_draw_intern(ArrowWidget *arrow, bool select, bool highlight)
{
	float rot[3][3];
	float mat[4][4];
	float up[3] = {0.0f, 0.0f, 1.0f};
	GLuint buf[3];
	if (!select)
		glGenBuffers(3, buf);
	else
		glGenBuffers(2, buf);
	
	rotation_between_vecs_to_mat3(rot, up, arrow->direction);
	copy_m4_m3(mat, rot);
	copy_v3_v3(mat[3], arrow->origin);
	
	glEnableClientState(GL_VERTEX_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, buf[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * _WIDGET_nverts_arrow, _WIDGET_verts_arrow, GL_STATIC_DRAW);
	glVertexPointer(3, GL_FLOAT, 0, NULL);	

	if (!select) {
		float lightpos[4] = {0.0, 0.0, 1.0, 0.0};
		float diffuse[4] = {1.0, 1.0, 1.0, 0.0};
		glEnableClientState(GL_NORMAL_ARRAY);
		glBindBuffer(GL_ARRAY_BUFFER, buf[2]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * _WIDGET_nverts_arrow, _WIDGET_normals_arrow, GL_STATIC_DRAW);
		glNormalPointer(GL_FLOAT, 0, NULL);	

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
		glShadeModel(GL_SMOOTH);
	}
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf[1]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short) * (3 * _WIDGET_nfaces_arrow), _WIDGET_indices_arrow, GL_STATIC_DRAW);

	if (highlight)
		glColor3f(1.0, 1.0, 0.0);
	else 
		glColor3fv(arrow->color);

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glPushMatrix();
	glMultMatrixf(&mat[0][0]);	
	
	glDrawElements(GL_TRIANGLES, _WIDGET_nfaces_arrow * 3, GL_UNSIGNED_SHORT, NULL);

	glPopMatrix();
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	glDisableClientState(GL_VERTEX_ARRAY);
		
	if (!select) {
		glDisableClientState(GL_NORMAL_ARRAY);
		glPopAttrib();
		glShadeModel(GL_FLAT);
		glDeleteBuffers(3, buf);
	}
	else {
		glDeleteBuffers(2, buf);
	}
}

static void widget_arrow_render_3d_intersect(const struct bContext *UNUSED(C), struct wmWidget *widget, int selectionbase)
{
	GPU_select_load_id(selectionbase);
	widget_draw_intern((ArrowWidget *)widget, true, false);
}

static void widget_arrow_draw(struct wmWidget *widget, const struct bContext *UNUSED(C))
{
	widget_draw_intern((ArrowWidget *)widget, false, (widget->flag & WM_WIDGET_HIGHLIGHT) != 0);
}


wmWidget *WIDGET_arrow_new(int style, int (*handler)(struct bContext *C, const struct wmEvent *event, struct wmWidget *widget))
{
	float dir_default[3] = {0.0f, 0.0f, 1.0f};
	ArrowWidget *arrow;
	
	arrow = MEM_callocN(sizeof(ArrowWidget), "arrowwidget");
	
	arrow->widget.draw = widget_arrow_draw;
	arrow->widget.handler = handler;
	arrow->widget.intersect = NULL;
	arrow->widget.render_3d_intersection = widget_arrow_render_3d_intersect;
	arrow->widget.customdata = NULL;
	
	arrow->style = style;
	copy_v3_v3(arrow->direction, dir_default);
	
	return (wmWidget *)arrow;
}

void WIDGET_arrow_set_color(struct wmWidget *widget, float color[4])
{
	ArrowWidget *arrow = (ArrowWidget *)widget;
	
	copy_v4_v4(arrow->color, color);
}

void WIDGET_arrow_set_origin(struct wmWidget *widget, float origin[3])
{
	ArrowWidget *arrow = (ArrowWidget *)widget;
	
	copy_v3_v3(arrow->origin, origin);	
}

void WIDGET_arrow_set_direction(struct wmWidget *widget, float direction[3])
{
	ArrowWidget *arrow = (ArrowWidget *)widget;
	
	copy_v3_v3(arrow->direction, direction);
	normalize_v3(arrow->direction);
}

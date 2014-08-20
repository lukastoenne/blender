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
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/drawarmature.c
 *  \ingroup spview3d
 */

#include "MEM_guardedalloc.h"

#include "DNA_hair_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_hair.h"
#include "BKE_mesh_sample.h"
#include "BKE_modifier.h"

#include "view3d_intern.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_resources.h"

#include "HAIR_debug_types.h"

/* ******** Hair Drawing ******** */

/* TODO vertex/index buffers, etc. etc., avoid direct mode ... */

static void draw_hair_line(HairSystem *hsys)
{
	HairCurve *hair;
	int i;
	
	for (hair = hsys->curves, i = 0; i < hsys->totcurves; ++hair, ++i) {
		HairPoint *point;
		int k;
		
		glColor3f(0.4f, 0.7f, 1.0f);
		
		glBegin(GL_LINE_STRIP);
		for (point = hair->points, k = 0; k < hair->totpoints; ++point, ++k) {
			glVertex3fv(point->co);
		}
		glEnd();
		
		glPointSize(2.5f);
		glBegin(GL_POINTS);
		for (point = hair->points, k = 0; k < hair->totpoints; ++point, ++k) {
			if (k == 0)
				glColor3f(1.0f, 0.0f, 1.0f);
			else
				glColor3f(0.2f, 0.0f, 1.0f);
			glVertex3fv(point->co);
		}
		glEnd();
		glPointSize(1.0f);
	}
}

#define USE_BUFFERS

static void draw_hair_render(HairSystem *hsys)
{
	/* number of hairs drawn with one glDrawElements call */
	static const int hair_buffer_size = 1024; /* XXX arbitrary */
	
	static unsigned int vertex_glbuf = 0;
	static unsigned int elem_glbuf = 0;
	
	int maxverts, maxelems;
	float (*vertex_data)[3];
	unsigned int *elem_data;
	
	HairRenderIterator iter;
	unsigned int vertex_offset;
	unsigned int elem_offset;
	int num_buffered_hairs = 0;
	
	BKE_hair_render_iter_init(&iter, hsys);
	maxverts = iter.maxsteps * hair_buffer_size;
	maxelems = 2 * (iter.maxsteps - 1) * hair_buffer_size;
	if (maxelems < 1) {
		BKE_hair_render_iter_end(&iter);
		return;
	}
	
	glColor3f(0.4f, 0.7f, 1.0f);
	
#ifdef USE_BUFFERS
	/* set up OpenGL buffers */
	if (!vertex_glbuf) {
		glGenBuffers(1, &vertex_glbuf);
		glGenBuffers(1, &elem_glbuf);
	}
	glBindBuffer(GL_ARRAY_BUFFER, vertex_glbuf);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elem_glbuf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * maxverts, NULL, GL_DYNAMIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * maxelems, NULL, GL_DYNAMIC_DRAW);
	
	glEnableClientState(GL_VERTEX_ARRAY);
	
	glVertexPointer(3, GL_FLOAT, 3 * sizeof(float), NULL);
#else
	vertex_data = MEM_mallocN(sizeof(float) * 3 * maxverts, "hair vertex data");
	elem_data = MEM_mallocN(sizeof(unsigned int) * maxelems, "hair elem data");
#endif
	
//	glEnable(GL_LIGHTING);
	
	for (; BKE_hair_render_iter_valid_hair(&iter);  BKE_hair_render_iter_next_hair(&iter)) {
		BKE_hair_render_iter_init_hair(&iter);
		
		if (num_buffered_hairs == 0) {
			/* initialize buffers */
			vertex_data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
			elem_data = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
			vertex_offset = 0;
			elem_offset = 0;
		}
		
		for (; BKE_hair_render_iter_valid_step(&iter); BKE_hair_render_iter_next_step(&iter)) {
			float radius;
			float co[3];
			
			BKE_hair_render_iter_get(&iter, co, &radius);
			
			copy_v3_v3(vertex_data[vertex_offset], co);
			
			if (iter.step < iter.totsteps - 1) {
				elem_data[elem_offset] = vertex_offset;
				elem_data[elem_offset + 1] = vertex_offset + 1;
			}
			
			vertex_offset += 1;
			elem_offset += 2;
		}
		
		++num_buffered_hairs;
		
		if (num_buffered_hairs >= hair_buffer_size) {
			num_buffered_hairs = 0;
			
			/* finalize buffers and draw */
#ifdef USE_BUFFERS
			glUnmapBuffer(GL_ARRAY_BUFFER);
			glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
			
//			glShadeModel(GL_SMOOTH);
			glDrawElements(GL_LINES, elem_offset, GL_UNSIGNED_INT, NULL);
#else
			{
				int u;
				glBegin(GL_LINES);
				for (u = 0; u < totelems; ++u) {
					glVertex3fv(vertex_data[elem_data[u]]);
				}
				glEnd();
			}
#endif
		}
	}
	
	if (num_buffered_hairs > 0) {
		/* finalize buffers and draw */
#ifdef USE_BUFFERS
		glUnmapBuffer(GL_ARRAY_BUFFER);
		glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
		
//			glShadeModel(GL_SMOOTH);
		glDrawElements(GL_LINES, elem_offset, GL_UNSIGNED_INT, NULL);
#else
		{
			int u;
			glBegin(GL_LINES);
			for (u = 0; u < totelems; ++u) {
				glVertex3fv(vertex_data[elem_data[u]]);
			}
			glEnd();
		}
#endif
	}
	
#ifdef USE_BUFFERS
//	glDisable(GL_LIGHTING);
	
	glDisableClientState(GL_VERTEX_ARRAY);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
#else
	MEM_freeN(vertex_data);
	MEM_freeN(elem_data);
#endif
	
	BKE_hair_render_iter_end(&iter);
}

#undef USE_BUFFERS


static void count_hairs(HairSystem *hsys, int *totpoints, int *validhairs)
{
	HairCurve *hair;
	int i;
	
	*totpoints = *validhairs = 0;
	for (hair = hsys->curves, i = 0; i < hsys->totcurves; ++hair, ++i) {
		if (hair->totpoints > 1) {
			*totpoints += hair->totpoints;
			*validhairs += 1;
		}
	}
}

/* preview of hairs as cylinders */
/* XXX warning, computation here hurts a lot! */
static void draw_hair_hulls(HairSystem *hsys)
{
	HairCurve *hair;
	HairPoint *point, *next_point;
	int k, i, s;
	float upvec[] = {0.0f, 0.0f, 1.0f};
	float sidevec[] = {1.0f, 0.0f, 0.0f};

	float radius_factor = 1.0f;
	/* number of cylinder subdivisions */
	int subdiv = 8;
	
	int totpoints, validhairs;
	int tot_verts, tot_elems;

	/* vertex array variables */
	float (*vert_data)[3];
	unsigned int *elem_data;
	unsigned int offset = 0;
	unsigned int elem_offset = 0;

	static unsigned int hairbuf = 0;
	static unsigned int hairelem = 0;

	count_hairs(hsys, &totpoints, &validhairs);
	/* twice for all for the normals */
	tot_verts = totpoints * 2 * subdiv;
	tot_elems = (totpoints - validhairs) * 6 * subdiv;

	/* set up OpenGL code */
	if (!hairbuf) {
		glGenBuffers(1, &hairbuf);
		glGenBuffers(1, &hairelem);
	}
	glBindBuffer(GL_ARRAY_BUFFER, hairbuf);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, hairelem);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * tot_verts, NULL, GL_DYNAMIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * tot_elems, NULL, GL_DYNAMIC_DRAW);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	glVertexPointer(3, GL_FLOAT, 6 * sizeof(float), NULL);
	glNormalPointer(GL_FLOAT, 6 * sizeof(float), (GLubyte *)NULL + 3 * sizeof(float));

	vert_data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	elem_data = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);

	/* generate the data and copy to the display buffers */
	for (hair = hsys->curves, i = 0; i < hsys->totcurves; ++hair, ++i) {
		float normal[3];
		float dir[3];
		float tangent[3];
		unsigned int cur_offset = offset / 2;

		if (hair->totpoints == 1)
			continue;
		point = hair->points;
		next_point = hair->points + 1;

		sub_v3_v3v3(dir, next_point->co, point->co);
		normalize_v3_v3(normal, dir);

		/* calculate a tangent by cross product between z vector and normal */
		if (fabs(dot_v3v3(normal, upvec)) < 0.99f) {
			cross_v3_v3v3(tangent, normal, upvec);
		}
		else
			cross_v3_v3v3(tangent, normal, sidevec);

		normalize_v3(tangent);

		for (k = 0; k < hair->totpoints - 1; ++point, ++next_point, ++k) {
			float pivot_axis[3];
			float new_normal[3];
			float cosine;
			/* first step is to compute a tangent vector to the surface and rotate around the normal */
			sub_v3_v3v3(dir, next_point->co, point->co);
			normalize_v3_v3(new_normal, dir);
			cosine = dot_v3v3(new_normal, normal);

			cur_offset = offset / 2;

			/* if needed rotate the previous original tangent to the new frame by using cross product between current
			 * and previous segment */
			if (fabs(cosine) < 0.999f) {
				float rot_quat[4];
				float halfcosine;
				float halfsine;
				/* substitute by cosine of half angle because we are doing smooth-like interpolation */
				cosine = sqrt(0.5 + cosine * 0.5);

				/* half angle cosines needed for quaternion rotation */
				halfcosine = sqrt(0.5 + cosine * 0.5);
				halfsine = sqrt(0.5 - cosine * 0.5);

				cross_v3_v3v3(pivot_axis, normal, new_normal);
				normalize_v3(pivot_axis);

				rot_quat[0] = halfcosine;
				rot_quat[1] = halfsine * pivot_axis[0];
				rot_quat[2] = halfsine * pivot_axis[1];
				rot_quat[3] = halfsine * pivot_axis[2];

				mul_qt_v3(rot_quat, tangent);

				/* also rotate by the half amount the rotation axis */
				copy_v3_v3(pivot_axis, normal);
				mul_qt_v3(rot_quat, pivot_axis);

				normalize_v3(tangent);
			}
			else
				copy_v3_v3(pivot_axis, normal);

			copy_v3_v3(normal, new_normal);

			/* and repeat */
			copy_v3_v3(vert_data[offset], tangent);
			mul_v3_fl(vert_data[offset], point->radius * radius_factor);
			add_v3_v3(vert_data[offset++], point->co);
			copy_v3_v3(vert_data[offset++], tangent);

			/* create quaternion to rotate tangent around normal */
			for (s = 1; s < subdiv; s++) {
				float v_nor[3];
				float rot_quat[4];
				float half_angle = (M_PI * s) / subdiv;
				float sine = sin(half_angle);

				copy_v3_v3(v_nor, tangent);

				rot_quat[0] = cos(half_angle);
				rot_quat[1] = sine * pivot_axis[0];
				rot_quat[2] = sine * pivot_axis[1];
				rot_quat[3] = sine * pivot_axis[2];

				mul_qt_v3(rot_quat, v_nor);
				copy_v3_v3(vert_data[offset], v_nor);
				mul_v3_fl(vert_data[offset], point->radius * radius_factor);
				add_v3_v3(vert_data[offset++], point->co);
				copy_v3_v3(vert_data[offset++], v_nor);
			}

			for (s = 0; s < subdiv; s++) {
				elem_data[elem_offset++] = cur_offset + s;
				elem_data[elem_offset++] = cur_offset + s + subdiv;
				elem_data[elem_offset++] = cur_offset + (s + 1) % subdiv;

				elem_data[elem_offset++] = cur_offset + s + subdiv;
				elem_data[elem_offset++] = cur_offset + (s + 1) % subdiv;
				elem_data[elem_offset++] = cur_offset + (s + 1) % subdiv + subdiv;
			}
		}

		/* finally add the last point */
		for (s = 0; s < subdiv; s++) {
			add_v3_v3v3(vert_data[offset], vert_data[offset - 2 * subdiv], dir);
			offset++;
			copy_v3_v3(vert_data[offset], vert_data[offset - 2 * subdiv]);
			offset++;
		}

	}

	glUnmapBuffer(GL_ARRAY_BUFFER);
	glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

	glEnable(GL_LIGHTING);
	/* draw */
	glShadeModel(GL_SMOOTH);
	glDrawElements(GL_TRIANGLES, elem_offset, GL_UNSIGNED_INT, NULL);
	glDisable(GL_LIGHTING);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

/* called from drawobject.c, return true if nothing was drawn */
bool draw_hair_system(Scene *UNUSED(scene), View3D *UNUSED(v3d), ARegion *ar, Base *base, HairSystem *hsys)
{
	RegionView3D *rv3d = ar->regiondata;
	Object *ob = base->object;
	bool retval = true;
	
	glLoadMatrixf(rv3d->viewmat);
	glMultMatrixf(ob->obmat);
	
	switch (hsys->display.mode) {
		case HAIR_DISPLAY_LINE:
			draw_hair_line(hsys);
			break;
		case HAIR_DISPLAY_RENDER:
			draw_hair_render(hsys);
			break;
		case HAIR_DISPLAY_HULL:
			draw_hair_hulls(hsys);
			break;
	}
	
	return retval;
}

/* ---------------- debug drawing ---------------- */

#define SHOW_POINTS
#define SHOW_SIZE
#define SHOW_ROOTS
#define SHOW_FRAMES
//#define SHOW_SMOOTHING
#define SHOW_CONTACTS
#define SHOW_BENDING

static void draw_hair_debug_points(HairSystem *hsys, HAIR_SolverDebugPoint *dpoints, int dtotpoints)
{
#ifdef SHOW_POINTS
	int i, k, ktot = 0;
	
	glColor3f(0.8f, 1.0f, 1.0f);
	glBegin(GL_LINES);
	
	for (i = 0; i < hsys->totcurves; ++i) {
		HairCurve *hair = hsys->curves + i;
		for (k = 0; k < hair->totpoints; ++k, ++ktot) {
			HairPoint *point = hair->points + k;
			
			if (ktot < dtotpoints) {
				HAIR_SolverDebugPoint *dpoint = dpoints + ktot;
				float loc[3];
				
				glVertex3fv(point->co);
				add_v3_v3v3(loc, point->co, dpoint->bend);
				glVertex3fv(loc);
			}
		}
	}
	
	glEnd();
#else
	(void)hsys;
	(void)dpoints;
	(void)dtotpoints;
#endif
}

static void draw_hair_debug_size(HairSystem *hsys, float tmat[4][4])
{
#ifdef SHOW_SIZE
	int i, k;
	
	glColor3f(1.0f, 0.4f, 0.4f);
	
	for (i = 0; i < hsys->totcurves; ++i) {
		HairCurve *hair = hsys->curves + i;
		for (k = 0; k < hair->totpoints; ++k) {
			HairPoint *point = hair->points + k;
			
			drawcircball(GL_LINE_LOOP, point->co, point->radius, tmat);
		}
	}
#else
	(void)hsys;
#endif
}

static void draw_hair_debug_roots(HairSystem *hsys, struct DerivedMesh *dm)
{
#ifdef SHOW_ROOTS
	HairCurve *hair;
	int i;
	
	/* hair roots */
	if (dm) {
		glPointSize(3.0f);
		glColor3f(1.0f, 1.0f, 0.0f);
		glBegin(GL_LINES);
		for (hair = hsys->curves, i = 0; i < hsys->totcurves; ++hair, ++i) {
			float loc[3], nor[3];
			if (BKE_mesh_sample_eval(dm, &hair->root, loc, nor)) {
				glVertex3f(loc[0], loc[1], loc[2]);
				madd_v3_v3fl(loc, nor, 0.1f);
				glVertex3f(loc[0], loc[1], loc[2]);
			}
		}
		glEnd();
	}
#else
	(void)hsys;
	(void)dm;
#endif
}

static void draw_hair_debug_frames(HairSystem *hsys, HAIR_SolverDebugPoint *dpoints, int dtotpoints)
{
#ifdef SHOW_FRAMES
	const float scale = 0.2f;
	int i, k, ktot;
	
	glColor3f(0.8f, 1.0f, 1.0f);
	glBegin(GL_LINES);
	
	ktot = 0;
	for (i = 0; i < hsys->totcurves; ++i) {
		HairCurve *hair = hsys->curves + i;
		for (k = 0; k < hair->totpoints; ++k, ++ktot) {
//			HairPoint *point = hair->points + k;
			
			if (ktot < dtotpoints) {
				HAIR_SolverDebugPoint *dpoint = dpoints + ktot;
				float co[3], nor[3], tan[3], cotan[3];
				
				copy_v3_v3(co, dpoint->co);
				madd_v3_v3v3fl(nor, co, dpoint->frame[0], scale);
				madd_v3_v3v3fl(tan, co, dpoint->frame[1], scale);
				madd_v3_v3v3fl(cotan, co, dpoint->frame[2], scale);
				
				glColor3f(1.0f, 0.0f, 0.0f);
				glVertex3fv(co);
				glVertex3fv(nor);
				glColor3f(0.0f, 1.0f, 0.0f);
				glVertex3fv(co);
				glVertex3fv(tan);
				glColor3f(0.0f, 0.0f, 1.0f);
				glVertex3fv(co);
				glVertex3fv(cotan);
			}
		}
	}
	
	glEnd();
#else
	(void)hsys;
	(void)dpoints;
	(void)dtotpoints;
#endif
}

static void draw_hair_debug_bending(HairSystem *hsys, HAIR_SolverDebugPoint *dpoints, int dtotpoints)
{
#ifdef SHOW_BENDING
	int i, k, ktot;
	
	glBegin(GL_LINES);
	
	ktot = 0;
	for (i = 0; i < hsys->totcurves; ++i) {
		HairCurve *hair = hsys->curves + i;
		for (k = 0; k < hair->totpoints; ++k, ++ktot) {
//			HairPoint *point = hair->points + k;
			
			if (ktot < dtotpoints) {
				HAIR_SolverDebugPoint *dpoint = dpoints + ktot;
				float co[3], bend[3];
				
				copy_v3_v3(co, dpoint->co);
				
				add_v3_v3v3(bend, co, dpoint->bend);
				glColor3f(0.4f, 0.25f, 0.55f);
				glVertex3fv(co);
				glVertex3fv(bend);
				
				add_v3_v3v3(bend, co, dpoint->rest_bend);
				glColor3f(0.15f, 0.55f, 0.55f);
				glVertex3fv(co);
				glVertex3fv(bend);
			}
		}
	}
	
	glEnd();
#else
	(void)hsys;
	(void)dpoints;
	(void)dtotpoints;
#endif
}

#if 0
static void draw_hair_curve_debug_frames(HairSystem *hsys, HairCurve *hair)
{
#ifdef SHOW_FRAMES
	struct HAIR_FrameIterator *iter;
	float co[3], nor[3], tan[3], cotan[3];
	int k = 0;
	const float scale = 0.1f;
	
	glBegin(GL_LINES);
	iter = HAIR_frame_iter_new(hair, 1.0f / hair->totpoints, hsys->smooth);
	
	while (HAIR_frame_iter_valid(iter)) {
		HAIR_frame_iter_get(iter, nor, tan, cotan);
		copy_v3_v3(co, hair->points[k].co);
		mul_v3_fl(nor, scale);
		mul_v3_fl(tan, scale);
		mul_v3_fl(cotan, scale);
		add_v3_v3(nor, co);
		add_v3_v3(tan, co);
		add_v3_v3(cotan, co);
		++k;
		
		glColor3f(1.0f, 0.0f, 0.0f);
		glVertex3fv(co);
		glVertex3fv(nor);
		glColor3f(0.0f, 1.0f, 0.0f);
		glVertex3fv(co);
		glVertex3fv(tan);
		glColor3f(0.0f, 0.0f, 1.0f);
		glVertex3fv(co);
		glVertex3fv(cotan);
		
		HAIR_frame_iter_next(iter);
	}
	HAIR_frame_iter_free(iter);
	glEnd();
#else
	(void)hsys;
	(void)hair;
#endif
}
#endif

static void draw_hair_curve_debug_smoothing(HairSystem *hsys, HairCurve *hair)
{
#ifdef SHOW_SMOOTHING
	struct HAIR_SmoothingIteratorFloat3 *iter;
	float smooth_co[3];
	
	if (hair->totpoints < 2)
		return;
	
	glColor3f(0.5f, 1.0f, 0.1f);
	
	glBegin(GL_LINE_STRIP);
	iter = HAIR_smoothing_iter_new(hair, 1.0f / hair->totpoints, hsys->smooth);
	while (HAIR_smoothing_iter_valid(iter)) {
		HAIR_smoothing_iter_get(iter, smooth_co);
		glVertex3fv(smooth_co);
		
		HAIR_smoothing_iter_next(iter);
	}
	HAIR_smoothing_iter_free(iter);
	glEnd();
	
	glPointSize(2.5f);
	glBegin(GL_POINTS);
	iter = HAIR_smoothing_iter_new(hair, 1.0f / hair->totpoints, hsys->smooth);
	while (HAIR_smoothing_iter_valid(iter)) {
		HAIR_smoothing_iter_get(iter, smooth_co);
		glVertex3fv(smooth_co);
		
		HAIR_smoothing_iter_next(iter);
	}
	HAIR_smoothing_iter_free(iter);
	glEnd();
	glPointSize(1.0f);
#else
	(void)hsys;
	(void)hair;
#endif
}

static void draw_hair_debug_contacts(HairSystem *UNUSED(hsys), HAIR_SolverDebugContact *contacts, int totcontacts)
{
#ifdef SHOW_CONTACTS
	int i;
	
	glBegin(GL_LINES);
	glColor3f(0.7f, 0.7f, 0.9f);
	for (i = 0; i < totcontacts; ++i) {
		HAIR_SolverDebugContact *c = contacts + i;
		
		glVertex3f(c->coA[0], c->coA[1], c->coA[2]);
		glVertex3f(c->coB[0], c->coB[1], c->coB[2]);
	}
	glEnd();
	
	glPointSize(3.0f);
	glBegin(GL_POINTS);
	for (i = 0; i < totcontacts; ++i) {
		HAIR_SolverDebugContact *c = contacts + i;
		
		glColor3f(1.0f, 0.1f, 0.0f);
		glVertex3f(c->coA[0], c->coA[1], c->coA[2]);
		glColor3f(0.0f, 1.0f, 0.7f);
		glVertex3f(c->coB[0], c->coB[1], c->coB[2]);
	}
	glEnd();
	glPointSize(1.0f);
#else
	(void)contacts;
	(void)totcontacts;
#endif
}

void draw_hair_debug_info(Scene *UNUSED(scene), View3D *UNUSED(v3d), ARegion *ar, Base *base, HairModifierData *hmd)
{
	RegionView3D *rv3d = ar->regiondata;
	Object *ob = base->object;
	HairSystem *hsys = hmd->hairsys;
	int debug_flag = hmd->debug_flag;
	HairCurve *hair;
	int i;
	float imat[4][4];
	
	invert_m4_m4(imat, rv3d->viewmatob);
	
	glLoadMatrixf(rv3d->viewmat);
	glMultMatrixf(ob->obmat);
	
	if (debug_flag & MOD_HAIR_DEBUG_SIZE)
		draw_hair_debug_size(hsys, imat);
	if (debug_flag & MOD_HAIR_DEBUG_ROOTS)
		draw_hair_debug_roots(hsys, ob->derivedFinal);
	
	if (debug_flag & MOD_HAIR_DEBUG_SMOOTHING) {
		for (hair = hsys->curves, i = 0; i < hsys->totcurves; ++hair, ++i)
			draw_hair_curve_debug_smoothing(hsys, hair);
	}
	
	if (hmd->debug_data) {
		if (debug_flag & MOD_HAIR_DEBUG_FRAMES)
			draw_hair_debug_frames(hsys, hmd->debug_data->points, hmd->debug_data->totpoints);
		if (debug_flag & MOD_HAIR_DEBUG_BENDING)
			draw_hair_debug_bending(hsys, hmd->debug_data->points, hmd->debug_data->totpoints);
//		draw_hair_debug_points(hsys, hmd->debug_data->points, hmd->debug_data->totpoints);
		if (debug_flag & MOD_HAIR_DEBUG_CONTACTS)
			draw_hair_debug_contacts(hsys, hmd->debug_data->contacts, hmd->debug_data->totcontacts);
	}
}

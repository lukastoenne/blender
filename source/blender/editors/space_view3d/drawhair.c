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

#include "HAIR_capi.h"

/* ******** Hair Drawing ******** */

/* TODO vertex/index buffers, etc. etc., avoid direct mode ... */

static void draw_hair_curve(HairSystem *UNUSED(hsys), HairCurve *hair)
{
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

/* called from drawobject.c, return true if nothing was drawn */
bool draw_hair_system(Scene *UNUSED(scene), View3D *UNUSED(v3d), ARegion *ar, Base *base, HairSystem *hsys)
{
	RegionView3D *rv3d = ar->regiondata;
	Object *ob = base->object;
	HairCurve *hair;
	int i;
	bool retval = true;
	
	glLoadMatrixf(rv3d->viewmat);
	glMultMatrixf(ob->obmat);
	
	for (hair = hsys->curves, i = 0; i < hsys->totcurves; ++hair, ++i) {
		draw_hair_curve(hsys, hair);
	}
	
	return retval;
}

/* ---------------- debug drawing ---------------- */

//#define SHOW_ROOTS
#define SHOW_FRAMES
//#define SHOW_SMOOTHING
#define SHOW_CYLINDERS
#define SHOW_CONTACTS

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

static void draw_hair_curve_debug_frames(HairSystem *hsys, HairCurve *hair)
{
#ifdef SHOW_FRAMES
	struct HAIR_FrameIterator *iter;
	float co[3], nor[3], tan[3], cotan[3];
	int k = 0;
	const float scale = 0.1f;
	
	glBegin(GL_LINES);
	iter = HAIR_frame_iter_new(hair, 1.0f / hair->totpoints, hsys->smooth);
//	copy_v3_v3(co, hair->points[0].co);
//	mul_v3_fl(nor, scale);
//	mul_v3_fl(tan, scale);
//	mul_v3_fl(cotan, scale);
//	add_v3_v3(nor, co);
//	add_v3_v3(tan, co);
//	add_v3_v3(cotan, co);
//	++k;
	
//	glColor3f(1.0f, 0.0f, 0.0f);
//	glVertex3fv(co);
//	glVertex3fv(nor);
//	glColor3f(0.0f, 1.0f, 0.0f);
//	glVertex3fv(co);
//	glVertex3fv(tan);
//	glColor3f(0.0f, 0.0f, 1.0f);
//	glVertex3fv(co);
//	glVertex3fv(cotan);
	
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

static void draw_hair_debug_contacts(HAIR_SolverContact *contacts, int totcontacts)
{
#ifdef SHOW_CONTACTS
	int i;
	
	glBegin(GL_LINES);
	glColor3f(0.7f, 0.7f, 0.9f);
	for (i = 0; i < totcontacts; ++i) {
		HAIR_SolverContact *c = contacts + i;
		
		glVertex3f(c->coA[0], c->coA[1], c->coA[2]);
		glVertex3f(c->coB[0], c->coB[1], c->coB[2]);
	}
	glEnd();
	
	glPointSize(3.0f);
	glBegin(GL_POINTS);
	for (i = 0; i < totcontacts; ++i) {
		HAIR_SolverContact *c = contacts + i;
		
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

/* debug preview of hairs as cylinders. warning, computation here hurts a lot! */
static void draw_hair_debug_cylinders(HairSystem *hsys, int totpoints)
{
#ifdef SHOW_CYLINDERS
	HairCurve *hair;
	HairPoint *point, *next_point;
	int k, i, s;
	float upvec[] = {0.0f, 0.0f, 1.0f};
	float sidevec[] = {1.0f, 0.0f, 0.0f};

	float diameter = 0.05f;
	/* number of cylinder subdivisions */
	int subdiv = 8;

	/* vertex array variables */
	float (*vert_data)[3];
	unsigned int *elem_data;
	unsigned int offset = 0;
	unsigned int elem_offset = 0;

	/* twice the subdivisions for top and bottom plus twive all for the normals */
	int tot_verts = totpoints * 4 * subdiv;
	int tot_elems = totpoints * 6 * subdiv;

	static unsigned int hairbuf = 0;
	static unsigned int hairelem = 0;

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
				cosine = sqrt3f(0.5 + cosine * 0.5);

				/* half angle cosines needed for quaternion rotation */
				halfcosine = sqrt3f(0.5 + cosine * 0.5);
				halfsine = sqrt3f(0.5 - cosine * 0.5);

				cross_v3_v3v3(pivot_axis, new_normal, normal);
				normalize_v3(pivot_axis);

				rot_quat[0] = halfcosine;
				rot_quat[1] = halfsine * pivot_axis[0];
				rot_quat[2] = halfsine * pivot_axis[1];
				rot_quat[3] = halfsine * pivot_axis[2];

				mul_qt_v3(rot_quat, tangent);
			}

			copy_v3_v3(normal, new_normal);

			/* and repeat */
			copy_v3_v3(vert_data[offset], tangent);
			mul_v3_fl(vert_data[offset], diameter);
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
				rot_quat[1] = sine * normal[0];
				rot_quat[2] = sine * normal[1];
				rot_quat[3] = sine * normal[2];

				mul_qt_v3(rot_quat, v_nor);
				copy_v3_v3(vert_data[offset], v_nor);
				mul_v3_fl(vert_data[offset], diameter);
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
#endif
}

void draw_hair_debug_info(Scene *UNUSED(scene), View3D *UNUSED(v3d), ARegion *ar, Base *base, HairModifierData *hmd)
{
	RegionView3D *rv3d = ar->regiondata;
	Object *ob = base->object;
	HairSystem *hsys = hmd->hairsys;
	HairCurve *hair;
	int i;
	int tot_points = 0;
	
	glLoadMatrixf(rv3d->viewmat);
	glMultMatrixf(ob->obmat);
	
	draw_hair_debug_roots(hsys, ob->derivedFinal);
	
	for (hair = hsys->curves, i = 0; i < hsys->totcurves; ++hair, ++i) {
		draw_hair_curve_debug_frames(hsys, hair);
		draw_hair_curve_debug_smoothing(hsys, hair);
		tot_points += (hair->totpoints > 1) ? hair->totpoints - 1 : 0;
	}
	
	draw_hair_debug_cylinders(hsys, tot_points);
	draw_hair_debug_contacts(hmd->debug_contacts, hmd->debug_totcontacts);
}

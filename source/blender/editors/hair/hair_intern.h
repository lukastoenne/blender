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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/hair/hair_intern.h
 *  \ingroup edhair
 */

#ifndef __HAIR_INTERN_H__
#define __HAIR_INTERN_H__

#include "BIF_glutil.h"

#include "ED_view3d.h"

struct ARegion;
struct bContext;
struct wmOperatorType;
struct rcti;

struct Object;

/* hair_edit.c */
bool hair_use_mirror_x(struct Object *ob);
bool hair_use_mirror_topology(struct Object *ob);

int hair_edit_toggle_poll(struct bContext *C);
int hair_edit_poll(struct bContext *C);

void HAIR_OT_hair_edit_toggle(struct wmOperatorType *ot);

/* hair_select.c */
void HAIR_OT_select_all(struct wmOperatorType *ot);
void HAIR_OT_select_linked(struct wmOperatorType *ot);

/* hair_stroke.c */
void HAIR_OT_stroke(struct wmOperatorType *ot);


/* ==== Hair Brush ==== */

typedef struct HairViewData {
	ViewContext vc;
	bglMats mats;
} HairViewData;

void hair_init_viewdata(struct bContext *C, struct HairViewData *viewdata);

bool hair_test_depth(struct HairViewData *viewdata, const float co[3], const int screen_co[2]);
bool hair_test_vertex_inside_circle(struct HairViewData *viewdata, const float mval[2], float radsq,
                             struct BMVert *v, float *r_dist);
bool hair_test_edge_inside_circle(struct HairViewData *viewdata, const float mval[2], float radsq,
                                  struct BMVert *v1, struct BMVert *v2, float *r_dist, float *r_lambda);
bool hair_test_vertex_inside_rect(struct HairViewData *viewdata, struct rcti *rect, struct BMVert *v);
bool hair_test_vertex_inside_lasso(struct HairViewData *viewdata, const int mcoords[][2], short moves, struct BMVert *v);

typedef struct HairToolData {
	/* context */
	struct Scene *scene;
	struct Object *ob;
	struct BMEditStrands *edit;
	struct HairEditSettings *settings;
	HairViewData viewdata;
	
	/* view space */
	float mval[2];      /* mouse coordinates */
	float mdepth;       /* mouse z depth */
	
	/* object space */
	float imat[4][4];   /* obmat inverse */
	float loc[3];       /* start location */
	float delta[3];     /* stroke step */
} HairToolData;

bool hair_brush_step(struct HairToolData *data);

/* ==== Cursor ==== */

void hair_edit_cursor_start(struct bContext *C, int (*poll)(struct bContext *C));

/* ==== BMesh utilities ==== */

struct BMEditStrands;

void hair_bm_min_max(struct BMEditStrands *edit, float min[3], float max[3]);

#endif

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
 * The Original Code is Copyright (C) 2007 by Janne Karhu.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_physics.h
 *  \ingroup editors
 */

#ifndef __ED_PHYSICS_H__
#define __ED_PHYSICS_H__

struct bContext;
struct ReportList;
struct wmKeyConfig;
struct ViewContext;
struct rcti;

struct Scene;
struct Object;
struct BMEditStrands;

/* particle_edit.c */
int PE_poll(struct bContext *C);
int PE_hair_poll(struct bContext *C);
int PE_poll_view3d(struct bContext *C);

/* rigidbody_object.c */
bool ED_rigidbody_object_add(struct Scene *scene, struct Object *ob, int type, struct ReportList *reports);
void ED_rigidbody_object_remove(struct Scene *scene, struct Object *ob);

/* rigidbody_constraint.c */
bool ED_rigidbody_constraint_add(struct Scene *scene, struct Object *ob, int type, struct ReportList *reports);
void ED_rigidbody_constraint_remove(struct Scene *scene, struct Object *ob);

/* operators */
void ED_operatortypes_physics(void);
void ED_keymap_physics(struct wmKeyConfig *keyconf);

/* hair edit */
void undo_push_strands(struct bContext *C, const char *name);

void           ED_strands_mirror_cache_begin_ex(struct BMEditStrands *edit, const int axis,
                                                const bool use_self, const bool use_select,
                                                const bool use_topology, float maxdist, int *r_index);
void           ED_strands_mirror_cache_begin(struct BMEditStrands *edit, const int axis,
                                             const bool use_self, const bool use_select,
                                             const bool use_topology);
void           ED_strands_mirror_apply(struct BMEditStrands *edit, const int sel_from, const int sel_to);
struct BMVert *ED_strands_mirror_get(struct BMEditStrands *edit, struct BMVert *v);
struct BMEdge *ED_strands_mirror_get_edge(struct BMEditStrands *edit, struct BMEdge *e);
void           ED_strands_mirror_cache_clear(struct BMEditStrands *edit, struct BMVert *v);
void           ED_strands_mirror_cache_end(struct BMEditStrands *edit);

int ED_hair_mouse_select(struct bContext *C, const int mval[2], bool extend, bool deselect, bool toggle);
int ED_hair_border_select(struct bContext *C, struct rcti *rect, bool select, bool extend);
int ED_hair_circle_select(struct bContext *C, bool select, const int mval[2], float radius);
int ED_hair_lasso_select(struct bContext *C, const int mcoords[][2], short moves, bool extend, bool select);

void ED_operatortypes_hair(void);
void ED_keymap_hair(struct wmKeyConfig *keyconf);

#endif /* __ED_PHYSICS_H__ */


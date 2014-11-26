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

#ifndef __EDITHAIR_INLINE_H__
#define __EDITHAIR_INLINE_H__

/** \file blender/editors/hair/edithair_inline.h
 *  \ingroup bke
 */

#include "BLI_mempool.h"
#include "BLI_utildefines.h"

#include "BKE_edithair.h"

/* iterator type structs */
struct HairEditIter__elem_of_mesh {
	BLI_mempool_iter pooliter;
};
struct HairEditIter__vert_of_curve {
	HairEditCurve *cdata;
	HairEditVertex *v_first, *v_next;
};

typedef void  (*HairEditIter__begin_cb) (void *);
typedef void *(*HairEditIter__step_cb) (void *);

typedef struct HairEditIter {
	/* keep union first */
	union {
		struct HairEditIter__elem_of_mesh elem_of_mesh;
		struct HairEditIter__vert_of_curve vert_of_curve;
	} data;

	HairEditIter__begin_cb begin;
	HairEditIter__step_cb step;

	int count;  /* note, only some iterators set this, don't rely on it */
	char itype;
} HairEditIter;

#define HAIREDIT_ITER_CB_DEF(name) \
	struct HairEditIter__##name; \
	void  hairedit_iter__##name##_begin(struct HairEditIter__##name *iter); \
	void *hairedit_iter__##name##_step(struct HairEditIter__##name *iter)

HAIREDIT_ITER_CB_DEF(elem_of_mesh);
HAIREDIT_ITER_CB_DEF(vert_of_curve);

#undef HAIREDIT_ITER_CB_DEF

/* inline here optimizes out the switch statement when called with
 * constant values (which is very common), nicer for loop-in-loop situations */

/**
 * \brief Iterator Step
 *
 * Calls an iterators step function to return the next element.
 */
BLI_INLINE void *HairEdit_iter_step(HairEditIter *iter)
{
	return iter->step(iter);
}

/**
 * \brief Iterator Init
 *
 * Takes a hair iterator structure and fills
 * it with the appropriate function pointers based
 * upon its type.
 */
BLI_INLINE bool HairEdit_iter_init(HairEditIter *iter, HairEditData *hedit, const char itype, void *data)
{
	/* int argtype; */
	iter->itype = itype;
	
	/* inlining optimizes out this switch when called with the defined type */
	switch ((HairEditIterType)itype) {
		case HAIREDIT_CURVES_OF_MESH:
			BLI_assert(hedit != NULL);
			BLI_assert(data == NULL);
			iter->begin = (HairEditIter__begin_cb)hairedit_iter__elem_of_mesh_begin;
			iter->step  = (HairEditIter__step_cb)hairedit_iter__elem_of_mesh_step;
			iter->data.elem_of_mesh.pooliter.pool = hedit->cpool;
			break;
		case HAIREDIT_VERTS_OF_MESH:
			BLI_assert(hedit != NULL);
			BLI_assert(data == NULL);
			iter->begin = (HairEditIter__begin_cb)hairedit_iter__elem_of_mesh_begin;
			iter->step  = (HairEditIter__step_cb)hairedit_iter__elem_of_mesh_step;
			iter->data.elem_of_mesh.pooliter.pool = hedit->vpool;
			break;
		case HAIREDIT_VERTS_OF_CURVE:
			BLI_assert(data != NULL);
			iter->begin = (HairEditIter__begin_cb)hairedit_iter__vert_of_curve_begin;
			iter->step  = (HairEditIter__step_cb)hairedit_iter__vert_of_curve_step;
			iter->data.vert_of_curve.cdata = (HairEditCurve *)data;
			break;
		default:
			/* should never happen */
			BLI_assert(0);
			return false;
			break;
	}
	
	iter->begin(iter);
	
	return true;
}

/**
 * \brief Iterator New
 *
 * Takes a bmesh iterator structure and fills
 * it with the appropriate function pointers based
 * upon its type and then calls BMeshIter_step()
 * to return the first element of the iterator.
 *
 */
BLI_INLINE void *HairEdit_iter_new(HairEditIter *iter, HairEditData *hedit, const char itype, void *data)
{
	if (LIKELY(HairEdit_iter_init(iter, hedit, itype, data))) {
		return HairEdit_iter_step(iter);
	}
	else {
		return NULL;
	}
}

#endif

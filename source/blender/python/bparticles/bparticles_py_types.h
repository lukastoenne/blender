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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/bparticles/bparticles_py_types.h
 *  \ingroup pybparticles
 */

#ifndef __BPARTICLES_PY_TYPES_H__
#define __BPARTICLES_PY_TYPES_H__

extern PyTypeObject BPy_NParticleState_Type;
extern PyTypeObject BPy_NParticleAttributeState_Type;
extern PyTypeObject BPy_NParticleAttributeStateSeq_Type;
extern PyTypeObject BPy_NParticleAttributeStateIter_Type;
extern PyTypeObject BPy_NParticleParticle_Type;
extern PyTypeObject BPy_NParticleParticleSeq_Type;
extern PyTypeObject BPy_NParticleParticleIter_Type;

#define BPy_NParticleState_Check(v)                 (Py_TYPE(v) == &BPy_NParticleState_Type)
#define BPy_NParticleAttributeState_Check(v)        (Py_TYPE(v) == &BPy_NParticleAttributeState_Type)
#define BPy_NParticleAttributeStateSeq_Check(v)     (Py_TYPE(v) == &BPy_NParticleAttributeStateSeq_Type)
#define BPy_NParticleAttributeStateIter_Check(v)    (Py_TYPE(v) == &BPy_NParticleAttributeStateIter_Type)
#define BPy_NParticleParticle_Check(v)              (Py_TYPE(v) == &BPy_NParticleParticle_Type)
#define BPy_NParticleParticleSeq_Check(v)           (Py_TYPE(v) == &BPy_NParticleParticleSeq_Type)
#define BPy_NParticleParticleIter_Check(v)          (Py_TYPE(v) == &BPy_NParticleParticleIter_Type)

typedef struct BPy_NParticleState {
	PyObject_VAR_HEAD
	struct NParticleState *state; /* keep first */
} BPy_NParticleState;

typedef struct BPy_NParticleAttributeState {
	PyObject_VAR_HEAD
	struct NParticleState *state; /* keep first */
	struct NParticleAttributeState *attrstate;
} BPy_NParticleAttributeState;

typedef struct BPy_NParticleAttributeStateSeq {
	PyObject_VAR_HEAD
	struct NParticleState *state; /* keep first */
} BPy_NParticleAttributeStateSeq;

typedef struct BPy_NParticleAttributeStateIter {
	PyObject_VAR_HEAD
	struct NParticleState *state; /* keep first */
	NParticleAttributeStateIterator iter;
} BPy_NParticleAttributeStateIter;

typedef struct BPy_NParticleParticle {
	PyObject_VAR_HEAD
	struct NParticleState *state; /* keep first */
	NParticleIterator iter;
} BPy_NParticleParticle;

typedef struct BPy_NParticleParticleSeq {
	PyObject_VAR_HEAD
	struct NParticleState *state; /* keep first */
} BPy_NParticleParticleSeq;

typedef struct BPy_NParticleParticleIter {
	PyObject_VAR_HEAD
	struct NParticleState *state; /* keep first */
	NParticleIterator iter;
} BPy_NParticleParticleIter;

void BPy_BPAR_init_types(void);

PyObject *BPyInit_bparticles_types(void);

PyObject *BPy_NParticleState_CreatePyObject(NParticleState *state);
PyObject *BPy_NParticleAttributeState_CreatePyObject(NParticleState *state, NParticleAttributeState *attr);
PyObject *BPy_NParticleAttributeStateSeq_CreatePyObject(NParticleState *state);
PyObject *BPy_NParticleAttributeStateIter_CreatePyObject(NParticleState *state);
PyObject *BPy_NParticleParticle_CreatePyObject(NParticleState *state, NParticleIterator iter);
PyObject *BPy_NParticleParticleSeq_CreatePyObject(NParticleState *state);
PyObject *BPy_NParticleParticleIter_CreatePyObject(NParticleState *state);

#endif /* __BPARTICLES_PY_TYPES_H__ */

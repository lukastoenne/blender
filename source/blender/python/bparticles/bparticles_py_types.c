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

/** \file blender/python/bparticles/bparticles_py_types.c
 *  \ingroup pybparticles
 */

#include <Python.h>

#include "BLI_math.h"

#include "DNA_nparticle_types.h"

#include "BKE_nparticle.h"

#include "../generic/py_capi_utils.h"

#include "bparticles_py_types.h" /* own include */

/* Type Docstrings
 * =============== */

PyDoc_STRVAR(bpy_bpar_state_doc,
"Particle state data\n"
);

PyDoc_STRVAR(bpy_bpar_attrstate_doc,
"Particle attribute state data\n"
);

PyDoc_STRVAR(bpy_bpar_attrstateseq_doc,
"Particle attribute state sequence\n"
);

PyDoc_STRVAR(bpy_bpar_attrstateiter_doc,
"Iterator for looping over particle attribute states\n"
);


PyDoc_STRVAR(bpy_bpar_state_attributes_doc,
"State attributes (read-only).\n\n:type: :class:`NParticleAttributeStateSeq`"
);
static PyObject *bpy_bpar_state_attributes_get(BPy_NParticleState *self, void *UNUSED(closure))
{
	return BPy_NParticleAttributeStateSeq_CreatePyObject(self->state);
}

PyDoc_STRVAR(bpy_bpar_attrstate_name_doc,
"Attribute name\n"
);
static PyObject *bpy_bpar_attrstate_name_get(BPy_NParticleAttributeState *self)
{
	return PyUnicode_FromString(self->attrstate->desc.name);
}

static PyObject *bpy_bpar_state_repr(BPy_NParticleState *self)
{
	NParticleState *state = self->state;

	if (state) {
		return PyUnicode_FromFormat("<NParticleState(%p)>",
		                            state);
	}
	else {
		return PyUnicode_FromFormat("<NParticleState dead at %p>", self);
	}
}

static PyObject *bpy_bpar_attrstate_repr(BPy_NParticleAttributeState *self)
{
	NParticleAttributeState *attrstate = self->attrstate;

	if (attrstate) {
		return PyUnicode_FromFormat("<NParticleAttributeState(%p) name=%s, datatype=%s>",
		                            attrstate, attrstate->desc.name,
		                            BKE_nparticle_datatype_name(attrstate->desc.datatype));
	}
	else {
		return PyUnicode_FromFormat("<NParticleAttributeState dead at %p>", self);
	}
}

static PyGetSetDef bpy_bpar_state_getseters[] = {
	{(char *)"attributes", (getter)bpy_bpar_state_attributes_get, (setter)NULL, (char *)bpy_bpar_state_attributes_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static PyGetSetDef bpy_bpar_attrstate_getseters[] = {
	{(char *)"name", (getter)bpy_bpar_attrstate_name_get, (setter)NULL, (char *)bpy_bpar_attrstate_name_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static struct PyMethodDef bpy_bpar_state_methods[] = {
	{NULL, NULL, 0, NULL}
};

static struct PyMethodDef bpy_bpar_attrstate_methods[] = {
	{NULL, NULL, 0, NULL}
};

static Py_hash_t bpy_bpar_state_hash(PyObject *self)
{
	return _Py_HashPointer(((BPy_NParticleState *)self)->state);
}

static Py_hash_t bpy_bpar_attrstate_hash(PyObject *self)
{
	return _Py_HashPointer(((BPy_NParticleAttributeState *)self)->attrstate);
}


static Py_ssize_t bpy_bpar_attrstateseq_length(BPy_NParticleAttributeStateSeq *self)
{
	return BKE_nparticle_state_num_attributes(self->state);
}

static PyObject *bpy_bpar_attrstateseq_subscript_int(BPy_NParticleAttributeStateSeq *self, int keynum)
{
	if (keynum < 0)
		keynum += BKE_nparticle_state_num_attributes(self->state);
	
	if (keynum >= 0) {
		NParticleAttributeState *attrstate = BKE_nparticle_state_get_attribute_by_index(self->state, keynum);
		if (attrstate)
			return BPy_NParticleAttributeState_CreatePyObject(self->state, attrstate);
	}

	PyErr_Format(PyExc_IndexError,
	             "NParticleAttributeStateSeq[index]: index %d out of range", keynum);
	return NULL;
}

static PyObject *bpy_bpar_attrstateseq_subscript_str(BPy_NParticleAttributeStateSeq *self, const char *keyname)
{
	NParticleAttributeState *attrstate = BKE_nparticle_state_find_attribute(self->state, keyname);
	
	if (attrstate)
		return BPy_NParticleAttributeState_CreatePyObject(self->state, attrstate);

	PyErr_Format(PyExc_KeyError, "NParticleAttributeStateSeq[key]: key \"%.200s\" not found", keyname);
	return NULL;
}

static PyObject *bpy_bpar_attrstateseq_subscript_slice(BPy_NParticleAttributeStateSeq *self, Py_ssize_t start, Py_ssize_t stop)
{
	NParticleAttributeStateIterator iter;
	int count;

	PyObject *list;
	PyObject *item;

	list = PyList_New(0);

	/* skip to start */
	for (count = 0, BKE_nparticle_state_attributes_begin(self->state, &iter);
	     count < start && BKE_nparticle_state_attribute_iter_valid(&iter);
	     ++count, BKE_nparticle_state_attribute_iter_next(&iter))
	{}
	
	/* add items until stop */
	for (; count < stop && BKE_nparticle_state_attribute_iter_valid(&iter);
	     ++count, BKE_nparticle_state_attribute_iter_next(&iter))
	{
		item = BPy_NParticleAttributeState_CreatePyObject(self->state, iter.attrstate);
		PyList_Append(list, item);
		Py_DECREF(item);
	}
	BKE_nparticle_state_attribute_iter_end(&iter);
	
	return list;
}

static int bpy_bpar_attrstateseq_contains(BPy_NParticleAttributeStateSeq *self, PyObject *value)
{
	if (BPy_NParticleAttributeState_Check(value)) {
		BPy_NParticleAttributeState *attrstate_py = (BPy_NParticleAttributeState *)value;
		NParticleAttributeState *attrstate = attrstate_py->attrstate;
		if (BKE_nparticle_state_find_attribute(self->state, attrstate->desc.name) == attrstate)
			return 1;
	}
	return 0;
}

static PyObject *bpy_bpar_attrstateseq_subscript(BPy_NParticleAttributeStateSeq *self, PyObject *key)
{
	if (PyUnicode_Check(key)) {
		return bpy_bpar_attrstateseq_subscript_str(self, _PyUnicode_AsString(key));
	}
	else if (PyIndex_Check(key)) {
		Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return NULL;
		return bpy_bpar_attrstateseq_subscript_int(self, i);
	}
	else if (PySlice_Check(key)) {
		PySliceObject *key_slice = (PySliceObject *)key;
		Py_ssize_t step = 1;

		if (key_slice->step != Py_None && !_PyEval_SliceIndex(key, &step)) {
			return NULL;
		}
		else if (step != 1) {
			PyErr_SetString(PyExc_TypeError, "NParticleAttributeStateSeq[slice]: slice steps not supported");
			return NULL;
		}
		else if (key_slice->start == Py_None && key_slice->stop == Py_None) {
			return bpy_bpar_attrstateseq_subscript_slice(self, 0, PY_SSIZE_T_MAX);
		}
		else {
			Py_ssize_t start = 0, stop = PY_SSIZE_T_MAX;

			/* avoid PySlice_GetIndicesEx because it needs to know the length ahead of time. */
			if (key_slice->start != Py_None && !_PyEval_SliceIndex(key_slice->start, &start)) return NULL;
			if (key_slice->stop  != Py_None && !_PyEval_SliceIndex(key_slice->stop,  &stop))  return NULL;

			if (start < 0 || stop < 0) {
				/* only get the length for negative values */
				Py_ssize_t len = (Py_ssize_t)BKE_nparticle_state_num_attributes(self->state);
				if (start < 0) start += len;
				if (stop  < 0) stop  += len;
			}

			if (stop - start <= 0) {
				return PyList_New(0);
			}
			else {
				return bpy_bpar_attrstateseq_subscript_slice(self, start, stop);
			}
		}
	}
	else {
		PyErr_Format(PyExc_TypeError,
		             "NParticleAttributeStateSeq[key]: invalid key, "
		             "must be a string or an int, not %.200s",
		             Py_TYPE(key)->tp_name);
		return NULL;
	}
}

static PySequenceMethods bpy_bpar_attrstateseq_as_sequence = {
	(lenfunc)bpy_bpar_attrstateseq_length,              /* sq_length */
	NULL,                                               /* sq_concat */
	NULL,                                               /* sq_repeat */
	(ssizeargfunc)bpy_bpar_attrstateseq_subscript_int,  /* sq_item */ /* Only set this so PySequence_Check() returns True */
	NULL,                                               /* sq_slice */
	(ssizeobjargproc)NULL,                              /* sq_ass_item */
	NULL,                                               /* *was* sq_ass_slice */
	(objobjproc)bpy_bpar_attrstateseq_contains,         /* sq_contains */
	(binaryfunc) NULL,                                  /* sq_inplace_concat */
	(ssizeargfunc) NULL,                                /* sq_inplace_repeat */
};

static PyMappingMethods bpy_bpar_attrstateseq_as_mapping = {
	(lenfunc)bpy_bpar_attrstateseq_length,              /* mp_length */
	(binaryfunc)bpy_bpar_attrstateseq_subscript,        /* mp_subscript */
	(objobjargproc)NULL,                                /* mp_ass_subscript */
};

/* Iterator
 * -------- */

static PyObject *bpy_bpar_attrstateseq_iter(BPy_NParticleAttributeStateSeq *self)
{
	BPy_NParticleAttributeStateIter *py_iter;

	py_iter = (BPy_NParticleAttributeStateIter *)BPy_NParticleAttributeStateIter_CreatePyObject(self->state);
	BKE_nparticle_state_attributes_begin(self->state, &py_iter->iter);
	return (PyObject *)py_iter;
}

static PyObject *bpy_bpar_attrstateiter_next(BPy_NParticleAttributeStateIter *self)
{
	if (BKE_nparticle_state_attribute_iter_valid(&self->iter)) {
		PyObject *result = BPy_NParticleAttributeState_CreatePyObject(self->state, self->iter.attrstate);
		BKE_nparticle_state_attribute_iter_next(&self->iter);
		return result;
	}
	else {
		PyErr_SetNone(PyExc_StopIteration);
		return NULL;
	}
}

/* Dealloc Functions
 * ================= */

static void bpy_bpar_state_dealloc(BPy_NParticleState *self)
{
	NParticleState *state = self->state;
	
	if (state) {
		state->py_handle = NULL;
		
		BKE_nparticle_state_free(state);
	}
	
	PyObject_DEL(self);
}

static void bpy_bpar_attrstate_dealloc(BPy_NParticleAttributeState *self)
{
	PyObject_DEL(self);
}

static void bpy_bpar_attrstateseq_dealloc(BPy_NParticleAttributeStateSeq *self)
{
	PyObject_DEL(self);
}

static void bpy_bpar_attrstateiter_dealloc(BPy_NParticleAttributeStateIter *self)
{
	BKE_nparticle_state_attribute_iter_end(&self->iter);
	PyObject_DEL(self);
}

/* Types
 * ===== */

PyTypeObject BPy_NParticleState_Type                = {{{0}}};
PyTypeObject BPy_NParticleAttributeStateSeq_Type    = {{{0}}};
PyTypeObject BPy_NParticleAttributeStateIter_Type   = {{{0}}};
PyTypeObject BPy_NParticleAttributeState_Type       = {{{0}}};

void BPy_BPAR_init_types(void)
{
	BPy_NParticleState_Type.tp_basicsize                = sizeof(BPy_NParticleState);
	BPy_NParticleAttributeState_Type.tp_basicsize       = sizeof(BPy_NParticleAttributeState);
	BPy_NParticleAttributeStateSeq_Type.tp_basicsize    = sizeof(BPy_NParticleAttributeStateSeq);
	BPy_NParticleAttributeStateIter_Type.tp_basicsize   = sizeof(BPy_NParticleAttributeStateIter);

	BPy_NParticleState_Type.tp_name                     = "NParticleState";
	BPy_NParticleAttributeState_Type.tp_name            = "NParticleAttributeState";
	BPy_NParticleAttributeStateSeq_Type.tp_name         = "NParticleAttributeStateSeq";
	BPy_NParticleAttributeStateIter_Type.tp_name        = "NParticleAttributeStateIter";

	BPy_NParticleState_Type.tp_doc                      = bpy_bpar_state_doc;
	BPy_NParticleAttributeState_Type.tp_doc             = bpy_bpar_attrstate_doc;
	BPy_NParticleAttributeStateSeq_Type.tp_doc          = bpy_bpar_attrstateseq_doc;
	BPy_NParticleAttributeStateIter_Type.tp_doc         = bpy_bpar_attrstateiter_doc;

	BPy_NParticleState_Type.tp_repr                     = (reprfunc)bpy_bpar_state_repr;
	BPy_NParticleAttributeState_Type.tp_repr            = (reprfunc)bpy_bpar_attrstate_repr;
	BPy_NParticleAttributeStateSeq_Type.tp_repr         = NULL;
	BPy_NParticleAttributeStateIter_Type.tp_repr        = NULL;

	BPy_NParticleState_Type.tp_getset                   = bpy_bpar_state_getseters;
	BPy_NParticleAttributeState_Type.tp_getset          = bpy_bpar_attrstate_getseters;
	BPy_NParticleAttributeStateSeq_Type.tp_getset       = NULL;
	BPy_NParticleAttributeStateIter_Type.tp_getset      = NULL;

	BPy_NParticleState_Type.tp_methods                  = bpy_bpar_state_methods;
	BPy_NParticleAttributeState_Type.tp_methods         = bpy_bpar_attrstate_methods;
	BPy_NParticleAttributeStateSeq_Type.tp_methods      = NULL;
	BPy_NParticleAttributeStateIter_Type.tp_methods     = NULL;

	BPy_NParticleState_Type.tp_hash                     = bpy_bpar_state_hash;
	BPy_NParticleAttributeState_Type.tp_hash            = bpy_bpar_attrstate_hash;
	BPy_NParticleAttributeStateSeq_Type.tp_hash         = NULL;
	BPy_NParticleAttributeStateIter_Type.tp_hash        = NULL;

	BPy_NParticleAttributeStateSeq_Type.tp_as_sequence  = &bpy_bpar_attrstateseq_as_sequence;

	BPy_NParticleAttributeStateSeq_Type.tp_as_mapping   = &bpy_bpar_attrstateseq_as_mapping;

	BPy_NParticleAttributeStateSeq_Type.tp_iter         = (getiterfunc)bpy_bpar_attrstateseq_iter;

	BPy_NParticleAttributeStateIter_Type.tp_iternext    = (iternextfunc)bpy_bpar_attrstateiter_next;
	BPy_NParticleAttributeStateIter_Type.tp_iter        = PyObject_SelfIter;

	BPy_NParticleState_Type.tp_dealloc                  = (destructor)bpy_bpar_state_dealloc;
	BPy_NParticleAttributeState_Type.tp_dealloc         = (destructor)bpy_bpar_attrstate_dealloc;
	BPy_NParticleAttributeStateSeq_Type.tp_dealloc      = (destructor)bpy_bpar_attrstateseq_dealloc;
	BPy_NParticleAttributeStateIter_Type.tp_dealloc     = (destructor)bpy_bpar_attrstateiter_dealloc;

	BPy_NParticleState_Type.tp_flags                    = Py_TPFLAGS_DEFAULT;
	BPy_NParticleAttributeState_Type.tp_flags           = Py_TPFLAGS_DEFAULT;
	BPy_NParticleAttributeStateSeq_Type.tp_flags        = Py_TPFLAGS_DEFAULT;
	BPy_NParticleAttributeStateIter_Type.tp_flags       = Py_TPFLAGS_DEFAULT;

	PyType_Ready(&BPy_NParticleState_Type);
	PyType_Ready(&BPy_NParticleAttributeState_Type);
	PyType_Ready(&BPy_NParticleAttributeStateSeq_Type);
	PyType_Ready(&BPy_NParticleAttributeStateIter_Type);
}


/* bparticles.types submodule
 * ********************* */

static struct PyModuleDef BPy_BPAR_types_module_def = {
	PyModuleDef_HEAD_INIT,
	"bparticles.types",  /* m_name */
	NULL,  /* m_doc */
	0,     /* m_size */
	NULL,  /* m_methods */
	NULL,  /* m_reload */
	NULL,  /* m_traverse */
	NULL,  /* m_clear */
	NULL,  /* m_free */
};

PyObject *BPyInit_bparticles_types(void)
{
	PyObject *submodule;

	submodule = PyModule_Create(&BPy_BPAR_types_module_def);

#define MODULE_TYPE_ADD(s, t) \
	PyModule_AddObject(s, t.tp_name, (PyObject *)&t); Py_INCREF((PyObject *)&t)

	/* bparticles_py_types.c */
	MODULE_TYPE_ADD(submodule, BPy_NParticleState_Type);
	MODULE_TYPE_ADD(submodule, BPy_NParticleAttributeState_Type);
	MODULE_TYPE_ADD(submodule, BPy_NParticleAttributeStateSeq_Type);
	MODULE_TYPE_ADD(submodule, BPy_NParticleAttributeStateIter_Type);

#undef MODULE_TYPE_ADD

	return submodule;
}


/* Utility Functions
 * ***************** */

PyObject *BPy_NParticleState_CreatePyObject(NParticleState *state)
{
	BPy_NParticleState *self;

	if (state->py_handle) {
		self = state->py_handle;
		Py_INCREF(self);
	}
	else {
		self = PyObject_New(BPy_NParticleState, &BPy_NParticleState_Type);
		self->state = state;

		state->py_handle = self; /* point back */
	}

	return (PyObject *)self;
}

PyObject *BPy_NParticleAttributeState_CreatePyObject(NParticleState *state, NParticleAttributeState *attr)
{
	BPy_NParticleAttributeState *self = PyObject_New(BPy_NParticleAttributeState, &BPy_NParticleAttributeState_Type);
	self->state = state;
	self->attrstate = attr;
	return (PyObject *)self;
}

PyObject *BPy_NParticleAttributeStateSeq_CreatePyObject(NParticleState *state)
{
	BPy_NParticleAttributeStateSeq *self = PyObject_New(BPy_NParticleAttributeStateSeq, &BPy_NParticleAttributeStateSeq_Type);
	self->state = state;
	return (PyObject *)self;
}

PyObject *BPy_NParticleAttributeStateIter_CreatePyObject(NParticleState *state)
{
	BPy_NParticleAttributeStateIter *self = PyObject_New(BPy_NParticleAttributeStateIter, &BPy_NParticleAttributeStateIter_Type);
	self->state = state;
	/* caller must initialize 'iter' member */
	return (PyObject *)self;
}

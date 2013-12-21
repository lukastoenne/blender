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
#include "BLI_string.h"

#include "DNA_nparticle_types.h"

#include "BKE_nparticle.h"

#include "../mathutils/mathutils.h"

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

PyDoc_STRVAR(bpy_bpar_particle_doc,
"Single particle in state data\n"
);

PyDoc_STRVAR(bpy_bpar_particleseq_doc,
"Particle sequence\n"
);

PyDoc_STRVAR(bpy_bpar_particleiter_doc,
"Iterator for looping over particles\n"
);


PyDoc_STRVAR(bpy_bpar_state_attributes_doc,
"State attributes (read-only).\n\n:type: :class:`NParticleAttributeStateSeq`"
);
static PyObject *bpy_bpar_state_attributes_get(BPy_NParticleState *self, void *UNUSED(closure))
{
	return BPy_NParticleAttributeStateSeq_CreatePyObject(self->state);
}

PyDoc_STRVAR(bpy_bpar_state_particles_doc,
"Particles.\n\n:type: :class:`NParticleParticleSeq`"
);
static PyObject *bpy_bpar_state_particles_get(BPy_NParticleState *self, void *UNUSED(closure))
{
	return BPy_NParticleParticleSeq_CreatePyObject(self->state);
}

PyDoc_STRVAR(bpy_bpar_attrstate_name_doc,
"Attribute name\n"
);
static PyObject *bpy_bpar_attrstate_name_get(BPy_NParticleAttributeState *self)
{
	return PyUnicode_FromString(self->attrstate->desc.name);
}

static PyObject *bpy_bpar_particle_dir(BPy_NParticleParticle *self)
{
	NParticleAttributeStateIterator iter;
	PyObject *ret;
	PyObject *pystring;

//	PYRNA_STRUCT_CHECK_OBJ(self);

	ret = PyList_New(0);

	for (BKE_nparticle_state_attributes_begin(self->state, &iter);
	     BKE_nparticle_state_attribute_iter_valid(&iter);
	     BKE_nparticle_state_attribute_iter_next(&iter)) {
		pystring = PyUnicode_FromString(iter.attrstate->desc.name);
		PyList_Append(ret, pystring);
		Py_DECREF(pystring);
	}

	return ret;
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

static PyObject *bpy_bpar_particle_repr(BPy_NParticleParticle *self)
{
	NParticleIterator *iter = &self->iter;

	if (iter->index >= 0) {
		return PyUnicode_FromFormat("<NParticleParticle index=%d>", iter->index);
	}
	else {
		return PyUnicode_FromFormat("<NParticleParticle invalid>");
	}
}

static PyGetSetDef bpy_bpar_state_getseters[] = {
	{(char *)"attributes", (getter)bpy_bpar_state_attributes_get, (setter)NULL, (char *)bpy_bpar_state_attributes_doc, NULL},
	{(char *)"particles", (getter)bpy_bpar_state_particles_get, (setter)NULL, (char *)bpy_bpar_state_particles_doc, NULL},
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

static struct PyMethodDef bpy_bpar_particle_methods[] = {
	{"__dir__", (PyCFunction)bpy_bpar_particle_dir, METH_NOARGS, NULL},
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

static Py_ssize_t bpy_bpar_particleseq_length(BPy_NParticleAttributeStateSeq *self)
{
	return BKE_nparticle_state_num_particles(self->state);
}

static PyObject *bpy_bpar_particleseq_subscript_int(BPy_NParticleAttributeStateSeq *self, int keynum)
{
	NParticleIterator iter;
	NParticleID id = (NParticleID)keynum;
	
	BKE_nparticle_iter_from_id(self->state, &iter, id);
	return BPy_NParticleParticle_CreatePyObject(self->state, id, iter);
}

static int bpy_bpar_particleseq_contains(BPy_NParticleParticleSeq *self, PyObject *value)
{
	if (BPy_NParticleParticle_Check(value)) {
		BPy_NParticleParticle *particle_py = (BPy_NParticleParticle *)value;
		return particle_py->state == self->state && BKE_nparticle_iter_valid(&particle_py->iter);
	}
	return 0;
}

static PyObject *bpy_bpar_particleseq_subscript(BPy_NParticleAttributeStateSeq *self, PyObject *key)
{
	if (PyIndex_Check(key)) {
		Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return NULL;
		return bpy_bpar_particleseq_subscript_int(self, i);
	}
	else {
		PyErr_Format(PyExc_TypeError,
		             "NParticleAttributeStateSeq[key]: invalid key, "
		             "must be an int, not %.200s",
		             Py_TYPE(key)->tp_name);
		return NULL;
	}
}

static PySequenceMethods bpy_bpar_particleseq_as_sequence = {
	(lenfunc)bpy_bpar_particleseq_length,               /* sq_length */
	NULL,                                               /* sq_concat */
	NULL,                                               /* sq_repeat */
	(ssizeargfunc)bpy_bpar_particleseq_subscript_int,   /* sq_item */ /* Only set this so PySequence_Check() returns True */
	NULL,                                               /* sq_slice */
	(ssizeobjargproc)NULL,                              /* sq_ass_item */
	NULL,                                               /* *was* sq_ass_slice */
	(objobjproc)bpy_bpar_particleseq_contains,          /* sq_contains */
	(binaryfunc) NULL,                                  /* sq_inplace_concat */
	(ssizeargfunc) NULL,                                /* sq_inplace_repeat */
};

static PyMappingMethods bpy_bpar_particleseq_as_mapping = {
	(lenfunc)bpy_bpar_particleseq_length,               /* mp_length */
	(binaryfunc)bpy_bpar_particleseq_subscript,         /* mp_subscript */
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

static PyObject *bpy_bpar_particleseq_iter(BPy_NParticleParticleSeq *self)
{
	BPy_NParticleParticleIter *py_iter;

	py_iter = (BPy_NParticleParticleIter *)BPy_NParticleParticleIter_CreatePyObject(self->state);
	BKE_nparticle_iter_init(self->state, &py_iter->iter);
	return (PyObject *)py_iter;
}

static PyObject *bpy_bpar_particleiter_next(BPy_NParticleParticleIter *self)
{
	if (BKE_nparticle_iter_valid(&self->iter)) {
		NParticleID id = BKE_nparticle_iter_get_id(&self->iter);
		PyObject *result = BPy_NParticleParticle_CreatePyObject(self->state, id, self->iter);
		BKE_nparticle_iter_next(&self->iter);
		return result;
	}
	else {
		PyErr_SetNone(PyExc_StopIteration);
		return NULL;
	}
}

/* Get/Set Functions
 * ================= */

static PyObject *bpy_bpar_particle_data_read(NParticleAttributeDescription *desc, void *data)
{
	switch (desc->datatype) {
		case PAR_ATTR_DATATYPE_FLOAT:
			return PyFloat_FromDouble(*(float*)data);
		case PAR_ATTR_DATATYPE_INT:
			return PyLong_FromLong(*(int*)data);
		case PAR_ATTR_DATATYPE_BOOL:
			return PyBool_FromLong(*(bool*)data);
		case PAR_ATTR_DATATYPE_VECTOR:
		case PAR_ATTR_DATATYPE_POINT:
		case PAR_ATTR_DATATYPE_NORMAL:
			return Vector_CreatePyObject((float*)data, 3, Py_WRAP, NULL);
		case PAR_ATTR_DATATYPE_COLOR:
			return Color_CreatePyObject((float*)data, Py_WRAP, NULL);
		case PAR_ATTR_DATATYPE_MATRIX:
			return Matrix_CreatePyObject((float*)data, 4, 4, Py_WRAP, NULL);
			
		default:
			return NULL;
	}
}

static int bpy_bpar_particle_data_write(NParticleAttributeDescription *desc, void *data, PyObject *value)
{
	/* XXX do we accept this sort of overhead just for error prints?
	 * only needs to happen for actual errors, but mathutils_array_parse needs string in advance.
	 */
//	char error_prefix[128];
//	BLI_snprintf(error_prefix, sizeof(error_prefix), "NParticleParticle.%.200s", desc->name);
	
	switch (desc->datatype) {
		case PAR_ATTR_DATATYPE_FLOAT:
			if (PyFloat_Check(value))
				*(float*)data = (float)PyFloat_AsDouble(value);
			else {
				PyErr_Format(PyExc_TypeError,
				             "NParticleParticle.%.200s expects float, not %.200s",
				             desc->name, Py_TYPE(value)->tp_name);
				return -1;
			}
			break;
		case PAR_ATTR_DATATYPE_INT:
			if (PyLong_Check(value))
				*(int*)data = (int)PyLong_AsLong(value);
			else {
				PyErr_Format(PyExc_TypeError,
				             "NParticleParticle.%.200s expects int, not %.200s",
				             desc->name, Py_TYPE(value)->tp_name);
				return -1;
			}
			break;
		case PAR_ATTR_DATATYPE_BOOL:
			if (PyBool_Check(value))
				*(bool*)data = (bool)PyLong_AsLong(value);
			else {
				PyErr_Format(PyExc_TypeError,
				             "NParticleParticle.%.200s expects bool, not %.200s",
				             desc->name, Py_TYPE(value)->tp_name);
				return -1;
			}
			break;
		case PAR_ATTR_DATATYPE_VECTOR:
		case PAR_ATTR_DATATYPE_POINT:
		case PAR_ATTR_DATATYPE_NORMAL: {
			if (mathutils_array_parse((float*)data, 3, 3, value, "NParticleParticle") == -1)
				return -1;
			break;
		}
		case PAR_ATTR_DATATYPE_COLOR: {
			if (mathutils_array_parse((float*)data, 3, 3, value, "NParticleParticle") == -1)
				return -1;
			break;
		}
		case PAR_ATTR_DATATYPE_MATRIX: {
			if (mathutils_array_parse((float*)data, 16, 16, value, "NParticleParticle") == -1)
				return -1;
			break;
		}
	}
	
	return 0;
}

static PyObject *bpy_bpar_particle_getattro(BPy_NParticleParticle *self, PyObject *pyname)
{
	const char *name = _PyUnicode_AsString(pyname);
	PyObject *ret;

//	PYRNA_STRUCT_CHECK_OBJ(self);

	if (!BKE_nparticle_iter_valid(&self->iter)) {
		ret = NULL;
	}
	else if (name == NULL) {
		PyErr_SetString(PyExc_AttributeError, "NParticleParticle: __getattr__ must be a string");
		ret = NULL;
	}
	else {
		NParticleAttributeState *attrstate = BKE_nparticle_state_find_attribute(self->state, name);
		void *data;
		
		if (!attrstate) {
			PyErr_Format(PyExc_AttributeError, "NParticleParticle.%.200s not found", name);
			ret = NULL;
		}
		else {
			data = BKE_nparticle_attribute_state_data(attrstate, self->iter.index);
			if (!data) {
				ret = NULL;
			}
			else {
				ret = bpy_bpar_particle_data_read(&attrstate->desc, data);
			}
		}
	}

	return ret;
}

static int bpy_bpar_particle_setattro(BPy_NParticleParticle *self, PyObject *pyname, PyObject *value)
{
	const char *name = _PyUnicode_AsString(pyname);
	
//	PYRNA_STRUCT_CHECK_INT(self);
	
	if (name == NULL) {
		PyErr_SetString(PyExc_AttributeError, "NParticleParticle: __setattr__ must be a string");
		return -1;
	}
	else {
		NParticleAttributeState *attrstate = BKE_nparticle_state_find_attribute(self->state, name);
		void *data;
		
		if (!attrstate) {
			PyErr_Format(PyExc_AttributeError, "NParticleParticle.%.200s not found", name);
			return -1;
		}
		
		if (!BKE_nparticle_iter_valid(&self->iter)) {
			int index = BKE_nparticle_add(self->state, self->id);
			BKE_nparticle_iter_from_index(self->state, &self->iter, index);
		}
		
		data = BKE_nparticle_attribute_state_data(attrstate, self->iter.index);
		if (!data) {
			return -1;
		}
		
		if (bpy_bpar_particle_data_write(&attrstate->desc, data, value) == -1) {
			return -1;
		}
	}
	
	return 0;
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

static void bpy_bpar_particle_dealloc(BPy_NParticleParticle *self)
{
	PyObject_DEL(self);
}

static void bpy_bpar_particleseq_dealloc(BPy_NParticleParticleSeq *self)
{
	PyObject_DEL(self);
}

static void bpy_bpar_particleiter_dealloc(BPy_NParticleParticleIter *self)
{
	PyObject_DEL(self);
}

/* Types
 * ===== */

PyTypeObject BPy_NParticleState_Type                = {{{0}}};
PyTypeObject BPy_NParticleAttributeState_Type       = {{{0}}};
PyTypeObject BPy_NParticleAttributeStateSeq_Type    = {{{0}}};
PyTypeObject BPy_NParticleAttributeStateIter_Type   = {{{0}}};
PyTypeObject BPy_NParticleParticle_Type             = {{{0}}};
PyTypeObject BPy_NParticleParticleSeq_Type          = {{{0}}};
PyTypeObject BPy_NParticleParticleIter_Type         = {{{0}}};

void BPy_BPAR_init_types(void)
{
	BPy_NParticleState_Type.tp_basicsize                = sizeof(BPy_NParticleState);
	BPy_NParticleAttributeState_Type.tp_basicsize       = sizeof(BPy_NParticleAttributeState);
	BPy_NParticleAttributeStateSeq_Type.tp_basicsize    = sizeof(BPy_NParticleAttributeStateSeq);
	BPy_NParticleAttributeStateIter_Type.tp_basicsize   = sizeof(BPy_NParticleAttributeStateIter);
	BPy_NParticleParticle_Type.tp_basicsize             = sizeof(BPy_NParticleParticle);
	BPy_NParticleParticleSeq_Type.tp_basicsize          = sizeof(BPy_NParticleParticleSeq);
	BPy_NParticleParticleIter_Type.tp_basicsize         = sizeof(BPy_NParticleParticleIter);

	BPy_NParticleState_Type.tp_name                     = "NParticleState";
	BPy_NParticleAttributeState_Type.tp_name            = "NParticleAttributeState";
	BPy_NParticleAttributeStateSeq_Type.tp_name         = "NParticleAttributeStateSeq";
	BPy_NParticleAttributeStateIter_Type.tp_name        = "NParticleAttributeStateIter";
	BPy_NParticleParticle_Type.tp_name                  = "NParticleParticle";
	BPy_NParticleParticleSeq_Type.tp_name               = "NParticleParticleSeq";
	BPy_NParticleParticleIter_Type.tp_name              = "NParticleParticleIter";

	BPy_NParticleState_Type.tp_doc                      = bpy_bpar_state_doc;
	BPy_NParticleAttributeState_Type.tp_doc             = bpy_bpar_attrstate_doc;
	BPy_NParticleAttributeStateSeq_Type.tp_doc          = bpy_bpar_attrstateseq_doc;
	BPy_NParticleAttributeStateIter_Type.tp_doc         = bpy_bpar_attrstateiter_doc;
	BPy_NParticleParticle_Type.tp_doc                   = bpy_bpar_particle_doc;
	BPy_NParticleParticleSeq_Type.tp_doc                = bpy_bpar_particleseq_doc;
	BPy_NParticleParticleIter_Type.tp_doc               = bpy_bpar_particleiter_doc;

	BPy_NParticleState_Type.tp_repr                     = (reprfunc)bpy_bpar_state_repr;
	BPy_NParticleAttributeState_Type.tp_repr            = (reprfunc)bpy_bpar_attrstate_repr;
	BPy_NParticleAttributeStateSeq_Type.tp_repr         = NULL;
	BPy_NParticleAttributeStateIter_Type.tp_repr        = NULL;
	BPy_NParticleParticle_Type.tp_repr                  = (reprfunc)bpy_bpar_particle_repr;
	BPy_NParticleParticleSeq_Type.tp_repr               = NULL;
	BPy_NParticleParticleIter_Type.tp_repr              = NULL;

	BPy_NParticleState_Type.tp_getset                   = bpy_bpar_state_getseters;
	BPy_NParticleAttributeState_Type.tp_getset          = bpy_bpar_attrstate_getseters;
	BPy_NParticleAttributeStateSeq_Type.tp_getset       = NULL;
	BPy_NParticleAttributeStateIter_Type.tp_getset      = NULL;
	BPy_NParticleParticleSeq_Type.tp_getset             = NULL;
	BPy_NParticleParticleIter_Type.tp_getset            = NULL;

	BPy_NParticleParticle_Type.tp_getattro              = (getattrofunc)bpy_bpar_particle_getattro;
	BPy_NParticleParticle_Type.tp_setattro              = (setattrofunc)bpy_bpar_particle_setattro;

	BPy_NParticleState_Type.tp_methods                  = bpy_bpar_state_methods;
	BPy_NParticleAttributeState_Type.tp_methods         = bpy_bpar_attrstate_methods;
	BPy_NParticleAttributeStateSeq_Type.tp_methods      = NULL;
	BPy_NParticleAttributeStateIter_Type.tp_methods     = NULL;
	BPy_NParticleParticle_Type.tp_methods               = bpy_bpar_particle_methods;
	BPy_NParticleParticleSeq_Type.tp_methods            = NULL;
	BPy_NParticleParticleIter_Type.tp_methods           = NULL;

	BPy_NParticleState_Type.tp_hash                     = bpy_bpar_state_hash;
	BPy_NParticleAttributeState_Type.tp_hash            = bpy_bpar_attrstate_hash;
	BPy_NParticleAttributeStateSeq_Type.tp_hash         = NULL;
	BPy_NParticleAttributeStateIter_Type.tp_hash        = NULL;
	BPy_NParticleParticle_Type.tp_hash                  = NULL;
	BPy_NParticleParticleSeq_Type.tp_hash               = NULL;
	BPy_NParticleParticleIter_Type.tp_hash              = NULL;

	BPy_NParticleAttributeStateSeq_Type.tp_as_sequence  = &bpy_bpar_attrstateseq_as_sequence;
	BPy_NParticleParticleSeq_Type.tp_as_sequence        = &bpy_bpar_particleseq_as_sequence;

	BPy_NParticleAttributeStateSeq_Type.tp_as_mapping   = &bpy_bpar_attrstateseq_as_mapping;
	BPy_NParticleParticleSeq_Type.tp_as_mapping         = &bpy_bpar_particleseq_as_mapping;

	BPy_NParticleAttributeStateSeq_Type.tp_iter         = (getiterfunc)bpy_bpar_attrstateseq_iter;
	BPy_NParticleParticleSeq_Type.tp_iter               = (getiterfunc)bpy_bpar_particleseq_iter;

	BPy_NParticleAttributeStateIter_Type.tp_iternext    = (iternextfunc)bpy_bpar_attrstateiter_next;
	BPy_NParticleAttributeStateIter_Type.tp_iter        = PyObject_SelfIter;
	BPy_NParticleParticleIter_Type.tp_iternext          = (iternextfunc)bpy_bpar_particleiter_next;
	BPy_NParticleParticleIter_Type.tp_iter              = PyObject_SelfIter;

	BPy_NParticleState_Type.tp_dealloc                  = (destructor)bpy_bpar_state_dealloc;
	BPy_NParticleAttributeState_Type.tp_dealloc         = (destructor)bpy_bpar_attrstate_dealloc;
	BPy_NParticleAttributeStateSeq_Type.tp_dealloc      = (destructor)bpy_bpar_attrstateseq_dealloc;
	BPy_NParticleAttributeStateIter_Type.tp_dealloc     = (destructor)bpy_bpar_attrstateiter_dealloc;
	BPy_NParticleParticle_Type.tp_dealloc               = (destructor)bpy_bpar_particle_dealloc;
	BPy_NParticleParticleSeq_Type.tp_dealloc            = (destructor)bpy_bpar_particleseq_dealloc;
	BPy_NParticleParticleIter_Type.tp_dealloc           = (destructor)bpy_bpar_particleiter_dealloc;

	BPy_NParticleState_Type.tp_flags                    = Py_TPFLAGS_DEFAULT;
	BPy_NParticleAttributeState_Type.tp_flags           = Py_TPFLAGS_DEFAULT;
	BPy_NParticleAttributeStateSeq_Type.tp_flags        = Py_TPFLAGS_DEFAULT;
	BPy_NParticleAttributeStateIter_Type.tp_flags       = Py_TPFLAGS_DEFAULT;
	BPy_NParticleParticle_Type.tp_flags                 = Py_TPFLAGS_DEFAULT;
	BPy_NParticleParticleSeq_Type.tp_flags              = Py_TPFLAGS_DEFAULT;
	BPy_NParticleParticleIter_Type.tp_flags             = Py_TPFLAGS_DEFAULT;

	PyType_Ready(&BPy_NParticleState_Type);
	PyType_Ready(&BPy_NParticleAttributeState_Type);
	PyType_Ready(&BPy_NParticleAttributeStateSeq_Type);
	PyType_Ready(&BPy_NParticleAttributeStateIter_Type);
	PyType_Ready(&BPy_NParticleParticle_Type);
	PyType_Ready(&BPy_NParticleParticleSeq_Type);
	PyType_Ready(&BPy_NParticleParticleIter_Type);
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
	MODULE_TYPE_ADD(submodule, BPy_NParticleParticle_Type);
	MODULE_TYPE_ADD(submodule, BPy_NParticleParticleSeq_Type);
	MODULE_TYPE_ADD(submodule, BPy_NParticleParticleIter_Type);

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

PyObject *BPy_NParticleParticle_CreatePyObject(NParticleState *state, NParticleID id, NParticleIterator iter)
{
	BPy_NParticleParticle *self = PyObject_New(BPy_NParticleParticle, &BPy_NParticleParticle_Type);
	self->state = state;
	self->id = id;
	self->iter = iter;
	return (PyObject *)self;
}

PyObject *BPy_NParticleParticleSeq_CreatePyObject(NParticleState *state)
{
	BPy_NParticleParticleSeq *self = PyObject_New(BPy_NParticleParticleSeq, &BPy_NParticleParticleSeq_Type);
	self->state = state;
	return (PyObject *)self;
}

PyObject *BPy_NParticleParticleIter_CreatePyObject(NParticleState *state)
{
	BPy_NParticleParticleIter *self = PyObject_New(BPy_NParticleParticleIter, &BPy_NParticleParticleIter_Type);
	self->state = state;
	/* caller must initialize 'iter' member */
	return (PyObject *)self;
}

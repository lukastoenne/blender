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

PyDoc_STRVAR(bpy_nparticlestate_doc,
"Particle state data\n"
);

PyDoc_STRVAR(bpy_nparticleattributestate_doc,
"Particle attribute state data\n"
);

static PyObject *bpy_nparticlestate_repr(BPy_NParticleState *self)
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

static PyObject *bpy_nparticleattributestate_repr(BPy_NParticleAttributeState *self)
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

static PyGetSetDef bpy_nparticlestate_getseters[] = {
	{NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static PyGetSetDef bpy_nparticleattributestate_getseters[] = {
	{NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static struct PyMethodDef bpy_nparticlestate_methods[] = {
	{NULL, NULL, 0, NULL}
};

static struct PyMethodDef bpy_nparticleattributestate_methods[] = {
	{NULL, NULL, 0, NULL}
};

static Py_hash_t bpy_nparticlestate_hash(PyObject *self)
{
	return _Py_HashPointer(((BPy_NParticleState *)self)->state);
}

static Py_hash_t bpy_nparticleattributestate_hash(PyObject *self)
{
	return _Py_HashPointer(((BPy_NParticleAttributeState *)self)->attrstate);
}

/* Dealloc Functions
 * ================= */

static void bpy_nparticlestate_dealloc(BPy_NParticleState *self)
{
	NParticleState *state = self->state;
	
	if (state) {
		state->py_handle = NULL;
		
		BKE_nparticle_state_free(state);
	}
	
	PyObject_DEL(self);
}

static void bpy_nparticleattributestate_dealloc(BPy_NParticleAttributeState *self)
{
	PyObject_DEL(self);
}

/* Types
 * ===== */

PyTypeObject BPy_NParticleState_Type        = {{{0}}};
PyTypeObject BPy_NParticleAttributeState_Type        = {{{0}}};

void BPy_BPAR_init_types(void)
{
	BPy_NParticleState_Type.tp_basicsize            = sizeof(BPy_NParticleState);
	BPy_NParticleAttributeState_Type.tp_basicsize   = sizeof(BPy_NParticleAttributeState);

	BPy_NParticleState_Type.tp_name                 = "NParticleState";
	BPy_NParticleAttributeState_Type.tp_name        = "NParticleAttributeState";

	BPy_NParticleState_Type.tp_doc                  = bpy_nparticlestate_doc;
	BPy_NParticleAttributeState_Type.tp_doc         = bpy_nparticleattributestate_doc;

	BPy_NParticleState_Type.tp_repr                 = (reprfunc)bpy_nparticlestate_repr;
	BPy_NParticleAttributeState_Type.tp_repr        = (reprfunc)bpy_nparticleattributestate_repr;

	BPy_NParticleState_Type.tp_getset               = bpy_nparticlestate_getseters;
	BPy_NParticleAttributeState_Type.tp_getset      = bpy_nparticleattributestate_getseters;

	BPy_NParticleState_Type.tp_methods              = bpy_nparticlestate_methods;
	BPy_NParticleAttributeState_Type.tp_methods     = bpy_nparticleattributestate_methods;

	BPy_NParticleState_Type.tp_hash                 = bpy_nparticlestate_hash;
	BPy_NParticleAttributeState_Type.tp_hash        = bpy_nparticleattributestate_hash;

	BPy_NParticleState_Type.tp_dealloc              = (destructor)bpy_nparticlestate_dealloc;
	BPy_NParticleAttributeState_Type.tp_dealloc     = (destructor)bpy_nparticleattributestate_dealloc;

	BPy_NParticleState_Type.tp_flags                = Py_TPFLAGS_DEFAULT;
	BPy_NParticleAttributeState_Type.tp_flags       = Py_TPFLAGS_DEFAULT;

	PyType_Ready(&BPy_NParticleState_Type);
	PyType_Ready(&BPy_NParticleAttributeState_Type);
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

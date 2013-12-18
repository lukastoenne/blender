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

/** \file blender/python/bparticles/bparticles_py_api.c
 *  \ingroup pybparticles
 *
 * This file defines the 'bparticles' module.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "DNA_nparticle_types.h"

#include "BKE_nparticle.h"

#include "bparticles_py_types.h"

#include "../generic/py_capi_utils.h"

#include "bparticles_py_api.h" /* own include */

PyDoc_STRVAR(bpy_bpar_new_doc,
".. method:: new()\n"
"\n"
"   :arg psys: The particle system.\n"
"   :type mesh: :class:`bpy.types.NParticleSystem`\n"
"   :return: Return a new, empty NParticleState.\n"
"   :rtype: :class:`bparticles.types.NParticleState`\n"
);

static PyObject *bpy_bpar_new(PyObject *UNUSED(self), PyObject *value)
{
	NParticleSystem *psys = PyC_RNA_AsPointer(value, "NParticleSystem");
	NParticleState *state;

	if (!psys)
		return NULL;

	state = BKE_nparticle_state_new(psys);

	return BPy_NParticleState_CreatePyObject(state);
}

static struct PyMethodDef BPy_BPAR_methods[] = {
	{"new", (PyCFunction)bpy_bpar_new, METH_O, bpy_bpar_new_doc},
	{NULL, NULL, 0, NULL}
};

PyDoc_STRVAR(BPy_BPAR_doc,
"This module provides access to blenders particle data structures.\n"
"\n"
".. include:: include__bparticles.rst\n"
);
static struct PyModuleDef BPy_BPAR_module_def = {
	PyModuleDef_HEAD_INIT,
	"bparticles",		/* m_name */
	BPy_BPAR_doc,		/* m_doc */
	0,					/* m_size */
	BPy_BPAR_methods,	/* m_methods */
	NULL,				/* m_reload */
	NULL,				/* m_traverse */
	NULL,				/* m_clear */
	NULL,				/* m_free */
};

PyObject *BPyInit_bparticles(void)
{
	PyObject *mod;
	PyObject *submodule;
	PyObject *sys_modules = PyThreadState_GET()->interp->modules;

	BPy_BPAR_init_types();

	mod = PyModule_Create(&BPy_BPAR_module_def);

	/* bparticles.types */
	PyModule_AddObject(mod, "types", (submodule = BPyInit_bparticles_types()));
	PyDict_SetItemString(sys_modules, PyModule_GetName(submodule), submodule);
	Py_INCREF(submodule);

#if 0
	PyModule_AddObject(mod, "ops", (submodule = BPyInit_bmesh_ops()));
	/* PyDict_SetItemString(sys_modules, PyModule_GetName(submodule), submodule); */
	PyDict_SetItemString(sys_modules, "bmesh.ops", submodule); /* fake module */
	Py_INCREF(submodule);

	PyModule_AddObject(mod, "utils", (submodule = BPyInit_bmesh_utils()));
	PyDict_SetItemString(sys_modules, PyModule_GetName(submodule), submodule);
	Py_INCREF(submodule);

	PyModule_AddObject(mod, "geometry", (submodule = BPyInit_bmesh_geometry()));
	PyDict_SetItemString(sys_modules, PyModule_GetName(submodule), submodule);
	Py_INCREF(submodule);
#endif

	return mod;
}

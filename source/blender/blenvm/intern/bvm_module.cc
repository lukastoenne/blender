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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenvm/intern/module.cc
 *  \ingroup bvm
 */

#include <cstdlib> /* needed for BLI_assert */

extern "C" {
#include "BLI_utildefines.h"
#include "BLI_ghash.h"
}

#include "MEM_guardedalloc.h"

#include "BVM_function.h"
#include "BVM_module.h"

// forward declaration
static void ghash_function_free(BVMFunction *fun);

void BVM_module_init(BVMModule *lib)
{
	lib->functions = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "BVM library functions");
}

void BVM_module_free(BVMModule *lib)
{
	BLI_assert(lib->functions != NULL);
	BLI_ghash_free(lib->functions, NULL, (GHashValFreeFP)ghash_function_free);
}

static void ghash_function_free(BVMFunction *fun)
{
	MEM_freeN(fun);
}

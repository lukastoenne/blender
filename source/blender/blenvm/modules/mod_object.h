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

#ifndef __MOD_OBJECT_H__
#define __MOD_OBJECT_H__

extern "C" {
#include "DNA_object_types.h"

#include "RNA_access.h"
}

#include "mod_defines.h"

#include "util_eval_globals.h"
#include "util_math.h"

BVM_MOD_NAMESPACE_BEGIN

bvm_extern void V__OBJECT_LOOKUP(const EvalGlobals &globals, PointerRNA &ob_ptr, int key)
{
	ob_ptr = globals.lookup_object(key);
}
BVM_DECL_FUNCTION_VALUE(OBJECT_LOOKUP)

bvm_extern void V__OBJECT_TRANSFORM(matrix44 &tfm, const PointerRNA &ob_ptr)
{
	if (ob_ptr.data && RNA_struct_is_a(&RNA_Object, ob_ptr.type)) {
		Object *ob = (Object *)ob_ptr.data;
		copy_m4_m4(tfm.c_data(), ob->obmat);
	}
	else
		tfm = matrix44::identity();
}
BVM_DECL_FUNCTION_VALUE(OBJECT_TRANSFORM)

BVM_MOD_NAMESPACE_END

#endif /* __MOD_OBJECT_H__ */

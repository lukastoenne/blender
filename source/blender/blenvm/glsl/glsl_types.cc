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

/** \file blender/blenvm/glsl/glsl_types.cc
 *  \ingroup glsl
 */

#include "glsl_types.h"
#include "glsl_value.h"

namespace blenvm {

string bvm_glsl_get_type(const TypeSpec *spec, bool use_dual)
{
	if (spec->is_structure()) {
		/* XXX TODO */
#if 0
		const StructSpec *sspec = spec->structure();
		std::vector<const char *> fields;
		for (int i = 0; i < sspec->num_fields(); ++i) {
			const char *ftypestr = bvm_get_glsl_type(sspec->field(i).typespec, use_dual);
			fields.push_back(ftypestr);
		}
		return StructType::create(context, ArrayRef<Type*>(fields));
#else
		BLI_assert(false);
		return NULL;
#endif
	}
	else {
		return bvm_glsl_get_type(spec->base_type(), use_dual);
	}
	return NULL;
}

string bvm_glsl_get_type(BVMType type, bool /*use_dual*/)
{
	const char *typestr = "";
#define BVM_DO_TYPE(t) \
	typestr = BVMTypeGLSLTraits<t>::get_type_string();
	BVM_TYPE_SELECT(type);
#undef BVM_DO_TYPE
	return typestr;
}

bool bvm_glsl_type_has_dual_value(const TypeSpec *spec)
{
	if (spec->is_structure()) {
		/* for structs we use invidual dual values for fields */
		return false;
	}
	else {
		bool has_dual_value = false;
#define BVM_DO_TYPE(t) \
	has_dual_value = BVMTypeGLSLTraits<t>::use_dual_value;
		BVM_TYPE_SELECT(spec->base_type());
#undef BVM_DO_TYPE
		return has_dual_value;
	}
}

string bvm_glsl_create_constant(const NodeConstant *node_value)
{
	const TypeSpec *spec = node_value->typedesc().get_typespec();
	if (spec->is_structure()) {
		/* TODO */
		BLI_assert(false);
		return "";
	}
	else {
		string s;
#define BVM_DO_TYPE(t) \
	s = BVMTypeGLSLTraits<t>::create_constant(node_value);
		BVM_TYPE_SELECT(spec->base_type());
#undef BVM_DO_TYPE
		return s;
	}
}

string bvm_glsl_create_zero(const TypeSpec *spec)
{
	if (spec->is_structure()) {
		/* TODO */
		BLI_assert(false);
		return "";
	}
	else {
		string s;
#define BVM_DO_TYPE(t) \
	s = BVMTypeGLSLTraits<t>::create_zero();
		BVM_TYPE_SELECT(spec->base_type());
#undef BVM_DO_TYPE
		return s;
	}
}

void bvm_glsl_copy_value(stringstream &code, GLSLValue *dst, GLSLValue *src, const TypeSpec *spec)
{
	code << dst->name() << " = " << src->name() << ";\n";
	UNUSED_VARS(spec);
}

} /* namespace blenvm */

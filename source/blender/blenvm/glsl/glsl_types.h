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

#ifndef __GLSL_TYPES_H__
#define __GLSL_TYPES_H__

/** \file blender/blenvm/glsl/glsl_types.h
 *  \ingroup glsl
 */

#include "MEM_guardedalloc.h"

#include "node_value.h"
#include "typedesc.h"

#include "util_string.h"


namespace blenvm {

/* expects a macro DO_BVM_TYPE that handles the individual types */
#define BVM_TYPE_SELECT(t) \
switch (t) { \
	case BVM_FLOAT: BVM_DO_TYPE(BVM_FLOAT); break; \
	case BVM_FLOAT3: BVM_DO_TYPE(BVM_FLOAT3); break; \
	case BVM_FLOAT4: BVM_DO_TYPE(BVM_FLOAT4); break; \
	case BVM_INT: BVM_DO_TYPE(BVM_INT); break; \
	case BVM_MATRIX44: BVM_DO_TYPE(BVM_MATRIX44); break; \
	case BVM_STRING: BVM_DO_TYPE(BVM_STRING); break; \
	case BVM_RNAPOINTER: BVM_DO_TYPE(BVM_RNAPOINTER); break; \
	case BVM_MESH: BVM_DO_TYPE(BVM_MESH); break; \
	case BVM_DUPLIS: BVM_DO_TYPE(BVM_DUPLIS); break; \
} (void)0

#if 0 /* template-based version of the BVM_TYPE_SELECT macro */
template <template <BVMType> typename F>
void select_type(BVMType type)
{
	switch (type) {
		case BVM_FLOAT: F<BVM_FLOAT>(); break;
		case BVM_FLOAT3: F<BVM_FLOAT3>(); break;
	}
}
#endif

template <BVMType type>
struct BVMTypeGLSLTraits;

template <>
struct BVMTypeGLSLTraits<BVM_FLOAT> {
	enum { use_dual_value = true };
	
	static const char *get_type_string() { return "float"; }
	
	static string create_constant(const NodeConstant *node_value)
	{
		float f = 0.0f;
		node_value->get(&f);
		
		stringstream ss;
		ss << "float(" << f << ")";
		return ss.str();
	}
	
	static string create_zero()
	{
		return "float(0.0)";
	}
};

template <>
struct BVMTypeGLSLTraits<BVM_FLOAT3> {
	enum { use_dual_value = true };
	
	static const char *get_type_string() { return "vec3"; }
	
	static string create_constant(const NodeConstant *node_value)
	{
		float3 f = float3(0.0f, 0.0f, 0.0f);
		node_value->get(&f);
		
		stringstream ss;
		ss << "vec3(" << f.x << ", " << f.y << ", " << f.z << ")";
		return ss.str();
	}
	
	static string create_zero()
	{
		return "vec3(0.0, 0.0, 0.0)";
	}
};

template <>
struct BVMTypeGLSLTraits<BVM_FLOAT4> {
	enum { use_dual_value = true };
	
	static const char *get_type_string() { return "vec4"; }
	
	static string create_constant(const NodeConstant *node_value)
	{
		float4 f = float4(0.0f, 0.0f, 0.0f, 0.0f);
		node_value->get(&f);
		
		stringstream ss;
		ss << "vec4(" << f.x << ", " << f.y << ", " << f.z << ", " << f.w << ")";
		return ss.str();
	}
	
	static string create_zero()
	{
		return "vec4(0.0, 0.0, 0.0, 0.0)";
	}
};

template <>
struct BVMTypeGLSLTraits<BVM_INT> {
	enum { use_dual_value = false };
	
	static const char *get_type_string() { return "int"; }
	
	static string create_constant(const NodeConstant *node_value)
	{
		int i = 0;
		node_value->get(&i);
		
		stringstream ss;
		ss << "int(" << i << ")";
		return ss.str();
	}
	
	static string create_zero()
	{
		return "int(0)";
	}
};

template <>
struct BVMTypeGLSLTraits<BVM_MATRIX44> {
	enum { use_dual_value = false };
	
	static const char *get_type_string() { return "mat4"; }
	
	static string create_constant(const NodeConstant *node_value)
	{
		matrix44 m = matrix44::identity();
		node_value->get(&m);
		
		stringstream ss;
		ss << "mat4(" << m.data[0][0] << ", " << m.data[0][1] << ", " << m.data[0][2] << ", " << m.data[0][3] << ", "
		              << m.data[1][0] << ", " << m.data[1][1] << ", " << m.data[1][2] << ", " << m.data[1][3] << ", "
		              << m.data[2][0] << ", " << m.data[2][1] << ", " << m.data[2][2] << ", " << m.data[2][3] << ", "
		              << m.data[3][0] << ", " << m.data[3][1] << ", " << m.data[3][2] << ", " << m.data[3][3] << ")";
		return ss.str();
	}
	
	static string create_zero()
	{
		return "mat4(0.0)";
	}
};

template <>
struct BVMTypeGLSLTraits<BVM_STRING> {
	enum { use_dual_value = false };
	
	static const char *get_type_string() { return NULL; }
	
	static string create_constant(const NodeConstant *UNUSED(node_value))
	{
		BLI_assert(false && "Unsupported data type!");
		return "";
	}
	
	static string create_zero()
	{
		BLI_assert(false && "Unsupported data type!");
		return "";
	}
};

template <>
struct BVMTypeGLSLTraits<BVM_RNAPOINTER> {
	enum { use_dual_value = false };
	
	static const char *get_type_string() { return NULL; }
	
	static string create_constant(const NodeConstant *UNUSED(node_value))
	{
		BLI_assert(false && "Unsupported data type!");
		return "";
	}
	
	static string create_zero()
	{
		BLI_assert(false && "Unsupported data type!");
		return "";
	}
};

template <>
struct BVMTypeGLSLTraits<BVM_MESH> {
	enum { use_dual_value = false };
	
	static const char *get_type_string() { return NULL; }
	
	static string create_constant(const NodeConstant *UNUSED(node_value))
	{
		BLI_assert(false && "Unsupported data type!");
		return "";
	}
	
	static string create_zero()
	{
		BLI_assert(false && "Unsupported data type!");
		return "";
	}
};

template <>
struct BVMTypeGLSLTraits<BVM_DUPLIS> {
	enum { use_dual_value = false };
	
	static const char *get_type_string() { return NULL; }
	
	static string create_constant(const NodeConstant *UNUSED(node_value))
	{
		BLI_assert(false && "Unsupported data type!");
		return "";
	}
	
	static string create_zero()
	{
		BLI_assert(false && "Unsupported data type!");
		return "";
	}
};

/* ------------------------------------------------------------------------- */

struct GLSLValue;

string bvm_glsl_get_type(const TypeSpec *spec, bool use_dual);
string bvm_glsl_get_type(BVMType type, bool use_dual);
bool bvm_glsl_type_has_dual_value(const TypeSpec *spec);

string bvm_glsl_create_constant(const NodeConstant *node_value);
string bvm_glsl_create_zero(const TypeSpec *spec);

void bvm_glsl_copy_value(stringstream &code, GLSLValue *dst, GLSLValue *src, const TypeSpec *spec);

} /* namespace blenvm */

#endif /* __GLSL_TYPES_H__ */

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

#ifndef __BVM_EVAL_COMMON_H__
#define __BVM_EVAL_COMMON_H__

/** \file bvm_eval_common.h
 *  \ingroup bvm
 */

#include "bvm_eval.h"
#include "bvm_function.h"

namespace bvm {

struct EvalContext;

struct EvalKernelData {
	const EvalContext *context;
	const Function *function;
};

inline static float stack_load_float(float *stack, StackIndex offset)
{
	return *(float *)(&stack[offset]);
}

inline static float3 stack_load_float3(float *stack, StackIndex offset)
{
	return *(float3 *)(&stack[offset]);
}

inline static float4 stack_load_float4(float *stack, StackIndex offset)
{
	return *(float4 *)(&stack[offset]);
}

inline static int stack_load_int(float *stack, StackIndex offset)
{
	return *(int *)(&stack[offset]);
}

inline static matrix44 stack_load_matrix44(float *stack, StackIndex offset)
{
	return *(matrix44 *)(&stack[offset]);
}

inline static PointerRNA stack_load_pointer(float *stack, StackIndex offset)
{
	return *(PointerRNA *)(&stack[offset]);
}

inline static mesh_ptr stack_load_mesh_ptr(float *stack, StackIndex offset)
{
	return *(mesh_ptr *)(&stack[offset]);
}

inline static DerivedMesh *stack_load_mesh(float *stack, StackIndex offset)
{
	return ((mesh_ptr *)(&stack[offset]))->get();
}

inline static void stack_store_float(float *stack, StackIndex offset, float f)
{
	*(float *)(&stack[offset]) = f;
}

inline static void stack_store_float3(float *stack, StackIndex offset, float3 f)
{
	*(float3 *)(&stack[offset]) = f;
}

inline static void stack_store_float4(float *stack, StackIndex offset, float4 f)
{
	*(float4 *)(&stack[offset]) = f;
}

inline static void stack_store_int(float *stack, StackIndex offset, int i)
{
	*(int *)(&stack[offset]) = i;
}

inline static void stack_store_matrix44(float *stack, StackIndex offset, matrix44 m)
{
	*(matrix44 *)(&stack[offset]) = m;
}

inline static void stack_store_pointer(float *stack, StackIndex offset, PointerRNA p)
{
	*(PointerRNA *)(&stack[offset]) = p;
}

inline static void stack_store_mesh_ptr(float *stack, StackIndex offset, mesh_ptr p)
{
	*(mesh_ptr *)(&stack[offset]) = p;
}

inline static void stack_store_mesh(float *stack, StackIndex offset, DerivedMesh *dm)
{
	((mesh_ptr *)(&stack[offset]))->set(dm);
}

} /* namespace bvm */

#endif /* __BVM_EVAL_COMMON_H__ */

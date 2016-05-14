/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License) \ or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful) \
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not) \ write to the Free Software Foundation) \
 * Inc.) \ 51 Franklin Street) \ Fifth Floor) \ Boston) \ MA 02110-1301) \ USA.
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

#ifndef __BVM_OPCODE_H__
#define __BVM_OPCODE_H__

/** \file blender/blenvm/util/util_opcode.h
 *  \ingroup bvm
 */

extern "C" {
#include "BLI_utildefines.h"
}

namespace blenvm {

/* Macros to handle opcodes.
 * Expects DEF_OPCODE to be defined locally for this purpose.
 */
/*************************************/
#define BVM_DEFINE_OPCODES_BASE \
	DEF_OPCODE(NOOP) \
	\
	DEF_OPCODE(VALUE_FLOAT) \
	DEF_OPCODE(VALUE_FLOAT3) \
	DEF_OPCODE(VALUE_FLOAT4) \
	DEF_OPCODE(VALUE_INT) \
	DEF_OPCODE(VALUE_MATRIX44) \
	DEF_OPCODE(VALUE_STRING) \
	DEF_OPCODE(VALUE_RNAPOINTER) \
	DEF_OPCODE(VALUE_MESH) \
	DEF_OPCODE(VALUE_DUPLIS) \
	\
	DEF_OPCODE(FLOAT_TO_INT) \
	DEF_OPCODE(INT_TO_FLOAT) \
	DEF_OPCODE(SET_FLOAT3) \
	DEF_OPCODE(GET_ELEM_FLOAT3) \
	DEF_OPCODE(SET_FLOAT4) \
	DEF_OPCODE(GET_ELEM_FLOAT4) \
	
/*************************************/
#define BVM_DEFINE_OPCODES_POINTERS \
	DEF_OPCODE(INIT_MESH_PTR) \
	DEF_OPCODE(RELEASE_MESH_PTR) \
	DEF_OPCODE(INIT_DUPLIS_PTR) \
	DEF_OPCODE(RELEASE_DUPLIS_PTR) \
	
/*************************************/
#define BVM_DEFINE_OPCODES_ITERATOR \
	DEF_OPCODE(RANGE_INT) \
	
/*************************************/
#define BVM_DEFINE_OPCODES_MATH \
	DEF_OPCODE(ADD_FLOAT) \
	DEF_OPCODE(SUB_FLOAT) \
	DEF_OPCODE(MUL_FLOAT) \
	DEF_OPCODE(DIV_FLOAT) \
	DEF_OPCODE(SINE) \
	DEF_OPCODE(COSINE) \
	DEF_OPCODE(TANGENT) \
	DEF_OPCODE(ARCSINE) \
	DEF_OPCODE(ARCCOSINE) \
	DEF_OPCODE(ARCTANGENT) \
	DEF_OPCODE(POWER) \
	DEF_OPCODE(LOGARITHM) \
	DEF_OPCODE(MINIMUM) \
	DEF_OPCODE(MAXIMUM) \
	DEF_OPCODE(ROUND) \
	DEF_OPCODE(LESS_THAN) \
	DEF_OPCODE(GREATER_THAN) \
	DEF_OPCODE(MODULO) \
	DEF_OPCODE(ABSOLUTE) \
	DEF_OPCODE(CLAMP_ONE) \
	DEF_OPCODE(SQRT) \
	\
	DEF_OPCODE(ADD_FLOAT3) \
	DEF_OPCODE(SUB_FLOAT3) \
	DEF_OPCODE(MUL_FLOAT3) \
	DEF_OPCODE(DIV_FLOAT3) \
	DEF_OPCODE(MUL_FLOAT3_FLOAT) \
	DEF_OPCODE(DIV_FLOAT3_FLOAT) \
	DEF_OPCODE(AVERAGE_FLOAT3) \
	DEF_OPCODE(DOT_FLOAT3) \
	DEF_OPCODE(CROSS_FLOAT3) \
	DEF_OPCODE(NORMALIZE_FLOAT3) \
	DEF_OPCODE(LENGTH_FLOAT3) \
	\
	DEF_OPCODE(ADD_MATRIX44) \
	DEF_OPCODE(SUB_MATRIX44) \
	DEF_OPCODE(MUL_MATRIX44) \
	DEF_OPCODE(MUL_MATRIX44_FLOAT) \
	DEF_OPCODE(DIV_MATRIX44_FLOAT) \
	DEF_OPCODE(NEGATE_MATRIX44) \
	DEF_OPCODE(TRANSPOSE_MATRIX44) \
	DEF_OPCODE(INVERT_MATRIX44) \
	DEF_OPCODE(ADJOINT_MATRIX44) \
	DEF_OPCODE(DETERMINANT_MATRIX44) \
	\
	DEF_OPCODE(MUL_MATRIX44_FLOAT3) \
	DEF_OPCODE(MUL_MATRIX44_FLOAT4) \
	\
	DEF_OPCODE(MATRIX44_TO_LOC) \
	DEF_OPCODE(MATRIX44_TO_EULER) \
	DEF_OPCODE(MATRIX44_TO_AXISANGLE) \
	DEF_OPCODE(MATRIX44_TO_SCALE) \
	DEF_OPCODE(LOC_TO_MATRIX44) \
	DEF_OPCODE(EULER_TO_MATRIX44) \
	DEF_OPCODE(AXISANGLE_TO_MATRIX44) \
	DEF_OPCODE(SCALE_TO_MATRIX44) \
	
/*************************************/
#define BVM_DEFINE_OPCODES_COLOR \
	DEF_OPCODE(MIX_RGB) \
	
/*************************************/
#define BVM_DEFINE_OPCODES_RANDOM \
	DEF_OPCODE(INT_TO_RANDOM) \
	DEF_OPCODE(FLOAT_TO_RANDOM) \
	
/*************************************/
#define BVM_DEFINE_OPCODES_TEXTURE \
	DEF_OPCODE(TEX_PROC_VORONOI) \
	DEF_OPCODE(TEX_PROC_MAGIC) \
	DEF_OPCODE(TEX_PROC_MARBLE) \
	DEF_OPCODE(TEX_PROC_CLOUDS) \
	DEF_OPCODE(TEX_PROC_WOOD) \
	DEF_OPCODE(TEX_PROC_MUSGRAVE) \
	DEF_OPCODE(TEX_PROC_STUCCI) \
	DEF_OPCODE(TEX_PROC_DISTNOISE) \
	
/*************************************/
#define BVM_DEFINE_OPCODES_OBJECT \
	DEF_OPCODE(OBJECT_LOOKUP) \
	DEF_OPCODE(OBJECT_TRANSFORM) \
	DEF_OPCODE(OBJECT_FINAL_MESH) \
	\
	DEF_OPCODE(EFFECTOR_TRANSFORM) \
	DEF_OPCODE(EFFECTOR_CLOSEST_POINT) \
	
/*************************************/
#define BVM_DEFINE_OPCODES_MODIFIER \
	DEF_OPCODE(MESH_LOAD) \
	DEF_OPCODE(MESH_COMBINE) \
	DEF_OPCODE(MESH_ARRAY) \
	DEF_OPCODE(MESH_DISPLACE) \
	DEF_OPCODE(MESH_BOOLEAN) \
	DEF_OPCODE(MESH_CLOSEST_POINT) \
	
/*************************************/
#define BVM_DEFINE_OPCODES_CURVE \
	DEF_OPCODE(CURVE_PATH) \
	
/*************************************/
#define BVM_DEFINE_OPCODES_IMAGE \
	DEF_OPCODE(IMAGE_SAMPLE) \
	
/*************************************/
#define BVM_DEFINE_OPCODES_DUPLI \
	DEF_OPCODE(MAKE_DUPLI) \
	DEF_OPCODE(DUPLIS_COMBINE) \
	
/*************************************/

/* all possible opcodes */
#define BVM_DEFINE_ALL_OPCODES \
	BVM_DEFINE_OPCODES_BASE \
	BVM_DEFINE_OPCODES_COLOR \
	BVM_DEFINE_OPCODES_CURVE \
	BVM_DEFINE_OPCODES_DUPLI \
	BVM_DEFINE_OPCODES_IMAGE \
	BVM_DEFINE_OPCODES_ITERATOR \
	BVM_DEFINE_OPCODES_MATH \
	BVM_DEFINE_OPCODES_MODIFIER \
	BVM_DEFINE_OPCODES_OBJECT \
	BVM_DEFINE_OPCODES_POINTERS \
	BVM_DEFINE_OPCODES_RANDOM \
	BVM_DEFINE_OPCODES_TEXTURE \

/* implemented subset of opcodes */
#define BVM_DEFINE_OPCODES \
	BVM_DEFINE_OPCODES_BASE \
	BVM_DEFINE_OPCODES_COLOR \
	BVM_DEFINE_OPCODES_MATH \
	

/* Define the main enum for opcodes */
#define DEF_OPCODE(op) \
	OP_##op,
enum OpCode {
	BVM_DEFINE_ALL_OPCODES \
	OP_END,
};
#undef DEF_OPCODE


/* define a name string for each opcode */
#define DEF_OPCODE(op) \
	case OP_##op: return STRINGIFY(op);
BLI_INLINE const char *opcode_name(OpCode op)
{
	switch (op) {
		BVM_DEFINE_ALL_OPCODES
		default: return "";
	}
}
#undef DEF_OPCODE

} /* namespace blenvm */

#endif /* __BVM_OPCODE_H__ */

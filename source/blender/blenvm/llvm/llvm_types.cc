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

/** \file blender/blenvm/llvm/llvm_types.cc
 *  \ingroup llvm
 */

#include "llvm_headers.h"
#include "llvm_types.h"


namespace blenvm {

llvm::Type *llvm_create_value_type(llvm::LLVMContext &context, const string &name, const TypeDesc *td)
{
	using namespace llvm;
	
	if (td->is_structure()) {
		return llvm_create_struct_type(context, name, td->structure());
	}
	else {
		switch (td->base_type()) {
			case BVM_FLOAT:
				return TypeBuilder<types::ieee_float, true>::get(context);
			case BVM_FLOAT3:
				return TypeBuilder<float3, true>::get(context);
			case BVM_FLOAT4:
				return TypeBuilder<float4, true>::get(context);
			case BVM_INT:
				return TypeBuilder<types::i<32>, true>::get(context);
			case BVM_MATRIX44:
				return TypeBuilder<matrix44, true>::get(context);
				
			case BVM_STRING:
			case BVM_RNAPOINTER:
			case BVM_MESH:
			case BVM_DUPLIS:
				/* TODO */
				return NULL;
		}
	}
	return NULL;
}

bool llvm_use_argument_pointer(const TypeDesc *td)
{
	using namespace llvm;
	
	if (td->is_structure()) {
		/* pass by reference */
		return true;
	}
	else {
		switch (td->base_type()) {
			case BVM_FLOAT:
			case BVM_INT:
				/* pass by value */
				return false;
			case BVM_FLOAT3:
			case BVM_FLOAT4:
			case BVM_MATRIX44:
				/* pass by reference */
				return true;
				
			case BVM_STRING:
			case BVM_RNAPOINTER:
			case BVM_MESH:
			case BVM_DUPLIS:
				/* TODO */
				break;
		}
	}
	
	return false;
}

llvm::StructType *llvm_create_struct_type(llvm::LLVMContext &context, const string &name, const StructSpec *s)
{
	using namespace llvm;
	
	std::vector<Type*> elemtypes;
	for (int i = 0; i < s->num_fields(); ++s) {
		Type *ftype = llvm_create_value_type(context, s->field(i).name, &s->field(i).typedesc);
		elemtypes.push_back(ftype);
	}
	
	return StructType::create(context, ArrayRef<Type*>(elemtypes), name);
}

llvm::FunctionType *llvm_create_node_function_type(llvm::LLVMContext &context,
                                              const std::vector<llvm::Type*> &inputs,
                                              const std::vector<llvm::Type*> &outputs)
{
	using namespace llvm;
	
	std::vector<llvm::Type*> arg_types;
	for (int i = 0; i < outputs.size(); ++i) {
		Type *value_type = outputs[i];
		/* use a pointer to store output values */
		arg_types.push_back(value_type->getPointerTo());
	}
	arg_types.insert(arg_types.end(), inputs.begin(), inputs.end());
	
	return FunctionType::get(TypeBuilder<void, true>::get(context), arg_types, false);
}

} /* namespace blenvm */

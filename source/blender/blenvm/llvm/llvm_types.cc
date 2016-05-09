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

llvm::StructType *codegen_struct_type(llvm::LLVMContext &context, const string &name, const StructSpec *s)
{
	using namespace llvm;
	
	std::vector<Type*> elemtypes;
	for (int i = 0; i < s->num_fields(); ++s) {
		Type *ftype = codegen_type(context, s->field(i).name, &s->field(i).typedesc);
		elemtypes.push_back(ftype);
	}
	
	return StructType::create(context, ArrayRef<Type*>(elemtypes), name);
}

llvm::Type *codegen_type(llvm::LLVMContext &context, const string &name, const TypeDesc *td)
{
	using namespace llvm;
	
	if (td->is_structure()) {
		return codegen_struct_type(context, name, td->structure());
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

#if 0 // TODO
llvm::FunctionType *codegen_node_function_type(llvm::LLVMContext &context,
                                               const TypeDescList &inputs,
                                               const TypeDescList &outputs)
{
	using namespace llvm;
	
	std::vector<Type *> input_types, output_types;
	
	for (int i = 0; i < outputs.size(); ++i) {
		const TypeDesc *desc = outputs[i];
		Type *type = codegen_type(context, dummy_type_name(output.key), &output.typedesc);
		output_types.push_back(type);
	}
	StructType *return_type = StructType::get(context(), output_types);
	
	input_types.push_back(PointerType::get(return_type, 0));
	for (int i = 0; i < graph.inputs.size(); ++i) {
		const NodeGraph::Input &input = graph.inputs[i];
		Type *type = codegen_type(dummy_type_name(input.key), &input.typedesc);
//		type = PointerType::get(type, 0);
		input_types.push_back(type);
	}
	
	return FunctionType::get(TypeBuilder<void, true>::get(context()), input_types, false);
}
#endif

} /* namespace blenvm */

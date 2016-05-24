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

/** \file blender/blenvm/llvm/llvm_compiler_dual.cc
 *  \ingroup llvm
 */

#include <cstdio>
#include <set>
#include <sstream>

#include "node_graph.h"

#include "llvm_compiler.h"
#include "llvm_engine.h"
#include "llvm_function.h"
#include "llvm_headers.h"
#include "llvm_modules.h"
#include "llvm_types.h"

#include "util_opcode.h"

#include "modules.h"

namespace blenvm {

llvm::Module *LLVMTextureCompilerImpl::m_nodes_module = NULL;

llvm::Type *LLVMTextureCompilerImpl::get_value_type(const TypeSpec *spec, bool is_constant)
{
	return bvm_get_llvm_type(context(), spec, !is_constant);
}

bool LLVMTextureCompilerImpl::use_argument_pointer(const TypeSpec *typespec, bool is_constant)
{
	using namespace llvm;
	
	if (typespec->is_structure()) {
		/* pass by reference */
		return true;
	}
	else if (!is_constant && bvm_type_has_dual_value(typespec)) {
		return true;
	}
	else {
		switch (typespec->base_type()) {
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

bool LLVMTextureCompilerImpl::use_elementary_argument_pointer(const TypeSpec *typespec)
{
	using namespace llvm;
	
	if (typespec->is_structure()) {
		/* pass by reference */
		return true;
	}
	else {
		switch (typespec->base_type()) {
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

llvm::Constant *LLVMTextureCompilerImpl::create_node_value_constant(const NodeValue *node_value)
{
	return bvm_create_llvm_constant(context(), node_value);
}

llvm::Function *LLVMTextureCompilerImpl::declare_elementary_node_function(llvm::Module *mod, const NodeType *nodetype, const string &name)
{
	using namespace llvm;
	
	std::vector<Type *> input_types, output_types;
	for (int i = 0; i < nodetype->num_inputs(); ++i) {
		const NodeInput *input = nodetype->find_input(i);
		const TypeSpec *typespec = input->typedesc.get_typespec();
		Type *type = bvm_get_llvm_type(context(), typespec, false);
		if (type == NULL)
			break;
		if (use_elementary_argument_pointer(typespec))
			type = type->getPointerTo();
		input_types.push_back(type);
	}
	for (int i = 0; i < nodetype->num_outputs(); ++i) {
		const NodeOutput *output = nodetype->find_output(i);
		const TypeSpec *typespec = output->typedesc.get_typespec();
		Type *type = bvm_get_llvm_type(context(), typespec, false);
		if (type == NULL)
			break;
		output_types.push_back(type);
	}
	if (input_types.size() != nodetype->num_inputs() ||
	    output_types.size() != nodetype->num_outputs()) {
		/* some arguments could not be handled */
		return NULL;
	}
	
	FunctionType *functype = get_node_function_type(input_types, output_types);
	
	Function *func = Function::Create(functype, Function::ExternalLinkage, name, mod);
	return func;
}

bool LLVMTextureCompilerImpl::set_node_function_impl(OpCode op, const NodeType *nodetype,
                                                     llvm::Function *value_func,
                                                     std::vector<llvm::Function*> deriv_funcs)
{
	using namespace llvm;
	
	typedef std::vector<Value*> ValueList;
	
	ValueList value_args;
	value_args.reserve(value_func->arg_size());
	for (Function::arg_iterator a = value_func->arg_begin(); a != value_func->arg_end(); ++a)
		value_args.push_back(a);
	
#if 0 /* TODO only do the value function for now */
	std::vector<ValueList> deriv_args(nodetype->num_inputs());
	for (int n = 0; n < nodetype->num_inputs(); ++n) {
		deriv_args[n].reserve(deriv_funcs[n]->arg_size());
		for (Function::arg_iterator a = deriv_funcs[n]->arg_begin(); a != deriv_funcs[n]->arg_end(); ++a)
			deriv_args[n].push_back(a);
	}
#endif
	
	switch (op) {
		case OP_VALUE_FLOAT: {
			BasicBlock *block = BasicBlock::Create(context(), "entry", value_func);
			def_node_VALUE_FLOAT(context(), block, value_args[0], value_args[1]);
			return true;
		}
		case OP_VALUE_INT: {
			BasicBlock *block = BasicBlock::Create(context(), "entry", value_func);
			def_node_VALUE_INT(context(), block, value_args[0], value_args[1]);
			return true;
		}
		case OP_VALUE_FLOAT3: {
			BasicBlock *block = BasicBlock::Create(context(), "entry", value_func);
			def_node_VALUE_FLOAT3(context(), block, value_args[0], value_args[1]);
			return true;
		}
		case OP_VALUE_FLOAT4: {
			BasicBlock *block = BasicBlock::Create(context(), "entry", value_func);
			def_node_VALUE_FLOAT4(context(), block, value_args[0], value_args[1]);
			return true;
		}
		case OP_VALUE_MATRIX44: {
			BasicBlock *block = BasicBlock::Create(context(), "entry", value_func);
			def_node_VALUE_MATRIX44(context(), block, value_args[0], value_args[1]);
			return true;
		}
		
		default:
			return false;
	}
}

template <OpCode op>
static void define_elementary_functions(LLVMTextureCompilerImpl &C, llvm::Module *mod, const string &nodetype_name)
{
	using namespace llvm;
	
	const NodeType *nodetype = NodeGraph::find_node_type(nodetype_name);
	if (nodetype == NULL)
		return;
	
	string value_name = llvm_value_function_name(nodetype->name());
	/* declare function */
	Function *value_func = C.declare_elementary_node_function(mod, nodetype, value_name);
	/* declare partial derivatives wrt. the input arguments */
	std::vector<Function *> deriv_funcs(nodetype->num_inputs());
#if 0 /* TODO only do the value function for now */
	for (int arg_n = -1; arg_n < nodetype->num_inputs(); ++arg_n) {
		string deriv_name = llvm_deriv_function_name(nodetype->name(), arg_n);
		deriv_funcs[arg_n] = C.declare_elementary_node_function(mod, nodetype, deriv_name);
	}
#endif
	
	C.set_node_function_impl(op, nodetype, value_func, deriv_funcs);
}

void LLVMTextureCompilerImpl::define_dual_function_wrapper(llvm::Module *mod, const string &nodetype_name)
{
	using namespace llvm;
	
	const NodeType *nodetype = NodeGraph::find_node_type(nodetype_name);
	if (nodetype == NULL)
		return;
	
	/* get evaluation function(s) */
	string value_name = llvm_value_function_name(nodetype->name());
	Function *value_func = llvm_find_external_function(mod, value_name);
	BLI_assert(value_func != NULL && "Could not find node function!");
	
#if 0 /* TODO only do the value function for now */
	std::vector<Function *> deriv_funcs(nodetype->num_inputs());
	for (int n = -1; n < nodetype->num_inputs(); ++n) {
		string deriv_name = llvm_deriv_function_name(nodetype->name(), n);
		deriv_funcs[n] = llvm_find_external_function(mod, deriv_name);
	}
#endif
	
	/* wrapper function */
	Function *func = declare_node_function(mod, nodetype);
	if (func == NULL)
		return;
	
	BasicBlock *block = BasicBlock::Create(context(), "entry", func);
	IRBuilder<> builder(context());
	builder.SetInsertPoint(block);
	
	/* collect arguments for calling internal elementary functions */
	/* value and derivative components of input/output duals */
	std::vector<Value*> in_value, in_dx, in_dy;
	std::vector<Value*> out_value, out_dx, out_dy; 
	/* arguments for calculating main value and partial derivatives */
	std::vector<Value*> call_args;
	
	Function::arg_iterator arg_it = func->arg_begin();
	/* output arguments */
	for (int i = 0; i < nodetype->num_outputs(); ++i, ++arg_it) {
		Argument *arg = &(*arg_it);
		const NodeOutput *output = nodetype->find_output(i);
		
		Value *val, *dx, *dy;
		if (bvm_type_has_dual_value(output->typedesc.get_typespec())) {
			val = builder.CreateStructGEP(arg, 0);
			dx = builder.CreateStructGEP(arg, 1);
			dy = builder.CreateStructGEP(arg, 2);
		}
		else {
			val = arg;
			dx = dy = NULL;
		}
		
		out_value.push_back(val);
		out_dx.push_back(dx);
		out_dy.push_back(dy);
		call_args.push_back(val);
	}
	/* input arguments */
	for (int i = 0; i < nodetype->num_inputs(); ++i, ++arg_it) {
		Argument *arg = &(*arg_it);
		const NodeInput *input = nodetype->find_input(i);
		const TypeSpec *typespec = input->typedesc.get_typespec();
		
		Value *val, *dx, *dy;
		if (input->value_type != INPUT_CONSTANT && bvm_type_has_dual_value(typespec)) {
			val = builder.CreateStructGEP(arg, 0);
			dx = builder.CreateStructGEP(arg, 1);
			dy = builder.CreateStructGEP(arg, 2);
			
			if (!use_elementary_argument_pointer(typespec)) {
				val = builder.CreateLoad(val);
				dx = builder.CreateLoad(dx);
				dy = builder.CreateLoad(dy);
			}
		}
		else {
			val = arg;
			dx = dy = NULL;
		}
		
		in_value.push_back(val);
		in_dx.push_back(dx);
		in_dy.push_back(dy);
		call_args.push_back(val);
	}
	
	/* calculate value */
	builder.CreateCall(value_func, call_args);
#if 0 /* TODO only do the value function for now */
	/* calculate partial derivatives wrt. each input */
	for (int i = 0; i < nodetype->num_inputs(); ++i, ++arg_it) {
		const NodeInput *input = nodetype->find_input(i);
		const TypeSpec *typespec = input->typedesc.get_typespec();
		
		if (deriv_funcs[i] != NULL) {
			
		}
	}
#endif
	
	builder.CreateRetVoid();
}

void LLVMTextureCompilerImpl::define_nodes_module()
{
	using namespace llvm;
	
	Module *mod = new llvm::Module("texture_nodes", context());
	
#define DEF_OPCODE(op) \
	define_elementary_functions<OP_##op>(*this, mod, STRINGIFY(op));
	
	BVM_DEFINE_OPCODES
	
#undef DEF_OPCODE
	
	/* link base functions into the module */
//	std::string error;
//	Linker::LinkModules(mod, mod_basefuncs, Linker::LinkerMode::PreserveSource, &error);
	
#define DEF_OPCODE(op) \
	define_dual_function_wrapper(mod, STRINGIFY(op));
	
	BVM_DEFINE_OPCODES
	
#undef DEF_OPCODE
	
//	llvm_execution_engine()->addModule(mod_basefuncs);
//	llvm_execution_engine()->addModule(mod);
	
	m_nodes_module = mod;
}

} /* namespace blenvm */

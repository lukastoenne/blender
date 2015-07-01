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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenjit/intern/forcefield.cpp
 *  \ingroup bjit
 */

#include "llvm/Analysis/Passes.h"
#include "llvm/IR/AssemblyAnnotationWriter.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/PassManager.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"

#include "llvm-c/ExecutionEngine.h"

extern "C" {
#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_linklist.h"
#include "BLI_utildefines.h"

#include "BKE_effect.h"
}

#include "BJIT_forcefield.h"
#include "bjit_intern.h"

using namespace llvm;

static Module *theModule = NULL;

/* ------------------------------------------------------------------------- */
/* specialization of TypeBuilder for external struct types */

typedef types::ieee_float vec2_t[2];
typedef types::ieee_float vec3_t[3];
typedef types::ieee_float vec4_t[4];
typedef types::ieee_float mat3_t[3][3];
typedef types::ieee_float mat4_t[4][4];

namespace llvm {

template<bool cross>
class TypeBuilder<EffectorEvalInput, cross> {
public:
	static StructType *get(LLVMContext &context) {
		// If you cache this result, be sure to cache it separately
		// for each LLVMContext.
		return StructType::get(
		            TypeBuilder<vec3_t, cross>::get(context),
		            TypeBuilder<vec3_t, cross>::get(context),
		            NULL);
	}
	
	// You may find this a convenient place to put some constants
	// to help with getelementptr.  They don't have any effect on
	// the operation of TypeBuilder.
	enum Fields {
		FIELD_LOC,
		FIELD_VEL,
	};
};

template<bool cross>
class TypeBuilder<EffectorEvalResult, cross> {
public:
	static StructType *get(LLVMContext &context) {
		return StructType::get(
		            TypeBuilder<vec3_t, cross>::get(context),
		            TypeBuilder<vec3_t, cross>::get(context),
		            NULL);
	}
	
	enum Fields {
		FIELD_FORCE,
		FIELD_IMPULSE,
	};
};

template<bool cross>
class TypeBuilder<EffectorEvalSettings, cross> {
public:
	static StructType *get(LLVMContext &context) {
		return StructType::get(
		            TypeBuilder<mat4_t, cross>::get(context),
		            NULL);
	}
	
	enum Fields {
		FIELD_TRANSFORM,
	};
};

} /* namespace llvm */

/* ------------------------------------------------------------------------- */

void BJIT_build_effector_module(void)
{
	LLVMContext &context = getGlobalContext();
	
	theModule = new Module("effectors", context);
	
	bjit_link_module(theModule);
}

void BJIT_free_effector_module(void)
{
	bjit_remove_module(theModule);
	
	delete theModule;
	theModule = NULL;
}

static std::string get_effector_prefix(short forcefield)
{
	switch (forcefield) {
		case PFIELD_FORCE:
			return "effector_force";
			
		case PFIELD_NULL:
		case PFIELD_VORTEX:
		case PFIELD_MAGNET:
		case PFIELD_WIND:
		case PFIELD_GUIDE:
		case PFIELD_TEXTURE:
		case PFIELD_HARMONIC:
		case PFIELD_CHARGE:
		case PFIELD_LENNARDJ:
		case PFIELD_BOID:
		case PFIELD_TURBULENCE:
		case PFIELD_DRAG:
		case PFIELD_SMOKEFLOW:
			return "";
		
		default: {
			/* unknown type, should not happen */
			BLI_assert(false);
			return "";
		}
	}
	return "";
}

static Constant *make_const_vec3(LLVMContext &context, const float vec[3])
{
	return ConstantDataArray::get(context, ArrayRef<float>(vec, 3));
}

static Constant *make_const_vec4(LLVMContext &context, const float vec[4])
{
	return ConstantDataArray::get(context, ArrayRef<float>(vec, 4));
}

static Constant *make_const_mat4(LLVMContext &context, float mat[4][4])
{
	Constant *col1 = make_const_vec4(context, mat[0]);
	Constant *col2 = make_const_vec4(context, mat[1]);
	Constant *col3 = make_const_vec4(context, mat[2]);
	Constant *col4 = make_const_vec4(context, mat[3]);
	return ConstantArray::get(TypeBuilder<mat4_t, true>::get(context), std::vector<Constant*>({col1, col2, col3, col4}));
}

static Constant *make_effector_settings(LLVMContext &context, EffectorCache *eff)
{
	ArrayRef<Constant*> fields = {
	    make_const_mat4(context, eff->ob->obmat)
	};
	
	return ConstantStruct::get(TypeBuilder<EffectorEvalSettings, true>::get(context), fields);
}

void BJIT_build_effector_function(EffectorContext *effctx)
{
	LLVMContext &context = getGlobalContext();
	IRBuilder<> builder(context);
	
	FunctionType *functype = TypeBuilder<void(const EffectorEvalInput*, EffectorEvalResult*), true>::get(context);
	
	Function *func = Function::Create(functype, Function::ExternalLinkage, "effector", theModule);
	Value *arg_input, *arg_result;
	{
		Function::ArgumentListType::iterator it = func->arg_begin();
		arg_input = it++;
		arg_result = it++;
	}
	
	BasicBlock *entry = BasicBlock::Create(context, "entry", func);
	builder.SetInsertPoint(entry);
	
	/* initialize result */
	Value *res_force = builder.CreateStructGEP(arg_result, 0);
	Value *res_impulse = builder.CreateStructGEP(arg_result, 1);
	Value *vec3_zero = ConstantDataArray::get(context, std::vector<float>({0.0f, 0.0f, 0.0f}));
	builder.CreateStore(vec3_zero, res_force);
	builder.CreateStore(vec3_zero, res_impulse);
	
	for (EffectorCache *eff = (EffectorCache *)effctx->effectors.first; eff; eff = eff->next) {
		if (!eff->ob || !eff->pd)
			continue;
		
		std::string prefix = get_effector_prefix(eff->pd->forcefield);
		if (prefix.empty()) {
			/* undefined force type */
			continue;
		}
		
		std::string funcname = prefix + "_eval";
		
		EffectorEvalSettings settings;
		copy_m4_m4(settings.transform, eff->ob->obmat);
		Value *arg_settings = make_effector_settings(context, eff);
//		Value *arg_settings = builder.CreateLoad(make_effector_settings(context, eff));
		
		Function *evalfunc = theModule->getFunction(funcname);
		Value *evalarg_input, *evalarg_result, *evalarg_settings;
		{
			Function::ArgumentListType::iterator it = evalfunc->arg_begin();
			evalarg_input = it++;
			evalarg_result = it++;
			evalarg_settings = it++;
		}
		
		std::vector<Value *> args;
		args.push_back(builder.CreatePointerCast(arg_input, evalarg_input->getType()));
		args.push_back(builder.CreatePointerCast(arg_result, evalarg_result->getType()));
		builder.CreateStore(arg_settings, evalarg_settings);
//		args.push_back();
//		args.push_back(builder.CreatePointerCast(arg_settings, evalarg_settings->getType()));
		//		Value *arg_settings = builder.CreateLoad(make_effector_settings(context, eff));
		
		printf("--- Type of input (%d) ---------------\n", arg_settings->getType()->getStructNumElements());
		arg_settings->getType()->dump();
		for (int i; i < arg_settings->getType()->getStructNumElements(); ++i) {
			printf("\n  "); arg_settings->getType()->getStructElementType(i)->dump(); printf("\n");
		}
		printf("\n--- Type of argument (%d) ---------------\n", evalarg_settings->getType()->getStructNumElements());
		evalarg_settings->getType()->dump();
		for (int i; i < evalarg_settings->getType()->getStructNumElements(); ++i) {
			printf("\n  "); evalarg_settings->getType()->getStructElementType(i)->dump(); printf("\n");
		}
		printf("\n------------------\n");
//		TypeBuilder<EffectorEvalSettings, true>::get(context);
//		ArrayRef<Constant*>(&settings);
//		ConstantStruct(TypeBuilder<EffectorEvalSettings>::get(context), ArrayRef<Constant*>(&settings));
//		args.push_back(ConstantStruct(TypeBuilder<EffectorEvalSettings, true>::get(context), ArrayRef<Constant*>(settings)));
//		args.push_back(arg_settings);
		builder.CreateCall(evalfunc, args);
	}
	
	builder.CreateRetVoid();
	
	verifyFunction(*func, &outs());
	effctx->eval = (EffectorEvalFp)bjit_compile_function(func);
	effctx->eval_data = func;
}

void BJIT_free_effector_function(EffectorContext *effctx)
{
	Function *func = (Function *)effctx->eval_data;
	
	bjit_free_function(func);
	
	effctx->eval = NULL;
	effctx->eval_data = NULL;
}

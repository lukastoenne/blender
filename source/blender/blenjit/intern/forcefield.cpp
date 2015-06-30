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

} /* namespace llvm */

/* ------------------------------------------------------------------------- */

void BJIT_build_effector_function(EffectorContext *effctx)
{
	LLVMContext &context = getGlobalContext();
	IRBuilder<> builder(getGlobalContext());
	
	Module *mod = new Module("effectors", context);
	
	FunctionType *functype = TypeBuilder<types::i<32>(EffectorEvalInput*), true>::get(context);
	Function *func = Function::Create(functype, Function::ExternalLinkage, "effector", mod);
	
	BasicBlock *entry = BasicBlock::Create(context, "entry", func);
	builder.SetInsertPoint(entry);
	
	Function *effector_force = Function::Create(functype, Function::ExternalLinkage, "effector_force");
	Value *par_input = func->arg_begin();
	Value *args[] = { par_input };
	CallInst::Create(effector_force, ArrayRef<Value*>(args, 1), "effector_force", entry);
	
	Value *retval = ConstantInt::get(context, APInt(32, 6));
	builder.CreateRet(retval);
	
	verifyFunction(*func, &outs());
	
	bjit_add_module(mod);
	
	effctx->eval = (EffectorEvalFp)bjit_compile_function(func);
}

void BJIT_free_effector_function(EffectorContext *effctx)
{
	effctx->eval = NULL;
}

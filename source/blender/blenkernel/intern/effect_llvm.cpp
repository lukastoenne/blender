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

/** \file blender/source/blenkernel/intern/effect_llvm.cpp
 *  \ingroup bke
 */

#include "llvm/Analysis/Passes.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/PassManager.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Support/raw_ostream.h"

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

using namespace llvm;
using legacy::FunctionPassManager;
using legacy::PassManager;

typedef struct EffectorFunction EffectorFunction;

static Module *build_effector_module(EffectorCache *eff)
{
	return NULL;
}

static ExecutionEngine *create_execution_engine(Module *mod)
{
	std::string error;
	
	ExecutionEngine *engine = EngineBuilder(mod)
	                          .setErrorStr(&error)
//	                          .setMCJITMemoryManager(std::unique_ptr<HelpingMemoryManager>(
//	                                                     new HelpingMemoryManager(this)))
	                          .create();
	if (!engine) {
		fprintf(stderr, "Could not create ExecutionEngine: %s\n", error.c_str());
	}
	return engine;
}

static PassManager *create_pass_manager(ExecutionEngine *UNUSED(engine))
{
	PassManager *pm = new PassManager();
	
	// Set up the optimizer pipeline.  Start with registering info about how the
	// target lays out data structures.
//	pm->add(new DataLayout(*engine->getDataLayout()));
	// Provide basic AliasAnalysis support for GVN.
//	pm->add(createBasicAliasAnalysisPass());
	// Do simple "peephole" optimizations and bit-twiddling optzns.
//	pm->add(createInstructionCombiningPass());
	// Reassociate expressions.
//	pm->add(createReassociatePass());
	// Eliminate Common SubExpressions.
//	pm->add(createGVNPass());
	// Simplify the control flow graph (deleting unreachable blocks, etc).
//	pm->add(createCFGSimplificationPass());
	
//	pm->doInitialization();
	
	return pm;
}

static Function *codegen(EffectorContext *effctx)
{
//	std::vector<Type*> args(2, Type::getFloatTy(getGlobalContext()));
//	FunctionType *functype = FunctionType::get(Type::getInt8Ty(getGlobalContext()), args, false);
	std::vector<Type*> args;
	FunctionType *functype = FunctionType::get(Type::getInt8Ty(getGlobalContext()), args, false);
	Function *func = Function::Create(functype, Function::ExternalLinkage, "MyFunction");
	return func;
}

void BKE_effect_build_function(EffectorContext *effctx)
{
//	for (EffectorCache *eff = (EffectorCache *)effctx->effectors.first; eff; eff = eff->next) {
//		Module *effmod = build_effector_module();
//	}
	
	Module* mod = new Module("test", getGlobalContext());
	
	verifyModule(*mod);
	
	ExecutionEngine *engine = create_execution_engine(mod);
	
	PassManager *pm = create_pass_manager(engine);
	pm->run(*mod);
	delete pm;
	
	Function *func = codegen(effctx);
	
	effctx->eval = (EffectorEvalFp)engine->getPointerToFunction(func);
}

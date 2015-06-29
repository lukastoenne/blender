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
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"

extern "C" {
#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_path_util.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.h"
}

#include "BJIT_modules.h"
#include "bjit_intern.h"

using namespace llvm;

static ExecutionEngine *theEngine = NULL;

static ExecutionEngine *create_execution_engine()
{
	std::string error;
	
	Module *mod = new Module("main", getGlobalContext());
	
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

void BJIT_init(void)
{
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	InitializeNativeTargetAsmParser();
	
	BLI_assert(theEngine == NULL);
	theEngine = create_execution_engine();
	
	/* load modules */
	BJIT_load_all_modules(NULL, false);
}

void BJIT_free(void)
{
	BJIT_unload_all_modules();
	
	if (theEngine) {
		delete theEngine;
		theEngine = NULL;
	}
}

static PassManager *create_pass_manager()
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

void BJIT_load_module(const char *modfile, const char *modname)
{
	printf("Loading module '%s'\n", modfile);
	LLVMContext &llvmctx = getGlobalContext();
	SMDiagnostic err;
	
	Module *mod = getLazyIRFileModule(modfile, err, llvmctx);
//	Module *mod = ParseIRFile(modfile, err, llvmctx);
	if (!mod) {
		err.print(modfile, errs());
		return;
	}
	
	mod->setModuleIdentifier(modname);
	
	verifyModule(*mod, &outs());
	
	for (Module::FunctionListType::const_iterator it = mod->getFunctionList().begin(); it != mod->getFunctionList().end(); ++it) {
		printf("    %s\n", it->getName().str().c_str());
	}
	
	theEngine->addModule(mod);
}

void BJIT_load_all_modules(const char *modpath, bool reload)
{
	if (!modpath)
		modpath = BKE_appdir_folder_id(BLENDER_SYSTEM_SCRIPTS, "llvm/modules/");
	if (!modpath)
		return;
	
	if (reload) {
		BJIT_unload_all_modules();
	}
	
	struct direntry *dir;
	int totfile = BLI_filelist_dir_contents(modpath, &dir);
	for (int i = 0; i < totfile; ++i) {
		if ((dir[i].type & S_IFREG)) {
			const char *filename = dir[i].relname;
			const char *filepath = dir[i].path;
			
			if (BLI_testextensie(filename, ".ll")) {
				/* found a potential llvm IR module, try parsing it */
				BJIT_load_module(filepath, filename);
			}
		}
	}
	BLI_filelist_free(dir, totfile, NULL);
}

void BJIT_unload_all_modules()
{
	// TODO
}

void bjit_add_module(Module *mod)
{
	PassManager *pm = create_pass_manager();
	pm->run(*mod);
	delete pm;
	
	theEngine->finalizeObject();
}

void bjit_remove_module(Module *mod)
{
	if (mod)
		theEngine->removeModule(mod);
}

#if 0
void *bjit_compile_module(Module *mod, const char *funcname)
{
	PassManager *pm = create_pass_manager();
	pm->run(*mod);
	delete pm;
	
	theEngine->finalizeObject();
	
	return theEngine->getPointerToNamedFunction(funcname);
}
#endif

void *bjit_compile_function(Function *func)
{
//	return theEngine->getPointerToNamedFunction("BJIT_effector_force");
	return theEngine->getPointerToFunction(func);
}

void bjit_free_function(Function *func)
{
	theEngine->freeMachineCodeForFunction(func);
}

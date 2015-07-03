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

#include <map>
#include <string>

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
#include "llvm/Linker/Linker.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

extern "C" {
#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_path_util.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.h"
}

#include "BJIT_forcefield.h"
#include "BJIT_modules.h"
#include "bjit_intern.h"

using namespace llvm;

struct LLVMModule { int unused; };
struct LLVMFunction { int unused; };

static ExecutionEngine *theEngine = NULL;

static ModuleMap theModules;

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
	BJIT_build_effector_module();
}

void BJIT_free(void)
{
	BJIT_free_effector_module();
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

static void init_function_pass_manager(FunctionPassManager *fpm, int opt_level)
{
	PassManagerBuilder builder;
	builder.OptLevel = opt_level;
	builder.populateFunctionPassManager(*fpm);
//	builder.populateModulePassManager(MPM);
}

Function *bjit_find_function(Module *mod, const std::string &name)
{
	for (Module::FunctionListType::iterator it = mod->getFunctionList().begin(); it != mod->getFunctionList().end(); ++it) {
		Function *func = &(*it);
		if (func->hasFnAttribute("name")) {
			std::string value = func->getFnAttribute("name").getValueAsString();
			if (value == name)
				return func;
		}
	}
	return NULL;
}

/* Based on
 * http://homes.cs.washington.edu/~bholt/posts/llvm-quick-tricks.html
 */
static void bjit_parse_function_annotations(Module *mod)
{
	GlobalVariable *global_annos = mod->getNamedGlobal("llvm.global.annotations");
	if (global_annos) {
		ConstantArray *a = static_cast<ConstantArray*>(global_annos->getOperand(0));
		for (int i = 0; i < a->getNumOperands(); i++) {
			ConstantStruct *e = static_cast<ConstantStruct*>(a->getOperand(i));
			StringRef anno = static_cast<ConstantDataArray*>(static_cast<GlobalVariable*>(e->getOperand(1)->getOperand(0))->getOperand(0))->getAsCString();
			
			Function *fn = dynamic_cast<Function*>(e->getOperand(0)->getOperand(0));
			if (fn) {
				fn->addFnAttr("name", anno); /* add function annotation */
			}
		}
	}
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
	
	printf("Module Functions for '%s'\n", mod->getModuleIdentifier().c_str());
	for (Module::FunctionListType::const_iterator it = mod->getFunctionList().begin(); it != mod->getFunctionList().end(); ++it) {
		const Function &func = *it;
		printf("    %s\n", func.getName().str().c_str());
		
//		func.dump();
//		printf("++++++++++++++++++++++++++++++++++\n");
	}
	
	bjit_parse_function_annotations(mod);
	mod->setModuleIdentifier(modname);
	
	verifyModule(*mod, &outs());
	
	theEngine->addModule(mod);
	theModules[mod->getModuleIdentifier()] = mod;
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
	theModules.clear();
}

void bjit_link_module(Module *mod)
{
	for (ModuleMap::const_iterator it = theModules.begin(); it != theModules.end(); ++it) {
		Module *lmod = it->second;
		std::string error;
		Linker::LinkModules(mod, lmod, Linker::LinkerMode::PreserveSource, &error);
	}
	
//	printf("Linked Module Functions for '%s'\n", mod->getModuleIdentifier().c_str());
//	for (Module::FunctionListType::const_iterator it = mod->getFunctionList().begin(); it != mod->getFunctionList().end(); ++it) {
//		printf("    %s\n", it->getName().str().c_str());
//	}
	
	verifyModule(*mod, &outs());
	
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

void bjit_finalize_function(Module *mod, Function *func, int opt_level)
{
	FunctionPassManager fpm(mod);
	init_function_pass_manager(&fpm, opt_level);
	
	fpm.run(*func);
	
//	printf("=== Optimized Function '%s'' ===\n", func->getName().str().c_str());
//	func->dump();
//	printf("================================\n");
}

void *bjit_compile_function(Function *func)
{
//	return theEngine->getPointerToNamedFunction("BJIT_effector_force");
	return theEngine->getPointerToFunction(func);
}

void bjit_free_function(Function *func)
{
	theEngine->freeMachineCodeForFunction(func);
	func->eraseFromParent();
}

ModuleMap &bjit_get_modules()
{
	return theModules;
}

Module *bjit_get_module(const std::string &name)
{
	ModuleMap::const_iterator it = theModules.find(name);
	if (it != theModules.end())
		return it->second;
	else
		return NULL;
}

int BJIT_num_loaded_modules(void)
{
	return (int)theModules.size();
}

struct LLVMModule *BJIT_get_loaded_module_n(int n)
{
	ModuleMap::const_iterator it = theModules.begin();
	for (int i = 0; i < n; ++i) {
		if (it == theModules.end())
			return NULL;
		++it;
	}
	if (it == theModules.end())
		return NULL;
	return (LLVMModule *)it->second;
}

struct LLVMModule *BJIT_get_loaded_module(const char *name)
{
	ModuleMap::const_iterator it = theModules.find(name);
	if (it != theModules.end())
		return (LLVMModule *)it->second;
	else
		return NULL;
}

const char *BJIT_module_name(LLVMModule *_mod)
{
	Module *mod = (Module *)_mod;
	return mod->getModuleIdentifier().c_str();
}

int BJIT_module_num_functions(LLVMModule *_mod)
{
	Module *mod = (Module *)_mod;
	return (int)mod->getFunctionList().size();
}

struct LLVMFunction *BJIT_module_get_function_n(LLVMModule *_mod, int n)
{
	Module *mod = (Module *)_mod;
	Module::FunctionListType::const_iterator it = mod->getFunctionList().begin();
	for (int i = 0; i < n; ++i) {
		if (it == mod->getFunctionList().end())
			return NULL;
		++it;
	}
	if (it == mod->getFunctionList().end())
		return NULL;
	return (LLVMFunction *)(&(*it));
}

struct LLVMFunction *BJIT_module_get_function(LLVMModule *_mod, const char *name)
{
	Module *mod = (Module *)_mod;
	return (LLVMFunction *)bjit_find_function(mod, name);
}

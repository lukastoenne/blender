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

/** \file blender/blenvm/llvm/llvm_engine.cc
 *  \ingroup llvm
 */

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_string.h"
}

#include "util_opcode.h"

#include "llvm_codegen.h"
#include "llvm_engine.h"
#include "llvm_headers.h"
#include "llvm_modules.h"

#include "llvm/ExecutionEngine/SectionMemoryManager.h"

namespace blenvm {

static llvm::ExecutionEngine *theEngine = NULL;
static llvm::Module *theModule = NULL;
static llvm::legacy::PassManager *theModulePassMgr[3] = {0};
static llvm::legacy::FunctionPassManager *theFunctionPassMgr[3] = {0};

static struct GHash *extern_functions = NULL;


class MemoryManager : public llvm::SectionMemoryManager {
public:
	MemoryManager()
	{}
	virtual ~MemoryManager()
	{}
	
	/// This method returns the address of the specified function or variable.
	/// It is used to resolve symbols during module linking.
	uint64_t getSymbolAddress(const std::string &Name) override
	{
		uint64_t addr = llvm::SectionMemoryManager::getSymbolAddress(Name);
		if (addr)
			return addr;
		
		void *ptr = BLI_ghash_lookup(extern_functions, Name.c_str());
		return (uint64_t)ptr;
	}
};

static llvm::ExecutionEngine *create_execution_engine()
{
	using namespace llvm;
	
	std::string error;
	
	Module *mod = new Module("main", getGlobalContext());
	theModule = mod;
	
	EngineBuilder builder = EngineBuilder(mod);
	builder.setEngineKind(EngineKind::JIT);
	builder.setUseMCJIT(true);
	builder.setErrorStr(&error);
	builder.setMCJITMemoryManager(new MemoryManager());
	
	ExecutionEngine *engine = builder.create();
	if (!engine) {
		printf("Could not create ExecutionEngine: %s\n", error.c_str());
	}
	return engine;
}

static void create_pass_managers()
{
	using namespace llvm;
	using legacy::FunctionPassManager;
	using legacy::PassManager;
	
	BLI_assert(theModule != NULL && "theModule must be initialized for pass managers!");
	
	for (int opt_level = 0; opt_level < 3; ++opt_level) {
		theModulePassMgr[opt_level] = new PassManager();
		theFunctionPassMgr[opt_level] = new FunctionPassManager(theModule);
		
		PassManager &MPM = *theModulePassMgr[opt_level];
		FunctionPassManager &FPM = *theFunctionPassMgr[opt_level];
		
#if 0
		/* Set up the optimizer pipeline.
		 * Start with registering info about how the
		 * target lays out data structures.
		 */
		FPM.add(new DataLayoutPass(*llvm_execution_engine()->getDataLayout()));
		/* Provide basic AliasAnalysis support for GVN. */
		FPM.add(createBasicAliasAnalysisPass());
		/* Do simple "peephole" optimizations and bit-twiddling optzns. */
		FPM.add(createInstructionCombiningPass());
		/* Reassociate expressions. */
		FPM.add(createReassociatePass());
		/* Eliminate Common SubExpressions. */
		FPM.add(createGVNPass());
		/* Simplify the control flow graph (deleting unreachable blocks, etc). */
		FPM.add(createCFGSimplificationPass());
		
		FPM.doInitialization();
#endif
		
		PassManagerBuilder builder;
		builder.OptLevel = opt_level;
		
		builder.populateModulePassManager(MPM);
		if (opt_level > 1) {
			/* Inline small functions */
			MPM.add(createFunctionInliningPass());
		}
		
		builder.populateFunctionPassManager(FPM);
		if (opt_level > 1) {
			/* Optimize memcpy intrinsics */
			FPM.add(createMemCpyOptPass());
		}
	}
}

void llvm_init()
{
	using namespace llvm;
	
	extern_functions = BLI_ghash_str_new("LLVM external functions hash");
	register_extern_node_functions();
	
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	InitializeNativeTargetAsmParser();
	
	BLI_assert(theEngine == NULL);
	theEngine = create_execution_engine();
	
	create_pass_managers();
	
	LLVMCodeGenerator::define_nodes_module(getGlobalContext());
}

static void extern_functions_keyfree(void *key)
{
	MEM_freeN(key);
}

void llvm_free()
{
	if (theEngine) {
		delete theEngine;
		theEngine = NULL;
	}
	
	for (int opt_level = 0; opt_level < 3; ++opt_level) {
		if (theModulePassMgr[opt_level])
			delete theModulePassMgr[opt_level];
		if (theFunctionPassMgr[opt_level])
			delete theFunctionPassMgr[opt_level];
	}
	
	BLI_ghash_free(extern_functions, extern_functions_keyfree, NULL);
}

llvm::ExecutionEngine *llvm_execution_engine()
{
	return theEngine;
}

void llvm_register_external_function(const string &name, void *func)
{
	char *namebuf = BLI_strdup(name.c_str());
	BLI_ghash_insert(extern_functions, namebuf, func);
}

bool llvm_has_external_function(const string &name)
{
	return BLI_ghash_haskey(extern_functions, name.c_str());
}

void llvm_optimize_module(llvm::Module *mod, int opt_level)
{
	using namespace llvm;
	
	BLI_assert(opt_level >= 0 && opt_level <= 3 && "Invalid optimization level (must be between 0 and 3)");
	
	PassManager &MPM = *theModulePassMgr[opt_level];
	
	mod->setDataLayout(theEngine->getDataLayout());
	mod->setTargetTriple(theEngine->getTargetMachine()->getTargetTriple());
	
	MPM.run(*mod);
}

void llvm_optimize_function(llvm::Function *func, int opt_level)
{
	using namespace llvm;
	
	BLI_assert(opt_level >= 0 && opt_level <= 3 && "Invalid optimization level (must be between 0 and 3)");
	
	FunctionPassManager &FPM = *theFunctionPassMgr[opt_level];
	
	FPM.run(*func);
}

} /* namespace llvm */

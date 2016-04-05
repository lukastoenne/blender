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

#include <map>

extern "C" {
#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_path_util.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.h"
}

#include "llvm_engine.h"
#include "llvm_headers.h"

namespace blenvm {

typedef std::map<string, llvm::Module*> ModuleMap;

static llvm::ExecutionEngine *theEngine = NULL;
static llvm::Module *theModule = NULL;
static ModuleMap theModules;

//static ModuleMap theModules;

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
//	builder.setMCJITMemoryManager(std::unique_ptr<HelpingMemoryManager>(new HelpingMemoryManager(this)));
	
	ExecutionEngine *engine = builder.create();
	if (!engine) {
		printf("Could not create ExecutionEngine: %s\n", error.c_str());
	}
	return engine;
}

void llvm_init()
{
	using namespace llvm;
	
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	InitializeNativeTargetAsmParser();
	
	BLI_assert(theEngine == NULL);
	theEngine = create_execution_engine();
	
	/* load modules */
	llvm_load_all_modules("", false);
}

void llvm_free()
{
	llvm_unload_all_modules();
	
	if (theEngine) {
		delete theEngine;
		theEngine = NULL;
	}
}

llvm::ExecutionEngine *llvm_execution_engine()
{
	return theEngine;
}

bool llvm_function_is_external(const llvm::Function *func)
{
	return func->hasFnAttribute("name");
}

llvm::Function *llvm_find_external_function(llvm::Module *mod, const string &name)
{
	using namespace llvm;
	
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

string llvm_get_external_function_name(llvm::Function *func)
{
	if (func->hasFnAttribute("name"))
		return func->getFnAttribute("name").getValueAsString().str();
	else
		return func->getName().str();
}

#if 0
/* Based on
 * http://homes.cs.washington.edu/~bholt/posts/llvm-quick-tricks.html
 */
static void llvm_parse_function_annotations(llvm::Module *mod)
{
	using namespace llvm;
	
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
#endif

void llvm_load_module(const string &modfile, const string &modname)
{
	using namespace llvm;
	
	printf("Loading module '%s'\n", modfile.c_str());
	LLVMContext &llvmctx = getGlobalContext();
	SMDiagnostic err;
	
	Module *mod = getLazyIRFileModule(modfile, err, llvmctx);
//	Module *mod = ParseIRFile(modfile, err, llvmctx);
	if (!mod) {
		err.print(modfile.c_str(), errs());
		return;
	}
	
	printf("Module Functions for '%s'\n", mod->getModuleIdentifier().c_str());
	for (Module::FunctionListType::const_iterator it = mod->getFunctionList().begin(); it != mod->getFunctionList().end(); ++it) {
		const Function &func = *it;
		printf("    %s\n", func.getName().str().c_str());
		
//		func.dump();
//		printf("++++++++++++++++++++++++++++++++++\n");
	}
	
	/* XXX this code was used to identify special node functions,
	 * similar to the "shader" keyword in OSL.
	 * Not sure if needed eventually.
	 */
//	bjit_parse_function_annotations(mod);
	mod->setModuleIdentifier(modname);
	
	verifyModule(*mod, &outs());
	
	theEngine->addModule(mod);
	theModules[mod->getModuleIdentifier()] = mod;
}

void llvm_load_all_modules(const string &modpath, bool reload)
{
	using namespace llvm;
	
	string path = modpath;
	if (path.empty())
		path = string(BKE_appdir_folder_id(BLENDER_SYSTEM_SCRIPTS, "llvm/modules/"));
	if (path.empty())
		return;
	
	if (reload) {
		llvm_unload_all_modules();
	}
	
	struct direntry *dir;
	int totfile = BLI_filelist_dir_contents(path.c_str(), &dir);
	for (int i = 0; i < totfile; ++i) {
		if ((dir[i].type & S_IFREG)) {
			const char *filename = dir[i].relname;
			const char *filepath = dir[i].path;
			
			if (BLI_testextensie(filename, ".ll")) {
				/* found a potential llvm IR module, try parsing it */
				llvm_load_module(filepath, filename);
			}
		}
	}
	BLI_filelist_free(dir, totfile);
}

void llvm_unload_all_modules()
{
	using namespace llvm;
	
	// TODO
	theModules.clear();
}

/* links the module to all other modules in the module map */
void llvm_link_module_full(llvm::Module *mod)
{
	using namespace llvm;
	
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
	
//	PassManager *pm = create_pass_manager();
//	pm->run(*mod);
//	delete pm;
	
	theEngine->finalizeObject();
}

} /* namespace llvm */

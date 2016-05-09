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

extern "C" {
#include "BLI_utildefines.h"
}

#include "llvm_engine.h"
#include "llvm_headers.h"
#include "llvm_modules.h"

namespace blenvm {

static llvm::ExecutionEngine *theEngine = NULL;
static llvm::Module *theModule = NULL;

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

} /* namespace llvm */

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

/** \file blender/blenvm/llvm/llvm_modules.cc
 *  \ingroup llvm
 */

#include <map>
#include <sstream>

extern "C" {
#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_path_util.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.h"
}

#include "node_graph.h"

#include "llvm_engine.h"
#include "llvm_modules.h"
#include "llvm_headers.h"
#include "llvm_types.h"

#include "util_math.h"
#include "util_opcode.h"

#include "modules.h"

namespace blenvm {

typedef std::map<string, llvm::Module*> ModuleMap;

static ModuleMap theModules;

#ifdef WITH_BLENVM_IRMODULES
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
			
			if (e->getOperand(0)->getOperand(0)->getValueID() == Value::FunctionVal) {
				Function *fn = static_cast<Function*>(e->getOperand(0)->getOperand(0));
				fn->addFnAttr("name", anno); /* add function annotation */
			}
		}
	}
}

static bool llvm_function_has_external_name(const llvm::Function *func)
{
	return func->hasFnAttribute("name");
}

static string llvm_function_get_external_name(const llvm::Function *func)
{
	if (func->hasFnAttribute("name"))
		return func->getFnAttribute("name").getValueAsString().str();
	else
		return func->getName().str();
}
#endif

llvm::Function *llvm_find_external_function(llvm::Module *mod, const string &name)
{
	using namespace llvm;
	
#ifdef WITH_BLENVM_IRMODULES
	for (Module::FunctionListType::iterator it = mod->getFunctionList().begin(); it != mod->getFunctionList().end(); ++it) {
		Function *func = &(*it);
		if (func->hasFnAttribute("name")) {
			std::string value = func->getFnAttribute("name").getValueAsString();
			if (value == name)
				return func;
		}
	}
#else
	return mod->getFunction(name);
#endif
	
	return NULL;
}

void llvm_load_module(const string &modfile, const string &modname)
{
	using namespace llvm;
	
#ifdef WITH_BLENVM_IRMODULES
	printf("Loading module '%s'\n", modfile.c_str());
	LLVMContext &llvmctx = getGlobalContext();
	SMDiagnostic err;
	
	Module *mod = getLazyIRFileModule(modfile, err, llvmctx);
//	Module *mod = ParseIRFile(modfile, err, llvmctx);
	if (!mod) {
		err.print(modfile.c_str(), errs());
		return;
	}
	
	llvm_parse_function_annotations(mod);
	mod->setModuleIdentifier(modname);
	
	verifyModule(*mod, &outs());
	
	llvm_execution_engine()->addModule(mod);
	theModules[mod->getModuleIdentifier()] = mod;
	
	printf("Module Functions for '%s'\n", mod->getModuleIdentifier().c_str());
	for (Module::FunctionListType::const_iterator it = mod->getFunctionList().begin(); it != mod->getFunctionList().end(); ++it) {
		const Function &func = *it;
		
		if (!llvm_function_has_external_name(&func))
			continue;
		printf("    %s\n", llvm_function_get_external_name(&func).c_str());
		
//		printf("    %s\n", func.getName().str().c_str());
		
//		func.dump();
//		printf("++++++++++++++++++++++++++++++++++\n");
	}
#else
	UNUSED_VARS(modfile, modname);
#endif
}

void llvm_load_all_modules(const string &modpath, bool reload)
{
	using namespace llvm;
	
	if (reload) {
		llvm_unload_all_modules();
	}
	
#ifdef WITH_BLENVM_IRMODULES
	string path = modpath;
	if (path.empty()) {
		const char *moddir = BKE_appdir_folder_id(BLENDER_SYSTEM_SCRIPTS, "llvm/modules/");
		path = (moddir != NULL ? string(moddir) : "");
	}
	if (path.empty())
		return;
	
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
#else
	UNUSED_VARS(modpath);
#endif
}

void llvm_unload_all_modules()
{
	using namespace llvm;
	
	// TODO
	theModules.clear();
}

inline string dummy_input_name(const NodeType *nodetype, int index)
{
	std::stringstream ss;
	ss << "I" << index << "_" << nodetype->name();
	return ss.str();
}

inline string dummy_output_name(const NodeType *nodetype, int index)
{
	std::stringstream ss;
	ss << "O" << index << "_" << nodetype->name();
	return ss.str();
}

static void define_function_OP_VALUE_SINGLE(llvm::LLVMContext &context, llvm::BasicBlock *block,
                                            llvm::Value *result, llvm::Value *value)
{
	using namespace llvm;
	
	IRBuilder<> builder(context);
	builder.SetInsertPoint(block);
	
	builder.CreateStore(value, result);
	
	builder.CreateRetVoid();
}

static void define_function_OP_VALUE_AGGREGATE(llvm::LLVMContext &context, llvm::BasicBlock *block,
                                               llvm::Value *result, llvm::Value *value, size_t size)
{
	using namespace llvm;
	
	IRBuilder<> builder(context);
	builder.SetInsertPoint(block);
	
	Value *size_v = ConstantInt::get(context, APInt(32, size));
	builder.CreateMemCpy(result, value, size_v, 0);
	
	builder.CreateRetVoid();
}

static bool define_internal_function(llvm::LLVMContext &context, OpCode op, llvm::Function *func)
{
	using namespace llvm;
	
	std::vector<Value*> args;
	args.reserve(func->arg_size());
	for (Function::arg_iterator a = func->arg_begin(); a != func->arg_end(); ++a)
		args.push_back(a);
	
	switch (op) {
		case OP_VALUE_FLOAT:
		case OP_VALUE_INT: {
			BasicBlock *block = BasicBlock::Create(context, "entry", func);
			define_function_OP_VALUE_SINGLE(context, block, args[0], args[1]);
			return true;
		}
		case OP_VALUE_FLOAT3: {
			BasicBlock *block = BasicBlock::Create(context, "entry", func);
			define_function_OP_VALUE_AGGREGATE(context, block, args[0], args[1], sizeof(float3));
			return true;
		}
		case OP_VALUE_FLOAT4: {
			BasicBlock *block = BasicBlock::Create(context, "entry", func);
			define_function_OP_VALUE_AGGREGATE(context, block, args[0], args[1], sizeof(float4));
			return true;
		}
		case OP_VALUE_MATRIX44: {
			BasicBlock *block = BasicBlock::Create(context, "entry", func);
			define_function_OP_VALUE_AGGREGATE(context, block, args[0], args[1], sizeof(matrix44));
			return true;
		}
		
		default:
			return false;
	}
}

static void define_node_function(llvm::LLVMContext &context, llvm::Module *mod,
                                 OpCode op, const NodeType *nodetype,
                                 void *funcptr)
{
	using namespace llvm;
	
	std::vector<Type *> input_types, output_types;
	for (int i = 0; i < nodetype->num_inputs(); ++i) {
		const NodeInput *input = nodetype->find_input(i);
		Type *type = llvm_create_value_type(context, dummy_input_name(nodetype, i), &input->typedesc);
		if (type == NULL)
			break;
		if (llvm_use_argument_pointer(&input->typedesc))
			type = type->getPointerTo();
		input_types.push_back(type);
	}
	for (int i = 0; i < nodetype->num_outputs(); ++i) {
		const NodeOutput *output = nodetype->find_output(i);
		Type *type = llvm_create_value_type(context, dummy_output_name(nodetype, i), &output->typedesc);
		if (type == NULL)
			break;
		output_types.push_back(type);
	}
	if (input_types.size() != nodetype->num_inputs() ||
	    output_types.size() != nodetype->num_outputs()) {
		/* some arguments could not be handled */
		return;
	}
	
	FunctionType *functype = llvm_create_node_function_type(context, input_types, output_types);
	
	Function *func = Function::Create(functype, Function::ExternalLinkage, nodetype->name(), mod);
	
//	printf("Declared function for node type '%s':\n", nodetype->name().c_str());
//	func->dump();
	
	bool has_internal_impl = define_internal_function(context, op, func);
	if (!has_internal_impl) {
		/* register implementation of the function */
		llvm_execution_engine()->addGlobalMapping(func, funcptr);
	}
}

void llvm_define_node_functions()
{
	using namespace llvm;
	
	LLVMContext &context = getGlobalContext();
	
	Module *mod = new llvm::Module("nodes", context);
	
#define DEF_OPCODE(op) \
	{ \
		const NodeType *nodetype = NodeGraph::find_node_type(STRINGIFY(op)); \
		if (nodetype != NULL) { \
			define_node_function(context, mod, OP_##op, nodetype, (void*)(intptr_t)modules::op); \
		} \
	}
	
	BVM_DEFINE_OPCODES
	
#undef DEF_OPCODE
	
	llvm_execution_engine()->addModule(mod);
	theModules[mod->getModuleIdentifier()] = mod;
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
	
	llvm_execution_engine()->finalizeObject();
}

} /* namespace llvm */

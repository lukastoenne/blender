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

/** \file blender/blenjit/intern/codegen.cpp
 *  \ingroup bjit
 */

#include <map>
#include <string>

#include "bjit_intern.h"
#include "bjit_llvm.h"
#include "BJIT_forcefield.h"

namespace bjit {

using namespace llvm;

#if 0
typedef std::map<std::string, Value *> SocketValueMap;
typedef std::map<std::string, CallInst *> NodeInstanceMap;

static inline Value *find_value(const SocketValueMap &values, const std::string &key)
{
	SocketValueMap::const_iterator it = values.find(key);
	if (it != values.end())
		return it->second;
	else
		return NULL;
}

static CallInst *gen_node_function_call(IRBuilder<> &builder, Module *module, const std::string &funcname,
                                        const SocketValueMap &inputs, const SocketValueMap &outputs)
{
	/* call evaluation function */
	Function *evalfunc = bjit_find_function(module, funcname);
	assert(evalfunc != NULL);
	
	std::vector<Value *> args;
	Function::ArgumentListType::iterator it = evalfunc->arg_begin();
	for (; it != evalfunc->arg_end(); ++it) {
		Argument *arg = it;
		
		Value *input_value = find_value(inputs, arg->getName());
		Value *output_value = find_value(outputs, arg->getName());
		/* exactly one must be defined (xor) */
		assert(input_value != output_value);
		
		if (input_value)
			args.push_back(input_value);
		else
			args.push_back(output_value);
	}
	
	CallInst *call = builder.CreateCall(evalfunc, args);
	
	return call;
}
#endif

#if 0
static void bjit_gen_nodetree_function(NodeInstanceMap &nodes)
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
		
		Value *arg_settings = make_effector_settings(builder, eff);
		Value *arg_eff_result = make_effector_result(builder);
		
		{
			SocketValueMap inputs, outputs;
			inputs["input"] = arg_input;
			inputs["settings"] = arg_settings;
			outputs["result"] = arg_eff_result;
			gen_node_function_call(builder, funcname, inputs, outputs);
		}
		
		/* call function to combine results */
		{
			SocketValueMap inputs, outputs;
			inputs["R"] = arg_result;
			inputs["a"] = arg_result;
			outputs["b"] = arg_eff_result;
			gen_node_function_call(builder, "effector_result_combine", inputs, outputs);
		}
	}
	
	builder.CreateRetVoid();
	
	verifyFunction(*func, &outs());
	bjit_finalize_function(theModule, func, 2);
	
//	printf("=== The Module ===\n");
//	theModule->dump();
//	printf("==================\n");
	
	effctx->eval = (EffectorEvalFp)bjit_compile_function(func);
	effctx->eval_data = func;
}
#endif

} /* namespace bjit */

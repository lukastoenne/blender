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

/** \file blender/blenvm/llvm/llvm_codegen.cc
 *  \ingroup llvm
 */

#include <set>
#include <sstream>

#include "node_graph.h"

#include "llvm_codegen.h"
#include "llvm_engine.h"
#include "llvm_function.h"
#include "llvm_headers.h"

/* TypeBuilder specializations for own structs */

namespace llvm {

template <bool xcompile>
class TypeBuilder<blenvm::float3, xcompile> {
public:
	static StructType *get(LLVMContext &context) {
		return StructType::get(
		            TypeBuilder<types::ieee_float, xcompile>::get(context),
		            TypeBuilder<types::ieee_float, xcompile>::get(context),
		            TypeBuilder<types::ieee_float, xcompile>::get(context),
		            NULL);
	}
	
	enum Fields {
		FIELD_X = 0,
		FIELD_Y = 1,
		FIELD_Z = 2,
	};
};

template <bool xcompile>
class TypeBuilder<blenvm::float4, xcompile> {
public:
	static StructType *get(LLVMContext &context) {
		return StructType::get(
		            TypeBuilder<types::ieee_float, xcompile>::get(context),
		            TypeBuilder<types::ieee_float, xcompile>::get(context),
		            TypeBuilder<types::ieee_float, xcompile>::get(context),
		            TypeBuilder<types::ieee_float, xcompile>::get(context),
		            NULL);
	}
	
	enum Fields {
		FIELD_X = 0,
		FIELD_Y = 1,
		FIELD_Z = 2,
		FIELD_W = 3,
	};
};

template <bool xcompile>
class TypeBuilder<blenvm::matrix44, xcompile> {
public:
	static StructType *get(LLVMContext &context) {
		return StructType::get(
		            ArrayType::get(
		                ArrayType::get(
		                    TypeBuilder<types::ieee_float, xcompile>::get(context),
		                    4),
		                4),
		            NULL);
	}
};

} /* namespace llvm */


namespace blenvm {

/* XXX Should eventually declare and reference types by name,
 * rather than storing full TypeDesc in each socket!
 * These functions just provides per-socket type names in the meantime.
 */
static string dummy_type_name(const InputKey &input)
{
	size_t hash = std::hash<const NodeInstance *>()(input.node) ^ std::hash<const NodeInput *>()(input.socket);
	std::stringstream ss;
	ss << "InputType" << (unsigned short)hash;
	return ss.str();
}
static string dummy_type_name(const OutputKey &output)
{
	size_t hash = std::hash<const NodeInstance *>()(output.node) ^ std::hash<const NodeOutput *>()(output.socket);
	std::stringstream ss;
	ss << "OutputType" << (unsigned short)hash;
	return ss.str();
}

LLVMCompiler::LLVMCompiler()
{
}

LLVMCompiler::~LLVMCompiler()
{
}

llvm::LLVMContext &LLVMCompiler::context() const
{
	return llvm::getGlobalContext();
}

llvm::Type *LLVMCompiler::codegen_typedesc(const string &name, const TypeDesc *td)
{
	using namespace llvm;
	
	if (td->is_structure()) {
		const StructSpec *s = td->structure();
		std::vector<Type*> elemtypes;
		for (int i = 0; i < s->num_fields(); ++s) {
			Type *ftype = codegen_typedesc(s->field(i).name, &s->field(i).typedesc);
			elemtypes.push_back(ftype);
		}
		
		StructType *stype = StructType::create(context(), ArrayRef<Type*>(elemtypes), name);
		return stype;
	}
	else {
		switch (td->base_type()) {
			case BVM_FLOAT:
				return TypeBuilder<types::ieee_float, true>::get(context());
			case BVM_FLOAT3:
				return TypeBuilder<float3, true>::get(context());
			case BVM_FLOAT4:
				return TypeBuilder<float4, true>::get(context());
			case BVM_INT:
				return TypeBuilder<types::i<32>, true>::get(context());
			case BVM_MATRIX44:
				return TypeBuilder<matrix44, true>::get(context());
				
			case BVM_STRING:
			case BVM_RNAPOINTER:
			case BVM_MESH:
			case BVM_DUPLIS:
				/* TODO */
				return NULL;
		}
	}
	return NULL;
}

llvm::FunctionType *LLVMCompiler::codegen_node_function_type(const NodeGraph &graph)
{
	using namespace llvm;
	
	std::vector<Type *> input_types, output_types;
	
	for (int i = 0; i < graph.outputs.size(); ++i) {
		const NodeGraph::Output &output = graph.outputs[i];
		Type *type = codegen_typedesc(dummy_type_name(output.key), &output.typedesc);
		output_types.push_back(type);
	}
	StructType *return_type = StructType::get(context(), output_types);
	
	input_types.push_back(PointerType::get(return_type, 0));
	for (int i = 0; i < graph.inputs.size(); ++i) {
		const NodeGraph::Input &input = graph.inputs[i];
		Type *type = codegen_typedesc(dummy_type_name(input.key), &input.typedesc);
//		type = PointerType::get(type, 0);
		input_types.push_back(type);
	}
	
	return FunctionType::get(TypeBuilder<void, true>::get(context()), input_types, false);
}

/* Compile nodes as a simple expression.
 * Every node can be treated as a single statement. Each node is translated
 * into a function call, with regular value arguments. The resulting value is
 * assigned to a variable and can be used for subsequent node function calls.
 */
llvm::BasicBlock *LLVMCompiler::codegen_function_body_expression(const NodeGraph &graph, llvm::Function *func)
{
	using namespace llvm;
	
	IRBuilder<> builder(context());
	
	BasicBlock *entry = BasicBlock::Create(context(), "entry", func);
	builder.SetInsertPoint(entry);
	
	int num_inputs = graph.inputs.size();
	int num_outputs = graph.outputs.size();
	
	OutputValueMap output_values;
	
	Argument *retarg = func->getArgumentList().begin();
	Function::ArgumentListType::iterator it = retarg;
	++it; /* skip return arg */
	for (int i = 0; i < num_inputs; ++i) {
		const NodeGraph::Input &input = graph.inputs[i];
		
		Argument *arg = &(*it++);
		output_values[input.key] = arg;
	}
	
#if 0 // TODO
	NodeRefList sorted_nodes = toposort_nodes(graph);
	for (NodeRefList::iterator it = sorted_nodes.begin(); it != sorted_nodes.end(); ++it) {
		NodeInstance *node = *it;
		
		CallInst *call = codegen_node_function_call(builder, module, node);
		if (!call)
			continue;
	}
#endif
	
	for (int i = 0; i < num_outputs; ++i) {
		const NodeGraph::Output &output = graph.outputs[i];
		
		Value *retptr = builder.CreateStructGEP(retarg, i);
		
//		Value *value = output_values.at(output.key);
//		Value *retval = builder.CreateLoad(value);
//		builder.CreateStore(retval, retptr);
	}
	
	builder.CreateRetVoid();
	
	return entry;
}

llvm::Function *LLVMCompiler::codegen_node_function(const string &name, const NodeGraph &graph, llvm::Module *module)
{
	using namespace llvm;
	
	FunctionType *functype = codegen_node_function_type(graph);
	Function *func = llvm::cast<Function>(module->getOrInsertFunction(name, functype));
	Argument *retarg = func->getArgumentList().begin();
	retarg->addAttr(AttributeSet::get(context(), AttributeSet::ReturnIndex, Attribute::StructRet));
	
	if (func->getArgumentList().size() != graph.inputs.size() + 1) {
		printf("Error: Function has wrong number of arguments for node tree\n");
		return func;
	}
	
	codegen_function_body_expression(graph, func);
	
	return func;
}

FunctionLLVM *LLVMCompiler::compile_function(const string &name, const NodeGraph &graph)
{
	using namespace llvm;
	
	Module *module = new Module(name, context());
	llvm_execution_engine()->addModule(module);
	
	Function *func = codegen_node_function(name, graph, module);
	assert(func != NULL && "codegen_node_function returned NULL!");
	
	printf("=== NODE FUNCTION ===\n");
	func->dump();
	printf("=====================\n");
	
	FunctionPassManager fpm(module);
	PassManagerBuilder builder;
	builder.OptLevel = 0;
	builder.populateFunctionPassManager(fpm);
//	builder.populateModulePassManager(MPM);
	
	fpm.run(*func);
	
	llvm_execution_engine()->finalizeObject();
	
	uint64_t address = llvm_execution_engine()->getFunctionAddress(name);
	BLI_assert(address != 0);
	
	llvm_execution_engine()->removeModule(module);
	delete module;
	
	FunctionLLVM *fn = new FunctionLLVM(address);
	return fn;
}

} /* namespace blenvm */

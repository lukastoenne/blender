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

/* call signature convention in llvm modules:
 * BYVALUE:   Pass struct types directly into (inlined) functions,
 *            Return type is a struct with all outputs, or a single
 *            value if node has just one output.
 * BYPOINTER: Pass arguments as pointers/references.
 *            First function args are output pointers, followed by
 *            inputs const pointers.
 *            This call style is necessary to avoid type coercion by
 *            the clang compiler! See
 *            http://stackoverflow.com/questions/22776391/why-does-clang-coerce-struct-parameters-to-ints
 */
//#define BVM_NODE_CALL_BYVALUE
#define BVM_NODE_CALL_BYPOINTER

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
inline string dummy_type_name(ConstInputKey input)
{
	size_t hash = std::hash<const NodeInstance *>()(input.node) ^ std::hash<const NodeInput *>()(input.socket);
	std::stringstream ss;
	ss << "InputType" << (unsigned short)hash;
	return ss.str();
}
inline string dummy_type_name(ConstOutputKey output)
{
	size_t hash = std::hash<const NodeInstance *>()(output.node) ^ std::hash<const NodeOutput *>()(output.socket);
	std::stringstream ss;
	ss << "OutputType" << (unsigned short)hash;
	return ss.str();
}

LLVMCompiler::LLVMCompiler() :
    m_module(NULL)
{
}

LLVMCompiler::~LLVMCompiler()
{
}

llvm::LLVMContext &LLVMCompiler::context() const
{
	return llvm::getGlobalContext();
}

llvm::StructType *LLVMCompiler::codegen_struct_type(const string &name, const StructSpec *s)
{
	using namespace llvm;
	
	std::vector<Type*> elemtypes;
	for (int i = 0; i < s->num_fields(); ++s) {
		Type *ftype = codegen_type(s->field(i).name, &s->field(i).typedesc);
		elemtypes.push_back(ftype);
	}
	
	return StructType::create(context(), ArrayRef<Type*>(elemtypes), name);
}

llvm::Type *LLVMCompiler::codegen_type(const string &name, const TypeDesc *td)
{
	using namespace llvm;
	
	if (td->is_structure()) {
		return codegen_struct_type(name, td->structure());
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

llvm::Constant *LLVMCompiler::codegen_constant(const NodeValue *node_value)
{
	using namespace llvm;
	
	const TypeDesc &td = node_value->typedesc();
	if (td.is_structure()) {
//		const StructSpec *s = td.structure();
		/* TODO don't have value storage for this yet */
		return NULL;
	}
	else {
		switch (td.base_type()) {
			case BVM_FLOAT: {
				float f;
				node_value->get(&f);
				return ConstantFP::get(context(), APFloat(f));
			}
			case BVM_FLOAT3: {
				StructType *stype = TypeBuilder<float3, true>::get(context());
				
				float3 f;
				node_value->get(&f);
				return ConstantStruct::get(stype,
				                           ConstantFP::get(context(), APFloat(f.x)),
				                           ConstantFP::get(context(), APFloat(f.y)),
				                           ConstantFP::get(context(), APFloat(f.z)),
				                           NULL);
			}
			case BVM_FLOAT4: {
				StructType *stype = TypeBuilder<float4, true>::get(context());
				
				float4 f;
				node_value->get(&f);
				return ConstantStruct::get(stype,
				                           ConstantFP::get(context(), APFloat(f.x)),
				                           ConstantFP::get(context(), APFloat(f.y)),
				                           ConstantFP::get(context(), APFloat(f.z)),
				                           ConstantFP::get(context(), APFloat(f.w)),
				                           NULL);
			}
			case BVM_INT: {
				int i;
				node_value->get(&i);
				return ConstantInt::get(context(), APInt(32, i, true));
			}
			case BVM_MATRIX44: {
				Type *elem_t = TypeBuilder<types::ieee_float, true>::get(context());
				ArrayType *inner_t = ArrayType::get(elem_t, 4);
				ArrayType *outer_t = ArrayType::get(inner_t, 4);
				StructType *matrix_t = StructType::get(outer_t, NULL);
				
				matrix44 m;
				node_value->get(&m);
				Constant *constants[4][4];
				for (int i = 0; i < 4; ++i)
					for (int j = 0; j < 4; ++j)
						constants[i][j] = ConstantFP::get(context(), APFloat(m.data[i][j]));
				Constant *cols[4];
				for (int i = 0; i < 4; ++i)
					cols[i] = ConstantArray::get(inner_t, ArrayRef<Constant*>(constants[i], 4));
				Constant *data = ConstantArray::get(outer_t, ArrayRef<Constant*>(cols, 4));
				return ConstantStruct::get(matrix_t,
				                           data, NULL);
			}
				
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
		Type *type = codegen_type(dummy_type_name(output.key), &output.typedesc);
		output_types.push_back(type);
	}
	StructType *return_type = StructType::get(context(), output_types);
	
	input_types.push_back(PointerType::get(return_type, 0));
	for (int i = 0; i < graph.inputs.size(); ++i) {
		const NodeGraph::Input &input = graph.inputs[i];
		Type *type = codegen_type(dummy_type_name(input.key), &input.typedesc);
//		type = PointerType::get(type, 0);
		input_types.push_back(type);
	}
	
	return FunctionType::get(TypeBuilder<void, true>::get(context()), input_types, false);
}

llvm::CallInst *LLVMCompiler::codegen_node_call(llvm::BasicBlock *block,
                                                const NodeInstance *node,
                                                OutputValueMap &output_values)
{
	using namespace llvm;
	
	IRBuilder<> builder(context());
	builder.SetInsertPoint(block);
	
	/* get evaluation function */
	const std::string &evalname = node->type->name();
	Function *evalfunc = llvm_find_external_function(module(), evalname);
	if (!evalfunc) {
		printf("Could not find node function '%s'\n", evalname.c_str());
		return NULL;
	}
	
	/* function call arguments (including possible return struct if MRV is used) */
	std::vector<Value *> args;
	
	Value *retval = NULL;
#ifdef BVM_NODE_CALL_BYVALUE
	if (evalfunc->hasStructRetAttr()) {
		Argument *retarg = &(*evalfunc->getArgumentList().begin());
		retval = builder.CreateAlloca(retarg->getType()->getPointerElementType());
		
		args.push_back(retval);
	}
#endif
#ifdef BVM_NODE_CALL_BYPOINTER
	for (int i = 0; i < node->num_outputs(); ++i) {
		ConstOutputKey output = node->output(i);
		Type *output_type = codegen_type(dummy_type_name(output), &output.socket->typedesc);
		AllocaInst *outputmem = builder.CreateAlloca(output_type);
		
		Type *arg_type = evalfunc->getFunctionType()->getParamType(args.size());
		Value *value = builder.CreatePointerBitCastOrAddrSpaceCast(outputmem, arg_type);
		args.push_back(value);
		
		/* use as node output values */
		bool ok = output_values.insert(OutputValuePair(output, outputmem)).second;
		BLI_assert(ok && "Value for node output already defined!");
	}
#endif
	
	/* set input arguments */
	for (int i = 0; i < node->num_inputs(); ++i) {
		ConstInputKey input = node->input(i);
		
		switch (input.value_type()) {
			case INPUT_CONSTANT: {
				/* create storage for the global value */
				Constant *constval = codegen_constant(input.value());
				AllocaInst *constmem = builder.CreateAlloca(constval->getType());
				builder.CreateStore(constval, constmem);
				
				/* use the pointer as function argument */
				Type *arg_type = evalfunc->getFunctionType()->getParamType(args.size());
				Value *value = builder.CreatePointerBitCastOrAddrSpaceCast(constmem, arg_type);
				args.push_back(value);
				break;
			}
			case INPUT_EXPRESSION:
				args.push_back(output_values.at(input.link()));
				break;
			case INPUT_VARIABLE:
				/* TODO */
				BLI_assert(false && "Variable inputs not supported yet!");
				break;
		}
	}
	
	CallInst *call = builder.CreateCall(evalfunc, args);
	if (!retval)
		retval = call;
	
	if (node->num_outputs() == 0) {
		/* nothing to return */
	}
	else {
#ifdef BVM_NODE_CALL_BYVALUE
		if (evalfunc->hasStructRetAttr()) {
			for (int i = 0; i < node->num_outputs(); ++i) {
				ConstOutputKey output = node->output(i);
				Value *value = builder.CreateStructGEP(retval, i);
				if (!value) {
					printf("Error: no output value defined for '%s':'%s'\n", node->name.c_str(), output.socket->name.c_str());
				}
				
				/* use as node output values */
				bool ok = output_values.insert(OutputValuePair(output, value)).second;
				BLI_assert(ok && "Value for node output already defined!");
			}
		}
		else {
			BLI_assert(node->num_outputs() == 1);
			ConstOutputKey output = node->output(0);
			/* use as node output values */
			bool ok = output_values.insert(OutputValuePair(output, retval)).second;
			BLI_assert(ok && "Value for node output already defined!");
		}
#endif
	}
	
	return call;
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
	
	BasicBlock *block = BasicBlock::Create(context(), "entry", func);
	builder.SetInsertPoint(block);
	
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
	
	OrderedNodeSet nodes;
	for (NodeGraph::NodeInstanceMap::const_iterator it = graph.nodes.begin(); it != graph.nodes.end(); ++it)
		nodes.insert(it->second);
	
	for (OrderedNodeSet::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
		const NodeInstance &node = **it;
		
		CallInst *call = codegen_node_call(block, &node, output_values);
		if (!call)
			continue;
	}
	
	for (int i = 0; i < num_outputs; ++i) {
		const NodeGraph::Output &output = graph.outputs[i];
		
		Value *retptr = builder.CreateStructGEP(retarg, i);
		
		Value *value = output_values.at(output.key);
		Value *retval = builder.CreateLoad(value);
		builder.CreateStore(retval, retptr);
	}
	
	builder.CreateRetVoid();
	
	return block;
}

llvm::Function *LLVMCompiler::codegen_node_function(const string &name, const NodeGraph &graph)
{
	using namespace llvm;
	
	FunctionType *functype = codegen_node_function_type(graph);
	Function *func = Function::Create(functype, Function::ExternalLinkage, name, module());
	Argument *retarg = func->getArgumentList().begin();
	retarg->addAttr(AttributeSet::get(context(), AttributeSet::ReturnIndex, Attribute::StructRet));
	
	if (func->getArgumentList().size() != graph.inputs.size() + 1) {
		printf("Error: Function has wrong number of arguments for node tree\n");
		return func;
	}
	
	codegen_function_body_expression(graph, func);
	
	return func;
}

void LLVMCompiler::optimize_function(llvm::Function *func, int opt_level)
{
	using namespace llvm;
	using legacy::FunctionPassManager;
	
	FunctionPassManager FPM(module());
	
	PassManagerBuilder builder;
	builder.OptLevel = opt_level;
	builder.populateFunctionPassManager(FPM);
	
	FPM.run(*func);
}

FunctionLLVM *LLVMCompiler::compile_function(const string &name, const NodeGraph &graph)
{
	using namespace llvm;
	
	m_module = new Module(name, context());
	llvm_link_module_full(m_module);
	
	Function *func = codegen_node_function(name, graph);
	BLI_assert(m_module->getFunction(name) && "Function not registered in module!");
	BLI_assert(func != NULL && "codegen_node_function returned NULL!");
	
	optimize_function(func, 2);
	
	printf("=== NODE FUNCTION ===\n");
	fflush(stdout);
	func->dump();
	printf("=====================\n");
	fflush(stdout);
	
	verifyFunction(*func, &outs());
	verifyModule(*m_module, &outs());
	
	/* Note: Adding module to exec engine before creating the function prevents compilation! */
	llvm_execution_engine()->addModule(m_module);
	uint64_t address = llvm_execution_engine()->getFunctionAddress(name);
	BLI_assert(address != 0);
	
	llvm_execution_engine()->removeModule(m_module);
	delete m_module;
	m_module = NULL;
	
	FunctionLLVM *fn = new FunctionLLVM(address);
	return fn;
}

} /* namespace blenvm */

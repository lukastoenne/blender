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

#include <cstdio>
#include <set>
#include <sstream>

#include "node_graph.h"

#include "llvm_codegen.h"
#include "llvm_engine.h"
#include "llvm_function.h"
#include "llvm_headers.h"
#include "llvm_modules.h"
#include "llvm_types.h"

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
#define BVM_NODE_CALL_BYVALUE
//#define BVM_NODE_CALL_BYPOINTER


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

LLVMCompilerBase::LLVMCompilerBase() :
    m_module(NULL)
{
}

LLVMCompilerBase::~LLVMCompilerBase()
{
}

llvm::LLVMContext &LLVMCompilerBase::context() const
{
	return llvm::getGlobalContext();
}

void LLVMCompilerBase::create_module(const string &name)
{
	m_module = new llvm::Module(name, context());
}

void LLVMCompilerBase::destroy_module()
{
	delete m_module;
	m_module = NULL;
}

llvm::Constant *LLVMCompilerBase::codegen_constant(const NodeValue *node_value)
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

llvm::CallInst *LLVMCompilerBase::codegen_node_call(llvm::BasicBlock *block,
                                                    const NodeInstance *node,
                                                    OutputValueMap &output_values)
{
	using namespace llvm;
	
	IRBuilder<> builder(context());
	builder.SetInsertPoint(block);
	
	/* get evaluation function */
	const std::string &evalname = node->type->name();
	Function *evalfunc = llvm_find_external_function(module(), evalname);
	BLI_assert(evalfunc != NULL && "Could not find node function!");
	
	/* function call arguments (including possible return struct if MRV is used) */
	std::vector<Value *> args;
	
	Value *retval = NULL;
#ifdef BVM_NODE_CALL_BYVALUE
	if (evalfunc->hasStructRetAttr()) {
		Argument *retarg = &(*evalfunc->getArgumentList().begin());
		retval = builder.CreateAlloca(retarg->getType()->getPointerElementType());
		
		args.push_back(retval);
	}
	else {
		for (int i = 0; i < node->num_outputs(); ++i) {
			ConstOutputKey output = node->output(i);
			Type *type = llvm_create_value_type(context(), dummy_type_name(output), &output.socket->typedesc);
			Value *value = builder.CreateAlloca(type);
			
			args.push_back(value);
			
			/* use as node output values */
			bool ok = output_values.insert(OutputValuePair(output, value)).second;
			BLI_assert(ok && "Value for node output already defined!");
		}
	}
#endif
#ifdef BVM_NODE_CALL_BYPOINTER
	for (int i = 0; i < node->num_outputs(); ++i) {
		ConstOutputKey output = node->output(i);
		Type *output_type = create_value_type(context(), dummy_type_name(output), &output.socket->typedesc);
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
				Constant *cvalue = codegen_constant(input.value());
				
#ifdef BVM_NODE_CALL_BYVALUE
				Value *value;
				if (llvm_use_argument_pointer(&input.socket->typedesc)) {
					AllocaInst *pvalue = builder.CreateAlloca(cvalue->getType());
					builder.CreateStore(cvalue, pvalue);
					value = pvalue;
				}
				else {
					value = cvalue;
				}
#endif
#ifdef BVM_NODE_CALL_BYPOINTER
				/* use the pointer as function argument */
				Type *arg_type = evalfunc->getFunctionType()->getParamType(args.size());
				Value *value = builder.CreatePointerBitCastOrAddrSpaceCast(constmem, arg_type);
#endif
				args.push_back(value);
				break;
			}
			case INPUT_EXPRESSION: {
				Value *pvalue = output_values.at(input.link());
#ifdef BVM_NODE_CALL_BYVALUE
				Value *value;
				if (llvm_use_argument_pointer(&input.socket->typedesc)) {
					value = pvalue;
				}
				else {
					value = builder.CreateLoad(pvalue);
				}
				
#endif
#ifdef BVM_NODE_CALL_BYPOINTER
				Type *arg_type = evalfunc->getFunctionType()->getParamType(args.size());
				Value *value = builder.CreatePointerBitCastOrAddrSpaceCast(valuemem, arg_type);
#endif
				args.push_back(value);
				break;
			}
			case INPUT_VARIABLE: {
				/* TODO */
				BLI_assert(false && "Variable inputs not supported yet!");
				break;
			}
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
#if 0 /* XXX currently we always use result pointers */
		else {
			BLI_assert(node->num_outputs() == 1);
			ConstOutputKey output = node->output(0);
			/* use as node output values */
			bool ok = output_values.insert(OutputValuePair(output, retval)).second;
			BLI_assert(ok && "Value for node output already defined!");
		}
#endif
#endif
	}
	
	return call;
}

/* Compile nodes as a simple expression.
 * Every node can be treated as a single statement. Each node is translated
 * into a function call, with regular value arguments. The resulting value is
 * assigned to a variable and can be used for subsequent node function calls.
 */
llvm::BasicBlock *LLVMCompilerBase::codegen_function_body_expression(const NodeGraph &graph, llvm::Function *func)
{
	using namespace llvm;
	
	IRBuilder<> builder(context());
	
	BasicBlock *block = BasicBlock::Create(context(), "entry", func);
	builder.SetInsertPoint(block);
	
	int num_inputs = graph.inputs.size();
	int num_outputs = graph.outputs.size();
	
	OutputValueMap output_values;
	
	Argument *retarg = func->getArgumentList().begin();
	{
		Function::ArgumentListType::iterator it = retarg;
//		++it; /* skip return arg */
		for (int i = 0; i < num_outputs; ++i) {
			++it; /* skip output arguments */
		}
		for (int i = 0; i < num_inputs; ++i) {
			const NodeGraph::Input &input = graph.inputs[i];
			
			Argument *arg = &(*it++);
			output_values[input.key] = arg;
		}
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
	
#if 0 /* XXX unused, struct return value construction */
	for (int i = 0; i < num_outputs; ++i) {
		const NodeGraph::Output &output = graph.outputs[i];
		
		Value *retptr = builder.CreateStructGEP(retarg, i);
		
		Value *value = output_values.at(output.key);
		Value *retval = builder.CreateLoad(value);
		builder.CreateStore(retval, retptr);
	}
#else
	{
		Function::ArgumentListType::iterator it = func->getArgumentList().begin();
//		++it; /* skip return arg */
		for (int i = 0; i < num_outputs; ++i) {
			const NodeGraph::Output &output = graph.outputs[i];
			Value *value = output_values.at(output.key);
			Argument *arg = &(*it++);
			Value *rvalue = builder.CreateLoad(value);
			builder.CreateStore(rvalue, arg);
		}
	}
#endif
	
	builder.CreateRetVoid();
	
	return block;
}

llvm::Function *LLVMCompilerBase::codegen_node_function(const string &name, const NodeGraph &graph)
{
	using namespace llvm;
	
	std::vector<llvm::Type*> input_types, output_types;
	for (int i = 0; i < graph.inputs.size(); ++i) {
		const NodeGraph::Input &input = graph.inputs[i];
		Type *type = llvm_create_value_type(context(), dummy_type_name(input.key), &input.typedesc);
		if (llvm_use_argument_pointer(&input.typedesc))
			type = type->getPointerTo();
		input_types.push_back(type);
	}
	for (int i = 0; i < graph.outputs.size(); ++i) {
		const NodeGraph::Output &output = graph.outputs[i];
		Type *type = llvm_create_value_type(context(), dummy_type_name(output.key), &output.typedesc);
		output_types.push_back(type);
	}
	FunctionType *functype = llvm_create_node_function_type(context(), input_types, output_types);
	
	Function *func = Function::Create(functype, Function::ExternalLinkage, name, module());
#if 0 /* unused, declare struct return value */
	Argument *retarg = func->getArgumentList().begin();
	retarg->addAttr(AttributeSet::get(context(), AttributeSet::ReturnIndex, Attribute::StructRet));
#endif
	
	BLI_assert(func->getArgumentList().size() == graph.inputs.size() + graph.outputs.size() &&
	           "Error: Function has wrong number of arguments for node tree\n");
	
	codegen_function_body_expression(graph, func);
	
	return func;
}

void LLVMCompilerBase::optimize_function(llvm::Function *func, int opt_level)
{
	using namespace llvm;
	using legacy::FunctionPassManager;
	using legacy::PassManager;
	
	FunctionPassManager FPM(module());
	PassManager MPM;
	
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
	
	MPM.run(*module());
	FPM.run(*func);
}

/* ------------------------------------------------------------------------- */

FunctionLLVM *LLVMCompiler::compile_function(const string &name, const NodeGraph &graph, int opt_level)
{
	using namespace llvm;
	
	create_module(name);
	llvm_link_module_full(module());
	
	Function *func = codegen_node_function(name, graph);
	BLI_assert(module()->getFunction(name) && "Function not registered in module!");
	BLI_assert(func != NULL && "codegen_node_function returned NULL!");
	
	BLI_assert(opt_level >= 0 && opt_level <= 3 && "Invalid optimization level (must be between 0 and 3)");
	optimize_function(func, opt_level);
	
#if 0
	printf("=== NODE FUNCTION ===\n");
	fflush(stdout);
//	func->dump();
	module()->dump();
	printf("=====================\n");
	fflush(stdout);
#endif
	
	verifyFunction(*func, &outs());
	verifyModule(*module(), &outs());
	
	/* Note: Adding module to exec engine before creating the function prevents compilation! */
	llvm_execution_engine()->addModule(module());
	uint64_t address = llvm_execution_engine()->getFunctionAddress(name);
	BLI_assert(address != 0);
	
	llvm_execution_engine()->removeModule(module());
	destroy_module();
	
	FunctionLLVM *fn = new FunctionLLVM(address);
	return fn;
}

/* ------------------------------------------------------------------------- */

/* llvm::ostream that writes to a FILE. */
class file_ostream : public llvm::raw_ostream {
	FILE *file;
	
	/* write_impl - See raw_ostream::write_impl. */
	void write_impl(const char *ptr, size_t size) override {
		fwrite(ptr, sizeof(char), size, file);
	}
	
	/* current_pos - Return the current position within the stream, not
	 * counting the bytes currently in the buffer.
	 */
	uint64_t current_pos() const override { return ftell(file); }
	
public:
	explicit file_ostream(FILE *f) : file(f) {}
	~file_ostream() {
		fflush(file);
	}
};

class debug_assembly_annotation_writer : public llvm::AssemblyAnnotationWriter
{
	/* add implementation here if needed */
};

void DebugLLVMCompiler::compile_function(const string &name, const NodeGraph &graph, int opt_level, FILE *file)
{
	using namespace llvm;
	
	create_module(name);
	llvm_link_module_full(module());
	
	Function *func = codegen_node_function(name, graph);
	BLI_assert(module()->getFunction(name) && "Function not registered in module!");
	BLI_assert(func != NULL && "codegen_node_function returned NULL!");
	
	BLI_assert(opt_level >= 0 && opt_level <= 3 && "Invalid optimization level (must be between 0 and 3)");
	optimize_function(func, opt_level);
	
	file_ostream stream(file);
	debug_assembly_annotation_writer aaw;
	module()->print(stream, &aaw);
	
	destroy_module();
}

} /* namespace blenvm */

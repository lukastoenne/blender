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

/** \file bvm_codegen.cc
 *  \ingroup bvm
 */

#include <cstdio>
#include <set>

#include "bvm_codegen.h"
#include "bvm_eval.h"
#include "bvm_function.h"
#include "bvm_nodegraph.h"

namespace bvm {

Compiler::Compiler()
{
	stack_users.resize(BVM_STACK_SIZE, 0);
}

Compiler::~Compiler()
{
}

void Compiler::get_local_args(const NodeGraph &graph, const NodeInstance *node, OutputSet &local_args)
{
	if (!node->type->is_kernel_node())
		return;
	
	for (int i = 0; i < node->num_outputs(); ++i) {
		const NodeOutput *output = node->type->find_output(i);
		
		if (output->value_type == OUTPUT_LOCAL) {
			const NodeGraph::Input *graph_input = graph.get_input(output->name);
			assert(graph_input);
			
			if (graph_input->key) {
				local_args.insert(graph_input->key);
			}
		}
	}
}

static bool is_arg_node(const NodeInstance *node, const OutputSet &args)
{
	for (int i = 0; i < node->num_outputs(); ++i) {
		ConstOutputKey output = node->output(i);
		if (args.find(output) != args.end())
			return true;
	}
	return false;
}

static void count_output_users(const NodeGraph &graph,
                               BasicBlock &block)
{
	block.output_users.clear();
	for (OrderedNodeSet::const_iterator it = block.nodes.begin(); it != block.nodes.end(); ++it) {
		const NodeInstance *node = *it;
		
		for (int i = 0; i < node->num_outputs(); ++i) {
			ConstOutputKey key = node->output(i);
			block.output_users[key] = 0;
		}
	}
	
	for (OrderedNodeSet::const_iterator it = block.nodes.begin(); it != block.nodes.end(); ++it) {
		const NodeInstance *node = *it;
		
		/* note: pass nodes are normally removed, but can exist for debugging purposes */
		if (node->type->is_pass_node())
			continue;
		
		for (int i = 0; i < node->num_inputs(); ++i) {
			if (node->link(i)) {
				block.output_users[node->link(i)] += 1;
			}
		}
	}
	
	/* inputs are defined externally, they should be retained during evaluation */
	for (NodeGraph::InputList::const_iterator it = graph.inputs.begin(); it != graph.inputs.end(); ++it) {
		const NodeGraph::Input &input = *it;
		block.output_users[input.key] += 1;
	}
	
	/* outputs are passed on to the caller, who is responsible for freeing them */
	for (NodeGraph::OutputList::const_iterator it = graph.outputs.begin(); it != graph.outputs.end(); ++it) {
		const NodeGraph::Output &output = *it;
		block.output_users[output.key] += 1;
	}
}

bool Compiler::add_block_node(const NodeGraph &graph, const NodeInstance *node, const OutputSet &block_args,
                              BasicBlock &block, int depth)
{
	bool is_block_node = false; /* determines if the node is part of the block */
	
	is_block_node |= is_arg_node(node, block_args);
	
	OutputSet local_args;
	get_local_args(graph, node, local_args);
	
	for (int i = 0; i < node->num_inputs(); ++i) {
		ConstInputKey input = node->input(i);
		if (input.is_constant()) {
			if (depth == 0)
				is_block_node |= true;
		}
		else if (input.is_expression()) {
			if (depth == 0)
				is_block_node |= parse_expression_block(graph, input, block_args, local_args, block, depth);
		}
		else {
			ConstOutputKey output = input.link();
			if (output) {
				is_block_node |= add_block_node(graph, output.node, block_args, block, depth);
			}
		}
	}
	
	if (is_block_node) {
		block.nodes.insert(node);
		return true;
	}
	else
		return false;
}

bool Compiler::parse_expression_block(const NodeGraph &graph, const ConstInputKey &input,
                                      const OutputSet &block_args, const OutputSet &local_args,
                                      BasicBlock &block, int depth)
{
	ConstOutputKey output = input.link();
	if (!output)
		return false;
	const NodeInstance *node = output.node;
	
	bool is_block_node = false;
	
	/* generate a local block for the input expression */
	BasicBlock &expr_block = block.expression_blocks[input];
	
	add_block_node(graph, node, local_args, expr_block, depth + 1);
	if (expr_block.nodes.empty()) {
		/* use the input directly if no expression nodes are generated (no local arg dependencies) */
		is_block_node |= add_block_node(graph, node, block_args, block, depth);
	}
	
	count_output_users(graph, expr_block);
	
	/* find node inputs in the expression block that use values outside of it,
	 * which means these must be included in the parent block
	 */
	for (OrderedNodeSet::const_iterator it = expr_block.nodes.begin(); it != expr_block.nodes.end(); ++it) {
		const NodeInstance *node = *it;
		for (int i = 0; i < node->num_inputs(); ++i) {
			ConstInputKey expr_input = node->input(i);
			const NodeInstance *link_node = expr_input.link().node;
			if (link_node && expr_block.nodes.find(link_node) == expr_block.nodes.end())
				is_block_node |= add_block_node(graph, link_node, block_args, block, depth);
		}
	}
	
	return is_block_node;
}

void Compiler::parse_blocks(const NodeGraph &graph)
{
	OutputSet main_args;
	for (NodeGraph::InputList::const_iterator it = graph.inputs.begin(); it != graph.inputs.end(); ++it) {
		const NodeGraph::Input &input = *it;
		if (input.key)
			main_args.insert(input.key);
	}
	
	main = BasicBlock();
	
	for (NodeGraph::OutputList::const_iterator it = graph.outputs.begin(); it != graph.outputs.end(); ++it) {
		const NodeGraph::Output &output = *it;
		if (output.key)
			add_block_node(graph, output.key.node, main_args, main, 0);
	}
	/* input argument nodes must always be included in main,
	 * to provide reliable storage for caller arguments
	 */
	for (NodeGraph::InputList::const_iterator it = graph.inputs.begin(); it != graph.inputs.end(); ++it) {
		const NodeGraph::Input &input = *it;
		if (input.key)
			add_block_node(graph, input.key.node, main_args, main, 0);
	}
	
	count_output_users(graph, main);
}

StackIndex Compiler::find_stack_index(int size) const
{
	int unused = 0;
	
	for (int i = 0; i < BVM_STACK_SIZE; ++i) {
		if (stack_users[i] == 0) {
			++unused;
			if (unused == size)
				return i + 1 - size;
		}
		else
			unused = 0;
	}
	
	// TODO better reporting ...
	printf("ERROR: out of stack space");
	
	return BVM_STACK_INVALID;
}

StackIndex Compiler::assign_stack_index(const TypeDesc &typedesc)
{
	int size = typedesc.stack_size();
	
	StackIndex stack_offset = find_stack_index(size);
	for (int i = 0; i < size; ++i) {
		// TODO keep track of value users
		stack_users[stack_offset + i] += 1;
	}
	
	return stack_offset;
}

void Compiler::resolve_basic_block_symbols(const NodeGraph &graph, BasicBlock &block)
{
	for (OrderedNodeSet::const_iterator it = block.nodes.begin(); it != block.nodes.end(); ++it) {
		const NodeInstance &node = **it;
		
		/* local arguments for expression inputs */
		OutputIndexMap local_output_index;
		local_output_index.insert(block.output_index.begin(), block.output_index.end());
		
		/* initialize output data stack entries */
		for (int i = 0; i < node.num_outputs(); ++i) {
			const NodeOutput *output = node.type->find_output(i);
			ConstOutputKey key(&node, output->name);
			
			StackIndex stack_index;
			if (block.output_index.find(key) == block.output_index.end()) {
				stack_index = assign_stack_index(output->typedesc);
				block.output_index[key] = stack_index;
			}
			else
				stack_index = block.output_index.at(key);
			
			if (output->value_type == OUTPUT_LOCAL) {
				const NodeGraph::Input *graph_input = graph.get_input(output->name);
				
				assert(graph_input);
				if (graph_input->key.node) {
					local_output_index[graph_input->key] = stack_index;
				}
			}
		}
		
		/* prepare input stack entries */
		for (int i = 0; i < node.num_inputs(); ++i) {
			const NodeInput *input = node.type->find_input(i);
			ConstInputKey key(&node, input->name);
			assert(block.input_index.find(key) == block.input_index.end());
			
			if (key.is_constant()) {
				/* stored directly in the instructions list after creating values */
			}
			else if (key.is_expression()) {
				BasicBlock &expr_block = block.expression_blocks.at(key);
				
				/* initialize local arguments */
				expr_block.output_index.insert(local_output_index.begin(), local_output_index.end());
				
				resolve_basic_block_symbols(graph, expr_block);
				
				if (key.link()) {
					expr_block.return_index = expr_block.output_index.at(key.link());
				}
				else {
					expr_block.return_index = assign_stack_index(input->typedesc);
				}
				block.input_index[key] = expr_block.return_index;
			}
			else if (key.link()) {
				block.input_index[key] = block.output_index.at(key.link());
			}
			else {
				block.input_index[key] = assign_stack_index(input->typedesc);
			}
		}
	}
}

void Compiler::resolve_symbols(const NodeGraph &graph)
{
	/* recursively sort node lists for functions */
	parse_blocks(graph);
	
	/* recursively resolve all stack assignments */
	resolve_basic_block_symbols(graph, main);
}


void Compiler::push_constant(const Value *value) const
{
	BLI_assert(value != NULL);
	switch (value->typedesc().base_type) {
		case BVM_FLOAT: {
			float f = 0.0f;
			value->get(&f);
			
			push_float(f);
			break;
		}
		case BVM_FLOAT3: {
			float3 f = float3(0.0f, 0.0f, 0.0f);
			value->get(&f);
			
			push_float3(f);
			break;
		}
		case BVM_FLOAT4: {
			float4 f = float4(0.0f, 0.0f, 0.0f, 0.0f);
			value->get(&f);
			
			push_float4(f);
			break;
		}
		case BVM_INT: {
			int i = 0;
			value->get(&i);
			
			push_int(i);
			break;
		}
		case BVM_MATRIX44: {
			matrix44 m = matrix44::identity();
			value->get(&m);
			
			push_matrix44(m);
			break;
		}
		case BVM_STRING: {
			const char *s = "";
			value->get(&s);
			
			push_string(s);
			break;
		}
		case BVM_POINTER: {
			/* POINTER type can not be stored as a constant */
			break;
		}
		
		case BVM_MESH: {
			/* MESH type can not be stored as a constant */
			break;
		}
		case BVM_DUPLIS: {
			/* DUPLIS type can not be stored as a constant */
			break;
		}
	}
}

void Compiler::codegen_value(const Value *value, StackIndex offset) const
{
	switch (value->typedesc().base_type) {
		case BVM_FLOAT: {
			float f = 0.0f;
			value->get(&f);
			
			push_opcode(OP_VALUE_FLOAT);
			push_float(f);
			push_stack_index(offset);
			break;
		}
		case BVM_FLOAT3: {
			float3 f = float3(0.0f, 0.0f, 0.0f);
			value->get(&f);
			
			push_opcode(OP_VALUE_FLOAT3);
			push_float3(f);
			push_stack_index(offset);
			break;
		}
		case BVM_FLOAT4: {
			float4 f = float4(0.0f, 0.0f, 0.0f, 0.0f);
			value->get(&f);
			
			push_opcode(OP_VALUE_FLOAT4);
			push_float4(f);
			push_stack_index(offset);
			break;
		}
		case BVM_INT: {
			int i = 0;
			value->get(&i);
			
			push_opcode(OP_VALUE_INT);
			push_int(i);
			push_stack_index(offset);
			break;
		}
		case BVM_MATRIX44: {
			matrix44 m = matrix44::identity();
			value->get(&m);
			
			push_opcode(OP_VALUE_MATRIX44);
			push_matrix44(m);
			push_stack_index(offset);
			break;
		}
		case BVM_STRING: {
			const char *s = "";
			value->get(&s);
			
			push_opcode(OP_VALUE_STRING);
			push_string(s);
			push_stack_index(offset);
			break;
		}
		case BVM_POINTER: {
			push_opcode(OP_VALUE_POINTER);
			push_stack_index(offset);
			break;
		}
		
		case BVM_MESH: {
			push_opcode(OP_VALUE_MESH);
			push_stack_index(offset);
			break;
		}
		case BVM_DUPLIS: {
			push_opcode(OP_VALUE_DUPLIS);
			push_stack_index(offset);
			break;
		}
	}
}

static OpCode ptr_init_opcode(const TypeDesc &typedesc)
{
	switch (typedesc.base_type) {
		case BVM_FLOAT:
		case BVM_FLOAT3:
		case BVM_FLOAT4:
		case BVM_INT:
		case BVM_MATRIX44:
		case BVM_STRING:
		case BVM_POINTER:
			return OP_NOOP;
		
		case BVM_MESH:
			return OP_INIT_MESH_PTR;
		case BVM_DUPLIS:
			return OP_INIT_DUPLIS_PTR;
	}
	return OP_NOOP;
}

static OpCode ptr_release_opcode(const TypeDesc &typedesc)
{
	switch (typedesc.base_type) {
		case BVM_FLOAT:
		case BVM_FLOAT3:
		case BVM_FLOAT4:
		case BVM_INT:
		case BVM_MATRIX44:
		case BVM_STRING:
		case BVM_POINTER:
			return OP_NOOP;
		
		case BVM_MESH:
			return OP_RELEASE_MESH_PTR;
		case BVM_DUPLIS:
			return OP_RELEASE_DUPLIS_PTR;
	}
	return OP_NOOP;
}

int Compiler::codegen_basic_block(BasicBlock &block) const
{
	/* do internal blocks first */
	for (BasicBlock::BasicBlockMap::iterator it = block.expression_blocks.begin(); it != block.expression_blocks.end(); ++it) {
		BasicBlock &expr_block = it->second;
		codegen_basic_block(expr_block);
	}
	
	int entry_point = current_address();
	block.entry_point = entry_point;
	
	for (OrderedNodeSet::const_iterator it = block.nodes.begin(); it != block.nodes.end(); ++it) {
		const NodeInstance &node = **it;
		
		/* store values for unconnected inputs */
		for (int i = 0; i < node.num_inputs(); ++i) {
			ConstInputKey key = node.input(i);
			
			if (key.is_constant() || key.is_expression()) {
				/* stored directly in instructions */
			}
			else if (key.link()) {
				/* uses linked output value on the stack */
			}
			else {
				/* create a value node for the input */
				codegen_value(key.value(), block.input_index.at(key));
			}
		}
		/* initialize output data stack entries */
		for (int i = 0; i < node.num_outputs(); ++i) {
			const NodeOutput *output = node.type->find_output(i);
			ConstOutputKey key(&node, output->name);
			
			/* if necessary, add a user count initializer */
			OpCode init_op = ptr_init_opcode(output->typedesc);
			if (init_op != OP_NOOP) {
				int users = block.output_users.at(key);
				if (users > 0) {
					push_opcode(init_op);
					push_stack_index(block.output_index.at(key));
					push_int(users);
				}
			}
		}
		
		OpCode op = get_opcode_from_node_type(node.type->name());
		if (op != OP_NOOP) {
			/* write main opcode */
			push_opcode(op);
			/* write input stack offsets and constants */
			for (int i = 0; i < node.num_inputs(); ++i) {
				ConstInputKey key = node.input(i);
				
				if (key.is_constant()) {
					push_constant(key.value());
				}
				else {
					if (key.is_expression()) {
						const BasicBlock &expr_block = block.expression_blocks.at(key);
						push_jump_address(expr_block.entry_point);
					}
					
					push_stack_index(block.input_index.at(key));
				}
			}
			/* write output stack offsets */
			for (int i = 0; i < node.num_outputs(); ++i) {
				ConstOutputKey key = node.output(i);
				
				push_stack_index(block.output_index.at(key));
			}
		}
		
		/* release input data stack entries */
		for (int i = 0; i < node.num_inputs(); ++i) {
			ConstInputKey key = node.input(i);
			const NodeInput *input = node.type->find_input(i);
			
			if (key.is_constant() || key.is_expression()) {
				/* pass */
			}
			else if (node.link(i)) {
				OpCode release_op = ptr_release_opcode(input->typedesc);
				
				if (release_op != OP_NOOP) {
					push_opcode(release_op);
					push_stack_index(block.output_index.at(node.link(i)));
				}
			}
		}
	}
	
	push_opcode(OP_END);
	
	return entry_point;
}

int Compiler::codegen_main()
{
	/* now generate the main function */
	int entry_point = codegen_basic_block(main);
	return entry_point;
}

/* ========================================================================= */

BVMCompiler::BVMCompiler() :
    fn(NULL)
{
}

BVMCompiler::~BVMCompiler()
{
}

void BVMCompiler::push_opcode(OpCode op) const
{
	fn->add_instruction(op);
}

void BVMCompiler::push_stack_index(StackIndex arg) const
{
	if (arg != BVM_STACK_INVALID)
		fn->add_instruction(arg);
}

void BVMCompiler::push_jump_address(int address) const
{
	fn->add_instruction(int_to_instruction(address));
}

void BVMCompiler::push_float(float f) const
{
	fn->add_instruction(float_to_instruction(f));
}

void BVMCompiler::push_float3(float3 f) const
{
	fn->add_instruction(float_to_instruction(f.x));
	fn->add_instruction(float_to_instruction(f.y));
	fn->add_instruction(float_to_instruction(f.z));
}

void BVMCompiler::push_float4(float4 f) const
{
	fn->add_instruction(float_to_instruction(f.x));
	fn->add_instruction(float_to_instruction(f.y));
	fn->add_instruction(float_to_instruction(f.z));
	fn->add_instruction(float_to_instruction(f.w));
}

void BVMCompiler::push_int(int i) const
{
	fn->add_instruction(int_to_instruction(i));
}

void BVMCompiler::push_matrix44(matrix44 m) const
{
	fn->add_instruction(float_to_instruction(m.data[0][0]));
	fn->add_instruction(float_to_instruction(m.data[0][1]));
	fn->add_instruction(float_to_instruction(m.data[0][2]));
	fn->add_instruction(float_to_instruction(m.data[0][3]));
	fn->add_instruction(float_to_instruction(m.data[1][0]));
	fn->add_instruction(float_to_instruction(m.data[1][1]));
	fn->add_instruction(float_to_instruction(m.data[1][2]));
	fn->add_instruction(float_to_instruction(m.data[1][3]));
	fn->add_instruction(float_to_instruction(m.data[2][0]));
	fn->add_instruction(float_to_instruction(m.data[2][1]));
	fn->add_instruction(float_to_instruction(m.data[2][2]));
	fn->add_instruction(float_to_instruction(m.data[2][3]));
	fn->add_instruction(float_to_instruction(m.data[3][0]));
	fn->add_instruction(float_to_instruction(m.data[3][1]));
	fn->add_instruction(float_to_instruction(m.data[3][2]));
	fn->add_instruction(float_to_instruction(m.data[3][3]));
}

void BVMCompiler::push_string(const char *s) const
{
	const char *c = s;
	while (true) {
		fn->add_instruction(int_to_instruction(*(const int *)c));
		if (c[0]=='\0' || c[1]=='\0' || c[2]=='\0' || c[3]=='\0')
			break;
		c += 4;
	}
}

int BVMCompiler::current_address() const
{
	return fn->get_instruction_count();
}

Function *BVMCompiler::compile_function(const NodeGraph &graph)
{
	resolve_symbols(graph);
	
	fn = new Function();
	
	int entry_point = codegen_main();
	fn->set_entry_point(entry_point);
	
	/* store stack indices for inputs/outputs, to store arguments from and return results to the caller */
	for (size_t i = 0; i < graph.inputs.size(); ++i) {
		const NodeGraph::Input &input = graph.inputs[i];
		
		StackIndex stack_index;
		if (input.key) {
			stack_index = main_block().output_index.at(input.key);
		}
		else {
			stack_index = BVM_STACK_INVALID;
		}
		
		fn->add_argument(input.typedesc, input.name, stack_index);
	}
	for (size_t i = 0; i < graph.outputs.size(); ++i) {
		const NodeGraph::Output &output = graph.outputs[i];
		
		/* every output must map to a node */
		assert(output.key.node);
		
		StackIndex stack_index = main_block().output_index.at(output.key);
		fn->add_return_value(output.typedesc, output.name, stack_index);
	}
	
	Function *result = fn;
	fn = NULL;
	return result;
}

} /* namespace bvm */

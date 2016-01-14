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

static bool is_parent_block(const NodeBlock *parent, const NodeBlock *block)
{
	while (block->parent()) {
		block = block->parent();
		if (block == parent)
			return true;
	}
	return false;
}

void Compiler::calc_node_dependencies(const NodeInstance *node, BlockDependencyMap &block_deps_map)
{
	if (node_deps_map.find(node) != node_deps_map.end())
		return;
	OutputSet &node_deps = node_deps_map[node];
	OutputSet &block_deps = block_deps_map.at(node->block);
	
	for (int i = 0; i < node->num_inputs(); ++i) {
		ConstInputKey input = node->input(i);
		
		if (input.link()) {
			const NodeInstance *link_node = input.link().node;
			
			if (link_node->block == node->block
			    || is_parent_block(node->block, link_node->block))
				node_deps.insert(input.link());
			else if (is_parent_block(link_node->block, node->block)) {
				block_deps.insert(input.link());
			}
			
			if (input.is_expression()) {
				if (link_node->block != node->block) {
					calc_block_dependencies(link_node->block, block_deps_map);
					
					const OutputSet &expr_deps = block_deps_map.at(link_node->block);
					node_deps.insert(expr_deps.begin(), expr_deps.end());
				}
			}
		}
	}
}

void Compiler::calc_block_dependencies(const NodeBlock *block, BlockDependencyMap &block_deps_map)
{
	if (block_deps_map.find(block) != block_deps_map.end())
		return;
	block_deps_map[block] = OutputSet();
	
	for (NodeSet::const_iterator it = block->nodes().begin(); it != block->nodes().end(); ++it) {
		const NodeInstance *node = *it;
		calc_node_dependencies(node, block_deps_map);
	}
}

void Compiler::calc_symbol_scope(const NodeGraph &graph)
{
	{
		BlockDependencyMap block_deps_map;
		calc_block_dependencies(&graph.main_block(), block_deps_map);
	}
	
	assert(output_users.empty());
	for (NodeDependencyMap::const_iterator it = node_deps_map.begin(); it != node_deps_map.end(); ++it) {
		const OutputSet &deps = it->second;
		
		for (OutputSet::const_iterator it_dep = deps.begin(); it_dep != deps.end(); ++it_dep) {
			ConstOutputKey output = *it_dep;
			
			++output_users[output];
		}
	}
	/* retain graph outputs for the main caller */
	for (NodeGraph::OutputList::const_iterator it = graph.outputs.begin(); it != graph.outputs.end(); ++it) {
		const NodeGraph::Output &output = *it;
		if (output.key)
			++output_users[output.key];
	}
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

void Compiler::get_local_arg_indices(const NodeInstance *node, const NodeBlock *local_block)
{
	for (int i = 0; i < node->num_outputs(); ++i) {
		ConstOutputKey output = node->output(i);
		
		if (output.socket->value_type == OUTPUT_LOCAL) {
			ConstOutputKey local_output = local_block->local_arg(output.socket->name);
			output_index[local_output] = output_index.at(output);
		}
	}
}

void Compiler::resolve_node_block_symbols(const NodeBlock *block)
{
	assert(block_info.find(block) == block_info.end());
	block_info[block] = BlockInfo();
	
	OrderedNodeSet nodes;
	nodes.insert(block->nodes().begin(), block->nodes().end());
	
	for (OrderedNodeSet::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
		const NodeInstance &node = **it;
		
		/* initialize output data stack entries */
		for (int i = 0; i < node.num_outputs(); ++i) {
			ConstOutputKey output = node.output(i);
			
			StackIndex stack_index;
			if (output_index.find(output) == output_index.end())
				stack_index = assign_stack_index(output.socket->typedesc);
			else
				stack_index = output_index.at(output);
			output_index[output] = stack_index;
		}
		
		/* prepare input stack entries */
		for (int i = 0; i < node.num_inputs(); ++i) {
			ConstInputKey input = node.input(i);
			assert(input_index.find(input) == input_index.end());
			
			if (input.is_constant()) {
				/* stored directly in the instructions list after creating values */
			}
			else if (input.is_expression()) {
				ConstOutputKey link = input.link();
				const NodeBlock *expr_block = link.node->block;
				
				if (block_info.find(expr_block) == block_info.end()) {
					/* copy local arguments */
					get_local_arg_indices(&node, expr_block);
					resolve_node_block_symbols(expr_block);
				}
				
				StackIndex return_index = output_index.at(link);
				input_index[input] = return_index;
			}
			else {
				ConstOutputKey link = input.link();
				input_index[input] = output_index.at(link);
			}
		}
	}
}

void Compiler::resolve_symbols(const NodeGraph &graph)
{
	/* recursively assign stack indices to outputs */
	resolve_node_block_symbols(&graph.main_block());
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
		case BVM_RNAPOINTER: {
			/* RNAPOINTER type can not be stored as a constant */
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
		case BVM_RNAPOINTER: {
			push_opcode(OP_VALUE_RNAPOINTER);
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
		case BVM_RNAPOINTER:
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
		case BVM_RNAPOINTER:
			return OP_NOOP;
		
		case BVM_MESH:
			return OP_RELEASE_MESH_PTR;
		case BVM_DUPLIS:
			return OP_RELEASE_DUPLIS_PTR;
	}
	return OP_NOOP;
}

int Compiler::codegen_node_block(const NodeBlock &block)
{
	int entry_point = current_address();
	BlockInfo &info = block_info[&block];
	info.entry_point = entry_point;
	
	OrderedNodeSet nodes;
	nodes.insert(block.nodes().begin(), block.nodes().end());
	
	for (OrderedNodeSet::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
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
				codegen_value(key.value(), input_index.at(key));
			}
		}
		/* initialize output data stack entries */
		for (int i = 0; i < node.num_outputs(); ++i) {
			const NodeOutput *output = node.type->find_output(i);
			ConstOutputKey key(&node, output->name);
			
			/* if necessary, add a user count initializer */
			OpCode init_op = ptr_init_opcode(output->typedesc);
			if (init_op != OP_NOOP) {
				int users = output_users.at(key);
				if (users > 0) {
					push_opcode(init_op);
					push_stack_index(output_index.at(key));
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
						if (key.link() && key.link().node->block != &block) {
							const NodeBlock *expr_block = key.link().node->block;
							push_jump_address(block_info.at(expr_block).entry_point);
						}
						else
							push_jump_address(BVM_JMP_INVALID);
					}
					
					push_stack_index(input_index.at(key));
				}
			}
			/* write output stack offsets */
			for (int i = 0; i < node.num_outputs(); ++i) {
				ConstOutputKey key = node.output(i);
				
				push_stack_index(output_index.at(key));
			}
		}
		
		/* release input data stack entries */
		{
			const OutputSet &node_deps = node_deps_map.at(&node);
			for (OutputSet::const_iterator it_dep = node_deps.begin(); it_dep != node_deps.end(); ++it_dep) {
				ConstOutputKey output = *it_dep;
				
				OpCode release_op = ptr_release_opcode(output.socket->typedesc);
				
				if (release_op != OP_NOOP) {
					push_opcode(release_op);
					push_stack_index(output_index.at(output));
				}
			}
		}
	}
	
	push_opcode(OP_END);
	
	return entry_point;
}

int Compiler::codegen_graph(const NodeGraph &graph)
{
	calc_symbol_scope(graph);
	
	/* do internal blocks first */
	for (NodeGraph::NodeBlockList::const_reverse_iterator it = graph.blocks.rbegin(); it != graph.blocks.rend(); ++it) {
		const NodeBlock &block = *it;
		codegen_node_block(block);
	}
	return block_info[&graph.main_block()].entry_point;
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
	
	int entry_point = codegen_graph(graph);
	fn->set_entry_point(entry_point);
	
	/* store stack indices for inputs/outputs, to store arguments from and return results to the caller */
	for (size_t i = 0; i < graph.inputs.size(); ++i) {
		const NodeGraph::Input &input = graph.inputs[i];
		
		StackIndex stack_index;
		if (input.key) {
			stack_index = output_index.at(input.key);
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
		
		StackIndex stack_index = output_index.at(output.key);
		fn->add_return_value(output.typedesc, output.name, stack_index);
	}
	
	Function *result = fn;
	fn = NULL;
	return result;
}

} /* namespace bvm */

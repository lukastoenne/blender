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

/** \file bvm_codegen_debug.cc
 *  \ingroup bvm
 */

#include <cstdio>
#include <set>

#include "bvm_codegen.h"
#include "bvm_nodegraph.h"

#include "bvm_util_debug.h"
#include "bvm_util_string.h"

namespace bvm {

#define NL "\r\n"

static const char *fontname = "helvetica";
static const char *color_opcode = "firebrick1";
static const char *color_stack_index = "dodgerblue1";
static const char *color_jump_address = "forestgreen";
static const char *color_value = "gold1";

static void debug_fprintf(FILE *f, const char *fmt, ...) ATTR_PRINTF_FORMAT(2, 3);
static void debug_fprintf(FILE *f, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(f, fmt, args);
	va_end(args);
}

inline void print_rows(FILE *f, int *cur, int num, const char *color, const char *fmt, ...)
{
	va_list args;
	
	for (int i = 0; i < num; ++i) {
		debug_fprintf(f, "<TR>");
		
		debug_fprintf(f, "<TD>%d</TD>", (*cur) + i);
		
		if (i == 0) {
			debug_fprintf(f, "<TD ROWSPAN=\"%d\" BGCOLOR=\"%s\">", num, color);
			
			va_start(args, fmt);
			vfprintf(f, fmt, args);
			va_end(args);
			
			debug_fprintf(f, "</TD>");
		}
		
		debug_fprintf(f, "</TR>" NL);
	}
	
	*cur += num;
}

inline void print_gap(FILE *f)
{
	debug_fprintf(f, "<TR>");
		debug_fprintf(f, "<TD HEIGHT=\"5\" BORDER=\"0\">");
		debug_fprintf(f, "</TD>");
	debug_fprintf(f, "</TR>" NL);
}

DebugGraphvizCompiler::DebugGraphvizCompiler() :
    m_file(NULL),
    m_current_address(0),
    m_current_opnode(NULL),
    m_current_arg(0)
{
}

DebugGraphvizCompiler::~DebugGraphvizCompiler()
{
}

void DebugGraphvizCompiler::push_opcode(OpCode op) const
{
	const char *opname = opcode_name(op);
	
	if (m_current_address > 0)
		print_gap(m_file);
	print_rows(m_file, &m_current_address, 1, color_opcode, "OP %s", opname);
	
	m_current_opnode = NodeGraph::find_node_type(opname);
	m_current_arg = 0;
}

void DebugGraphvizCompiler::push_stack_index(StackIndex arg) const
{
	const char *load_or_store = is_arg_output() ? "store" : "load";
	print_rows(m_file, &m_current_address, 1, color_stack_index, "%s %d [%s]", load_or_store, (int)arg, get_arg_name());
	++m_current_arg;
}

void DebugGraphvizCompiler::push_jump_address(int address) const
{
	print_rows(m_file, &m_current_address, 1, color_jump_address, "JMP %d", (int)address);
}

void DebugGraphvizCompiler::push_float(float f) const
{
	print_rows(m_file, &m_current_address, 1, color_value, "%f [%s]", f, get_arg_name());
	++m_current_arg;
}

void DebugGraphvizCompiler::push_float3(float3 f) const
{
	print_rows(m_file, &m_current_address, 3, color_value, "(%.2f, %.2f, %.2f)<BR/>[%s]", f.x, f.y, f.z, get_arg_name());
	++m_current_arg;
}

void DebugGraphvizCompiler::push_float4(float4 f) const
{
	print_rows(m_file, &m_current_address, 4, color_value, "(%.2f, %.2f, %.2f, %.2f)<BR/>[%s]", f.x, f.y, f.z, f.w, get_arg_name());
	++m_current_arg;
}

void DebugGraphvizCompiler::push_int(int i) const
{
	print_rows(m_file, &m_current_address, 1, color_value, "%d [%s]", i, get_arg_name());
	++m_current_arg;
}

void DebugGraphvizCompiler::push_matrix44(matrix44 m) const
{
	print_rows(m_file, &m_current_address, 16, color_value, "%.2f, %.2f, %.2f, %.2f<BR/>%.2f, %.2f, %.2f, %.2f<BR/>%.2f, %.2f, %.2f, %.2f<BR/>%.2f, %.2f, %.2f, %.2f<BR/>[%s]",
	           m.data[0][0], m.data[1][0], m.data[2][0], m.data[3][0],
	           m.data[0][1], m.data[1][1], m.data[2][1], m.data[3][1],
	           m.data[0][2], m.data[1][2], m.data[2][2], m.data[3][2],
	           m.data[0][3], m.data[1][3], m.data[2][3], m.data[3][3],
	           get_arg_name());
	++m_current_arg;
}

void DebugGraphvizCompiler::push_string(const char *s) const
{
	const char *c = s;
	int rows = 0;
	while (true) {
		rows += 1;
		if (c[0]=='\0' || c[1]=='\0' || c[2]=='\0' || c[3]=='\0')
			break;
		c += 4;
	}
	print_rows(m_file, &m_current_address, rows, color_value, "%s<BR/>[%s]", s, get_arg_name());
	++m_current_arg;
}

int DebugGraphvizCompiler::current_address() const
{
	return m_current_address;
}

const char *DebugGraphvizCompiler::get_arg_name() const
{
	if (m_current_opnode) {
		if (m_current_arg < m_current_opnode->num_inputs())
			return m_current_opnode->find_input(m_current_arg)->name.c_str();
		else
			return m_current_opnode->find_output(m_current_arg - m_current_opnode->num_inputs())->name.c_str();
	}
	return "";
}

bool DebugGraphvizCompiler::is_arg_output() const
{
	if (m_current_opnode) {
		return m_current_arg >= m_current_opnode->num_inputs();
	}
	return false;
}

void DebugGraphvizCompiler::init_graph(const string &label)
{
	static float label_size = 20.0f;
	
	debug_fprintf(m_file, "digraph depgraph {" NL);
	debug_fprintf(m_file, "rankdir=LR;" NL);
	debug_fprintf(m_file, "graph [");
	debug_fprintf(m_file, "labelloc=\"t\"");
	debug_fprintf(m_file, ",fontsize=%f", label_size);
	debug_fprintf(m_file, ",fontname=\"%s\"", fontname);
	debug_fprintf(m_file, ",label=\"%s\"", label.c_str());
//	debug_fprintf(m_file, ",splines=ortho");
	debug_fprintf(m_file, "];" NL);
}

void DebugGraphvizCompiler::close_graph()
{
	debug_fprintf(m_file, "}" NL);
}

void DebugGraphvizCompiler::init_node()
{
	static float label_size = 14.0f;
	const char *shape = "box";
	const char *style = "filled";
	const char *color = "black";
	const char *fillcolor = "gainsboro";
	float penwidth = 1.0f;
	
	debug_fprintf(m_file, "instructions");
	debug_fprintf(m_file, "[");
	
	debug_fprintf(m_file, "fontname=\"%s\"", fontname);
	debug_fprintf(m_file, ",fontsize=\"%f\"", label_size);
	debug_fprintf(m_file, ",shape=\"%s\"", shape);
	debug_fprintf(m_file, ",style=\"%s\"", style);
	debug_fprintf(m_file, ",color=\"%s\"", color);
	debug_fprintf(m_file, ",fillcolor=\"%s\"", fillcolor);
	debug_fprintf(m_file, ",penwidth=\"%f\"", penwidth);
	
	debug_fprintf(m_file, ",label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">" NL);
}

void DebugGraphvizCompiler::close_node()
{
	debug_fprintf(m_file, "</TABLE>>");
	debug_fprintf(m_file, "];" NL);
}

void DebugGraphvizCompiler::compile_function(const NodeGraph &graph, FILE *f, const string &label)
{
	m_file = f;
	m_current_address = 0;
	m_current_opnode = NULL;
	m_current_arg = 0;
	
	resolve_symbols(graph);
	
	init_graph(label);
	init_node();
	
	int entry_point = codegen_main(graph);
	(void)entry_point;
	
	close_node();
	
	close_graph();
	
#if 0
	/* store stack indices for inputs/outputs, to store arguments from and return results to the caller */
	for (size_t i = 0; i < graph.inputs.size(); ++i) {
		const NodeGraph::Input &input = graph.inputs[i];
		
		StackIndex stack_index;
		if (input.key.node) {
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
#endif
	
	m_current_opnode = NULL;
	m_current_arg = 0;
	m_file = NULL;
}

} /* namespace bvm */

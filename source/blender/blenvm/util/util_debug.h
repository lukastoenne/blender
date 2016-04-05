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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Brecht van Lommel
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/util/bvm_util_debug.h
 *  \ingroup bvm
 */

#ifndef __BVM_UTIL_DEBUG_H__
#define __BVM_UTIL_DEBUG_H__

#include <cstdio>
#include <sstream>

#include "node_graph.h"

namespace blenvm {
namespace debug {

#define NL "\r\n"

struct DebugContext {
	FILE *file;
};

static void debug_fprintf(const DebugContext &ctx, const char *fmt, ...) ATTR_PRINTF_FORMAT(2, 3);
static void debug_fprintf(const DebugContext &ctx, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(ctx.file, fmt, args);
	va_end(args);
}

static const char *fontname = "helvetica";
static float graph_label_size = 20.0f;
static float block_label_size = 16.0f;
static float node_label_size = 14.0f;
static const char *node_color_function = "gainsboro";
static const char *node_color_kernel = "lightsalmon1";
static const char *node_color_pass = "gray96";
static const char *node_color_argument = "steelblue";
static const char *node_color_return_value = "orange";
static const char *color_opcode = "firebrick1";
static const char *color_stack_index = "dodgerblue1";
static const char *color_jump_address = "forestgreen";
static const char *color_value = "gold1";

struct NodeGraphDumper {
	NodeGraphDumper(FILE *f) :
	    file(f)
	{
		ctx.file = f;
		
		(void)color_opcode;
		(void)color_stack_index;
		(void)color_jump_address;
		(void)color_value;
	}
	
	inline static int input_index(const NodeInstance *node, const string &name)
	{
		for (int i = 0; i < node->type->num_inputs(); ++i) {
			if (node->type->find_input(i)->name == name)
				return i;
		}
		return -1;
	}
	
	inline static int output_index(const NodeInstance *node, const string &name)
	{
		for (int i = 0; i < node->type->num_outputs(); ++i) {
			if (node->type->find_output(i)->name == name)
				return i;
		}
		return -1;
	}
	
	inline static string node_id(const NodeInstance *node, bool stringify=true)
	{
		std::stringstream ss;
		if (stringify)
			ss << "\"node_" << (void*)node << "\"";
		else
			ss << "node_" << (void*)node;
		return ss.str();
	}
	
	inline static string input_id(const ConstInputKey &key, bool stringify=true)
	{
		int index = input_index(key.node, key.socket->name);
		std::stringstream ss;
		if (stringify)
			ss << "\"I" << key.socket->name << "_" << index << "\"";
		else
			ss << "I" << key.socket->name << "_" << index;
		return ss.str();
	}
	
	inline static string output_id(const ConstOutputKey &key, bool stringify=true)
	{
		int index = output_index(key.node, key.socket->name);
		std::stringstream ss;
		if (stringify)
			ss << "\"O" << key.socket->name << "_" << index << "\"";
		else
			ss << "O" << key.socket->name << "_" << index;
		return ss.str();
	}
	
	inline void dump_node(const NodeInstance *node) const
	{
		const char *shape = "box";
		const char *style = "filled,rounded";
		const char *color = "black";
		const char *fillcolor =
		        (node->type->is_pass_node()) ? node_color_pass :
		        (node->type->is_kernel_node()) ? node_color_kernel :
		        node_color_function;
		float penwidth = 1.0f;
		string name = node->type->name();
		
		debug_fprintf(ctx, "// %s\n", node->name.c_str());
		debug_fprintf(ctx, "%s", node_id(node).c_str());
		debug_fprintf(ctx, "[");
		
		/* html label including rows for input/output sockets
		 * http://www.graphviz.org/doc/info/shapes.html#html
		 */
		debug_fprintf(ctx, "label=<<TABLE BORDER=\"0\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"4\">");
		debug_fprintf(ctx, "<TR><TD COLSPAN=\"2\">%s</TD></TR>", name.c_str());
		int numin = node->type->num_inputs();
		int numout = node->type->num_outputs();
		for (int i = 0; i < numin || i < numout; ++i) {
			debug_fprintf(ctx, "<TR>");
			
			if (i < numin) {
				ConstInputKey input = node->input(i);
				string name_in = input.socket->name;
				debug_fprintf(ctx, "<TD");
				debug_fprintf(ctx, " PORT=%s", input_id(input).c_str());
				debug_fprintf(ctx, " BORDER=\"1\"");
				switch (input.value_type()) {
					case INPUT_EXPRESSION:
						break;
					case INPUT_VARIABLE:
						debug_fprintf(ctx, " BGCOLOR=\"%s\"", node_color_argument);
						break;
					case INPUT_CONSTANT:
						debug_fprintf(ctx, " BGCOLOR=\"%s\"", color_value);
						break;
				}
				debug_fprintf(ctx, ">");
				debug_fprintf(ctx, "%s", name_in.c_str());
				debug_fprintf(ctx, "</TD>");
			}
			else
				debug_fprintf(ctx, "<TD></TD>");
				
			if (i < numout) {
				ConstOutputKey output = node->output(i);
				string name_out = output.socket->name;
				debug_fprintf(ctx, "<TD");
				debug_fprintf(ctx, " PORT=%s", output_id(output).c_str());
				debug_fprintf(ctx, " BORDER=\"1\"");
				if (output.value_type() == OUTPUT_VARIABLE)
					debug_fprintf(ctx, " BGCOLOR=\"%s\"", node_color_argument);
				debug_fprintf(ctx, ">");
				debug_fprintf(ctx, "%s", name_out.c_str());
				debug_fprintf(ctx, "</TD>");
			}
			else
				debug_fprintf(ctx, "<TD></TD>");
			
			debug_fprintf(ctx, "</TR>");
		}
		debug_fprintf(ctx, "</TABLE>>");
		
		debug_fprintf(ctx, ",fontname=\"%s\"", fontname);
		debug_fprintf(ctx, ",fontsize=\"%f\"", node_label_size);
		debug_fprintf(ctx, ",shape=\"%s\"", shape);
		debug_fprintf(ctx, ",style=\"%s\"", style);
		debug_fprintf(ctx, ",color=\"%s\"", color);
		debug_fprintf(ctx, ",fillcolor=\"%s\"", fillcolor);
		debug_fprintf(ctx, ",penwidth=\"%f\"", penwidth);
		debug_fprintf(ctx, "];" NL);
		debug_fprintf(ctx, NL);
	}
	
	inline void dump_local_arg(const string &local_arg_id, const ConstOutputKey arg_output) const
	{
		debug_fprintf(ctx, "%s -> %s:%s",
		              local_arg_id.c_str(),
		              node_id(arg_output.node).c_str(), output_id(arg_output).c_str());
		
		debug_fprintf(ctx, "[");
		/* Note: without label an id seem necessary to avoid bugs in graphviz/dot */
		debug_fprintf(ctx, "id=\"ARG%s:%s\"", node_id(arg_output.node, false).c_str(), output_id(arg_output, false).c_str());
		
		debug_fprintf(ctx, ",penwidth=\"%f\"", 2.0f);
		debug_fprintf(ctx, ",style=dashed");
		debug_fprintf(ctx, ",color=%s", node_color_argument);
//		debug_fprintf(ctx, ",constraint=false");
		debug_fprintf(ctx, "];" NL);
		debug_fprintf(ctx, NL);
	}
	
	inline void dump_return_value(const string &return_value_id, const ConstOutputKey &ret_output) const
	{
		debug_fprintf(ctx, "%s:%s -> %s",
		              node_id(ret_output.node).c_str(), output_id(ret_output).c_str(),
		              return_value_id.c_str());
		
		debug_fprintf(ctx, "[");
		/* Note: without label an id seem necessary to avoid bugs in graphviz/dot */
		debug_fprintf(ctx, "id=\"RET%s:%s\"", node_id(ret_output.node, false).c_str(), output_id(ret_output, false).c_str());
		
		debug_fprintf(ctx, ",penwidth=\"%f\"", 2.0f);
		debug_fprintf(ctx, ",style=dashed");
		debug_fprintf(ctx, ",color=%s", node_color_return_value);
		debug_fprintf(ctx, "];" NL);
		debug_fprintf(ctx, NL);
	}
	
	inline void dump_graph_input(const NodeGraph::Input *input) const
	{
		string name = input->name;
		std::stringstream ss;
		ss << "\"input_" << (void *)input << "\"";
		string id = ss.str();
		
		const char *shape = "box";
		const char *style = "filled,rounded";
		const char *color = "black";
		const char *fillcolor = node_color_argument;
		
		debug_fprintf(ctx, "%s", id.c_str());
		debug_fprintf(ctx, "[");
		
		debug_fprintf(ctx, "label=\"%s\"", name.c_str());
		debug_fprintf(ctx, ",fontname=\"%s\"", fontname);
		debug_fprintf(ctx, ",fontsize=\"%f\"", node_label_size);
		debug_fprintf(ctx, ",shape=\"%s\"", shape);
		debug_fprintf(ctx, ",style=\"%s\"", style);
		debug_fprintf(ctx, ",color=\"%s\"", color);
		debug_fprintf(ctx, ",fillcolor=\"%s\"", fillcolor);
		debug_fprintf(ctx, ",penwidth=\"%f\"", 1.0f);
		debug_fprintf(ctx, "];" NL);
		debug_fprintf(ctx, NL);
		
		const OutputKey &key = input->key;
		if (key) {
			dump_local_arg(id, key);
		}
	}
	
	inline void dump_graph_output(const NodeGraph::Output *output) const
	{
		string name = output->name;
		std::stringstream ss;
		ss << "\"output_" << (void *)output << "\"";
		string id = ss.str();
		
		const char *shape = "box";
		const char *style = "filled,rounded";
		const char *color = "black";
		const char *fillcolor = node_color_return_value;
		
		debug_fprintf(ctx, "%s", id.c_str());
		debug_fprintf(ctx, "[");
		
		debug_fprintf(ctx, "label=\"%s\"", name.c_str());
		debug_fprintf(ctx, ",fontname=\"%s\"", fontname);
		debug_fprintf(ctx, ",fontsize=\"%f\"", node_label_size);
		debug_fprintf(ctx, ",shape=\"%s\"", shape);
		debug_fprintf(ctx, ",style=\"%s\"", style);
		debug_fprintf(ctx, ",color=\"%s\"", color);
		debug_fprintf(ctx, ",fillcolor=\"%s\"", fillcolor);
		debug_fprintf(ctx, ",penwidth=\"%f\"", 1.0f);
		debug_fprintf(ctx, "];" NL);
		debug_fprintf(ctx, NL);
		
		const OutputKey &key = output->key;
		if (key) {
			dump_return_value(id, key);
		}
	}
	
	inline void dump_link(const ConstOutputKey &from, const ConstInputKey &to) const
	{
		float penwidth = 2.0f;
		
		debug_fprintf(ctx, "%s:%s -> %s:%s",
		              node_id(from.node).c_str(), output_id(from).c_str(),
		              node_id(to.node).c_str(), input_id(to).c_str());

		debug_fprintf(ctx, "[");
		/* Note: without label an id seem necessary to avoid bugs in graphviz/dot */
		debug_fprintf(ctx, "id=\"VAL%s:%s\"", node_id(to.node, false).c_str(), input_id(to, false).c_str());
		
		debug_fprintf(ctx, ",penwidth=\"%f\"", penwidth);
		if (to.value_type() == INPUT_VARIABLE) {
			debug_fprintf(ctx, ",constraint=\"false\"");
			debug_fprintf(ctx, ",style=\"dashed\"");
		}
		debug_fprintf(ctx, "];" NL);
		debug_fprintf(ctx, NL);
	}
	
	inline void dump_local_args(const NodeInstance *node, const NodeBlock *block) const
	{
		for (int i = 0; i < node->num_outputs(); ++i) {
			ConstOutputKey output = node->output(i);
			if (output.value_type() == OUTPUT_VARIABLE) {
				ConstOutputKey local_arg = block->local_arg(output.socket->name);
				if (local_arg)
					dump_local_arg(node_id(node)+":"+output_id(output), local_arg);
			}
		}
	}
	
	inline void dump_node_links(const NodeInstance *node) const
	{
		for (NodeInstance::InputMap::const_iterator it = node->inputs.begin(); it != node->inputs.end(); ++it) {
			const string &name = it->first;
			ConstInputKey input = node->input(name);
			
			if (input.link()) {
//				if (input.is_expression()) {
//					dump_return_value(node_id(node, false)+":"+input_id(input, false), input.link());
					
//					const NodeBlock *block = input.link().node->block;
//					if (block) {
//						dump_local_args(node, block);
//					}
//				}
//				else 
					dump_link(input.link(), input);
			}
		}
	}
	
	static int get_block_depth(const NodeBlock *block)
	{
		int depth = -1;
		while (block) {
			block = block->parent();
			++depth;
		}
		return depth;
	}
	
	typedef std::set<const NodeBlock *> BlockSet;
	typedef std::map<const NodeBlock *, BlockSet> BlockChildMap;
	
	inline void dump_block(const NodeBlock &block, const BlockChildMap &block_children) const
	{
		static const char *style = "filled,rounded";
		int depth = get_block_depth(&block);
		int gray_level = max_ii(95 - depth * 10, 0);
		
		debug_fprintf(ctx, "subgraph \"cluster_%p\" {" NL, &block);
		debug_fprintf(ctx, "margin=\"%d\";" NL, 16);
		debug_fprintf(ctx, "style=\"%s\";" NL, style);
		debug_fprintf(ctx, "fillcolor=\"gray%d\"" NL, gray_level);
		debug_fprintf(ctx, "fontsize=\"%f\"", block_label_size);
		debug_fprintf(ctx, "fontname=\"%s\"", fontname);
		debug_fprintf(ctx, "label=\"%s\"", block.name().c_str());
		
		for (NodeSet::const_iterator it = block.nodes().begin(); it != block.nodes().end(); ++it) {
			const NodeInstance *node = *it;
			dump_node(node);
		}
		
		BlockChildMap::const_iterator it = block_children.find(&block);
		if (it != block_children.end()) {
			for (BlockSet::const_iterator it_child = it->second.begin(); it_child != it->second.end(); ++it_child) {
				dump_block(**it_child, block_children);
			}
		}
		
		debug_fprintf(ctx, "}" NL);
	}
	
	inline void dump_graph(const NodeGraph *graph, const string &label) const
	{
		debug_fprintf(ctx, "digraph depgraph {" NL);
		debug_fprintf(ctx, "rankdir=LR;" NL);
		debug_fprintf(ctx, "graph [");
		debug_fprintf(ctx, "labelloc=\"t\"");
		debug_fprintf(ctx, ",fontsize=%f", graph_label_size);
		debug_fprintf(ctx, ",fontname=\"%s\"", fontname);
		debug_fprintf(ctx, ",label=\"%s\"", label.c_str());
	//	debug_fprintf(ctx, ",splines=ortho");
		debug_fprintf(ctx, "];" NL);
		
		BlockChildMap block_children;
		for (NodeGraph::NodeBlockList::const_iterator it = graph->blocks.begin(); it != graph->blocks.end(); ++it) {
			const NodeBlock &block = *it;
			if (block.parent())
				block_children[block.parent()].insert(&block);
		}
		if (!graph->blocks.empty()) {
			dump_block(graph->main_block(), block_children);
		}
		else {
			for (NodeGraph::NodeInstanceMap::const_iterator it = graph->nodes.begin(); it != graph->nodes.end(); ++it) {
				const NodeInstance *node = it->second;
				dump_node(node);
			}
		}
		
		for (NodeGraph::InputList::const_iterator it = graph->inputs.begin(); it != graph->inputs.end(); ++it) {
			const NodeGraph::Input *input = &(*it);
			dump_graph_input(input);
		}
		for (NodeGraph::OutputList::const_iterator it = graph->outputs.begin(); it != graph->outputs.end(); ++it) {
			const NodeGraph::Output *output = &(*it);
			dump_graph_output(output);
		}
		
		for (NodeGraph::NodeInstanceMap::const_iterator it = graph->nodes.begin(); it != graph->nodes.end(); ++it) {
			const NodeInstance *node = it->second;
			dump_node_links(node);
		}
	
	//	deg_debug_graphviz_legend(ctx);
	
		debug_fprintf(ctx, "}" NL);
	}
	
protected:
	FILE *file;
	DebugContext ctx;
};

#undef NL

} /* namespace debug */
} /* namespace blenvm */

#endif  /* __BVM_UTIL_DEBUG_H__ */

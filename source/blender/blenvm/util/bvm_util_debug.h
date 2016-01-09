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

#include "bvm_nodegraph.h"

namespace bvm {
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
static float node_label_size = 14.0f;
static const char *node_color_function = "gainsboro";
static const char *node_color_kernel = "lightsalmon1";
static const char *node_color_pass = "gray96";
static const char *node_color_argument = "steelblue";
static const char *node_color_return_value = "orange";

struct NodeGraphDumper {
	NodeGraphDumper(FILE *f) :
	    file(f)
	{
		ctx.file = f;
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
		debug_fprintf(ctx, "\"node_%p\"", node);
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
				string name_in = node->type->find_input(i)->name;
				debug_fprintf(ctx, "<TD PORT=\"I%s_%d\" BORDER=\"1\">%s</TD>", name_in.c_str(), i, name_in.c_str());
			}
			else
				debug_fprintf(ctx, "<TD></TD>");
				
			if (i < numout) {
				string name_out = node->type->find_output(i)->name;
				debug_fprintf(ctx, "<TD PORT=\"O%s_%d\" BORDER=\"1\">%s</TD>", name_out.c_str(), i, name_out.c_str());
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
	
	inline void dump_input_output(const NodeGraph::Input *input, const NodeGraph::Output *output) const
	{
		string name = input ? input->name : output->name;
		string id = input ? "input" : "output";
		void *ptr = input ? (void *)input : (void *)output;
		{
			const char *shape = "box";
			const char *style = "filled,rounded";
			const char *color = "black";
			const char *fillcolor = input ?
			                            node_color_argument :
			                            node_color_return_value;
			float penwidth = 1.0f;
			
			debug_fprintf(ctx, "// %s\n", name.c_str());
			debug_fprintf(ctx, "\"%s_%p\"", id.c_str(), ptr);
			debug_fprintf(ctx, "[");
			
			/* html label including rows for input/output sockets
			 * http://www.graphviz.org/doc/info/shapes.html#html
			 */
			debug_fprintf(ctx, "label=\"%s\"", name.c_str());
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
		
		const float penwidth = 2.0f;
		if (input) {
			const OutputKey &key = input->key;
			if (key) {
				const NodeGraph::Input *tail = input;
				const NodeInstance *head = key.node;
				const string &head_socket = key.socket->name;
				debug_fprintf(ctx, "// %s:%s -> %s\n",
				              tail->name.c_str(),
				              head->name.c_str(), head_socket.c_str());
				debug_fprintf(ctx, "\"input_%p\"", tail);
				debug_fprintf(ctx, " -> ");
				debug_fprintf(ctx, "\"node_%p\"", head);
				
				debug_fprintf(ctx, "[");
				/* Note: without label an id seem necessary to avoid bugs in graphviz/dot */
				debug_fprintf(ctx, "id=\"A%sB%s\"",
				              head->name.c_str(),
				              tail->name.c_str());
				
				debug_fprintf(ctx, ",penwidth=\"%f\"", penwidth);
				debug_fprintf(ctx, "];" NL);
				debug_fprintf(ctx, NL);
			}
		}
		else {
			const OutputKey &key = output->key;
			if (key) {
				const NodeInstance *tail = key.node;
				const string &tail_socket = key.socket->name;
				int tail_index = output_index(tail, tail_socket);
				const NodeGraph::Output *head = output;
				debug_fprintf(ctx, "// %s:%s -> %s\n",
				              tail->name.c_str(), tail_socket.c_str(),
				              head->name.c_str());
				debug_fprintf(ctx, "\"node_%p\":\"O%s_%d\"", tail, tail_socket.c_str(), tail_index);
				debug_fprintf(ctx, " -> ");
				debug_fprintf(ctx, "\"output_%p\"", head);
				
				debug_fprintf(ctx, "[");
				/* Note: without label an id seem necessary to avoid bugs in graphviz/dot */
				debug_fprintf(ctx, "id=\"A%sB%s:O%s_%d\"",
				              head->name.c_str(),
				              tail->name.c_str(), tail_socket.c_str(), tail_index);
				
				debug_fprintf(ctx, ",penwidth=\"%f\"", penwidth);
				debug_fprintf(ctx, "];" NL);
				debug_fprintf(ctx, NL);
			}
		}
	}
	
	inline void dump_node_links(const NodeGraph *graph, const NodeInstance *node) const
	{
		float penwidth = 2.0f;
		const char *localarg_color = "gray50";
		
		for (NodeInstance::InputMap::const_iterator it = node->inputs.begin(); it != node->inputs.end(); ++it) {
			const NodeInstance::InputInstance &input = it->second;
			
			if (input.link) {
				const NodeInstance *tail = input.link.node;
				const string &tail_socket = input.link.socket->name;
				int tail_index = output_index(tail, tail_socket);
				const NodeInstance *head = node;
				const string &head_socket = it->first;
				int head_index = input_index(head, head_socket);
				debug_fprintf(ctx, "// %s:%s -> %s:%s\n",
				              tail->name.c_str(), tail_socket.c_str(),
				              head->name.c_str(), head_socket.c_str());
				debug_fprintf(ctx, "\"node_%p\":\"O%s_%d\"", tail, tail_socket.c_str(), tail_index);
				debug_fprintf(ctx, " -> ");
				debug_fprintf(ctx, "\"node_%p\":\"I%s_%d\"", head, head_socket.c_str(), head_index);
		
				debug_fprintf(ctx, "[");
				/* Note: without label an id seem necessary to avoid bugs in graphviz/dot */
				debug_fprintf(ctx, "id=\"A%s:O%s_%dB%s:O%s_%d\"",
				              head->name.c_str(), head_socket.c_str(), head_index,
				              tail->name.c_str(), tail_socket.c_str(), tail_index);
				
				debug_fprintf(ctx, ",penwidth=\"%f\"", penwidth);
				debug_fprintf(ctx, "];" NL);
				debug_fprintf(ctx, NL);
			}
		}
		
		/* local argument outputs */
		for (int i = 0; i < node->num_outputs(); ++i) {
			ConstOutputKey key = node->output(i);
			const NodeOutput *output = node->type->find_output(i);
			
			if (output->value_type == OUTPUT_LOCAL) {
				const NodeGraph::Input *graph_input = graph->get_input(output->name);
				
				assert(graph_input);
				if (graph_input->key.node) {
					const NodeInstance *tail = key.node;
					const string &tail_socket = key.socket->name;
					int tail_index = output_index(tail, tail_socket);
					const NodeGraph::Input *head = graph_input;
					debug_fprintf(ctx, "\"node_%p\":\"O%s_%d\"", tail, tail_socket.c_str(), tail_index);
					debug_fprintf(ctx, " -> ");
					debug_fprintf(ctx, "\"input_%p\"", head);
					
					debug_fprintf(ctx, "[");
					/* Note: without label an id seem necessary to avoid bugs in graphviz/dot */
					debug_fprintf(ctx, "id=\"A%sB%s:O%s_%d\"",
					              head->name.c_str(),
					              tail->name.c_str(), tail_socket.c_str(), tail_index);
					
					debug_fprintf(ctx, ",penwidth=\"%f\"", penwidth);
					/* this lets us link back to argument nodes
					 * without disturbing other nodes' placement
					 */
					debug_fprintf(ctx, ",constraint=false");
					debug_fprintf(ctx, ",style=dashed");
					debug_fprintf(ctx, ",color=%s", localarg_color);
					debug_fprintf(ctx, "];" NL);
					debug_fprintf(ctx, NL);
				}
			}
		}
	}
	
	static int get_block_depth(const NodeBlock *block)
	{
		int depth = -1;
		while (block) {
			block = block->parent;
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
		int gray_level = max_ii(100 - depth * 10, 0);
		
		debug_fprintf(ctx, "subgraph \"cluster_%p\" {" NL, &block);
		debug_fprintf(ctx, "margin=\"%d\";" NL, 16);
		debug_fprintf(ctx, "style=\"%s\";" NL, style);
		debug_fprintf(ctx, "fillcolor=\"gray%d\"" NL, gray_level);
		
		for (NodeSet::const_iterator it = block.nodes.begin(); it != block.nodes.end(); ++it) {
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
			if (block.parent)
				block_children[block.parent].insert(&block);
		}
		if (!graph->blocks.empty()) {
			/* first block is main */
			dump_block(graph->blocks.front(), block_children);
		}
		
		for (NodeGraph::InputList::const_iterator it = graph->inputs.begin(); it != graph->inputs.end(); ++it) {
			const NodeGraph::Input *input = &(*it);
			dump_input_output(input, NULL);
		}
		for (NodeGraph::OutputList::const_iterator it = graph->outputs.begin(); it != graph->outputs.end(); ++it) {
			const NodeGraph::Output *output = &(*it);
			dump_input_output(NULL, output);
		}
		
		for (NodeGraph::NodeInstanceMap::const_iterator it = graph->nodes.begin(); it != graph->nodes.end(); ++it) {
			const NodeInstance *node = it->second;
			dump_node_links(graph, node);
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
} /* namespace bvm */

#endif  /* __BVM_UTIL_DEBUG_H__ */

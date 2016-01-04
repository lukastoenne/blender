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

static const char *debug_graphviz_fontname = "helvetica";
static float debug_graphviz_graph_label_size = 20.0f;
static float debug_graphviz_node_label_size = 14.0f;
static const char *debug_graphviz_node_color_function = "gainsboro";
static const char *debug_graphviz_node_color_kernel = "lightsalmon1";
static const char *debug_graphviz_node_color_pass = "gray96";
static const char *debug_graphviz_node_color_argument = "steelblue";
static const char *debug_graphviz_node_color_return_value = "orange";

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

inline static int debug_input_index(const NodeInstance *node, const string &name)
{
	for (int i = 0; i < node->type->num_inputs(); ++i) {
		if (node->type->find_input(i)->name == name)
			return i;
	}
	return -1;
}

inline static int debug_output_index(const NodeInstance *node, const string &name)
{
	for (int i = 0; i < node->type->num_outputs(); ++i) {
		if (node->type->find_output(i)->name == name)
			return i;
	}
	return -1;
}

static void debug_graphviz_node(const DebugContext &ctx, const NodeInstance *node)
{
	const char *shape = "box";
	const char *style = "filled,rounded";
	const char *color = "black";
	const char *fillcolor =
	        (node->type->is_pass_node()) ? debug_graphviz_node_color_pass :
	        (node->type->is_kernel_node()) ? debug_graphviz_node_color_kernel :
	        debug_graphviz_node_color_function;
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
	
	debug_fprintf(ctx, ",fontname=\"%s\"", debug_graphviz_fontname);
	debug_fprintf(ctx, ",fontsize=\"%f\"", debug_graphviz_node_label_size);
	debug_fprintf(ctx, ",shape=\"%s\"", shape);
	debug_fprintf(ctx, ",style=\"%s\"", style);
	debug_fprintf(ctx, ",color=\"%s\"", color);
	debug_fprintf(ctx, ",fillcolor=\"%s\"", fillcolor);
	debug_fprintf(ctx, ",penwidth=\"%f\"", penwidth);
	debug_fprintf(ctx, "];" NL);
	debug_fprintf(ctx, NL);
}

static void debug_graphviz_input_output(const DebugContext &ctx,
                                        const NodeGraph::Input *input,
                                        const NodeGraph::Output *output)
{
	string name = input ? input->name : output->name;
	string id = input ? "input" : "output";
	void *ptr = input ? (void *)input : (void *)output;
	{
		const char *shape = "box";
		const char *style = "filled,rounded";
		const char *color = "black";
		const char *fillcolor = input ?
		                            debug_graphviz_node_color_argument :
		                            debug_graphviz_node_color_return_value;
		float penwidth = 1.0f;
		
		debug_fprintf(ctx, "// %s\n", name.c_str());
		debug_fprintf(ctx, "\"%s_%p\"", id.c_str(), ptr);
		debug_fprintf(ctx, "[");
		
		/* html label including rows for input/output sockets
		 * http://www.graphviz.org/doc/info/shapes.html#html
		 */
		debug_fprintf(ctx, "label=\"%s\"", name.c_str());
		debug_fprintf(ctx, ",fontname=\"%s\"", debug_graphviz_fontname);
		debug_fprintf(ctx, ",fontsize=\"%f\"", debug_graphviz_node_label_size);
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
			const string &head_socket = key.socket;
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
			const string &tail_socket = key.socket;
			int tail_index = debug_output_index(tail, tail_socket);
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

static void debug_graphviz_node_links(const DebugContext &ctx, const NodeGraph *graph, const NodeInstance *node)
{
	float penwidth = 2.0f;
	const char *localarg_color = "gray50";
	
	for (NodeInstance::InputMap::const_iterator it = node->inputs.begin(); it != node->inputs.end(); ++it) {
		const NodeInstance::InputInstance &input = it->second;
		
		if (input.link_node && input.link_socket) {
			const NodeInstance *tail = input.link_node;
			const string &tail_socket = input.link_socket->name;
			int tail_index = debug_output_index(tail, tail_socket);
			const NodeInstance *head = node;
			const string &head_socket = it->first;
			int head_index = debug_input_index(head, head_socket);
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
				const string &tail_socket = key.socket;
				int tail_index = debug_output_index(tail, tail_socket);
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

inline void dump_graphviz(FILE *f, const NodeGraph *graph, const string &label)
{
	DebugContext ctx;
	ctx.file = f;
	
	debug_fprintf(ctx, "digraph depgraph {" NL);
	debug_fprintf(ctx, "rankdir=LR;" NL);
	debug_fprintf(ctx, "graph [");
	debug_fprintf(ctx, "labelloc=\"t\"");
	debug_fprintf(ctx, ",fontsize=%f", debug_graphviz_graph_label_size);
	debug_fprintf(ctx, ",fontname=\"%s\"", debug_graphviz_fontname);
	debug_fprintf(ctx, ",label=\"%s\"", label.c_str());
//	debug_fprintf(ctx, ",splines=ortho");
	debug_fprintf(ctx, "];" NL);

	for (NodeGraph::NodeInstanceMap::const_iterator it = graph->nodes.begin(); it != graph->nodes.end(); ++it) {
		const NodeInstance *node = it->second;
		debug_graphviz_node(ctx, node);
	}
	
	for (NodeGraph::InputList::const_iterator it = graph->inputs.begin(); it != graph->inputs.end(); ++it) {
		const NodeGraph::Input *input = &(*it);
		debug_graphviz_input_output(ctx, input, NULL);
	}
	for (NodeGraph::OutputList::const_iterator it = graph->outputs.begin(); it != graph->outputs.end(); ++it) {
		const NodeGraph::Output *output = &(*it);
		debug_graphviz_input_output(ctx, NULL, output);
	}
	
	for (NodeGraph::NodeInstanceMap::const_iterator it = graph->nodes.begin(); it != graph->nodes.end(); ++it) {
		const NodeInstance *node = it->second;
		debug_graphviz_node_links(ctx, graph, node);
	}

//	deg_debug_graphviz_legend(ctx);

	debug_fprintf(ctx, "}" NL);
}

#undef NL

} /* namespace debug */
} /* namespace bvm */

#endif  /* __BVM_UTIL_DEBUG_H__ */

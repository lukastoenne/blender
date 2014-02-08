/*
 * Copyright 2013, Blender Foundation.
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
 * Contributor: 
 *		Lukas Toenne
 */

#ifndef _COM_NodeCompiler_h
#define _COM_NodeCompiler_h

#include <map>
#include <vector>

extern "C" {
#include "DNA_node_types.h"
}

using std::vector;

class CompositorContext;
class ExecutionSystem;
class InputSocket;
class Node;
class NodeOperation;
class OutputSocket;
class SocketConnection;

class NodeGraph {
public:
	typedef vector<Node *> Nodes;
	typedef vector<SocketConnection *> Connections;
	
private:
	Nodes m_nodes;
	Connections m_connections;
	
public:
	NodeGraph();
	~NodeGraph();
	
	const Nodes &nodes() const { return m_nodes; }
	const Connections &connections() const { return m_connections; }
	
	void from_bNodeTree(const CompositorContext &context, bNodeTree *tree);
	
	void add_group_node_input_proxy(const CompositorContext &context, InputSocket *group_input, bNode *b_input_node, bNodeSocket *b_input_socket);
	void add_group_node_output_proxy(const CompositorContext &context, OutputSocket *group_output, bNode *b_output_node, bNodeSocket *b_output_socket);
	
protected:
	typedef vector<Node *> NodeList;
	typedef NodeList::iterator NodeIterator;
	typedef pair<NodeIterator, NodeIterator> NodeRange;
	
	void add_node(Node *node, bNodeTree *b_ntree, bNodeInstanceKey key, bool is_active_group);
	void add_connection(OutputSocket *fromSocket, InputSocket *toSocket);
	
	void add_bNodeTree(const CompositorContext &context, int nodes_start, bNodeTree *tree, bNodeInstanceKey parent_key);
	
	void add_bNode_mute_proxies(bNodeTree *b_ntree, bNode *b_node, bNodeInstanceKey key, bool is_active_group);
	void add_bNode_skip_proxies(bNodeTree *b_ntree, bNode *b_node, bNodeInstanceKey key, bool is_active_group);
	void add_group_node_input_proxies(bNode *b_node, bNode *b_node_io);
	void add_group_node_output_proxies(bNode *b_node, bNode *b_node_io, bool use_buffer);
	void add_node_group(const CompositorContext &context, bNode *b_node, bNodeInstanceKey key);
	void add_bNode(const CompositorContext &context, bNodeTree *b_ntree, bNode *b_node, bNodeInstanceKey key, bool is_active_group);
	
	static bNodeSocket *find_b_node_input(bNode *b_node, const char *identifier);
	static bNodeSocket *find_b_node_output(bNode *b_node, const char *identifier);
	
	InputSocket *find_input(const NodeRange &node_range, bNodeSocket *b_socket);
	OutputSocket *find_output(const NodeRange &node_range, bNodeSocket *b_socket);
	void add_bNodeLink(const NodeRange &node_range, bNodeLink *bNodeLink);
};

class NodeCompiler {
public:
	typedef std::map<InputSocket *, InputSocket *> InputSocketMap;
	typedef std::map<OutputSocket *, OutputSocket *> OutputSocketMap;
	
private:
	const CompositorContext *m_context;
	NodeGraph m_graph;
	
	InputSocketMap m_input_map;
	OutputSocketMap m_output_map;
	
	ExecutionSystem *m_current_system;
	Node *m_current_node;
	
public:
	NodeCompiler(const CompositorContext *context, bNodeTree *b_nodetree);
	~NodeCompiler();

	const CompositorContext &context() const { return *m_context; }

	void convertToOperations(ExecutionSystem *system);

	void addOperation(NodeOperation *operation);
	
	/** Map input socket of the current node to an operation socket */
	void mapInputSocket(InputSocket *node_socket, InputSocket *operation_socket);
	/** Map output socket of the current node to an operation socket */
	void mapOutputSocket(OutputSocket *node_socket, OutputSocket *operation_socket);
	/** Map all input sockets of the current node to operation sockets by index */
	void mapAllInputSockets(NodeOperation *operation);
	/** Map all output sockets of the current node to operation sockets by index */
	void mapAllOutputSockets(NodeOperation *operation);
	/** Map all input and output sockets of the current node to operation sockets by index */
	void mapAllSockets(NodeOperation *operation);
	
	void addConnection(OutputSocket *from, InputSocket *to);
	
protected:
	InputSocket *find_operation_input(InputSocket *node_input) const;
	OutputSocket *find_operation_output(OutputSocket *node_output) const;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:Converter")
#endif
};

#endif /* _COM_NodeCompiler_h */

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

#ifndef _COM_NodeGraph_h
#define _COM_NodeGraph_h

#include <map>
#include <set>
#include <vector>

extern "C" {
#include "DNA_node_types.h"
}

using std::vector;

class Node;
class InputSocket;
class OutputSocket;
class SocketConnection;

/** Internal representation of DNA node data.
 *  This structure is converted into operations by \a NodeCompiler.
 */
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
	
protected:
	typedef vector<Node *> NodeList;
	typedef NodeList::iterator NodeIterator;
	typedef pair<NodeIterator, NodeIterator> NodeRange;
	
	static bNodeSocket *find_b_node_input(bNode *b_node, const char *identifier);
	static bNodeSocket *find_b_node_output(bNode *b_node, const char *identifier);
	
	void add_node(Node *node, bNodeTree *b_ntree, bNodeInstanceKey key, bool is_active_group);
	void add_connection(OutputSocket *fromSocket, InputSocket *toSocket);
	
	void add_bNodeTree(const CompositorContext &context, int nodes_start, bNodeTree *tree, bNodeInstanceKey parent_key);
	
	void add_bNode(const CompositorContext &context, bNodeTree *b_ntree, bNode *b_node, bNodeInstanceKey key, bool is_active_group);
	
	InputSocket *find_input(const NodeRange &node_range, bNodeSocket *b_socket);
	OutputSocket *find_output(const NodeRange &node_range, bNodeSocket *b_socket);
	void add_bNodeLink(const NodeRange &node_range, bNodeLink *bNodeLink);
	
	/* **** Special proxy node type conversions **** */
	/* These nodes are not represented in the node graph themselves,
	 * but converted into a number of proxy connections
	 */
	
	void add_proxies_mute(bNodeTree *b_ntree, bNode *b_node, bNodeInstanceKey key, bool is_active_group);
	void add_proxies_skip(bNodeTree *b_ntree, bNode *b_node, bNodeInstanceKey key, bool is_active_group);
	
	void add_proxies_group_inputs(bNode *b_node, bNode *b_node_io);
	void add_proxies_group_outputs(bNode *b_node, bNode *b_node_io, bool use_buffer);
	void add_proxies_group(const CompositorContext &context, bNode *b_node, bNodeInstanceKey key);

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:NodeGraph")
#endif
};

#endif /* _COM_NodeGraph_h */

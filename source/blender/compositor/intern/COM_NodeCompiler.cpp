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

extern "C" {
#include "BLI_utildefines.h"

#include "DNA_node_types.h"

#include "BKE_node.h"
}

#include "COM_Converter.h"
#include "COM_Debug.h"
#include "COM_ExecutionSystem.h"
#include "COM_Node.h"
#include "COM_SocketConnection.h"
#include "COM_SocketProxyNode.h"

#include "COM_NodeCompiler.h" /* own include */


NodeGraph::NodeGraph()
{
}

NodeGraph::~NodeGraph()
{
	for (int index = 0; index < this->m_nodes.size(); index++) {
		Node *node = this->m_nodes[index];
		delete node;
	}

	for (int index = 0; index < this->m_connections.size(); index++) {
		SocketConnection *connection = this->m_connections[index];
		delete connection;
	}
}

void NodeGraph::from_bNodeTree(const CompositorContext &context, bNodeTree *tree)
{
	add_bNodeTree(context, 0, tree, NODE_INSTANCE_KEY_BASE);
}

void NodeGraph::add_node(Node *node, bNodeTree *b_ntree, bNodeInstanceKey key, bool is_active_group)
{
	node->setbNodeTree(b_ntree);
	node->setInstanceKey(key);
	node->setIsInActiveGroup(is_active_group);
	
	m_nodes.push_back(node);
	
	DebugInfo::node_added(node);
}

void NodeGraph::add_connection(OutputSocket *fromSocket, InputSocket *toSocket)
{
	m_connections.push_back(new SocketConnection(fromSocket, toSocket));
}

void NodeGraph::add_bNodeTree(const CompositorContext &context, int nodes_start, bNodeTree *tree, bNodeInstanceKey parent_key)
{
	const bNodeTree *basetree = context.getbNodeTree();
	
	/* update viewers in the active edittree as well the base tree (for backdrop) */
	bool is_active_group = ((parent_key.value == basetree->active_viewer_key.value) ||
	                        (tree == basetree));
	
	/* add all nodes of the tree to the node list */
	for (bNode *node = (bNode *)tree->nodes.first; node; node = node->next) {
		bNodeInstanceKey key = BKE_node_instance_key(parent_key, tree, node);
		add_bNode(context, tree, node, key, is_active_group);
	}

	NodeRange node_range(m_nodes.begin() + nodes_start, m_nodes.end());
	/* add all nodelinks of the tree to the link list */
	for (bNodeLink *nodelink = (bNodeLink *)tree->links.first; nodelink; nodelink = nodelink->next) {
		add_bNodeLink(node_range, nodelink);
	}
}

void NodeGraph::add_bNode_mute_proxies(bNodeTree *b_ntree, bNode *b_node, bNodeInstanceKey key, bool is_active_group)
{
	for (bNodeLink *b_link = (bNodeLink *)b_node->internal_links.first; b_link; b_link = b_link->next) {
		SocketProxyNode *proxy = new SocketProxyNode(b_node, b_link->fromsock, b_link->tosock);
		add_node(proxy, b_ntree, key, is_active_group);
	}
}

void NodeGraph::add_bNode_skip_proxies(bNodeTree *b_ntree, bNode *b_node, bNodeInstanceKey key, bool is_active_group)
{
	for (bNodeSocket *output = (bNodeSocket *)b_node->outputs.first; output; output = output->next) {
		bNodeSocket *input;
		
		/* look for first input with matching datatype for each output */
		for (input = (bNodeSocket *)b_node->inputs.first; input; input = input->next) {
			if (input->type == output->type)
				break;
		}
		
		if (input) {
			SocketProxyNode *proxy = new SocketProxyNode(b_node, input, output);
			add_node(proxy, b_ntree, key, is_active_group);
		}
	}
}

bNodeSocket *NodeGraph::find_b_node_input(bNode *b_group_node, const char *identifier)
{
	for (bNodeSocket *b_sock = (bNodeSocket *)b_group_node->inputs.first; b_sock; b_sock = b_sock->next) {
		if (STREQ(b_sock->identifier, identifier))
			return b_sock;
	}
	return NULL;
}

bNodeSocket *NodeGraph::find_b_node_output(bNode *b_group_node, const char *identifier)
{
	for (bNodeSocket *b_sock = (bNodeSocket *)b_group_node->outputs.first; b_sock; b_sock = b_sock->next) {
		if (STREQ(b_sock->identifier, identifier))
			return b_sock;
	}
	return NULL;
}

void NodeGraph::add_group_node_input_proxies(bNode *b_node, bNode *b_node_io)
{
	bNodeTree *b_group_tree = (bNodeTree *)b_node->id;
	BLI_assert(b_group_tree); /* should have been checked in advance */
	
	/* not important for proxies */
	bNodeInstanceKey key = NODE_INSTANCE_KEY_BASE;
	bool is_active_group = false;
	
	for (bNodeSocket *b_sock_io = (bNodeSocket *)b_node_io->outputs.first; b_sock_io; b_sock_io = b_sock_io->next) {
		bNodeSocket *b_sock_group = find_b_node_input(b_node, b_sock_io->identifier);
		if (b_sock_group) {
			SocketProxyNode *proxy = new SocketProxyNode(b_node_io, b_sock_group, b_sock_io);
			add_node(proxy, b_group_tree, key, is_active_group);
		}
	}
}

void NodeGraph::add_group_node_output_proxies(bNode *b_node, bNode *b_node_io, bool use_buffer)
{
	bNodeTree *b_group_tree = (bNodeTree *)b_node->id;
	BLI_assert(b_group_tree); /* should have been checked in advance */
	
	/* not important for proxies */
	bNodeInstanceKey key = NODE_INSTANCE_KEY_BASE;
	bool is_active_group = false;
	
	for (bNodeSocket *b_sock_io = (bNodeSocket *)b_node_io->inputs.first; b_sock_io; b_sock_io = b_sock_io->next) {
		bNodeSocket *b_sock_group = find_b_node_output(b_node, b_sock_io->identifier);
		if (b_sock_group) {
			if (use_buffer) {
				SocketBufferNode *buffer = new SocketBufferNode(b_node_io, b_sock_group, b_sock_io);
				add_node(buffer, b_group_tree, key, is_active_group);
			}
			else {
				SocketProxyNode *proxy = new SocketProxyNode(b_node_io, b_sock_group, b_sock_io);
				add_node(proxy, b_group_tree, key, is_active_group);
			}
		}
	}
}

void NodeGraph::add_node_group(const CompositorContext &context, bNode *b_node, bNodeInstanceKey key)
{
	bNodeTree *b_group_tree = (bNodeTree *)b_node->id;

	/* missing node group datablock can happen with library linking */
	if (!b_group_tree) {
		/* this error case its handled in convertToOperations() so we don't get un-convertred sockets */
		return;
	}

	/* use node list size before adding proxies, so they can be connected in add_bNodeTree */
	int nodes_start = m_nodes.size();

	/* create proxy nodes for group input/output nodes */
	for (bNode *b_node_io = (bNode *)b_group_tree->nodes.first; b_node_io; b_node_io = b_node_io->next) {
		if (b_node_io->type == NODE_GROUP_INPUT)
			add_group_node_input_proxies(b_node, b_node_io);
		
		if (b_node_io->type == NODE_GROUP_OUTPUT && (b_node_io->flag & NODE_DO_OUTPUT))
			add_group_node_output_proxies(b_node, b_node_io, context.isGroupnodeBufferEnabled());
	}
	
	add_bNodeTree(context, nodes_start, b_group_tree, key);
}

void NodeGraph::add_bNode(const CompositorContext &context, bNodeTree *b_ntree, bNode *b_node, bNodeInstanceKey key, bool is_active_group)
{
	/* replace muted nodes by proxies for internal links */
	if (b_node->flag & NODE_MUTED) {
		add_bNode_mute_proxies(b_ntree, b_node, key, is_active_group);
		return;
	}
	
	/* replace slow nodes with proxies for fast execution */
	if (context.isFastCalculation() && !Converter::is_fast_node(b_node)) {
		add_bNode_skip_proxies(b_ntree, b_node, key, is_active_group);
		return;
	}
	
	/* expand group nodes */
	if (b_node->type == NODE_GROUP) {
		add_node_group(context, b_node, key);
		return;
	}
	
	Node *node = Converter::convert(b_node);
	if (node)
		add_node(node, b_ntree, key, is_active_group);
}

InputSocket *NodeGraph::find_input(const NodeRange &node_range, bNodeSocket *b_socket)
{
	for (NodeGraph::NodeIterator it = node_range.first; it != node_range.second; ++it) {
		Node *node = *it;
		for (int index = 0; index < node->getNumberOfInputSockets(); index++) {
			InputSocket *input = node->getInputSocket(index);
			if (input->getbNodeSocket() == b_socket)
				return input;
		}
	}
	return NULL;
}

OutputSocket *NodeGraph::find_output(const NodeRange &node_range, bNodeSocket *b_socket)
{
	for (NodeGraph::NodeIterator it = node_range.first; it != node_range.second; ++it) {
		Node *node = *it;
		for (int index = 0; index < node->getNumberOfOutputSockets(); index++) {
			OutputSocket *output = node->getOutputSocket(index);
			if (output->getbNodeSocket() == b_socket)
				return output;
		}
	}
	return NULL;
}

void NodeGraph::add_bNodeLink(const NodeRange &node_range, bNodeLink *b_nodelink)
{
	/// @note: ignore invalid links
	if (!(b_nodelink->flag & NODE_LINK_VALID))
		return;

	InputSocket *input = find_input(node_range, b_nodelink->tosock);
	OutputSocket *output = find_output(node_range, b_nodelink->fromsock);
	if (!input || !output)
		return;
	if (input->isConnected())
		return;
	
	add_connection(output, input);
}


NodeCompiler::NodeCompiler(const CompositorContext *context, bNodeTree *b_nodetree) :
    m_context(context),
    m_current_system(NULL),
    m_current_node(NULL)
{
	m_graph.from_bNodeTree(*context, b_nodetree);
}

NodeCompiler::~NodeCompiler()
{
}

#ifndef NDEBUG
/* if this fails, there are still connection to/from this node,
 * which have not been properly relinked to operations!
 */
static void debug_check_node_connections(Node *node)
{
	/* note: connected inputs are not checked here,
	 * it would break quite a lot and such inputs are ignored later anyway
	 */
#if 0
	for (int i = 0; i < node->getNumberOfInputSockets(); ++i) {
		BLI_assert(!node->getInputSocket(i)->isConnected());
	}
#endif
	for (int i = 0; i < node->getNumberOfOutputSockets(); ++i) {
		BLI_assert(!node->getOutputSocket(i)->isConnected());
	}
}
#else
/* stub */
#define debug_check_node_connections(node)
#endif

void NodeCompiler::convertToOperations(ExecutionSystem *system)
{
	/* temporary pointer, so we don't have to pass it down
	 * through all the node functions
	 */
	m_current_system = system;
	
	for (int index = 0; index < m_graph.nodes().size(); index++) {
		Node *node = (Node *)m_graph.nodes()[index];
		
		m_current_node = node;
		
		DebugInfo::node_to_operations(node);
		node->convertToOperations(this, m_context);
		
		debug_check_node_connections(node);
	}
	
	m_current_node = NULL;
	
	/* The input map constructed by nodes maps operation inputs to node inputs.
	 * Inverting yields a map of node inputs to all connected operation inputs,
	 * so multiple operations can use the same node input.
	 */
	InputSocketInverseMap inverse_input_map;
	for (InputSocketMap::const_iterator it = m_input_map.begin(); it != m_input_map.end(); ++it)
		inverse_input_map[it->second].push_back(it->first);
	
	for (int index = 0; index < m_graph.connections().size(); index++) {
		SocketConnection *connection = m_graph.connections()[index];
		OutputSocket *from = connection->getFromSocket();
		InputSocket *to = connection->getToSocket();
		
		OutputSocket *op_from = find_operation_output(m_output_map, from);
		const InputSocketList &op_to_list = find_operation_inputs(inverse_input_map, to);
		if (!op_from || op_to_list.empty()) {
			/* XXX allow this? error/debug message? */
			BLI_assert(false);
			continue;
		}
		
		for (InputSocketList::const_iterator it = op_to_list.begin(); it != op_to_list.end(); ++it) {
			InputSocket *op_to = *it;
			
			if (op_from->getDataType() != op_to->getDataType()) {
				NodeOperation *converter = Converter::convertDataType(op_from, op_to);
				if (converter) {
					addConnection(op_from, converter->getInputSocket(0));
					addConnection(converter->getOutputSocket(0), op_to);
				}
			}
			else
				addConnection(op_from, op_to);
		}
	}
	
	system->determineResolutions();
	
	m_current_system = NULL;
}

void NodeCompiler::addOperation(NodeOperation *operation)
{
	BLI_assert(m_current_system);
	m_current_system->addOperation(operation);
}

void NodeCompiler::mapInputSocket(InputSocket *node_socket, InputSocket *operation_socket)
{
	BLI_assert(m_current_system);
	BLI_assert(m_current_node);
	
	/* note: this maps operation sockets to node sockets.
	 * for resolving links the map will be inverted first in convertToOperations,
	 * to get a list of connections for each node input socket.
	 */
	m_input_map[operation_socket] = node_socket;
}

void NodeCompiler::mapOutputSocket(OutputSocket *node_socket, OutputSocket *operation_socket)
{
	BLI_assert(m_current_system);
	BLI_assert(m_current_node);
	
	m_output_map[node_socket] = operation_socket;
}

void NodeCompiler::mapAllInputSockets(NodeOperation *operation)
{
	BLI_assert(m_current_system);
	BLI_assert(m_current_node);
	
	for (int index = 0; index < m_current_node->getNumberOfInputSockets(); ++index)
		mapInputSocket(m_current_node->getInputSocket(index), operation->getInputSocket(index));
}

void NodeCompiler::mapAllOutputSockets(NodeOperation *operation)
{
	BLI_assert(m_current_system);
	BLI_assert(m_current_node);
	
	for (int index = 0; index < m_current_node->getNumberOfOutputSockets(); ++index)
		mapOutputSocket(m_current_node->getOutputSocket(index), operation->getOutputSocket(index));
}

void NodeCompiler::mapAllSockets(NodeOperation *operation)
{
	mapAllInputSockets(operation);
	mapAllOutputSockets(operation);
}

void NodeCompiler::addConnection(OutputSocket *from, InputSocket *to)
{
	BLI_assert(m_current_system);
	
	m_current_system->addConnection(from, to);
}

const NodeCompiler::InputSocketList &NodeCompiler::find_operation_inputs(const InputSocketInverseMap &map, InputSocket *node_input)
{
	static const InputSocketList empty_list;
	InputSocketInverseMap::const_iterator it = map.find(node_input);
	return (it != map.end() ? it->second : empty_list);
}

OutputSocket *NodeCompiler::find_operation_output(const OutputSocketMap &map, OutputSocket *node_output)
{
	OutputSocketMap::const_iterator it = map.find(node_output);
	return (it != map.end() ? it->second : NULL);
}

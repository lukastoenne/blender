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
}

#include "COM_Converter.h"
#include "COM_Debug.h"
#include "COM_ExecutionSystem.h"
#include "COM_Node.h"
#include "COM_SocketConnection.h"
#include "COM_SocketProxyNode.h"

#include "COM_PreviewOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_SetVectorOperation.h"
#include "COM_SetColorOperation.h"
#include "COM_SocketProxyOperation.h"

#include "COM_NodeCompiler.h" /* own include */

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
	BLI_assert(node_socket->getNode() == m_current_node);
	
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
	BLI_assert(node_socket->getNode() == m_current_node);
	
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

PreviewOperation *NodeCompiler::make_preview_operation() const
{
	BLI_assert(m_current_system);
	BLI_assert(m_current_node);
	
	if (!(m_current_node->getbNode()->flag & NODE_PREVIEW))
		return NULL;
	/* previews only in the active group */
	if (!m_current_node->isInActiveGroup())
		return NULL;
	/* do not calculate previews of hidden nodes */
	if (m_current_node->getbNode()->flag & NODE_HIDDEN)
		return NULL;
	
	bNodeInstanceHash *previews = m_context->getPreviewHash();
	if (previews) {
		PreviewOperation *operation = new PreviewOperation(m_context->getViewSettings(), m_context->getDisplaySettings());
		operation->setbNode(m_current_node->getbNode());
		operation->setbNodeTree(m_context->getbNodeTree());
		operation->verifyPreview(previews, m_current_node->getInstanceKey());
		return operation;
	}
	
	return NULL;
}

void NodeCompiler::addInputPreview(InputSocket *input)
{
	PreviewOperation *operation = make_preview_operation();
	if (operation) {
		addOperation(operation);
		
		/* need to add a proxy, so we can pass input to preview as well */
		OutputSocket *output = addInputProxy(input);
		addConnection(output, operation->getInputSocket(0));
	}
}

void NodeCompiler::addOutputPreview(OutputSocket *output)
{
	PreviewOperation *operation = make_preview_operation();
	if (operation) {
		addOperation(operation);
		
		addConnection(output, operation->getInputSocket(0));
	}
}

#if 0 /* XXX is this used anywhere? can't see the logic in using input parameter here */
void Node::addPreviewOperation(InputSocket *inputSocket)
{
	if (inputSocket->isConnected() && this->isInActiveGroup()) {
		OutputSocket *outputsocket = inputSocket->getConnection()->getFromSocket();
		this->addPreviewOperation(system, context, outputsocket);
	}
}
#endif

NodeOperation *NodeCompiler::set_invalid_output(OutputSocket *output)
{
	const float warning_color[4] = {1.0f, 0.0f, 1.0f, 1.0f};
	
	SetColorOperation *operation = new SetColorOperation();
	operation->setChannels(warning_color);
	
	addOperation(operation);
	mapOutputSocket(output, operation->getOutputSocket());
	
	return operation;
}

void NodeCompiler::set_invalid_node()
{
	BLI_assert(m_current_system);
	BLI_assert(m_current_node);
	
	/* this is a really bad situation - bring on the pink! - so artists know this is bad */
	for (int index = 0; index < m_current_node->getNumberOfOutputSockets(); index++) {
		set_invalid_output(m_current_node->getOutputSocket(index));
	}
}

OutputSocket *NodeCompiler::addInputProxy(InputSocket *input)
{
	SocketProxyOperation *proxy = new SocketProxyOperation(input->getDataType());
	addOperation(proxy);
	
	mapInputSocket(input, proxy->getInputSocket(0));
	
	return proxy->getOutputSocket();
}

InputSocket *NodeCompiler::addOutputProxy(OutputSocket *output)
{
	SocketProxyOperation *proxy = new SocketProxyOperation(output->getDataType());
	addOperation(proxy);
	
	mapOutputSocket(output, proxy->getOutputSocket());
	
	return proxy->getInputSocket(0);
}

void NodeCompiler::addOutputValue(OutputSocket *output, float value)
{
	SetValueOperation *operation = new SetValueOperation();
	operation->setValue(value);
	
	addOperation(operation);
	mapOutputSocket(output, operation->getOutputSocket());
}

void NodeCompiler::addOutputColor(OutputSocket *output, const float value[4])
{
	SetColorOperation *operation = new SetColorOperation();
	operation->setChannels(value);
	
	addOperation(operation);
	mapOutputSocket(output, operation->getOutputSocket());
}

void NodeCompiler::addOutputVector(OutputSocket *output, const float value[3])
{
	SetVectorOperation *operation = new SetVectorOperation();
	operation->setVector(value);
	
	addOperation(operation);
	mapOutputSocket(output, operation->getOutputSocket());
}

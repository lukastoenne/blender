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
#include <set>
#include <vector>

#include "COM_NodeGraph.h"

using std::vector;

class CompositorContext;
class ExecutionSystem;
class InputSocket;
class Node;
class NodeOperation;
class OutputSocket;
class PreviewOperation;
class SocketConnection;

class NodeCompiler {
public:
	typedef std::map<InputSocket *, InputSocket *> InputSocketMap;
	typedef std::map<OutputSocket *, OutputSocket *> OutputSocketMap;
	
	typedef std::vector<InputSocket *> InputSocketList;
	typedef std::map<InputSocket *, InputSocketList> InputSocketInverseMap;
	
private:
	const CompositorContext *m_context;
	NodeGraph m_graph;
	
	/** Maps operation inputs to node inputs */
	InputSocketMap m_input_map;
	/** Maps node outputs to operation outputs */
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
	
	OutputSocket *addInputProxy(InputSocket *input);
	InputSocket *addOutputProxy(OutputSocket *output);
	
	void addOutputValue(OutputSocket *output, float value);
	void addOutputColor(OutputSocket *output, const float value[4]);
	void addOutputVector(OutputSocket *output, const float value[3]);
	
	void addConnection(OutputSocket *from, InputSocket *to);
	
	/** Add a preview operation for a node input */
	void addInputPreview(InputSocket *input);
	/** Add a preview operation for a node output */
	void addOutputPreview(OutputSocket *output);
	
	/** When a node has no valid data
	 *  @note missing image / group pointer, or missing renderlayer from EXR
	 */
	NodeOperation *set_invalid_output(OutputSocket *output);
	void set_invalid_node();
	
protected:
	static const InputSocketList &find_operation_inputs(const InputSocketInverseMap &map, InputSocket *node_input);
	static OutputSocket *find_operation_output(const OutputSocketMap &map, OutputSocket *node_output);
	
	void add_operation_input_constant(InputSocket *input);
	
private:
	PreviewOperation *make_preview_operation() const;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:NodeCompiler")
#endif
};

#endif /* _COM_NodeCompiler_h */

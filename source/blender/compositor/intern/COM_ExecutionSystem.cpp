/*
 * Copyright 2011, Blender Foundation.
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
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#include "COM_ExecutionSystem.h"

#include "PIL_time.h"
#include "BLI_utildefines.h"
extern "C" {
#include "BKE_node.h"
}

#include "COM_Converter.h"
#include "COM_NodeCompiler.h"
#include "COM_NodeOperation.h"
#include "COM_ExecutionGroup.h"
#include "COM_NodeBase.h"
#include "COM_WorkScheduler.h"
#include "COM_ReadBufferOperation.h"
#include "COM_WriteBufferOperation.h"
#include "COM_ReadBufferOperation.h"
#include "COM_ExecutionSystemHelper.h"
#include "COM_Debug.h"

#include "BKE_global.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

ExecutionSystem::ExecutionSystem(RenderData *rd, Scene *scene, bNodeTree *editingtree, bool rendering, bool fastcalculation,
                                 const ColorManagedViewSettings *viewSettings, const ColorManagedDisplaySettings *displaySettings)
{
	this->m_context.setScene(scene);
	this->m_context.setbNodeTree(editingtree);
	this->m_context.setPreviewHash(editingtree->previews);
	this->m_context.setFastCalculation(fastcalculation);
	/* initialize the CompositorContext */
	if (rendering) {
		this->m_context.setQuality((CompositorQuality)editingtree->render_quality);
	}
	else {
		this->m_context.setQuality((CompositorQuality)editingtree->edit_quality);
	}
	this->m_context.setRendering(rendering);
	this->m_context.setHasActiveOpenCLDevices(WorkScheduler::hasGPUDevices() && (editingtree->flag & NTREE_COM_OPENCL));

	this->m_context.setRenderData(rd);
	this->m_context.setViewSettings(viewSettings);
	this->m_context.setDisplaySettings(displaySettings);

	{
		NodeCompiler compiler(&m_context, editingtree);
		compiler.convertToOperations(this);
	}

	this->groupOperations(); /* group operations in ExecutionGroups */
	unsigned int index;
	unsigned int resolution[2];

	rctf *viewer_border = &editingtree->viewer_border;
	bool use_viewer_border = (editingtree->flag & NTREE_VIEWER_BORDER) &&
	                         viewer_border->xmin < viewer_border->xmax &&
	                         viewer_border->ymin < viewer_border->ymax;

	for (index = 0; index < this->m_groups.size(); index++) {
		resolution[0] = 0;
		resolution[1] = 0;
		ExecutionGroup *executionGroup = this->m_groups[index];
		executionGroup->determineResolution(resolution);

		if (rendering) {
			/* case when cropping to render border happens is handled in
			 * compositor output and render layer nodes
			 */
			if ((rd->mode & R_BORDER) && !(rd->mode & R_CROP)) {
				executionGroup->setRenderBorder(rd->border.xmin, rd->border.xmax,
				                                rd->border.ymin, rd->border.ymax);
			}
		}

		if (use_viewer_border) {
			executionGroup->setViewerBorder(viewer_border->xmin, viewer_border->xmax,
			                                viewer_border->ymin, viewer_border->ymax);
		}
	}

//	DebugInfo::graphviz(this);
}

ExecutionSystem::~ExecutionSystem()
{
	unsigned int index;
	for (index = 0; index < this->m_connections.size(); index++) {
		SocketConnection *connection = this->m_connections[index];
		delete connection;
	}
	this->m_connections.clear();
	for (index = 0; index < this->m_operations.size(); index++) {
		NodeOperation *operation = this->m_operations[index];
		delete operation;
	}
	this->m_operations.clear();
	for (index = 0; index < this->m_groups.size(); index++) {
		ExecutionGroup *group = this->m_groups[index];
		delete group;
	}
	this->m_groups.clear();
}

void ExecutionSystem::execute()
{
	DebugInfo::execute_started(this);
	
	unsigned int order = 0;
	for (vector<NodeOperation *>::iterator iter = this->m_operations.begin(); iter != this->m_operations.end(); ++iter) {
		NodeBase *node = *iter;
		NodeOperation *operation = (NodeOperation *) node;
		if (operation->isReadBufferOperation()) {
			ReadBufferOperation *readOperation = (ReadBufferOperation *)operation;
			readOperation->setOffset(order);
			order++;
		}
	}
	unsigned int index;

	for (index = 0; index < this->m_operations.size(); index++) {
		NodeOperation *operation = this->m_operations[index];
		operation->setbNodeTree(this->m_context.getbNodeTree());
		operation->initExecution();
	}
	for (index = 0; index < this->m_operations.size(); index++) {
		NodeOperation *operation = this->m_operations[index];
		if (operation->isReadBufferOperation()) {
			ReadBufferOperation *readOperation = (ReadBufferOperation *)operation;
			readOperation->updateMemoryBuffer();
		}
	}
	for (index = 0; index < this->m_groups.size(); index++) {
		ExecutionGroup *executionGroup = this->m_groups[index];
		executionGroup->setChunksize(this->m_context.getChunksize());
		executionGroup->initExecution();
	}

	WorkScheduler::start(this->m_context);

	executeGroups(COM_PRIORITY_HIGH);
	if (!this->getContext().isFastCalculation()) {
		executeGroups(COM_PRIORITY_MEDIUM);
		executeGroups(COM_PRIORITY_LOW);
	}

	WorkScheduler::finish();
	WorkScheduler::stop();

	for (index = 0; index < this->m_operations.size(); index++) {
		NodeOperation *operation = this->m_operations[index];
		operation->deinitExecution();
	}
	for (index = 0; index < this->m_groups.size(); index++) {
		ExecutionGroup *executionGroup = this->m_groups[index];
		executionGroup->deinitExecution();
	}
}

void ExecutionSystem::executeGroups(CompositorPriority priority)
{
	unsigned int index;
	vector<ExecutionGroup *> executionGroups;
	this->findOutputExecutionGroup(&executionGroups, priority);

	for (index = 0; index < executionGroups.size(); index++) {
		ExecutionGroup *group = executionGroups[index];
		group->execute(this);
	}
}

void ExecutionSystem::addOperation(NodeOperation *operation)
{
	ExecutionSystemHelper::addOperation(this->m_operations, operation);
	DebugInfo::operation_added(operation);
}

typedef std::vector<InputSocket*> InputSocketList;

/* helper function to store connected inputs for replacement */
static InputSocketList cache_output_connections(OutputSocket *output)
{
	int totconnections = output->getNumberOfConnections();
	InputSocketList targets;
	targets.reserve(totconnections);
	for (int index = 0; index < totconnections; ++index)
		targets.push_back(output->getConnection(index)->getToSocket());
	return targets;
}

void ExecutionSystem::addInputReadWriteBufferOperations(NodeOperation *operation, InputSocket *input)
{
	if (!input->isConnected())
		return;
	
	SocketConnection *connection = input->getConnection();
	if (connection->getFromOperation()->isReadBufferOperation()) {
		/* input is already buffered, no need to add another */
		return;
	}
	
	/* cache connected socket, so we can safely remove connection first before replacing it */
	OutputSocket *output = connection->getFromSocket();
	
	/* this connection will be replaced below */
	removeConnection(connection);
	
	/* check of other end already has write operation, otherwise add a new one */
	WriteBufferOperation *writeoperation = output->findAttachedWriteBufferOperation();
	if (!writeoperation) {
		writeoperation = new WriteBufferOperation();
		writeoperation->setbNodeTree(this->getContext().getbNodeTree());
		this->addOperation(writeoperation);
		
		addConnection(output, writeoperation->getInputSocket(0));
		
		writeoperation->readResolutionFromInputSocket();
	}
	
	/* add readbuffer op for the input */
	ReadBufferOperation *readoperation = new ReadBufferOperation();
	readoperation->setMemoryProxy(writeoperation->getMemoryProxy());
	this->addOperation(readoperation);
	
	addConnection(readoperation->getOutputSocket(), input);
	
	readoperation->readResolutionFromWriteBuffer();
}

void ExecutionSystem::addOutputReadWriteBufferOperations(NodeOperation *operation, OutputSocket *output)
{
	if (!output->isConnected())
		return;
	
	/* cache connected sockets, so we can safely remove connections first before replacing them */
	InputSocketList targets = cache_output_connections(output);
	
	/* remove all connections (avoid loop over output connections list while modifying) */
	for (int index = 0; index < targets.size(); ++index)
		removeConnection(targets[index]->getConnection());
	
	/* add new writebuffer op to the output */
	WriteBufferOperation *writeOperation = new WriteBufferOperation();
	writeOperation->setbNodeTree(this->getContext().getbNodeTree());
	addOperation(writeOperation);
	
	addConnection(output, writeOperation->getInputSocket(0));
	
	writeOperation->readResolutionFromInputSocket();
	
	/* add readbuffer op for every former connected input */
	for (int index = 0; index < targets.size(); ++index) {
		ReadBufferOperation *readoperation = new ReadBufferOperation();
		readoperation->setMemoryProxy(writeOperation->getMemoryProxy());
		addOperation(readoperation);
		
		addConnection(readoperation->getOutputSocket(), targets[index]);
	
		readoperation->readResolutionFromWriteBuffer();
	}
}

void ExecutionSystem::addReadWriteBufferOperations(NodeOperation *operation)
{
	DebugInfo::operation_read_write_buffer(operation);
	
	// for every input add write and read operation if input is not a read operation
	// only add read operation to other links when they are attached to buffered operations.
	for (int index = 0; index < operation->getNumberOfInputSockets(); index++) {
		addInputReadWriteBufferOperations(operation, operation->getInputSocket(index));
	}
	
	/* XXX this assumes there is only one relevant output socket! */
	addOutputReadWriteBufferOperations(operation, operation->getOutputSocket());
}

void ExecutionSystem::determineResolutions()
{
	// determine all resolutions of the operations (Width/Height)
	for (int index = 0; index < this->m_operations.size(); index++) {
		NodeOperation *operation = this->m_operations[index];
		if (operation->isOutputOperation(this->m_context.isRendering()) && !operation->isPreviewOperation()) {
			unsigned int resolution[2] = {0, 0};
			unsigned int preferredResolution[2] = {0, 0};
			operation->determineResolution(resolution, preferredResolution);
			operation->setResolution(resolution);
		}
	}
	for (int index = 0; index < this->m_operations.size(); index++) {
		NodeOperation *operation = this->m_operations[index];
		if (operation->isOutputOperation(this->m_context.isRendering()) && operation->isPreviewOperation()) {
			unsigned int resolution[2] = {0, 0};
			unsigned int preferredResolution[2] = {0, 0};
			operation->determineResolution(resolution, preferredResolution);
			operation->setResolution(resolution);
		}
	}
	
	// add convert resolution operations when needed.
	for (int index = 0; index < this->m_connections.size(); index++) {
		SocketConnection *connection = this->m_connections[index];
		if (connection->needsResolutionConversion())
			Converter::convertResolution(connection, this);
	}
}

void ExecutionSystem::groupOperations()
{
	vector<NodeOperation *> outputOperations;
	NodeOperation *operation;
	unsigned int index;
	// surround complex operations with ReadBufferOperation and WriteBufferOperation
	for (index = 0; index < this->m_operations.size(); index++) {
		operation = this->m_operations[index];
		if (operation->isComplex()) {
			this->addReadWriteBufferOperations(operation);
		}
	}
	ExecutionSystemHelper::findOutputNodeOperations(&outputOperations, this->getOperations(), this->m_context.isRendering());
	for (vector<NodeOperation *>::iterator iter = outputOperations.begin(); iter != outputOperations.end(); ++iter) {
		operation = *iter;
		ExecutionGroup *group = new ExecutionGroup();
		group->addOperation(this, operation);
		group->setOutputExecutionGroup(true);
		ExecutionSystemHelper::addExecutionGroup(this->getExecutionGroups(), group);
	}
}

SocketConnection *ExecutionSystem::addConnection(OutputSocket *from, InputSocket *to)
{
	if (to->isConnected())
		return NULL;
	
	SocketConnection *connection = new SocketConnection(from, to);
	m_connections.push_back(connection);
	return connection;
}

void ExecutionSystem::removeConnection(SocketConnection *connection)
{
	for (vector<SocketConnection *>::iterator it = m_connections.begin(); it != m_connections.end(); ++it) {
		if (*it == connection) {
			this->m_connections.erase(it);
			delete connection;
			break;
		}
	}
}

void ExecutionSystem::replaceInputConnections(InputSocket *old_input, InputSocket *new_input)
{
	if (!old_input->isConnected())
		return;
	
	OutputSocket *source = old_input->getConnection()->getFromSocket();
	
	removeConnection(old_input->getConnection());
	addConnection(source, new_input);
}

void ExecutionSystem::replaceOutputConnections(OutputSocket *old_output, OutputSocket *new_output)
{
	if (!old_output->isConnected())
		return;
	
	/* cache connected sockets, so we can safely remove connections first before replacing them */
	InputSocketList targets = cache_output_connections(old_output);
	
	/* remove all connections (avoid loop over output connections list while modifying)
	 * add connections to new output
	 */
	for (int index = 0; index < targets.size(); ++index) {
		removeConnection(targets[index]->getConnection());
		addConnection(new_output, targets[index]);
	}
}

void ExecutionSystem::findOutputExecutionGroup(vector<ExecutionGroup *> *result, CompositorPriority priority) const
{
	unsigned int index;
	for (index = 0; index < this->m_groups.size(); index++) {
		ExecutionGroup *group = this->m_groups[index];
		if (group->isOutputExecutionGroup() && group->getRenderPriotrity() == priority) {
			result->push_back(group);
		}
	}
}

void ExecutionSystem::findOutputExecutionGroup(vector<ExecutionGroup *> *result) const
{
	unsigned int index;
	for (index = 0; index < this->m_groups.size(); index++) {
		ExecutionGroup *group = this->m_groups[index];
		if (group->isOutputExecutionGroup()) {
			result->push_back(group);
		}
	}
}

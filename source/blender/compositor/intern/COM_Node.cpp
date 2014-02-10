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

#include <string.h>

#include "BKE_node.h"

#include "COM_ExecutionSystem.h"
#include "COM_NodeCompiler.h"
#include "COM_NodeOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_SetVectorOperation.h"
#include "COM_SetColorOperation.h"
#include "COM_SocketConnection.h"
#include "COM_TranslateOperation.h"

#include "COM_SocketProxyNode.h"

//#include <stdio.h>
#include "COM_defines.h"

#include "COM_Node.h" /* own include */

Node::Node(bNode *editorNode, bool create_sockets) : NodeBase()
{
	setbNode(editorNode);
	
	if (create_sockets) {
		bNodeSocket *input = (bNodeSocket *)editorNode->inputs.first;
		while (input != NULL) {
			DataType dt = COM_DT_VALUE;
			if (input->type == SOCK_RGBA) dt = COM_DT_COLOR;
			if (input->type == SOCK_VECTOR) dt = COM_DT_VECTOR;
			
			this->addInputSocket(dt, (InputSocketResizeMode)input->resizemode, input);
			input = input->next;
		}
		bNodeSocket *output = (bNodeSocket *)editorNode->outputs.first;
		while (output != NULL) {
			DataType dt = COM_DT_VALUE;
			if (output->type == SOCK_RGBA) dt = COM_DT_COLOR;
			if (output->type == SOCK_VECTOR) dt = COM_DT_VECTOR;
			
			this->addOutputSocket(dt, output);
			output = output->next;
		}
	}
}

void Node::addSetValueOperation(ExecutionSystem *system, InputSocket *inputsocket, int editorNodeInputSocketIndex)
{
	InputSocket *input = getInputSocket(editorNodeInputSocketIndex);
	SetValueOperation *operation = new SetValueOperation();
	operation->setValue(input->getEditorValueFloat());
	system->addOperation(operation);
	system->addConnection(operation->getOutputSocket(), inputsocket);
}

void Node::addSetColorOperation(ExecutionSystem *system, InputSocket *inputsocket, int editorNodeInputSocketIndex)
{
	InputSocket *input = getInputSocket(editorNodeInputSocketIndex);
	SetColorOperation *operation = new SetColorOperation();
	float col[4];
	input->getEditorValueColor(col);
	operation->setChannel1(col[0]);
	operation->setChannel2(col[1]);
	operation->setChannel3(col[2]);
	operation->setChannel4(col[3]);
	system->addOperation(operation);
	system->addConnection(operation->getOutputSocket(), inputsocket);
}

void Node::addSetVectorOperation(ExecutionSystem *system, InputSocket *inputsocket, int editorNodeInputSocketIndex)
{
	InputSocket *input = getInputSocket(editorNodeInputSocketIndex);
	SetVectorOperation *operation = new SetVectorOperation();
	float vec[3];
	input->getEditorValueVector(vec);
	operation->setX(vec[0]);
	operation->setY(vec[1]);
	operation->setZ(vec[2]);
	system->addOperation(operation);
	system->addConnection(operation->getOutputSocket(), inputsocket);
}

bNodeSocket *Node::getEditorInputSocket(int editorNodeInputSocketIndex)
{
	bNodeSocket *bSock = (bNodeSocket *)this->getbNode()->inputs.first;
	int index = 0;
	while (bSock != NULL) {
		if (index == editorNodeInputSocketIndex) {
			return bSock;
		}
		index++;
		bSock = bSock->next;
	}
	return NULL;
}
bNodeSocket *Node::getEditorOutputSocket(int editorNodeInputSocketIndex)
{
	bNodeSocket *bSock = (bNodeSocket *)this->getbNode()->outputs.first;
	int index = 0;
	while (bSock != NULL) {
		if (index == editorNodeInputSocketIndex) {
			return bSock;
		}
		index++;
		bSock = bSock->next;
	}
	return NULL;
}

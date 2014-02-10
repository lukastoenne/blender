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

#include "COM_TranslateNode.h"

#include "COM_TranslateOperation.h"
#include "COM_WrapOperation.h"
#include "COM_WriteBufferOperation.h"
#include "COM_ExecutionSystem.h"

TranslateNode::TranslateNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void TranslateNode::convertToOperations(NodeCompiler *compiler, const CompositorContext *context) const
{
	bNode *bnode = this->getbNode();
	NodeTranslateData *data = (NodeTranslateData *)bnode->storage;
	
	InputSocket *inputSocket = this->getInputSocket(0);
	InputSocket *inputXSocket = this->getInputSocket(1);
	InputSocket *inputYSocket = this->getInputSocket(2);
	OutputSocket *outputSocket = this->getOutputSocket(0);
	
	TranslateOperation *operation = new TranslateOperation();
	if (data->relative) {
		const RenderData *rd = context->getRenderData();
		float fx = rd->xsch * rd->size / 100.0f;
		float fy = rd->ysch * rd->size / 100.0f;
		
		operation->setFactorXY(fx, fy);
	}
	
	compiler->addOperation(operation);
	compiler->mapInputSocket(inputXSocket, operation->getInputSocket(1));
	compiler->mapInputSocket(inputYSocket, operation->getInputSocket(2));
	compiler->mapOutputSocket(outputSocket, operation->getOutputSocket(0));
	
	if (data->wrap_axis) {
		WriteBufferOperation *writeOperation = new WriteBufferOperation();
		WrapOperation *wrapOperation = new WrapOperation();
		wrapOperation->setMemoryProxy(writeOperation->getMemoryProxy());
		wrapOperation->setWrapping(data->wrap_axis);
		
		compiler->addOperation(writeOperation);
		compiler->addOperation(wrapOperation);
		compiler->mapInputSocket(inputSocket, writeOperation->getInputSocket(0));
		compiler->addConnection(wrapOperation->getOutputSocket(), operation->getInputSocket(0));
	}
	else {
		compiler->mapInputSocket(inputSocket, operation->getInputSocket(0));
	}
}


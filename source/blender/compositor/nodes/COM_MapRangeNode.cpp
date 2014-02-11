/*
 * Copyright 2012, Blender Foundation.
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
 *		Dalai Felinto
 *		Daniel Salazar
 */

#include "COM_MapRangeNode.h"

#include "COM_MapRangeOperation.h"
#include "COM_ExecutionSystem.h"

MapRangeNode::MapRangeNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void MapRangeNode::convertToOperations(NodeCompiler *compiler, const CompositorContext *context) const
{
	InputSocket *valueSocket = this->getInputSocket(0);
	InputSocket *sourceMinSocket = this->getInputSocket(1);
	InputSocket *sourceMaxSocket = this->getInputSocket(2);
	InputSocket *destMinSocket = this->getInputSocket(3);
	InputSocket *destMaxSocket = this->getInputSocket(4);
	OutputSocket *outputSocket = this->getOutputSocket(0);
	
	MapRangeOperation *operation = new MapRangeOperation();
	operation->setUseClamp(this->getbNode()->custom1);
	compiler->addOperation(operation);
	
	compiler->mapInputSocket(valueSocket, operation->getInputSocket(0));
	compiler->mapInputSocket(sourceMinSocket, operation->getInputSocket(1));
	compiler->mapInputSocket(sourceMaxSocket, operation->getInputSocket(2));
	compiler->mapInputSocket(destMinSocket, operation->getInputSocket(3));
	compiler->mapInputSocket(destMaxSocket, operation->getInputSocket(4));
	compiler->mapOutputSocket(outputSocket, operation->getOutputSocket(0));
}

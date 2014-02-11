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

#include "COM_IDMaskNode.h"
#include "COM_IDMaskOperation.h"
#include "COM_ExecutionSystem.h"
#include "COM_AntiAliasOperation.h"

IDMaskNode::IDMaskNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}
void IDMaskNode::convertToOperations(NodeCompiler *compiler, const CompositorContext *context) const
{
	bNode *bnode = this->getbNode();
	
	IDMaskOperation *operation;
	operation = new IDMaskOperation();
	operation->setObjectIndex(bnode->custom1);
	compiler->addOperation(operation);
	
	compiler->mapInputSocket(getInputSocket(0), operation->getInputSocket(0));
	if (bnode->custom2 == 0 || context->getRenderData()->scemode & R_FULL_SAMPLE) {
		compiler->mapOutputSocket(getOutputSocket(0), operation->getOutputSocket(0));
	}
	else {
		AntiAliasOperation *antiAliasOperation = new AntiAliasOperation();
		compiler->addOperation(antiAliasOperation);
		
		compiler->addConnection(operation->getOutputSocket(), antiAliasOperation->getInputSocket(0));
		compiler->mapOutputSocket(getOutputSocket(0), antiAliasOperation->getOutputSocket(0));
	}
}

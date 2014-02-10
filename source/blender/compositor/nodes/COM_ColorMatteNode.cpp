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
 *		Dalai Felinto
 */

#include "COM_ColorMatteNode.h"
#include "BKE_node.h"
#include "COM_ColorMatteOperation.h"
#include "COM_ConvertOperation.h"
#include "COM_SetAlphaOperation.h"

ColorMatteNode::ColorMatteNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void ColorMatteNode::convertToOperations(NodeCompiler *compiler, const CompositorContext *context) const
{
	bNode *editorsnode = getbNode();
	
	InputSocket *inputSocketImage = this->getInputSocket(0);
	InputSocket *inputSocketKey = this->getInputSocket(1);
	OutputSocket *outputSocketImage = this->getOutputSocket(0);
	OutputSocket *outputSocketMatte = this->getOutputSocket(1);
	
	ConvertRGBToHSVOperation *operationRGBToHSV_Image = new ConvertRGBToHSVOperation();
	ConvertRGBToHSVOperation *operationRGBToHSV_Key = new ConvertRGBToHSVOperation();
	compiler->addOperation(operationRGBToHSV_Image);
	compiler->addOperation(operationRGBToHSV_Key);
	
	ColorMatteOperation *operation = new ColorMatteOperation();
	operation->setSettings((NodeChroma *)editorsnode->storage);
	compiler->addOperation(operation);
	
	SetAlphaOperation *operationAlpha = new SetAlphaOperation();
	compiler->addOperation(operationAlpha);
	
	compiler->mapInputSocket(inputSocketImage, operationRGBToHSV_Image->getInputSocket(0));
	compiler->mapInputSocket(inputSocketKey, operationRGBToHSV_Key->getInputSocket(0));
	compiler->addConnection(operationRGBToHSV_Image->getOutputSocket(), operation->getInputSocket(0));
	compiler->addConnection(operationRGBToHSV_Key->getOutputSocket(), operation->getInputSocket(1));
	compiler->mapOutputSocket(outputSocketMatte, operation->getOutputSocket(0));
	
	compiler->mapInputSocket(inputSocketImage, operationAlpha->getInputSocket(0));
	compiler->addConnection(operation->getOutputSocket(), operationAlpha->getInputSocket(1));
	compiler->mapOutputSocket(outputSocketImage, operationAlpha->getOutputSocket());
	
	compiler->addOutputPreview(operationAlpha->getOutputSocket());
}

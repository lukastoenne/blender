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

#include "COM_DistanceMatteNode.h"
#include "BKE_node.h"
#include "COM_DistanceRGBMatteOperation.h"
#include "COM_DistanceYCCMatteOperation.h"
#include "COM_SetAlphaOperation.h"
#include "COM_ConvertOperation.h"

DistanceMatteNode::DistanceMatteNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void DistanceMatteNode::convertToOperations(NodeCompiler *compiler, const CompositorContext *context) const
{
	bNode *editorsnode = getbNode();
	NodeChroma *storage = (NodeChroma *)editorsnode->storage;
	
	InputSocket *inputSocketImage = this->getInputSocket(0);
	InputSocket *inputSocketKey = this->getInputSocket(1);
	OutputSocket *outputSocketImage = this->getOutputSocket(0);
	OutputSocket *outputSocketMatte = this->getOutputSocket(1);
	
	SetAlphaOperation *operationAlpha = new SetAlphaOperation();
	compiler->addOperation(operationAlpha);
	
	/* work in RGB color space */
	NodeOperation *operation;
	if (storage->channel == 1) {
		DistanceRGBMatteOperation *matte = new DistanceRGBMatteOperation();
		matte->setSettings(storage);
		compiler->addOperation(matte);
		
		compiler->mapInputSocket(inputSocketImage, matte->getInputSocket(0));
		compiler->mapInputSocket(inputSocketImage, operationAlpha->getInputSocket(0));
		
		compiler->mapInputSocket(inputSocketKey, matte->getInputSocket(1));
		
		operation = matte;
	}
	/* work in YCbCr color space */
	else {
		DistanceYCCMatteOperation *matte = new DistanceYCCMatteOperation();
		matte->setSettings(storage);
		compiler->addOperation(matte);
		
		ConvertRGBToYCCOperation *operationYCCImage = new ConvertRGBToYCCOperation();
		ConvertRGBToYCCOperation *operationYCCMatte = new ConvertRGBToYCCOperation();
		compiler->addOperation(operationYCCImage);
		compiler->addOperation(operationYCCMatte);
		
		compiler->mapInputSocket(inputSocketImage, operationYCCImage->getInputSocket(0));
		compiler->addConnection(operationYCCImage->getOutputSocket(), matte->getInputSocket(0));
		compiler->addConnection(operationYCCImage->getOutputSocket(), operationAlpha->getInputSocket(0));
		
		compiler->mapInputSocket(inputSocketKey, operationYCCMatte->getInputSocket(0));
		compiler->addConnection(operationYCCMatte->getOutputSocket(), matte->getInputSocket(1));
		
		operation = matte;
	}
	
	compiler->addConnection(operation->getOutputSocket(), operationAlpha->getInputSocket(1));
	
	compiler->mapOutputSocket(outputSocketMatte, operation->getOutputSocket());
	compiler->mapOutputSocket(outputSocketImage, operationAlpha->getOutputSocket());
	
	compiler->addOutputPreview(operationAlpha->getOutputSocket());
}

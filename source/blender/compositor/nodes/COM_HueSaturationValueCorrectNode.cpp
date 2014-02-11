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

#include "COM_HueSaturationValueCorrectNode.h"

#include "COM_ConvertOperation.h"
#include "COM_ExecutionSystem.h"
#include "COM_MixOperation.h"
#include "COM_SetColorOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_ChangeHSVOperation.h"
#include "DNA_node_types.h"
#include "COM_HueSaturationValueCorrectOperation.h"

HueSaturationValueCorrectNode::HueSaturationValueCorrectNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void HueSaturationValueCorrectNode::convertToOperations(NodeCompiler *compiler, const CompositorContext *context) const
{
	InputSocket *valueSocket = this->getInputSocket(0);
	InputSocket *colorSocket = this->getInputSocket(1);
	OutputSocket *outputSocket = this->getOutputSocket(0);
	bNode *editorsnode = getbNode();
	CurveMapping *storage = (CurveMapping *)editorsnode->storage;
	
	ConvertRGBToHSVOperation *rgbToHSV = new ConvertRGBToHSVOperation();
	compiler->addOperation(rgbToHSV);
	
	ConvertHSVToRGBOperation *hsvToRGB = new ConvertHSVToRGBOperation();
	compiler->addOperation(hsvToRGB);
	
	HueSaturationValueCorrectOperation *changeHSV = new HueSaturationValueCorrectOperation();
	changeHSV->setCurveMapping(storage);
	compiler->addOperation(changeHSV);
	
	MixBlendOperation *blend = new MixBlendOperation();
	blend->setResolutionInputSocketIndex(1);
	compiler->addOperation(blend);

	compiler->mapInputSocket(colorSocket, rgbToHSV->getInputSocket(0));
	compiler->addConnection(rgbToHSV->getOutputSocket(), changeHSV->getInputSocket(0));
	compiler->addConnection(changeHSV->getOutputSocket(), hsvToRGB->getInputSocket(0));
	compiler->addConnection(hsvToRGB->getOutputSocket(), blend->getInputSocket(2));
	compiler->mapInputSocket(colorSocket, blend->getInputSocket(1));
	compiler->mapInputSocket(valueSocket, blend->getInputSocket(0));
	compiler->mapOutputSocket(outputSocket, blend->getOutputSocket());
}

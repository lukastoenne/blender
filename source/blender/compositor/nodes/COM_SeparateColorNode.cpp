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
 *		Lukas Toenne
 */

#include "COM_SeparateColorNode.h"

#include "COM_ConvertOperation.h"


SeparateColorNode::SeparateColorNode(bNode *editorNode) :
    Node(editorNode)
{
}

void SeparateColorNode::convertToOperations(NodeCompiler *compiler, const CompositorContext *context) const
{
	InputSocket *imageSocket = this->getInputSocket(0);
	OutputSocket *outputRSocket = this->getOutputSocket(0);
	OutputSocket *outputGSocket = this->getOutputSocket(1);
	OutputSocket *outputBSocket = this->getOutputSocket(2);
	OutputSocket *outputASocket = this->getOutputSocket(3);
	
	NodeOperation *converter = getColorConverter(context);
	if (converter) {
		compiler->addOperation(converter);
		
		compiler->mapInputSocket(imageSocket, converter->getInputSocket(0));
	}
	
	{
		SeparateChannelOperation *operation = new SeparateChannelOperation();
		operation->setChannel(0);
		compiler->addOperation(operation);
		
		if (converter)
			compiler->addConnection(converter->getOutputSocket(), operation->getInputSocket(0));
		else
			compiler->mapInputSocket(imageSocket, operation->getInputSocket(0));
		compiler->mapOutputSocket(outputRSocket, operation->getOutputSocket(0));
	}
	
	{
		SeparateChannelOperation *operation = new SeparateChannelOperation();
		operation->setChannel(1);
		compiler->addOperation(operation);
		
		if (converter)
			compiler->addConnection(converter->getOutputSocket(), operation->getInputSocket(0));
		else
			compiler->mapInputSocket(imageSocket, operation->getInputSocket(0));
		compiler->mapOutputSocket(outputGSocket, operation->getOutputSocket(0));
	}
	
	{
		SeparateChannelOperation *operation = new SeparateChannelOperation();
		operation->setChannel(2);
		compiler->addOperation(operation);
		
		if (converter)
			compiler->addConnection(converter->getOutputSocket(), operation->getInputSocket(0));
		else
			compiler->mapInputSocket(imageSocket, operation->getInputSocket(0));
		compiler->mapOutputSocket(outputBSocket, operation->getOutputSocket(0));
	}
	
	{
		SeparateChannelOperation *operation = new SeparateChannelOperation();
		operation->setChannel(3);
		compiler->addOperation(operation);
		
		if (converter)
			compiler->addConnection(converter->getOutputSocket(), operation->getInputSocket(0));
		else
			compiler->mapInputSocket(imageSocket, operation->getInputSocket(0));
		compiler->mapOutputSocket(outputASocket, operation->getOutputSocket(0));
	}
}


NodeOperation *SeparateRGBANode::getColorConverter(const CompositorContext *context) const
{
	return NULL; /* no conversion needed */
}

NodeOperation *SeparateHSVANode::getColorConverter(const CompositorContext *context) const
{
	return new ConvertRGBToHSVOperation();
}

NodeOperation *SeparateYCCANode::getColorConverter(const CompositorContext *context) const
{
	return new ConvertRGBToYCCOperation();
}

NodeOperation *SeparateYUVANode::getColorConverter(const CompositorContext *context) const
{
	return new ConvertRGBToYUVOperation();
}

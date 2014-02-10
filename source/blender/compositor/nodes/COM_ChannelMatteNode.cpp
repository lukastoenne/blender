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
 */

#include "COM_ChannelMatteNode.h"
#include "BKE_node.h"
#include "COM_ChannelMatteOperation.h"
#include "COM_ConvertOperation.h"
#include "COM_SetAlphaOperation.h"

ChannelMatteNode::ChannelMatteNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void ChannelMatteNode::convertToOperations(NodeCompiler *compiler, const CompositorContext *context) const
{
	bNode *node = this->getbNode();
	
	InputSocket *inputSocketImage = this->getInputSocket(0);
	OutputSocket *outputSocketImage = this->getOutputSocket(0);
	OutputSocket *outputSocketMatte = this->getOutputSocket(1);
	
	NodeOperation *convert = NULL;
	/* colorspace */
	switch (node->custom1) {
		case CMP_NODE_CHANNEL_MATTE_CS_RGB:
			break;
		case CMP_NODE_CHANNEL_MATTE_CS_HSV: /* HSV */
			convert = new ConvertRGBToHSVOperation();
			break;
		case CMP_NODE_CHANNEL_MATTE_CS_YUV: /* YUV */
			convert = new ConvertRGBToYUVOperation();
			break;
		case CMP_NODE_CHANNEL_MATTE_CS_YCC: /* YCC */
			convert = new ConvertRGBToYCCOperation();
			((ConvertRGBToYCCOperation *)convert)->setMode(0); /* BLI_YCC_ITU_BT601 */
			break;
		default:
			break;
	}
	
	ChannelMatteOperation *operation = new ChannelMatteOperation();
	/* pass the ui properties to the operation */
	operation->setSettings((NodeChroma *)node->storage, node->custom2);
	compiler->addOperation(operation);
	
	SetAlphaOperation *operationAlpha = new SetAlphaOperation();
	compiler->addOperation(operationAlpha);
	
	if (convert) {
		compiler->addOperation(convert);
		
		compiler->mapInputSocket(inputSocketImage, convert->getInputSocket(0));
		compiler->addConnection(convert->getOutputSocket(), operation->getInputSocket(0));
		compiler->addConnection(convert->getOutputSocket(), operationAlpha->getInputSocket(0));
	}
	else {
		compiler->mapInputSocket(inputSocketImage, operation->getInputSocket(0));
		compiler->mapInputSocket(inputSocketImage, operationAlpha->getInputSocket(0));
	}
	
	compiler->mapOutputSocket(outputSocketMatte, operation->getOutputSocket(0));
	
	compiler->addConnection(operation->getOutputSocket(), operationAlpha->getInputSocket(1));
	compiler->mapOutputSocket(outputSocketImage, operationAlpha->getOutputSocket());
	
	compiler->addOutputPreview(operationAlpha->getOutputSocket());
}

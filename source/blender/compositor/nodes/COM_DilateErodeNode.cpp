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

#include "COM_DilateErodeNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_DilateErodeOperation.h"
#include "COM_AntiAliasOperation.h"
#include "COM_GaussianAlphaXBlurOperation.h"
#include "COM_GaussianAlphaYBlurOperation.h"
#include "BLI_math.h"

DilateErodeNode::DilateErodeNode(bNode *editorNode) : Node(editorNode)
{
	/* initialize node data */
	NodeBlurData *data = &m_alpha_blur;
	memset(data, 0, sizeof(NodeBlurData));
	data->filtertype = R_FILTER_GAUSS;

	if (editorNode->custom2 > 0) {
		data->sizex = data->sizey = editorNode->custom2;
	}
	else {
		data->sizex = data->sizey = -editorNode->custom2;
	}
}

void DilateErodeNode::convertToOperations(NodeCompiler *compiler, const CompositorContext *context) const
{
	
	bNode *editorNode = this->getbNode();
	if (editorNode->custom1 == CMP_NODE_DILATEERODE_DISTANCE_THRESH) {
		DilateErodeThresholdOperation *operation = new DilateErodeThresholdOperation();
		operation->setbNode(editorNode);
		operation->setDistance(editorNode->custom2);
		operation->setInset(editorNode->custom3);
		compiler->addOperation(operation);
		
		compiler->mapInputSocket(getInputSocket(0), operation->getInputSocket(0));
	
		if (editorNode->custom3 < 2.0f) {
			AntiAliasOperation *antiAlias = new AntiAliasOperation();
			compiler->addOperation(antiAlias);
			
			compiler->addConnection(operation->getOutputSocket(), antiAlias->getInputSocket(0));
			compiler->mapOutputSocket(getOutputSocket(0), antiAlias->getOutputSocket(0));
		}
		else {
			compiler->mapOutputSocket(getOutputSocket(0), operation->getOutputSocket(0));
		}
	}
	else if (editorNode->custom1 == CMP_NODE_DILATEERODE_DISTANCE) {
		if (editorNode->custom2 > 0) {
			DilateDistanceOperation *operation = new DilateDistanceOperation();
			operation->setbNode(editorNode);
			operation->setDistance(editorNode->custom2);
			compiler->addOperation(operation);
			
			compiler->mapInputSocket(getInputSocket(0), operation->getInputSocket(0));
			compiler->mapOutputSocket(getOutputSocket(0), operation->getOutputSocket(0));
		}
		else {
			ErodeDistanceOperation *operation = new ErodeDistanceOperation();
			operation->setbNode(editorNode);
			operation->setDistance(-editorNode->custom2);
			compiler->addOperation(operation);
			
			compiler->mapInputSocket(getInputSocket(0), operation->getInputSocket(0));
			compiler->mapOutputSocket(getOutputSocket(0), operation->getOutputSocket(0));
		}
	}
	else if (editorNode->custom1 == CMP_NODE_DILATEERODE_DISTANCE_FEATHER) {
		/* this uses a modified gaussian blur function otherwise its far too slow */
		CompositorQuality quality = context->getQuality();

		GaussianAlphaXBlurOperation *operationx = new GaussianAlphaXBlurOperation();
		operationx->setbNode(editorNode);
		operationx->setData(&m_alpha_blur);
		operationx->setQuality(quality);
		operationx->setFalloff(PROP_SMOOTH);
		compiler->addOperation(operationx);
		
		compiler->mapInputSocket(getInputSocket(0), operationx->getInputSocket(0));
		// compiler->mapInputSocket(getInputSocket(1), operationx->getInputSocket(1)); // no size input yet
		
		GaussianAlphaYBlurOperation *operationy = new GaussianAlphaYBlurOperation();
		operationy->setbNode(editorNode);
		operationy->setData(&m_alpha_blur);
		operationy->setQuality(quality);
		operationy->setFalloff(PROP_SMOOTH);
		compiler->addOperation(operationy);
		
		compiler->addConnection(operationx->getOutputSocket(), operationy->getInputSocket(0));
		// compiler->mapInputSocket(getInputSocket(1), operationy->getInputSocket(1)); // no size input yet
		compiler->mapOutputSocket(getOutputSocket(0), operationy->getOutputSocket());
		
		compiler->addOutputPreview(operationy->getOutputSocket());

		/* TODO? */
		/* see gaussian blue node for original usage */
#if 0
		if (!connectedSizeSocket) {
			operationx->setSize(size);
			operationy->setSize(size);
		}
#else
		operationx->setSize(1.0f);
		operationy->setSize(1.0f);
#endif
		operationx->setSubtract(editorNode->custom2 < 0);
		operationy->setSubtract(editorNode->custom2 < 0);

		if (editorNode->storage) {
			NodeDilateErode *data_storage = (NodeDilateErode *)editorNode->storage;
			operationx->setFalloff(data_storage->falloff);
			operationy->setFalloff(data_storage->falloff);
		}
	}
	else {
		if (editorNode->custom2 > 0) {
			DilateStepOperation *operation = new DilateStepOperation();
			operation->setbNode(editorNode);
			operation->setIterations(editorNode->custom2);
			compiler->addOperation(operation);
			
			compiler->mapInputSocket(getInputSocket(0), operation->getInputSocket(0));
			compiler->mapOutputSocket(getOutputSocket(0), operation->getOutputSocket(0));
		}
		else {
			ErodeStepOperation *operation = new ErodeStepOperation();
			operation->setbNode(editorNode);
			operation->setIterations(-editorNode->custom2);
			compiler->addOperation(operation);
			
			compiler->mapInputSocket(getInputSocket(0), operation->getInputSocket(0));
			compiler->mapOutputSocket(getOutputSocket(0), operation->getOutputSocket(0));
		}
	}
}

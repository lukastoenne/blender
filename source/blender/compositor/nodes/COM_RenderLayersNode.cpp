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

#include "COM_RenderLayersNode.h"
#include "COM_RenderLayersProg.h"
#include "COM_TranslateOperation.h"
#include "COM_RotateOperation.h"
#include "COM_ScaleOperation.h"
#include "COM_SetValueOperation.h"

RenderLayersNode::RenderLayersNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void RenderLayersNode::testSocketConnection(NodeCompiler *compiler, const CompositorContext *context,
                                            int outputSocketNumber, RenderLayersBaseProg *operation) const
{
	OutputSocket *outputSocket = this->getOutputSocket(outputSocketNumber);
	Scene *scene = (Scene *)this->getbNode()->id;
	short layerId = this->getbNode()->custom1;

	if (outputSocket->isConnected()) {
		operation->setScene(scene);
		operation->setLayerId(layerId);
		operation->setRenderData(context->getRenderData());
		
		compiler->mapOutputSocket(outputSocket, operation->getOutputSocket());
		compiler->addOperation(operation);
		
		if (outputSocketNumber == 0) { // only do for image socket if connected
			compiler->addOutputPreview(operation->getOutputSocket());
		}
	}
	else {
		if (outputSocketNumber == 0) {
			operation->setScene(scene);
			operation->setLayerId(layerId);
			operation->setRenderData(context->getRenderData());
			
			compiler->addOperation(operation);
			
			compiler->addOutputPreview(operation->getOutputSocket());
		}
		else {
			delete operation;
		}
	}
}

void RenderLayersNode::convertToOperations(NodeCompiler *compiler, const CompositorContext *context) const
{
	testSocketConnection(compiler, context, 0, new RenderLayersColorProg());
	testSocketConnection(compiler, context, 1, new RenderLayersAlphaProg());
	testSocketConnection(compiler, context, 2, new RenderLayersDepthProg());
	testSocketConnection(compiler, context, 3, new RenderLayersNormalOperation());
	testSocketConnection(compiler, context, 4, new RenderLayersUVOperation());
	testSocketConnection(compiler, context, 5, new RenderLayersSpeedOperation());
	testSocketConnection(compiler, context, 6, new RenderLayersColorOperation());
	testSocketConnection(compiler, context, 7, new RenderLayersDiffuseOperation());
	testSocketConnection(compiler, context, 8, new RenderLayersSpecularOperation());
	testSocketConnection(compiler, context, 9, new RenderLayersShadowOperation());
	testSocketConnection(compiler, context, 10, new RenderLayersAOOperation());
	testSocketConnection(compiler, context, 11, new RenderLayersReflectionOperation());
	testSocketConnection(compiler, context, 12, new RenderLayersRefractionOperation());
	testSocketConnection(compiler, context, 13, new RenderLayersIndirectOperation());
	testSocketConnection(compiler, context, 14, new RenderLayersObjectIndexOperation());
	testSocketConnection(compiler, context, 15, new RenderLayersMaterialIndexOperation());
	testSocketConnection(compiler, context, 16, new RenderLayersMistOperation());
	testSocketConnection(compiler, context, 17, new RenderLayersEmitOperation());
	testSocketConnection(compiler, context, 18, new RenderLayersEnvironmentOperation());
	
	// cycles passes
	testSocketConnection(compiler, context, 19, new RenderLayersCyclesOperation(SCE_PASS_DIFFUSE_DIRECT));
	testSocketConnection(compiler, context, 20, new RenderLayersCyclesOperation(SCE_PASS_DIFFUSE_INDIRECT));
	testSocketConnection(compiler, context, 21, new RenderLayersCyclesOperation(SCE_PASS_DIFFUSE_COLOR));
	testSocketConnection(compiler, context, 22, new RenderLayersCyclesOperation(SCE_PASS_GLOSSY_DIRECT));
	testSocketConnection(compiler, context, 23, new RenderLayersCyclesOperation(SCE_PASS_GLOSSY_INDIRECT));
	testSocketConnection(compiler, context, 24, new RenderLayersCyclesOperation(SCE_PASS_GLOSSY_COLOR));
	testSocketConnection(compiler, context, 25, new RenderLayersCyclesOperation(SCE_PASS_TRANSM_DIRECT));
	testSocketConnection(compiler, context, 26, new RenderLayersCyclesOperation(SCE_PASS_TRANSM_INDIRECT));
	testSocketConnection(compiler, context, 27, new RenderLayersCyclesOperation(SCE_PASS_TRANSM_COLOR));
	testSocketConnection(compiler, context, 28, new RenderLayersCyclesOperation(SCE_PASS_SUBSURFACE_DIRECT));
	testSocketConnection(compiler, context, 29, new RenderLayersCyclesOperation(SCE_PASS_SUBSURFACE_INDIRECT));
	testSocketConnection(compiler, context, 30, new RenderLayersCyclesOperation(SCE_PASS_SUBSURFACE_COLOR));
}

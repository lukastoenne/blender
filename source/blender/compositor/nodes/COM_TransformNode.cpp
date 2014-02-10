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

#include "COM_TransformNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_TranslateOperation.h"
#include "COM_RotateOperation.h"
#include "COM_ScaleOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_SetSamplerOperation.h"

TransformNode::TransformNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void TransformNode::convertToOperations(NodeCompiler *compiler, const CompositorContext *context) const
{
	InputSocket *imageInput = this->getInputSocket(0);
	InputSocket *xInput = this->getInputSocket(1);
	InputSocket *yInput = this->getInputSocket(2);
	InputSocket *angleInput = this->getInputSocket(3);
	InputSocket *scaleInput = this->getInputSocket(4);
	
	ScaleOperation *scaleOperation = new ScaleOperation();
	compiler->addOperation(scaleOperation);
	
	RotateOperation *rotateOperation = new RotateOperation();
	rotateOperation->setDoDegree2RadConversion(false);
	compiler->addOperation(rotateOperation);
	
	TranslateOperation *translateOperation = new TranslateOperation();
	compiler->addOperation(translateOperation);
	
	SetSamplerOperation *sampler = new SetSamplerOperation();
	sampler->setSampler((PixelSampler)this->getbNode()->custom1);
	compiler->addOperation(sampler);
	
	compiler->mapInputSocket(imageInput, sampler->getInputSocket(0));
	compiler->addConnection(sampler->getOutputSocket(), scaleOperation->getInputSocket(0));
	compiler->mapInputSocket(scaleInput, scaleOperation->getInputSocket(1));
	compiler->addConnection(sampler->getOutputSocket(), scaleOperation->getInputSocket(2)); // xscale = yscale
	
	compiler->addConnection(scaleOperation->getOutputSocket(), rotateOperation->getInputSocket(0));
	compiler->mapInputSocket(angleInput, rotateOperation->getInputSocket(1));
	
	compiler->addConnection(rotateOperation->getOutputSocket(), translateOperation->getInputSocket(0));
	compiler->mapInputSocket(xInput, translateOperation->getInputSocket(1));
	compiler->mapInputSocket(yInput, translateOperation->getInputSocket(2));
	
	compiler->mapOutputSocket(getOutputSocket(), translateOperation->getOutputSocket());
}

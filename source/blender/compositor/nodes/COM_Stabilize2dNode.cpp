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

#include "COM_Stabilize2dNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_TranslateOperation.h"
#include "COM_RotateOperation.h"
#include "COM_ScaleOperation.h"
#include "COM_MovieClipAttributeOperation.h"
#include "COM_SetSamplerOperation.h"

extern "C" {
#  include "DNA_movieclip_types.h"
#  include "BKE_tracking.h"
}

Stabilize2dNode::Stabilize2dNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void Stabilize2dNode::convertToOperations(NodeCompiler *compiler, const CompositorContext *context) const
{
	InputSocket *imageInput = this->getInputSocket(0);
	MovieClip *clip = (MovieClip *)getbNode()->id;
	
	ScaleOperation *scaleOperation = new ScaleOperation();
	scaleOperation->setSampler((PixelSampler)this->getbNode()->custom1);
	RotateOperation *rotateOperation = new RotateOperation();
	rotateOperation->setDoDegree2RadConversion(false);
	TranslateOperation *translateOperation = new TranslateOperation();
	MovieClipAttributeOperation *scaleAttribute = new MovieClipAttributeOperation();
	MovieClipAttributeOperation *angleAttribute = new MovieClipAttributeOperation();
	MovieClipAttributeOperation *xAttribute = new MovieClipAttributeOperation();
	MovieClipAttributeOperation *yAttribute = new MovieClipAttributeOperation();
	SetSamplerOperation *psoperation = new SetSamplerOperation();
	psoperation->setSampler((PixelSampler)this->getbNode()->custom1);
	
	scaleAttribute->setAttribute(MCA_SCALE);
	scaleAttribute->setFramenumber(context->getFramenumber());
	scaleAttribute->setMovieClip(clip);
	
	angleAttribute->setAttribute(MCA_ANGLE);
	angleAttribute->setFramenumber(context->getFramenumber());
	angleAttribute->setMovieClip(clip);
	
	xAttribute->setAttribute(MCA_X);
	xAttribute->setFramenumber(context->getFramenumber());
	xAttribute->setMovieClip(clip);
	
	yAttribute->setAttribute(MCA_Y);
	yAttribute->setFramenumber(context->getFramenumber());
	yAttribute->setMovieClip(clip);
	
	compiler->addOperation(scaleAttribute);
	compiler->addOperation(angleAttribute);
	compiler->addOperation(xAttribute);
	compiler->addOperation(yAttribute);
	compiler->addOperation(scaleOperation);
	compiler->addOperation(translateOperation);
	compiler->addOperation(rotateOperation);
	compiler->addOperation(psoperation);
	
	compiler->mapInputSocket(imageInput, scaleOperation->getInputSocket(0));
	compiler->addConnection(scaleAttribute->getOutputSocket(), scaleOperation->getInputSocket(1));
	compiler->addConnection(scaleAttribute->getOutputSocket(), scaleOperation->getInputSocket(2));
	
	compiler->addConnection(scaleOperation->getOutputSocket(), rotateOperation->getInputSocket(0));
	compiler->addConnection(angleAttribute->getOutputSocket(), rotateOperation->getInputSocket(1));

	compiler->addConnection(rotateOperation->getOutputSocket(), translateOperation->getInputSocket(0));
	compiler->addConnection(xAttribute->getOutputSocket(), translateOperation->getInputSocket(1));
	compiler->addConnection(yAttribute->getOutputSocket(), translateOperation->getInputSocket(2));
	
	compiler->addConnection(translateOperation->getOutputSocket(), psoperation->getInputSocket(0));
	compiler->mapOutputSocket(getOutputSocket(), psoperation->getOutputSocket());
}

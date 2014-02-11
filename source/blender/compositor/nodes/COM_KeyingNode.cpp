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
 *		Jeroen Bakker
 *		Monique Dewanchand
 *		Sergey Sharybin
 */

#include "COM_KeyingNode.h"

#include "COM_ExecutionSystem.h"

#include "COM_KeyingOperation.h"
#include "COM_KeyingBlurOperation.h"
#include "COM_KeyingDespillOperation.h"
#include "COM_KeyingClipOperation.h"

#include "COM_MathBaseOperation.h"

#include "COM_ConvertOperation.h"
#include "COM_SetValueOperation.h"

#include "COM_DilateErodeOperation.h"

#include "COM_SetAlphaOperation.h"

#include "COM_GaussianAlphaXBlurOperation.h"
#include "COM_GaussianAlphaYBlurOperation.h"

KeyingNode::KeyingNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

OutputSocket *KeyingNode::setupPreBlur(NodeCompiler *compiler, InputSocket *inputImage, int size) const
{
	ConvertRGBToYCCOperation *convertRGBToYCCOperation = new ConvertRGBToYCCOperation();
	convertRGBToYCCOperation->setMode(0);  /* ITU 601 */
	compiler->addOperation(convertRGBToYCCOperation);
	
	compiler->mapInputSocket(inputImage, convertRGBToYCCOperation->getInputSocket(0));
	
	CombineChannelsOperation *combineOperation = new CombineChannelsOperation();
	compiler->addOperation(combineOperation);

	for (int channel = 0; channel < 4; channel++) {
		SeparateChannelOperation *separateOperation = new SeparateChannelOperation();
		separateOperation->setChannel(channel);
		compiler->addOperation(separateOperation);
		
		compiler->addConnection(convertRGBToYCCOperation->getOutputSocket(0), separateOperation->getInputSocket(0));
		
		if (channel == 0 || channel == 3) {
			compiler->addConnection(separateOperation->getOutputSocket(0), combineOperation->getInputSocket(channel));
		}
		else {
			KeyingBlurOperation *blurXOperation = new KeyingBlurOperation();
			blurXOperation->setSize(size);
			blurXOperation->setAxis(KeyingBlurOperation::BLUR_AXIS_X);
			blurXOperation->setbNode(this->getbNode());
			compiler->addOperation(blurXOperation);
			
			KeyingBlurOperation *blurYOperation = new KeyingBlurOperation();
			blurYOperation->setSize(size);
			blurYOperation->setAxis(KeyingBlurOperation::BLUR_AXIS_Y);
			blurYOperation->setbNode(this->getbNode());
			compiler->addOperation(blurYOperation);
			
			compiler->addConnection(separateOperation->getOutputSocket(), blurXOperation->getInputSocket(0));
			compiler->addConnection(blurXOperation->getOutputSocket(), blurYOperation->getInputSocket(0));
			compiler->addConnection(blurYOperation->getOutputSocket(0), combineOperation->getInputSocket(channel));
		}
	}
	
	ConvertYCCToRGBOperation *convertYCCToRGBOperation = new ConvertYCCToRGBOperation();
	convertYCCToRGBOperation->setMode(0);  /* ITU 601 */
	compiler->addOperation(convertYCCToRGBOperation);
	
	compiler->addConnection(combineOperation->getOutputSocket(0), convertYCCToRGBOperation->getInputSocket(0));
	
	return convertYCCToRGBOperation->getOutputSocket(0);
}

OutputSocket *KeyingNode::setupPostBlur(NodeCompiler *compiler, OutputSocket *postBlurInput, int size) const
{
	KeyingBlurOperation *blurXOperation = new KeyingBlurOperation();
	blurXOperation->setSize(size);
	blurXOperation->setAxis(KeyingBlurOperation::BLUR_AXIS_X);
	blurXOperation->setbNode(this->getbNode());
	compiler->addOperation(blurXOperation);
	
	KeyingBlurOperation *blurYOperation = new KeyingBlurOperation();
	blurYOperation->setSize(size);
	blurYOperation->setAxis(KeyingBlurOperation::BLUR_AXIS_Y);
	blurYOperation->setbNode(this->getbNode());
	compiler->addOperation(blurYOperation);
	
	compiler->addConnection(postBlurInput, blurXOperation->getInputSocket(0));
	compiler->addConnection(blurXOperation->getOutputSocket(), blurYOperation->getInputSocket(0));
	
	return blurYOperation->getOutputSocket();
}

OutputSocket *KeyingNode::setupDilateErode(NodeCompiler *compiler, OutputSocket *dilateErodeInput, int distance) const
{
	DilateDistanceOperation *dilateErodeOperation;
	if (distance > 0) {
		dilateErodeOperation = new DilateDistanceOperation();
		dilateErodeOperation->setDistance(distance);
	}
	else {
		dilateErodeOperation = new ErodeDistanceOperation();
		dilateErodeOperation->setDistance(-distance);
	}
	dilateErodeOperation->setbNode(this->getbNode());
	compiler->addOperation(dilateErodeOperation);
	
	compiler->addConnection(dilateErodeInput, dilateErodeOperation->getInputSocket(0));
	
	return dilateErodeOperation->getOutputSocket(0);
}

OutputSocket *KeyingNode::setupFeather(NodeCompiler *compiler, const CompositorContext *context,
                                       OutputSocket *featherInput, int falloff, int distance) const
{
	/* this uses a modified gaussian blur function otherwise its far too slow */
	CompositorQuality quality = context->getQuality();

	/* initialize node data */
	NodeBlurData data;
	memset(&data, 0, sizeof(NodeBlurData));
	data.filtertype = R_FILTER_GAUSS;
	if (distance > 0) {
		data.sizex = data.sizey = distance;
	}
	else {
		data.sizex = data.sizey = -distance;
	}

	GaussianAlphaXBlurOperation *operationx = new GaussianAlphaXBlurOperation();
	operationx->setData(&data);
	operationx->setQuality(quality);
	operationx->setSize(1.0f);
	operationx->setSubtract(distance < 0);
	operationx->setFalloff(falloff);
	operationx->setbNode(this->getbNode());
	compiler->addOperation(operationx);
	
	GaussianAlphaYBlurOperation *operationy = new GaussianAlphaYBlurOperation();
	operationy->setData(&data);
	operationy->setQuality(quality);
	operationy->setSize(1.0f);
	operationy->setSubtract(distance < 0);
	operationy->setFalloff(falloff);
	operationy->setbNode(this->getbNode());
	compiler->addOperation(operationy);

	compiler->addConnection(featherInput, operationx->getInputSocket(0));
	compiler->addConnection(operationx->getOutputSocket(), operationy->getInputSocket(0));

	return operationy->getOutputSocket();
}

OutputSocket *KeyingNode::setupDespill(NodeCompiler *compiler, OutputSocket *despillInput, OutputSocket *inputScreen,
                                       float factor, float colorBalance) const
{
	KeyingDespillOperation *despillOperation = new KeyingDespillOperation();
	despillOperation->setDespillFactor(factor);
	despillOperation->setColorBalance(colorBalance);
	compiler->addOperation(despillOperation);
	
	compiler->addConnection(despillInput, despillOperation->getInputSocket(0));
	compiler->addConnection(inputScreen, despillOperation->getInputSocket(1));
	
	return despillOperation->getOutputSocket(0);
}

OutputSocket *KeyingNode::setupClip(NodeCompiler *compiler, OutputSocket *clipInput, int kernelRadius, float kernelTolerance,
                                    float clipBlack, float clipWhite, bool edgeMatte) const
{
	KeyingClipOperation *clipOperation = new KeyingClipOperation();
	clipOperation->setKernelRadius(kernelRadius);
	clipOperation->setKernelTolerance(kernelTolerance);
	clipOperation->setClipBlack(clipBlack);
	clipOperation->setClipWhite(clipWhite);
	clipOperation->setIsEdgeMatte(edgeMatte);
	compiler->addOperation(clipOperation);
	
	compiler->addConnection(clipInput, clipOperation->getInputSocket(0));
	
	return clipOperation->getOutputSocket(0);
}

void KeyingNode::convertToOperations(NodeCompiler *compiler, const CompositorContext *context) const
{
	bNode *editorNode = this->getbNode();
	NodeKeyingData *keying_data = (NodeKeyingData *) editorNode->storage;
	
	InputSocket *inputImage = this->getInputSocket(0);
	InputSocket *inputScreen = this->getInputSocket(1);
	InputSocket *inputGarbageMatte = this->getInputSocket(2);
	InputSocket *inputCoreMatte = this->getInputSocket(3);
	OutputSocket *outputImage = this->getOutputSocket(0);
	OutputSocket *outputMatte = this->getOutputSocket(1);
	OutputSocket *outputEdges = this->getOutputSocket(2);
	OutputSocket *postprocessedMatte = NULL, *postprocessedImage = NULL, *edgesMatte = NULL;
	
	/* keying operation */
	KeyingOperation *keyingOperation = new KeyingOperation();
	keyingOperation->setScreenBalance(keying_data->screen_balance);
	compiler->addOperation(keyingOperation);
	
	compiler->mapInputSocket(inputScreen, keyingOperation->getInputSocket(1));
	
	if (keying_data->blur_pre) {
		/* chroma preblur operation for input of keying operation  */
		OutputSocket *preBluredImage = setupPreBlur(compiler, inputImage, keying_data->blur_pre);
		compiler->addConnection(preBluredImage, keyingOperation->getInputSocket(0));
	}
	else {
		compiler->mapInputSocket(inputImage, keyingOperation->getInputSocket(0));
	}
	
	postprocessedMatte = keyingOperation->getOutputSocket();
	
	/* black / white clipping */
	if (keying_data->clip_black > 0.0f || keying_data->clip_white < 1.0f) {
		postprocessedMatte = setupClip(compiler, postprocessedMatte,
		                               keying_data->edge_kernel_radius, keying_data->edge_kernel_tolerance,
		                               keying_data->clip_black, keying_data->clip_white, false);
	}
	
	/* output edge matte */
	edgesMatte = setupClip(compiler, postprocessedMatte,
	                       keying_data->edge_kernel_radius, keying_data->edge_kernel_tolerance,
	                       keying_data->clip_black, keying_data->clip_white, true);
	
	/* apply garbage matte */
	if (inputGarbageMatte->isConnected()) {
		SetValueOperation *valueOperation = new SetValueOperation();
		valueOperation->setValue(1.0f);
		compiler->addOperation(valueOperation);
		
		MathSubtractOperation *subtractOperation = new MathSubtractOperation();
		compiler->addOperation(subtractOperation);
		
		MathMinimumOperation *minOperation = new MathMinimumOperation();
		compiler->addOperation(minOperation);
		
		compiler->addConnection(valueOperation->getOutputSocket(), subtractOperation->getInputSocket(0));
		compiler->mapInputSocket(inputGarbageMatte, subtractOperation->getInputSocket(1));
		
		compiler->addConnection(subtractOperation->getOutputSocket(), minOperation->getInputSocket(0));
		compiler->addConnection(postprocessedMatte, minOperation->getInputSocket(1));
		
		postprocessedMatte = minOperation->getOutputSocket();
	}
	
	/* apply core matte */
	if (inputCoreMatte->isConnected()) {
		MathMaximumOperation *maxOperation = new MathMaximumOperation();
		compiler->addOperation(maxOperation);
		
		compiler->mapInputSocket(inputCoreMatte, maxOperation->getInputSocket(0));
		compiler->addConnection(postprocessedMatte, maxOperation->getInputSocket(1));
		
		postprocessedMatte = maxOperation->getOutputSocket();
	}
	
	/* apply blur on matte if needed */
	if (keying_data->blur_post)
		postprocessedMatte = setupPostBlur(compiler, postprocessedMatte, keying_data->blur_post);

	/* matte dilate/erode */
	if (keying_data->dilate_distance != 0) {
		postprocessedMatte = setupDilateErode(compiler, postprocessedMatte, keying_data->dilate_distance);
	}

	/* matte feather */
	if (keying_data->feather_distance != 0) {
		postprocessedMatte = setupFeather(compiler, context, postprocessedMatte, keying_data->feather_falloff,
		                                  keying_data->feather_distance);
	}

	/* set alpha channel to output image */
	SetAlphaOperation *alphaOperation = new SetAlphaOperation();
	compiler->addOperation(alphaOperation);
	
	compiler->mapInputSocket(inputImage, alphaOperation->getInputSocket(0));
	compiler->addConnection(postprocessedMatte, alphaOperation->getInputSocket(1));

	postprocessedImage = alphaOperation->getOutputSocket();

	/* despill output image */
	if (keying_data->despill_factor > 0.0f) {
		postprocessedImage = setupDespill(compiler, postprocessedImage,
		                                  keyingOperation->getInputSocket(1)->getConnection()->getFromSocket(),
		                                  keying_data->despill_factor,
		                                  keying_data->despill_balance);
	}

	/* connect result to output sockets */
	compiler->mapOutputSocket(outputImage, postprocessedImage);
	compiler->mapOutputSocket(outputMatte, postprocessedMatte);

	if (edgesMatte)
		compiler->mapOutputSocket(outputEdges, edgesMatte);
}

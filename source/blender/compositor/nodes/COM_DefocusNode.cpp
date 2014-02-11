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

#include "COM_DefocusNode.h"
#include "DNA_scene_types.h"
#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_node_types.h"
#include "COM_ExecutionSystem.h"
#include "COM_ConvertDepthToRadiusOperation.h"
#include "COM_VariableSizeBokehBlurOperation.h"
#include "COM_BokehImageOperation.h"
#include "COM_MathBaseOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_GammaCorrectOperation.h"
#include "COM_FastGaussianBlurOperation.h"

DefocusNode::DefocusNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void DefocusNode::convertToOperations(NodeCompiler *compiler, const CompositorContext *context) const
{
	bNode *node = this->getbNode();
	NodeDefocus *data = (NodeDefocus *)node->storage;
	Scene *scene = node->id ? (Scene *)node->id : context->getScene();
	Object *camob = scene ? scene->camera : NULL;

	NodeOperation *radiusOperation;
	if (data->no_zbuf) {
		MathMultiplyOperation *multiply = new MathMultiplyOperation();
		SetValueOperation *multiplier = new SetValueOperation();
		multiplier->setValue(data->scale);
		SetValueOperation *maxRadius = new SetValueOperation();
		maxRadius->setValue(data->maxblur);
		MathMinimumOperation *minimize = new MathMinimumOperation();
		
		compiler->addOperation(multiply);
		compiler->addOperation(multiplier);
		compiler->addOperation(maxRadius);
		compiler->addOperation(minimize);
		
		compiler->mapInputSocket(getInputSocket(1), multiply->getInputSocket(0));
		compiler->addConnection(multiplier->getOutputSocket(), multiply->getInputSocket(1));
		compiler->addConnection(multiply->getOutputSocket(), minimize->getInputSocket(0));
		compiler->addConnection(maxRadius->getOutputSocket(), minimize->getInputSocket(1));
		
		radiusOperation = minimize;
	}
	else {
		ConvertDepthToRadiusOperation *converter = new ConvertDepthToRadiusOperation();
		converter->setCameraObject(camob);
		converter->setfStop(data->fstop);
		converter->setMaxRadius(data->maxblur);
		compiler->addOperation(converter);
		
		compiler->mapInputSocket(getInputSocket(1), converter->getInputSocket(0));
		
		FastGaussianBlurValueOperation *blur = new FastGaussianBlurValueOperation();
		/* maintain close pixels so far Z values don't bleed into the foreground */
		blur->setOverlay(FAST_GAUSS_OVERLAY_MIN);
		compiler->addOperation(blur);
		
		compiler->addConnection(converter->getOutputSocket(0), blur->getInputSocket(0));
		converter->setPostBlur(blur);
		
		radiusOperation = blur;
	}
	
	NodeBokehImage *bokehdata = new NodeBokehImage();
	bokehdata->angle = data->rotation;
	bokehdata->rounding = 0.0f;
	bokehdata->flaps = data->bktype;
	if (data->bktype < 3) {
		bokehdata->flaps = 5;
		bokehdata->rounding = 1.0f;
	}
	bokehdata->catadioptric = 0.0f;
	bokehdata->lensshift = 0.0f;
	
	BokehImageOperation *bokeh = new BokehImageOperation();
	bokeh->setData(bokehdata);
	bokeh->deleteDataOnFinish();
	compiler->addOperation(bokeh);
	
#ifdef COM_DEFOCUS_SEARCH
	InverseSearchRadiusOperation *search = new InverseSearchRadiusOperation();
	search->setMaxBlur(data->maxblur);
	compiler->addOperation(search);
	
	compiler->addConnection(radiusOperation->getOutputSocket(0), search->getInputSocket(0));
#endif
	
	VariableSizeBokehBlurOperation *operation = new VariableSizeBokehBlurOperation();
	if (data->preview)
		operation->setQuality(COM_QUALITY_LOW);
	else
		operation->setQuality(context->getQuality());
	operation->setMaxBlur(data->maxblur);
	operation->setbNode(node);
	operation->setThreshold(data->bthresh);
	compiler->addOperation(operation);
	
	compiler->addConnection(bokeh->getOutputSocket(), operation->getInputSocket(1));
	compiler->addConnection(radiusOperation->getOutputSocket(), operation->getInputSocket(2));
#ifdef COM_DEFOCUS_SEARCH
	compiler->addConnection(search->getOutputSocket(), operation->getInputSocket(3));
#endif
	
	if (data->gamco) {
		GammaCorrectOperation *correct = new GammaCorrectOperation();
		compiler->addOperation(correct);
		GammaUncorrectOperation *inverse = new GammaUncorrectOperation();
		compiler->addOperation(inverse);
		
		compiler->mapInputSocket(getInputSocket(0), correct->getInputSocket(0));
		compiler->addConnection(correct->getOutputSocket(), operation->getInputSocket(0));
		compiler->addConnection(operation->getOutputSocket(), inverse->getInputSocket(0));
		compiler->mapOutputSocket(getOutputSocket(), inverse->getOutputSocket());
	}
	else {
		compiler->mapInputSocket(getInputSocket(0), operation->getInputSocket(0));
		compiler->mapOutputSocket(getOutputSocket(), operation->getOutputSocket());
	}
}

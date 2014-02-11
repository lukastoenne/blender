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

#include "COM_ZCombineNode.h"

#include "COM_ZCombineOperation.h"

#include "COM_ExecutionSystem.h"
#include "COM_SetValueOperation.h"
#include "COM_MathBaseOperation.h"
#include "COM_AntiAliasOperation.h"
#include "COM_MixOperation.h"

#include "DNA_material_types.h" // the ramp types

void ZCombineNode::convertToOperations(NodeCompiler *compiler, const CompositorContext *context) const
{
	if ((context->getRenderData()->scemode & R_FULL_SAMPLE) || this->getbNode()->custom2) {
		ZCombineOperation *operation = NULL;
		if (this->getbNode()->custom1) {
			operation = new ZCombineAlphaOperation();
		}
		else {
			operation = new ZCombineOperation();
		}
		compiler->addOperation(operation);
		
		compiler->mapInputSocket(getInputSocket(0), operation->getInputSocket(0));
		compiler->mapInputSocket(getInputSocket(1), operation->getInputSocket(1));
		compiler->mapInputSocket(getInputSocket(2), operation->getInputSocket(2));
		compiler->mapInputSocket(getInputSocket(3), operation->getInputSocket(3));
		compiler->mapOutputSocket(getOutputSocket(0), operation->getOutputSocket());
		
		MathMinimumOperation *zoperation = new MathMinimumOperation();
		compiler->addOperation(zoperation);
		
		compiler->mapInputSocket(getInputSocket(1), zoperation->getInputSocket(0));
		compiler->mapInputSocket(getInputSocket(3), zoperation->getInputSocket(1));
		compiler->mapOutputSocket(getOutputSocket(1), zoperation->getOutputSocket());
	}
	else {
		/* XXX custom1 is "use_alpha", what on earth is this supposed to do here?!? */
		// not full anti alias, use masking for Z combine. be aware it uses anti aliasing.
		// step 1 create mask
		NodeOperation *maskoperation;
		if (this->getbNode()->custom1) {
			maskoperation = new MathGreaterThanOperation();
			compiler->addOperation(maskoperation);
			
			compiler->mapInputSocket(getInputSocket(1), maskoperation->getInputSocket(0));
			compiler->mapInputSocket(getInputSocket(3), maskoperation->getInputSocket(1));
		}
		else {
			maskoperation = new MathLessThanOperation();
			compiler->addOperation(maskoperation);
			
			compiler->mapInputSocket(getInputSocket(1), maskoperation->getInputSocket(0));
			compiler->mapInputSocket(getInputSocket(3), maskoperation->getInputSocket(1));
		}

		// step 2 anti alias mask bit of an expensive operation, but does the trick
		AntiAliasOperation *antialiasoperation = new AntiAliasOperation();
		compiler->addOperation(antialiasoperation);
		
		compiler->addConnection(maskoperation->getOutputSocket(), antialiasoperation->getInputSocket(0));

		// use mask to blend between the input colors.
		ZCombineMaskOperation *zcombineoperation = this->getbNode()->custom1 ? new ZCombineMaskAlphaOperation() : new ZCombineMaskOperation();
		compiler->addOperation(zcombineoperation);
		
		compiler->addConnection(antialiasoperation->getOutputSocket(), zcombineoperation->getInputSocket(0));
		compiler->mapInputSocket(getInputSocket(0), zcombineoperation->getInputSocket(1));
		compiler->mapInputSocket(getInputSocket(2), zcombineoperation->getInputSocket(2));
		compiler->mapOutputSocket(getOutputSocket(0), zcombineoperation->getOutputSocket());

		MathMinimumOperation *zoperation = new MathMinimumOperation();
		compiler->addOperation(zoperation);
		
		compiler->mapInputSocket(getInputSocket(1), zoperation->getInputSocket(0));
		compiler->mapInputSocket(getInputSocket(3), zoperation->getInputSocket(1));
		compiler->mapOutputSocket(getOutputSocket(1), zoperation->getOutputSocket());
	}
}

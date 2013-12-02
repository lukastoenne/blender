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
 *		Dalai Felinto
 */

#include "COM_DisplaceOperation.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

DisplaceOperation::DisplaceOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VECTOR);
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);

	this->m_inputColorProgram = NULL;
	this->m_inputVectorProgram = NULL;
	this->m_inputScaleXProgram = NULL;
	this->m_inputScaleYProgram = NULL;
}

void DisplaceOperation::initExecution()
{
	this->m_inputColorProgram = this->getInputSocketReader(0);
	this->m_inputVectorProgram = this->getInputSocketReader(1);
	this->m_inputScaleXProgram = this->getInputSocketReader(2);
	this->m_inputScaleYProgram = this->getInputSocketReader(3);

	this->m_width_x4 = this->getWidth() * 4;
	this->m_height_x4 = this->getHeight() * 4;
}


/* minimum distance (in pixels) a pixel has to be displaced
 * in order to take effect */
#define DISPLACE_EPSILON    0.01f

void DisplaceOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float xy[2] = { x, y };
	float uv[2], deriv[2][2];

	if (!pixelTransform(xy, uv, deriv)) {
		zero_v4(output);
		return;
	}

	/* EWA filtering (without nearest it gets blurry with NO distortion) */
	this->m_inputColorProgram->readFiltered(output, uv[0], uv[1], deriv[0], deriv[1], COM_PS_BILINEAR);
}

bool DisplaceOperation::pixelTransform(const float xy[2], float r_uv[2], float r_deriv[2][2])
{
	float col[4];

	m_inputScaleXProgram->readSampled(col, xy[0], xy[1], COM_PS_NEAREST);
	float xs = col[0];
	m_inputScaleYProgram->readSampled(col, xy[0], xy[1], COM_PS_NEAREST);
	float ys = col[0];
	/* clamp x and y displacement to triple image resolution - 
	 * to prevent hangs from huge values mistakenly plugged in eg. z buffers */
	CLAMP(xs, -m_width_x4, m_width_x4);
	CLAMP(ys, -m_height_x4, m_height_x4);

	m_inputVectorProgram->readSampled(col, xy[0], xy[1], COM_PS_NEAREST);
	/* displaced pixel in uv coords, for image sampling */
	r_uv[0] = xy[0] - col[0]*xs + 0.5f;
	r_uv[1] = xy[1] - col[1]*ys + 0.5f;

	/* XXX currently there is no way to get real derivatives from the UV map input.
	 * Instead use a simple 1st order estimate ...
	 */
	const float epsilon[2] = { 1.0f, 1.0f };

	m_inputVectorProgram->readSampled(col, xy[0] + epsilon[0], xy[1], COM_PS_NEAREST);
	float du_dx = col[0];
	float dv_dx = col[1];
	m_inputVectorProgram->readSampled(col, xy[0] - epsilon[0], xy[1], COM_PS_NEAREST);
	du_dx = 0.5f*(du_dx - col[0]) * xs;
	dv_dx = 0.5f*(dv_dx - col[1]) * ys;

	m_inputVectorProgram->readSampled(col, xy[0], xy[1] + epsilon[1], COM_PS_NEAREST);
	float du_dy = col[0];
	float dv_dy = col[1];
	m_inputVectorProgram->readSampled(col, xy[0], xy[1] - epsilon[1], COM_PS_NEAREST);
	du_dy = 0.5f*(du_dy - col[0]) * xs;
	dv_dy = 0.5f*(dv_dy - col[1]) * ys;

	/* XXX how does this translate to actual jacobian partial derivatives? */
	/* clamp derivatives to minimum displacement distance in UV space */
//	dxt = signf(dxt) * max_ff(fabsf(dxt), DISPLACE_EPSILON) / this->getWidth();
//	dyt = signf(dyt) * max_ff(fabsf(dyt), DISPLACE_EPSILON) / this->getHeight();
	
	r_deriv[0][0] = du_dx;
	r_deriv[0][1] = du_dy;
	r_deriv[1][0] = dv_dx;
	r_deriv[1][1] = dv_dy;

	return true;
}

void DisplaceOperation::deinitExecution()
{
	this->m_inputColorProgram = NULL;
	this->m_inputVectorProgram = NULL;
	this->m_inputScaleXProgram = NULL;
	this->m_inputScaleYProgram = NULL;
}

bool DisplaceOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti colorInput;
	rcti vectorInput;
	NodeOperation *operation = NULL;

	/* the vector buffer only needs a 2x2 buffer. The image needs whole buffer */
	/* image */
	operation = getInputOperation(0);
	colorInput.xmax = operation->getWidth();
	colorInput.xmin = 0;
	colorInput.ymax = operation->getHeight();
	colorInput.ymin = 0;
	if (operation->determineDependingAreaOfInterest(&colorInput, readOperation, output)) {
		return true;
	}

	/* vector */
	operation = getInputOperation(1);
	vectorInput.xmax = input->xmax + 1;
	vectorInput.xmin = input->xmin - 1;
	vectorInput.ymax = input->ymax + 1;
	vectorInput.ymin = input->ymin - 1;
	if (operation->determineDependingAreaOfInterest(&vectorInput, readOperation, output)) {
		return true;
	}

	/* scale x */
	operation = getInputOperation(2);
	if (operation->determineDependingAreaOfInterest(input, readOperation, output) ) {
		return true;
	}

	/* scale y */
	operation = getInputOperation(3);
	if (operation->determineDependingAreaOfInterest(input, readOperation, output) ) {
		return true;
	}

	return false;
}


/*
 * Copyright 2013, Blender Foundation.
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
 *		Sergey Sharybin
 */

#include "COM_PlaneTrackWarpImageOperation.h"
#include "COM_ReadBufferOperation.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color.h"

extern "C" {
	#include "BLI_jitter.h"

	#include "BKE_movieclip.h"
	#include "BKE_node.h"
	#include "BKE_tracking.h"
}

BLI_INLINE bool isPointInsideQuad(const float x, const float y, const float corners[4][2])
{
	float point[2];

	point[0] = x;
	point[1] = y;

	return isect_point_tri_v2(point, corners[0], corners[1], corners[2]) ||
	       isect_point_tri_v2(point, corners[0], corners[2], corners[3]);
}

BLI_INLINE void warpCoord(float x, float y, float matrix[3][3], float uv[2], float deriv[2][2])
{
	float vec[3] = {x, y, 1.0f};
	mul_m3_v3(matrix, vec);
	uv[0] = vec[0] / vec[2];
	uv[1] = vec[1] / vec[2];

	deriv[0][0] = (matrix[0][0] - matrix[0][2] * uv[0]) / vec[2];
	deriv[1][0] = (matrix[0][1] - matrix[0][2] * uv[1]) / vec[2];
	deriv[0][1] = (matrix[1][0] - matrix[1][2] * uv[0]) / vec[2];
	deriv[1][1] = (matrix[1][1] - matrix[1][2] * uv[1]) / vec[2];
}

PlaneTrackWarpImageOperation::PlaneTrackWarpImageOperation() : PlaneTrackCommonOperation()
{
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE);
	this->addOutputSocket(COM_DT_COLOR);
	this->m_pixelReader = NULL;
	this->setComplex(true);

	/* Currently hardcoded to 8 samples. */
	this->m_osa = 8;
}

void PlaneTrackWarpImageOperation::initExecution()
{
	PlaneTrackCommonOperation::initExecution();

	this->m_pixelReader = this->getInputSocketReader(0);

	BLI_jitter_init(this->m_jitter[0], this->m_osa);

	const int width = this->m_pixelReader->getWidth();
	const int height = this->m_pixelReader->getHeight();
	float frame_corners[4][2] = {{0.0f, 0.0f},
	                             {(float) width, 0.0f},
	                             {(float) width, (float) height},
	                             {0.0f, (float) height}};
	BKE_tracking_homography_between_two_quads(this->m_frameSpaceCorners,
	                                          frame_corners,
	                                          this->m_perspectiveMatrix);
}

void PlaneTrackWarpImageOperation::deinitExecution()
{
	this->m_pixelReader = NULL;
}

void PlaneTrackWarpImageOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float xy[2] = {x, y};
	float uv[2];
	float deriv[2][2];
	
	if (!pixelTransform(xy, uv, deriv)) {
		zero_v4(output);
		return;
	}

	m_pixelReader->readFiltered(output, uv[0], uv[1], deriv[0], deriv[1], COM_PS_BILINEAR);

#if 0
	float color_accum[4];

	zero_v4(color_accum);
	for (int sample = 0; sample < this->m_osa; sample++) {
		float current_x = x_ + this->m_jitter[sample][0],
		      current_y = y_ + this->m_jitter[sample][1];
		if (isPointInsideQuad(current_x, current_y, this->m_frameSpaceCorners)) {
			float current_color[4];
			float u, v, dx, dy;

			resolveUVAndDxDy(current_x, current_y, m_perspectiveMatrix, &u, &v, &dx, &dy);

			/* derivatives are to be in normalized space.. */
			dx /= this->m_pixelReader->getWidth();
			dy /= this->m_pixelReader->getHeight();

			this->m_pixelReader->readFiltered(current_color, u, v, dx, dy, COM_PS_NEAREST);
			add_v4_v4(color_accum, current_color);
		}
	}

	mul_v4_v4fl(output, color_accum, 1.0f / this->m_osa);
#endif
}

bool PlaneTrackWarpImageOperation::pixelTransform(const float xy[2], float r_uv[2], float r_deriv[2][2])
{
	if (!isPointInsideQuad(xy[0], xy[1], m_frameSpaceCorners))
		return false;

	warpCoord(xy[0], xy[1], m_perspectiveMatrix, r_uv, r_deriv);

	return true;
}

bool PlaneTrackWarpImageOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	float UVs[4][2];
	float deriv[2][2];

	/* TODO(sergey): figure out proper way to do this. */
	warpCoord(input->xmin - 2, input->ymin - 2, this->m_perspectiveMatrix, UVs[0], deriv);
	warpCoord(input->xmax + 2, input->ymin - 2, this->m_perspectiveMatrix, UVs[1], deriv);
	warpCoord(input->xmax + 2, input->ymax + 2, this->m_perspectiveMatrix, UVs[2], deriv);
	warpCoord(input->xmin - 2, input->ymax + 2, this->m_perspectiveMatrix, UVs[3], deriv);

	float min[2], max[2];
	INIT_MINMAX2(min, max);
	for (int i = 0; i < 4; i++) {
		minmax_v2v2_v2(min, max, UVs[i]);
	}

	rcti newInput;

	newInput.xmin = min[0] - 1;
	newInput.ymin = min[1] - 1;
	newInput.xmax = max[0] + 1;
	newInput.ymax = max[1] + 1;

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

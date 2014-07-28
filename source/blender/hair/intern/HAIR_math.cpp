/*
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <cstring>

extern "C" {
#include "BLI_utildefines.h"
}

#include "HAIR_math.h"

HAIR_NAMESPACE_BEGIN

static bool transform_matrix4_gj_inverse(float R[][4], float M[][4])
{
	/* forward elimination */
	for(int i = 0; i < 4; i++) {
		int pivot = i;
		float pivotsize = M[i][i];

		if(pivotsize < 0)
			pivotsize = -pivotsize;

		for(int j = i + 1; j < 4; j++) {
			float tmp = M[j][i];

			if(tmp < 0)
				tmp = -tmp;

			if(tmp > pivotsize) {
				pivot = j;
				pivotsize = tmp;
			}
		}

		if(UNLIKELY(pivotsize == 0.0f))
			return false;

		if(pivot != i) {
			for(int j = 0; j < 4; j++) {
				float tmp;

				tmp = M[i][j];
				M[i][j] = M[pivot][j];
				M[pivot][j] = tmp;

				tmp = R[i][j];
				R[i][j] = R[pivot][j];
				R[pivot][j] = tmp;
			}
		}

		for(int j = i + 1; j < 4; j++) {
			float f = M[j][i] / M[i][i];

			for(int k = 0; k < 4; k++) {
				M[j][k] -= f*M[i][k];
				R[j][k] -= f*R[i][k];
			}
		}
	}

	/* backward substitution */
	for(int i = 3; i >= 0; --i) {
		float f;

		if(UNLIKELY((f = M[i][i]) == 0.0f))
			return false;

		for(int j = 0; j < 4; j++) {
			M[i][j] /= f;
			R[i][j] /= f;
		}

		for(int j = 0; j < i; j++) {
			f = M[j][i];

			for(int k = 0; k < 4; k++) {
				M[j][k] -= f*M[i][k];
				R[j][k] -= f*R[i][k];
			}
		}
	}

	return true;
}

Transform transform_inverse(const Transform& tfm)
{
	Transform tfmR = Transform::Identity;
	float M[4][4], R[4][4];

	memcpy(R, &tfmR, sizeof(R));
	memcpy(M, &tfm, sizeof(M));

	if(UNLIKELY(!transform_matrix4_gj_inverse(R, M))) {
		/* matrix is degenerate (e.g. 0 scale on some axis), ideally we should
		 * never be in this situation, but try to invert it anyway with tweak */
		M[0][0] += 1e-8f;
		M[1][1] += 1e-8f;
		M[2][2] += 1e-8f;

		if(UNLIKELY(!transform_matrix4_gj_inverse(R, M))) {
			return Transform::Identity;
		}
	}

	memcpy(&tfmR, R, sizeof(R));

	return tfmR;
}

HAIR_NAMESPACE_END

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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __MOD_COLOR_H__
#define __MOD_COLOR_H__

#include "mod_defines.h"

#include "util_math.h"

BVM_MOD_NAMESPACE_BEGIN

enum BlendMode {
	MA_RAMP_BLEND       = 0,
	MA_RAMP_ADD         = 1,
	MA_RAMP_MULT        = 2,
	MA_RAMP_SUB         = 3,
	MA_RAMP_SCREEN      = 4,
	MA_RAMP_DIV         = 5,
	MA_RAMP_DIFF        = 6,
	MA_RAMP_DARK        = 7,
	MA_RAMP_LIGHT       = 8,
	MA_RAMP_OVERLAY     = 9,
	MA_RAMP_DODGE       = 10,
	MA_RAMP_BURN        = 11,
	MA_RAMP_HUE         = 12,
	MA_RAMP_SAT         = 13,
	MA_RAMP_VAL         = 14,
	MA_RAMP_COLOR       = 15,
	MA_RAMP_SOFT        = 16,
	MA_RAMP_LINEAR      = 17,
};

inline void MIX_RGB3(float3 &result, int mode, float fac,
                     const float3 &col_a, const float3 &col_b)
{
	float tmp, facm = 1.0f - fac;
	
	result = col_a;
	
	switch (mode) {
		case MA_RAMP_BLEND:
			result[0] = facm * (result[0]) + fac * col_b[0];
			result[1] = facm * (result[1]) + fac * col_b[1];
			result[2] = facm * (result[2]) + fac * col_b[2];
			break;
		case MA_RAMP_ADD:
			result[0] += fac * col_b[0];
			result[1] += fac * col_b[1];
			result[2] += fac * col_b[2];
			break;
		case MA_RAMP_MULT:
			result[0] *= (facm + fac * col_b[0]);
			result[1] *= (facm + fac * col_b[1]);
			result[2] *= (facm + fac * col_b[2]);
			break;
		case MA_RAMP_SCREEN:
			result[0] = 1.0f - (facm + fac * (1.0f - col_b[0])) * (1.0f - result[0]);
			result[1] = 1.0f - (facm + fac * (1.0f - col_b[1])) * (1.0f - result[1]);
			result[2] = 1.0f - (facm + fac * (1.0f - col_b[2])) * (1.0f - result[2]);
			break;
		case MA_RAMP_OVERLAY:
			if (result[0] < 0.5f)
				result[0] *= (facm + 2.0f * fac * col_b[0]);
			else
				result[0] = 1.0f - (facm + 2.0f * fac * (1.0f - col_b[0])) * (1.0f - result[0]);
			if (result[1] < 0.5f)
				result[1] *= (facm + 2.0f * fac * col_b[1]);
			else
				result[1] = 1.0f - (facm + 2.0f * fac * (1.0f - col_b[1])) * (1.0f - result[1]);
			if (result[2] < 0.5f)
				result[2] *= (facm + 2.0f * fac * col_b[2]);
			else
				result[2] = 1.0f - (facm + 2.0f * fac * (1.0f - col_b[2])) * (1.0f - result[2]);
			break;
		case MA_RAMP_SUB:
			result[0] -= fac * col_b[0];
			result[1] -= fac * col_b[1];
			result[2] -= fac * col_b[2];
			break;
		case MA_RAMP_DIV:
			if (col_b[0] != 0.0f)
				result[0] = facm * (result[0]) + fac * (result[0]) / col_b[0];
			if (col_b[1] != 0.0f)
				result[1] = facm * (result[1]) + fac * (result[1]) / col_b[1];
			if (col_b[2] != 0.0f)
				result[2] = facm * (result[2]) + fac * (result[2]) / col_b[2];
			break;
		case MA_RAMP_DIFF:
			result[0] = facm * (result[0]) + fac * fabsf(result[0] - col_b[0]);
			result[1] = facm * (result[1]) + fac * fabsf(result[1] - col_b[1]);
			result[2] = facm * (result[2]) + fac * fabsf(result[2] - col_b[2]);
			break;
		case MA_RAMP_DARK:
			result[0] = min_ff(result[0], col_b[0]) * fac + result[0] * facm;
			result[1] = min_ff(result[1], col_b[1]) * fac + result[1] * facm;
			result[2] = min_ff(result[2], col_b[2]) * fac + result[2] * facm;
			break;
		case MA_RAMP_LIGHT:
			tmp = fac * col_b[0];
			if (tmp > result[0]) result[0] = tmp;
			tmp = fac * col_b[1];
			if (tmp > result[1]) result[1] = tmp;
			tmp = fac * col_b[2];
			if (tmp > result[2]) result[2] = tmp;
			break;
		case MA_RAMP_DODGE:
			if (result[0] != 0.0f) {
				tmp = 1.0f - fac * col_b[0];
				if (tmp <= 0.0f)
					result[0] = 1.0f;
				else if ((tmp = (result[0]) / tmp) > 1.0f)
					result[0] = 1.0f;
				else
					result[0] = tmp;
			}
			if (result[1] != 0.0f) {
				tmp = 1.0f - fac * col_b[1];
				if (tmp <= 0.0f)
					result[1] = 1.0f;
				else if ((tmp = (result[1]) / tmp) > 1.0f)
					result[1] = 1.0f;
				else
					result[1] = tmp;
			}
			if (result[2] != 0.0f) {
				tmp = 1.0f - fac * col_b[2];
				if (tmp <= 0.0f)
					result[2] = 1.0f;
				else if ((tmp = (result[2]) / tmp) > 1.0f)
					result[2] = 1.0f;
				else
					result[2] = tmp;
			}
			break;
		case MA_RAMP_BURN:
			tmp = facm + fac * col_b[0];
			
			if (tmp <= 0.0f)
				result[0] = 0.0f;
			else if ((tmp = (1.0f - (1.0f - (result[0])) / tmp)) < 0.0f)
				result[0] = 0.0f;
			else if (tmp > 1.0f)
				result[0] = 1.0f;
			else
				result[0] = tmp;
			
			tmp = facm + fac * col_b[1];
			if (tmp <= 0.0f)
				result[1] = 0.0f;
			else if ((tmp = (1.0f - (1.0f - (result[1])) / tmp)) < 0.0f)
				result[1] = 0.0f;
			else if (tmp > 1.0f)
				result[1] = 1.0f;
			else
				result[1] = tmp;
			
			tmp = facm + fac * col_b[2];
			if (tmp <= 0.0f)
				result[2] = 0.0f;
			else if ((tmp = (1.0f - (1.0f - (result[2])) / tmp)) < 0.0f)
				result[2] = 0.0f;
			else if (tmp > 1.0f)
				result[2] = 1.0f;
			else
				result[2] = tmp;
			break;
		case MA_RAMP_HUE:
		{
			float rH, rS, rV;
			float colH, colS, colV;
			float tmpr, tmpg, tmpb;
			rgb_to_hsv(col_b[0], col_b[1], col_b[2], &colH, &colS, &colV);
			if (colS != 0) {
				rgb_to_hsv(result[0], result[1], result[2], &rH, &rS, &rV);
				hsv_to_rgb(colH, rS, rV, &tmpr, &tmpg, &tmpb);
				result[0] = facm * (result[0]) + fac * tmpr;
				result[1] = facm * (result[1]) + fac * tmpg;
				result[2] = facm * (result[2]) + fac * tmpb;
			}
			break;
		}
		case MA_RAMP_SAT:
		{
			float rH, rS, rV;
			float colH, colS, colV;
			rgb_to_hsv(result[0], result[1], result[2], &rH, &rS, &rV);
			if (rS != 0) {
				rgb_to_hsv(col_b[0], col_b[1], col_b[2], &colH, &colS, &colV);
				hsv_to_rgb(rH, (facm * rS + fac * colS), rV, &result[0], &result[1], &result[2]);
			}
			break;
		}
		case MA_RAMP_VAL:
		{
			float rH, rS, rV;
			float colH, colS, colV;
			rgb_to_hsv(result[0], result[1], result[2], &rH, &rS, &rV);
			rgb_to_hsv(col_b[0], col_b[1], col_b[2], &colH, &colS, &colV);
			hsv_to_rgb(rH, rS, (facm * rV + fac * colV), &result[0], &result[1], &result[2]);
			break;
		}
		case MA_RAMP_COLOR:
		{
			float rH, rS, rV;
			float colH, colS, colV;
			float tmpr, tmpg, tmpb;
			rgb_to_hsv(col_b[0], col_b[1], col_b[2], &colH, &colS, &colV);
			if (colS != 0) {
				rgb_to_hsv(result[0], result[1], result[2], &rH, &rS, &rV);
				hsv_to_rgb(colH, colS, rV, &tmpr, &tmpg, &tmpb);
				result[0] = facm * (result[0]) + fac * tmpr;
				result[1] = facm * (result[1]) + fac * tmpg;
				result[2] = facm * (result[2]) + fac * tmpb;
			}
			break;
		}
		case MA_RAMP_SOFT:
		{
			float scr, scg, scb;
			
			/* first calculate non-fac based Screen mix */
			scr = 1.0f - (1.0f - col_b[0]) * (1.0f - result[0]);
			scg = 1.0f - (1.0f - col_b[1]) * (1.0f - result[1]);
			scb = 1.0f - (1.0f - col_b[2]) * (1.0f - result[2]);
			
			result[0] = facm * (result[0]) + fac * (((1.0f - result[0]) * col_b[0] * (result[0])) + (result[0] * scr));
			result[1] = facm * (result[1]) + fac * (((1.0f - result[1]) * col_b[1] * (result[1])) + (result[1] * scg));
			result[2] = facm * (result[2]) + fac * (((1.0f - result[2]) * col_b[2] * (result[2])) + (result[2] * scb));
			break;
		}
		case MA_RAMP_LINEAR:
			if (col_b[0] > 0.5f)
				result[0] = result[0] + fac * (2.0f * (col_b[0] - 0.5f));
			else
				result[0] = result[0] + fac * (2.0f * (col_b[0]) - 1.0f);
			if (col_b[1] > 0.5f)
				result[1] = result[1] + fac * (2.0f * (col_b[1] - 0.5f));
			else
				result[1] = result[1] + fac * (2.0f * (col_b[1]) - 1.0f);
			if (col_b[2] > 0.5f)
				result[2] = result[2] + fac * (2.0f * (col_b[2] - 0.5f));
			else
				result[2] = result[2] + fac * (2.0f * (col_b[2]) - 1.0f);
			break;
	}
}

/* wrapper for float4 RGBA mixing (copies alpha from col_a) */
BVM_MOD_FUNCTION("MIX_RGB")
void MIX_RGB(float4 &result, int mode, float fac,
             const float4 &col_a, const float4 &col_b)
{
	float3 result3, col_a3(col_a.x, col_a.y, col_a.z), col_b3(col_b.x, col_b.y, col_b.z);
	MIX_RGB3(result3, mode, fac, col_a3, col_b3);
	result.x = result3.x;
	result.y = result3.y;
	result.z = result3.z;
	result.w = col_a.w;
}

BVM_MOD_NAMESPACE_END

#endif /* __MOD_COLOR_H__ */

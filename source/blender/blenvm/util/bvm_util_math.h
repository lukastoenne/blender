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

#ifndef __BVM_UTIL_MATH_H__
#define __BVM_UTIL_MATH_H__

/** \file bvm_util_math.h
 *  \ingroup bvm
 */

extern "C" {
#include "BLI_math.h"
}

namespace bvm {

typedef enum eEulerRotationOrders {
	EULER_ORDER_DEFAULT = ::EULER_ORDER_DEFAULT, /* blender classic = XYZ */
	EULER_ORDER_XYZ = ::EULER_ORDER_XYZ,
	EULER_ORDER_XZY = ::EULER_ORDER_XZY,
	EULER_ORDER_YXZ = ::EULER_ORDER_YXZ,
	EULER_ORDER_YZX = ::EULER_ORDER_YZX,
	EULER_ORDER_ZXY = ::EULER_ORDER_ZXY,
	EULER_ORDER_ZYX = ::EULER_ORDER_ZYX,
	/* there are 6 more entries with dulpicate entries included */
} eEulerRotationOrders;

inline static float div_safe(float a, float b)
{
	if (b != 0.0f)
		return a / b;
	else
		return 0.0f;
}

inline static float sqrt_safe(float a)
{
	if (a > 0.0f)
		return sqrtf(a);
	else
		return 0.0f;
}

} /* namespace bvm */

#endif /* __BVM_UTIL_MATH_H__ */

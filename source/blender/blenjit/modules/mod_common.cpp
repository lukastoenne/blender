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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "mod_common.h"
#include "mod_math.h"

#include <stdio.h>

#include "bjit_types.h"

namespace bjit {

__attribute__((annotate("effector_result_combine")))
EffectorEvalResult effector_result_combine(const vec3_t force1, const vec3_t impulse1,
                                           const vec3_t force2, const vec3_t impulse2)
{
	EffectorEvalResult result;
	add_v3_v3v3(result.force, force1, force2);
	add_v3_v3v3(result.impulse, impulse1, impulse2);
	return result;
}

__attribute__((annotate("print_vec3")))
void print_vec3(const vec3_t v)
{
	printf("(%.5f, %.5f, %.5f)\n", v[0], v[1], v[2]);
}

__attribute__((annotate("print_result")))
void print_result(const EffectorEvalResult *v)
{
	print_vec3(v->force);
	print_vec3(v->impulse);
}

typedef struct TEST {
	vec3_t a, b;
} TEST;

TEST testest(const vec3_t v)
{
	TEST t;
	zero_v3(t.a);
	zero_v3(t.b);
	t.a[0] = 1;
	t.a[1] = 2;
	t.a[2] = 3;
	return t;
}

} /* namespace bjit */

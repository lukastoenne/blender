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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __OPENVDB_TESTS_H__
#define __OPENVDB_TESTS_H__

#include <openvdb/tools/Composite.h>

#include "testing/testing.h"

// XXX TODO
#define EXPECT_TRANSFORM_NEAR(a, b, eps) \
{ \
} (void) 0

/* ------------------------------------------------------------------------- */

namespace local {

using namespace openvdb;

template <typename ValueType>
struct test_near_values_combine {
	ValueType eps;
	
	test_near_values_combine(const ValueType &eps) : eps(eps) {}
	
	inline void operator() (CombineArgs<ValueType, ValueType> &args) const
	{
		ValueType a = args.a();
		ValueType b = args.b();
		EXPECT_NEAR(a, b, eps);	
	}
};

template <typename TreeType>
static void test_near_values(TreeType &a, TreeType &b, typename TreeType::ValueType eps)
{
	(a).tree().combineExtended((b).tree(), local::test_near_values_combine<typename TreeType::ValueType>((eps))); \
}

}

#define EXPECT_GRID_NEAR(a, b, eps) \
{ \
	EXPECT_TRANSFORM_NEAR((a).transform(), (b).transform(), eps); \
	EXPECT_TRUE((a).tree().hasSameTopology((b).tree())); \
	local::test_near_values(a, b, eps); \
} (void) 0

#endif /* __OPENVDB_SMOKE_H__ */

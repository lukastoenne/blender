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
	typedef ValueType EpsType;
	
	ValueType eps;
	
	test_near_values_combine(const ValueType &eps) : eps(eps) {}
	
	inline void operator() (const ValueType &a, const ValueType &b, const Coord &ijk) const
	{
		EXPECT_NEAR(a, b, eps) << "Voxel " << ijk[0] << ", " << ijk[1] << ", " << ijk[2];
	}
};

template <typename ScalarType>
struct test_near_values_combine<math::Vec3<ScalarType> > {
	typedef math::Vec3<ScalarType> ValueType;
	typedef ScalarType EpsType;
	
	ScalarType eps;
	
	test_near_values_combine(const ScalarType &eps) : eps(eps) {}
	
	inline void operator() (const ValueType &a, const ValueType &b, const Coord &ijk) const
	{
		EXPECT_NEAR(a[0], b[0], eps) << "Voxel " << ijk[0] << ", " << ijk[1] << ", " << ijk[2];
		EXPECT_NEAR(a[1], b[1], eps) << "Voxel " << ijk[0] << ", " << ijk[1] << ", " << ijk[2];
		EXPECT_NEAR(a[2], b[2], eps) << "Voxel " << ijk[0] << ", " << ijk[1] << ", " << ijk[2];
	}
};

template <typename TreeType, typename EpsType>
static void test_near_values(TreeType &a, TreeType &b, EpsType eps)
{
	typedef typename TreeType::ValueType ValueType;
	
	typename TreeType::ConstAccessor acc = b.getConstAccessor();
	
	local::test_near_values_combine<ValueType> op(eps);
	for (typename TreeType::ValueOnIter it = a.beginValueOn(); it; ++it) {
		Coord ijk = it.getCoord();
		ValueType b_val = acc.getValue(ijk);
		
		op(it.getValue(), b_val, ijk);
	}
}

}

#define EXPECT_GRID_NEAR(a, b, eps) \
{ \
	EXPECT_TRANSFORM_NEAR((a).transform(), (b).transform(), eps); \
	EXPECT_TRUE((a).tree().hasSameTopology((b).tree())); \
	local::test_near_values(a, b, eps); \
} (void) 0

#endif /* __OPENVDB_SMOKE_H__ */

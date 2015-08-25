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

#include <openvdb/openvdb.h>

#include "testing/testing.h"

#include "openvdb_tests.h"
#include "openvdb_dense_convert.h"
#include "openvdb_smoke.h"

using namespace internal;
using namespace openvdb;
using namespace openvdb::math;

struct TestPoint {
	TestPoint(float l0, float l1, float l2, float _rad, float v0, float v1, float v2)
	{
		loc[0] = l0;
		loc[1] = l1;
		loc[2] = l2;
		rad = _rad;
		vel[0] = v0;
		vel[1] = v1;
		vel[2] = v2;
	}

	float loc[3];
	float rad;
	float vel[3];
};

struct TestIPoints {
	OpenVDBPointInputStream base;
	TestPoint *array;
	int len;
	
	TestIPoints(TestPoint *array, int len);
};
static bool test_ipoints_has_points(TestIPoints *points)
{
	return points->len > 0;
}
static void test_ipoints_next_point(TestIPoints *points)
{
	++points->array;
	--points->len;
}
static void test_ipoints_get_point(TestIPoints *points, float loc[3], float *rad, float vel[3])
{
	TestPoint *p = points->array;
	loc[0] = p->loc[0];
	loc[1] = p->loc[1];
	loc[2] = p->loc[2];
	vel[0] = p->vel[0];
	vel[1] = p->vel[1];
	vel[2] = p->vel[2];
	*rad = p->rad;
}
TestIPoints::TestIPoints(TestPoint *_array, int _len)
{
	base.has_points = (OpenVDBHasIPointsFn)test_ipoints_has_points;
	base.next_point = (OpenVDBNextIPointFn)test_ipoints_next_point;
	base.get_point = (OpenVDBGetIPointFn)test_ipoints_get_point;
	array = _array;
	len = _len;
}

struct TestOPoints {
	OpenVDBPointOutputStream base;
	TestPoint *array;
	int len;
	TestOPoints(TestPoint *array, int len);
};
static bool test_opoints_has_points(TestOPoints *points)
{
	return points->len > 0;
}
static void test_opoints_next_point(TestOPoints *points)
{
	++points->array;
	--points->len;
}
static void test_opoints_get_point(TestOPoints *points, float loc[3], float *rad, float vel[3])
{
	TestPoint *p = points->array;
	loc[0] = p->loc[0];
	loc[1] = p->loc[1];
	loc[2] = p->loc[2];
	vel[0] = p->vel[0];
	vel[1] = p->vel[1];
	vel[2] = p->vel[2];
	*rad = p->rad;
}
static void test_opoints_set_point(TestOPoints *points, const float loc[3], const float vel[3])
{
	TestPoint *p = points->array;
	p->loc[0] = loc[0];
	p->loc[1] = loc[1];
	p->loc[2] = loc[2];
	p->vel[0] = vel[0];
	p->vel[1] = vel[1];
	p->vel[2] = vel[2];
}
TestOPoints::TestOPoints(TestPoint *_array, int _len)
{
	base.has_points = (OpenVDBHasOPointsFn)test_opoints_has_points;
	base.next_point = (OpenVDBNextOPointFn)test_opoints_next_point;
	base.get_point = (OpenVDBGetOPointFn)test_opoints_get_point;
	base.set_point = (OpenVDBSetOPointFn)test_opoints_set_point;
	array = _array;
	len = _len;
}

TEST(OpenVDBSmoke, ParticleListFromStream) {
	std::vector<TestPoint> points;
	points.push_back(TestPoint(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f));
	points.push_back(TestPoint(1.1f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f));
	points.push_back(TestPoint(0.0f, 2.2f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f));
	points.push_back(TestPoint(0.0f, 0.0f, 3.3f, 1.0f, 0.0f, 0.0f, 0.0f));
	points.push_back(TestPoint(0.0f, 0.0f, 0.0f, 1.1f, 0.0f, 0.0f, 0.0f));
	points.push_back(TestPoint(0.0f, 0.0f, 0.0f, 2.2f, 0.0f, 0.0f, 0.0f));
	points.push_back(TestPoint(0.0f, 0.0f, 0.0f, 3.3f, 0.0f, 0.0f, 0.0f));
	points.push_back(TestPoint(0.0f, 0.0f, 0.0f, 1.0f, 1.1f, 0.0f, 0.0f));
	points.push_back(TestPoint(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 2.2f));
	points.push_back(TestPoint(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 3.3f, 0.0f));
	
	SmokeParticleList particles;
	TestIPoints istream(points.data(), points.size());
	particles.from_stream(&istream.base);
	EXPECT_EQ(particles.size(), points.size());
	for (size_t i = 0; i < particles.size(); ++i) {
		Vec3R pos, vel;
		Real rad;
		particles.getPosRadVel(i, pos, rad, vel);
		EXPECT_EQ(pos, Vec3R(points[i].loc));
		EXPECT_EQ(rad, Real(points[i].rad));
		EXPECT_EQ(vel, Vec3R(points[i].vel));
	}
}

TEST(OpenVDBSmoke, ParticleListToStream) {
	SmokeParticleList particles;
	particles.points().push_back(SmokeParticleList::Point(Vec3R(0.0f, 0.0f, 0.0f), 1.0f, Vec3R(0.0f, 0.0f, 0.0f)));
	particles.points().push_back(SmokeParticleList::Point(Vec3R(0.0f, 10.0f, 0.0f), 1.0f, Vec3R(0.0f, 0.0f, 0.0f)));
	particles.points().push_back(SmokeParticleList::Point(Vec3R(0.0f, 0.0f, 0.111f), 1.0f, Vec3R(0.0f, 20.0f, 0.0f)));
	particles.points().push_back(SmokeParticleList::Point(Vec3R(0.20f, 0.0f, 0.0f), 1.0f, Vec3R(0.0f, 0.0f, 20.0f)));
	particles.points().push_back(SmokeParticleList::Point(Vec3R(0.50f, 0.0f, 0.0f), 1.0f, Vec3R(0.0f, 0.0f, 0.0f)));
	particles.points().push_back(SmokeParticleList::Point(Vec3R(0.0f, 0.0f, 0.0f), 1.0f, Vec3R(0.0f, 0.0f, 0.0f)));
	particles.points().push_back(SmokeParticleList::Point(Vec3R(0.0f, 0.0f, 0.0f), 1.0f, Vec3R(03.0f, 80.0f, 0.0f)));
	particles.points().push_back(SmokeParticleList::Point(Vec3R(0.0f, 0.0f, 0.0f), 1.0f, Vec3R(51.0f, 0.0f, 0.0f)));
	
	std::vector<TestPoint> points(particles.size(), TestPoint(0, 0, 0, 0, 0, 0, 0));
	TestOPoints ostream(points.data(), points.size());
	particles.to_stream(&ostream.base);
	for (size_t i = 0; i < particles.size(); ++i) {
		Vec3R pos, vel;
		Real rad;
		particles.getPosRadVel(i, pos, rad, vel);
		EXPECT_V3_NEAR(points[i].loc, pos, 1e-20);
		EXPECT_V3_NEAR(points[i].vel, vel, 1e-20);
	}
}

TEST(OpenVDBSmoke, InitGrids) {
	std::vector<TestPoint> points;
	points.push_back(TestPoint(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f));
	TestIPoints istream(points.data(), points.size());
	
	Mat4R mat;
	mat.setIdentity();
	SmokeData data(mat);
	
	data.set_points(&istream.base);
	data.init_grids();
	
	FloatGrid::Ptr density = FloatGrid::create(0.0f);
	FloatTree &tree = density->tree();
	tree.setValue(Coord(0, 0, 0), 0.5f);
	tree.setValue(Coord(1, 0, 0), 0.5f);
	tree.setValue(Coord(0, 1, 0), 0.5f);
	tree.setValue(Coord(1, 1, 0), 0.5f);
	tree.setValue(Coord(0, 0, 1), 0.5f);
	tree.setValue(Coord(1, 0, 1), 0.5f);
	tree.setValue(Coord(0, 1, 1), 0.5f);
	tree.setValue(Coord(1, 1, 1), 0.5f);
	
	EXPECT_GRID_NEAR(*data.density, *density, 1e-5);
	
#if 0
  Tracks tracks;
  AddMarker(0, 1.0,  0.0, &tracks);
  AddMarker(1, 2.0,  5.0, &tracks);
  AddMarker(2, 3.0, 10.0, &tracks);
  AddMarker(3, 4.0, 15.0, &tracks);
  AddMarker(4, 5.0, 20.0, &tracks);
  AddMarker(5, 6.0, 25.0, &tracks);
  AddMarker(6, 7.0, 30.0, &tracks);
  AddMarker(7, 8.0, 35.0, &tracks);

  Marker predicted;
  predicted.clip = 0;
  predicted.track = 0;
  predicted.frame = 8;

  PredictMarkerPosition(tracks, &predicted);
  double error = (libmv::Vec2f(9.0, 40.0) - predicted.center).norm();
  LG << "Got error: " << error;
  EXPECT_LT(error, 0.1);

  // Check the patch coordinates as well.
  double x = 9, y = 40.0;
  Quad2Df expected_patch;
  expected_patch.coordinates << x - 1, y - 1,
                                x + 1, y - 1,
                                x + 1, y + 1,
                                x - 1, y + 1;

  error = (expected_patch.coordinates - predicted.patch.coordinates).norm();
  LG << "Patch error: " << error;
  EXPECT_LT(error, 0.1);
#endif
}

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

typedef std::vector<TestPoint> TestPointList;

struct TestIPoints {
	OpenVDBPointInputStream base;
	TestPointList::const_iterator it;
	TestPointList::const_iterator it_end;
	
	TestIPoints(const TestPointList &list);
};
static bool test_ipoints_has_points(TestIPoints *points)
{
	return points->it != points->it_end;
}
static void test_ipoints_next_point(TestIPoints *points)
{
	++points->it;
}
static void test_ipoints_get_point(TestIPoints *points, float loc[3], float *rad, float vel[3])
{
	const TestPoint &p = *points->it;
	loc[0] = p.loc[0];
	loc[1] = p.loc[1];
	loc[2] = p.loc[2];
	vel[0] = p.vel[0];
	vel[1] = p.vel[1];
	vel[2] = p.vel[2];
	*rad = p.rad;
}
TestIPoints::TestIPoints(const TestPointList &_list)
{
	base.has_points = (OpenVDBHasIPointsFn)test_ipoints_has_points;
	base.next_point = (OpenVDBNextIPointFn)test_ipoints_next_point;
	base.get_point = (OpenVDBGetIPointFn)test_ipoints_get_point;
	it_end = _list.end();
	it = _list.begin();
}

struct TestOPoints {
	OpenVDBPointOutputStream base;
	TestPointList *list;
	TestPointList::iterator it;
	
	TestOPoints(TestPointList &list);
};
static void test_opoints_create_points(TestOPoints *points, int num)
{
	points->list->resize(num, TestPoint(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f));
	points->it = points->list->begin();
}
static bool test_opoints_has_points(TestOPoints *points)
{
	return points->it != points->list->end();
}
static void test_opoints_next_point(TestOPoints *points)
{
	++points->it;
}
static void test_opoints_get_point(TestOPoints *points, float loc[3], float *rad, float vel[3])
{
	const TestPoint &p = *points->it;
	loc[0] = p.loc[0];
	loc[1] = p.loc[1];
	loc[2] = p.loc[2];
	vel[0] = p.vel[0];
	vel[1] = p.vel[1];
	vel[2] = p.vel[2];
	*rad = p.rad;
}
static void test_opoints_set_point(TestOPoints *points, const float loc[3], const float vel[3])
{
	TestPoint &p = *points->it;
	p.loc[0] = loc[0];
	p.loc[1] = loc[1];
	p.loc[2] = loc[2];
	p.vel[0] = vel[0];
	p.vel[1] = vel[1];
	p.vel[2] = vel[2];
}
TestOPoints::TestOPoints(TestPointList &_list)
{
	base.create_points = (OpenVDBCreatePointsFn)test_opoints_create_points;
	base.has_points = (OpenVDBHasOPointsFn)test_opoints_has_points;
	base.next_point = (OpenVDBNextOPointFn)test_opoints_next_point;
	base.get_point = (OpenVDBGetOPointFn)test_opoints_get_point;
	base.set_point = (OpenVDBSetOPointFn)test_opoints_set_point;
	list = &_list;
	it = _list.begin();
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
	TestIPoints istream(points);
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
	TestOPoints ostream(points);
	particles.to_stream(&ostream.base);
	for (size_t i = 0; i < particles.size(); ++i) {
		Vec3R pos, vel;
		Real rad;
		particles.getPosRadVel(i, pos, rad, vel);
		EXPECT_V3_NEAR(points[i].loc, pos, 1e-20);
		EXPECT_V3_NEAR(points[i].vel, vel, 1e-20);
	}
}

static ScalarGrid::Ptr init_scalar_grid(Transform::Ptr tfm, float *data,
                                        int xmin, int ymin, int zmin, int xmax, int ymax, int zmax)
{
	ScalarGrid::Ptr grid = ScalarGrid::create(0.0f);
	grid->setTransform(tfm);
	
	CoordBBox bbox(Coord(xmin, ymin, zmin), Coord(xmax, ymax, zmax));
	tools::Dense<const float, openvdb::tools::LayoutXYZ> dense_grid(bbox, data);
	tools::copyFromDense(dense_grid, grid->tree(), 1.0e-6f);
	
	return grid;
}

static VectorGrid::Ptr init_vector_grid(Transform::Ptr tfm, Vec3f *data,
                                        int xmin, int ymin, int zmin, int xmax, int ymax, int zmax)
{
	VectorGrid::Ptr grid = VectorGrid::create(Vec3f(0.0f, 0.0f, 0.0f));
	grid->setTransform(tfm);
	grid->setGridClass(GRID_STAGGERED);
	
	CoordBBox bbox(Coord(xmin, ymin, zmin), Coord(xmax, ymax, zmax));
	tools::Dense<const Vec3f, openvdb::tools::LayoutXYZ> dense_grid(bbox, data);
	tools::copyFromDense(dense_grid, grid->tree(), Vec3f(1.0e-6f, 1.0e-6f, 1.0e-6f));
	
	return grid;
}

TEST(OpenVDBSmoke, InitGrids) {
	std::vector<TestPoint> points;
	points.push_back(TestPoint(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f));
	TestIPoints istream(points);
	
	Mat4R mat;
	mat.setIdentity();
	SmokeData data(mat);
	data.points.from_stream(&istream.base);
	
	data.init_grids();
	
	float ddata[] = {
	    // z = 0
	    1.0f, 0.0f,
	    0.0f, 0.0f,
	    
	    // z = 1
	    0.0f, 0.0f,
	    0.0f, 0.0f,
	};
	Vec3f vdata[] = {
	    // z = 0
	    Vec3f(1.0f, 1.0f, 1.0f), Vec3f(0.0f, 1.0f, 1.0f),
	    Vec3f(1.0f, 1.0f, 1.0f), Vec3f(0.0f, 0.0f, 0.0f),
	    
	    // z = 1
	    Vec3f(1.0f, 1.0f, 1.0f), Vec3f(0.0f, 0.0f, 0.0f),
	    Vec3f(0.0f, 0.0f, 0.0f), Vec3f(0.0f, 0.0f, 0.0f),
	};
	ScalarGrid::Ptr density = init_scalar_grid(data.cell_transform, ddata, 0, 0, 0, 1, 1, 1);
	VectorGrid::Ptr velocity = init_vector_grid(data.cell_transform, vdata, 0, 0, 0, 1, 1, 1);
	
	EXPECT_GRID_NEAR(*data.density, *density, 1e-5);
	EXPECT_GRID_NEAR(*data.velocity, *velocity, 1e-5);
}

#if 0
TEST(OpenVDBSmoke, PoissonMatrix) {
	Mat4R mat;
	mat.setIdentity();
	SmokeData data(mat);
	
	data.density = FloatGrid::create(0.0f);
	data.velocity = VectorGrid::create(Vec3f(0.0f, 0.0f, 0.0f));
	data.velocity->setGridClass(GRID_STAGGERED);
	
	add_uniform_voxel(*data.density, *data.velocity, 0, 0, 0, Vec3f(1.0f, 1.0f, 1.0f));
	
	ScalarGrid::Ptr div = data.calc_divergence();
	
	ScalarGrid::Ptr exp_div = ScalarGrid::create(0.0f);
	{
		ScalarTree &tree = exp_div->tree();
		tree.setValue();
	}
	
	EXPECT_GRID_NEAR(*div, *density, 1e-5);
}
#endif

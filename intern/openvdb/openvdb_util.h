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
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __OPENVDB_UTIL_H__
#define __OPENVDB_UTIL_H__

#include <cstdio>
#include <string>

#include <openvdb/openvdb.h>

void catch_exception(int &ret);

double time_dt();

/* A utility class which prints the time elapsed during its lifetime, useful for
 * e.g. timing the overall execution time of a function */
class ScopeTimer {
	double m_start;
	std::string m_message;

public:
	ScopeTimer(const std::string &message)
	    : m_message(message)
	{
		m_start = time_dt();
	}

	~ScopeTimer()
	{
		printf("%s: %fs\n", m_message.c_str(), time_dt() - m_start);
	}
};

#ifndef NDEBUG
#	define Timer(x) \
		ScopeTimer func(x);
#else
#	define Timer(x)
#endif

namespace internal {

using namespace openvdb;

static void copy_v3_v3(float *r, const float *a)
{
	r[0] = a[0];
	r[1] = a[1];
	r[2] = a[2];
}

static void cross_v3_v3v3(float *r, const float *a, const float *b)
{
	r[0] = a[1] * b[2] - a[2] * b[1];
	r[1] = a[2] * b[0] - a[0] * b[2];
	r[2] = a[0] * b[1] - a[1] * b[0];
}

static void get_normal(float nor[3], const Vec3f &v1, const Vec3f &v2, const Vec3f &v3, const Vec3f &v4)
{
	openvdb::Vec3f n1, n2;

	n1 = v1 - v3;
	n2 = v2 - v4;

	cross_v3_v3v3(nor, n1.asV(), n2.asV());
}

static inline void add_quad(float (*verts)[3], float (*colors)[3], float (*normals)[3], int *verts_ofs,
                            const Vec3f &p1, const Vec3f &p2, const Vec3f &p3, const Vec3f &p4, const Vec3f &color)
{
	copy_v3_v3(verts[*verts_ofs+0], p1.asV());
	copy_v3_v3(verts[*verts_ofs+1], p2.asV());
	copy_v3_v3(verts[*verts_ofs+2], p3.asV());
	copy_v3_v3(verts[*verts_ofs+3], p4.asV());
	
	copy_v3_v3(colors[*verts_ofs+0], color.asV());
	copy_v3_v3(colors[*verts_ofs+1], color.asV());
	copy_v3_v3(colors[*verts_ofs+2], color.asV());
	copy_v3_v3(colors[*verts_ofs+3], color.asV());
	
	if (normals) {
		float nor[3];
		
		get_normal(nor, p1, p2, p3, p4);
		
		copy_v3_v3(normals[*verts_ofs+0], nor);
		copy_v3_v3(normals[*verts_ofs+1], nor);
		copy_v3_v3(normals[*verts_ofs+2], nor);
		copy_v3_v3(normals[*verts_ofs+3], nor);
	}
	
	*verts_ofs += 4;
}

static void add_box(float (*verts)[3], float (*colors)[3], float (*normals)[3], int *verts_ofs,
                    const openvdb::Vec3f &min, const openvdb::Vec3f &max,
                    const openvdb::Vec3f &color)
{
	using namespace openvdb;
	
	const Vec3f corners[8] = {
	    min,
	    Vec3f(min.x(), min.y(), max.z()),
	    Vec3f(max.x(), min.y(), max.z()),
	    Vec3f(max.x(), min.y(), min.z()),
	    Vec3f(min.x(), max.y(), min.z()),
	    Vec3f(min.x(), max.y(), max.z()),
	    max,
	    Vec3f(max.x(), max.y(), min.z())
	};

	add_quad(verts, colors, normals, verts_ofs, corners[0], corners[1], corners[2], corners[3], color);
	add_quad(verts, colors, normals, verts_ofs, corners[7], corners[6], corners[5], corners[4], color);
	add_quad(verts, colors, normals, verts_ofs, corners[4], corners[5], corners[1], corners[0], color);
	add_quad(verts, colors, normals, verts_ofs, corners[3], corners[2], corners[6], corners[7], color);
	add_quad(verts, colors, normals, verts_ofs, corners[3], corners[7], corners[4], corners[0], color);
	add_quad(verts, colors, normals, verts_ofs, corners[1], corners[5], corners[6], corners[2], color);
}

template <typename TreeType>
static void OpenVDB_get_draw_buffer_size_grid_levels(openvdb::Grid<TreeType> *grid, int min_level, int max_level,
                                                     int *r_numverts)
{
	using namespace openvdb;
	using namespace openvdb::math;
	
	if (!grid) {
		*r_numverts = 0;
		return;
	}
	
	/* count */
	int numverts = 0;
	for (typename TreeType::NodeCIter node_iter = grid->tree().cbeginNode(); node_iter; ++node_iter) {
		const int level = node_iter.getLevel();
		
		if (level < min_level || level > max_level)
			continue;
		
		numverts += 6 * 4;
	}
	
	*r_numverts = numverts;
}

template <typename TreeType>
static void OpenVDB_get_draw_buffers_grid_levels(openvdb::Grid<TreeType> *grid, int min_level, int max_level,
                                                 int /*numverts*/, float (*verts)[3], float (*colors)[3])
{
	using namespace openvdb;
	using namespace openvdb::math;
	
	if (!grid)
		return;
	
	int verts_ofs = 0;
	for (typename TreeType::NodeCIter node_iter = grid->tree().cbeginNode(); node_iter; ++node_iter) {
		const int level = node_iter.getLevel();
		
		if (level < min_level || level > max_level)
			continue;
		
		CoordBBox bbox;
		node_iter.getBoundingBox(bbox);
		
		const Vec3f min(bbox.min().x() - 0.5f, bbox.min().y() - 0.5f, bbox.min().z() - 0.5f);
		const Vec3f max(bbox.max().x() + 0.5f, bbox.max().y() + 0.5f, bbox.max().z() + 0.5f);
		
		Vec3f wmin = grid->indexToWorld(min);
		Vec3f wmax = grid->indexToWorld(max);
		
//		Vec3f color = node_color[std::max(3 - level, 0)];
		Vec3f color(0,0,1);
		
		add_box(verts, colors, NULL, &verts_ofs, wmin, wmax, color);
	}
}

} /* namespace internal */

#endif /* __OPENVDB_UTIL_H__ */

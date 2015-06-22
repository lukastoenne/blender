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

#include <GL/glew.h>
#include <openvdb/openvdb.h>

#include "openvdb_primitive.h"
#include "openvdb_render.h"

namespace internal {

struct vertex {
	float x, y, z;
};

static void add_point(std::vector<vertex> *vertices, std::vector<vertex> *colors,
                      const openvdb::Vec3f &point, const openvdb::Vec3f &color)
{
	vertex vert;
	vert.x = point.x();
	vert.y = point.y();
	vert.z = point.z();
	vertices->push_back(vert);

	vertex col;
	col.x = color.x();
	col.y = color.y();
	col.z = color.z();
	colors->push_back(col);
}

static inline vertex get_normal(
        const openvdb::Vec3f &v1, const openvdb::Vec3f &v2,
        const openvdb::Vec3f &v3, const openvdb::Vec3f &v4)
{
	vertex nor;
	openvdb::Vec3f n1, n2;

	n1 = v1 - v3;
	n2 = v2 - v4;

	nor.x = n1[1] * n2[2] - n1[2] * n2[1];
	nor.y = n1[2] * n2[0] - n1[0] * n2[2];
	nor.z = n1[0] * n2[1] - n1[1] * n2[0];

	return nor;
}

static void add_box(std::vector<vertex> *vertices,
                    std::vector<vertex> *colors,
                    std::vector<vertex> *normals,
                    const openvdb::Vec3f &min,
                    const openvdb::Vec3f &max,
                    const openvdb::Vec3f &color,
                    const bool shaded)
{
	using namespace openvdb;
	std::vector<Vec3f> points;

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

	if (shaded) {
		points.push_back(corners[0]);
		points.push_back(corners[1]);
		points.push_back(corners[2]);
		points.push_back(corners[3]);

		points.push_back(corners[7]);
		points.push_back(corners[6]);
		points.push_back(corners[5]);
		points.push_back(corners[4]);

		points.push_back(corners[4]);
		points.push_back(corners[5]);
		points.push_back(corners[1]);
		points.push_back(corners[0]);

		points.push_back(corners[3]);
		points.push_back(corners[2]);
		points.push_back(corners[6]);
		points.push_back(corners[7]);

		points.push_back(corners[3]);
		points.push_back(corners[7]);
		points.push_back(corners[4]);
		points.push_back(corners[0]);

		points.push_back(corners[1]);
		points.push_back(corners[5]);
		points.push_back(corners[6]);
		points.push_back(corners[2]);

		if (normals) {
			int i;
			vertex nor = get_normal(corners[0], corners[1], corners[2], corners[3]);

			for (i = 0; i < 4; ++i) {
				normals->push_back(nor);
			}

			nor = get_normal(corners[7], corners[6], corners[5], corners[1]);

			for (i = 0; i < 4; ++i) {
				normals->push_back(nor);
			}

			nor = get_normal(corners[4], corners[5], corners[1], corners[0]);

			for (i = 0; i < 4; ++i) {
				normals->push_back(nor);
			}

			nor = get_normal(corners[3], corners[2], corners[6], corners[7]);

			for (i = 0; i < 4; ++i) {
				normals->push_back(nor);
			}

			nor = get_normal(corners[3], corners[7], corners[4], corners[0]);

			for (i = 0; i < 4; ++i) {
				normals->push_back(nor);
			}

			nor = get_normal(corners[1], corners[5], corners[6], corners[2]);

			for (i = 0; i < 4; ++i) {
				normals->push_back(nor);
			}
		}
	}
	else {
		points.push_back(corners[0]);
		points.push_back(corners[3]);
		points.push_back(corners[2]);
		points.push_back(corners[1]);

		points.push_back(corners[0]);
		points.push_back(corners[4]);
		points.push_back(corners[5]);
		points.push_back(corners[1]);

		points.push_back(corners[7]);
		points.push_back(corners[3]);
		points.push_back(corners[2]);
		points.push_back(corners[6]);

		points.push_back(corners[7]);
		points.push_back(corners[4]);
		points.push_back(corners[5]);
		points.push_back(corners[6]);
	}

	for (size_t i = 0; i < points.size(); ++i) {
		add_point(vertices, colors, points[i], color);
	}
}

void OpenVDBPrimitive_draw_tree(OpenVDBPrimitive *vdb_prim, const bool draw_root,
                                const bool draw_level_1, const bool draw_level_2,
                                const bool draw_leaves)
{
	using namespace openvdb;
	using namespace openvdb::math;

	FloatGrid::Ptr grid = gridPtrCast<FloatGrid>(vdb_prim->getGridPtr());

	/* The following colors are meant to be the same as in the example images of
	 * "VDB: High-Resolution Sparse Volumes With Dynamic Topology",
	 * K. Museth, 2013 */
	Vec3f node_color[4] = {
	    Vec3f(0.0450f, 0.0450f, 0.0450f), // root node
	    Vec3f(0.0432f, 0.3300f, 0.0411f), // first internal node level
	    Vec3f(0.8710f, 0.3940f, 0.0191f), // intermediate internal node levels
	    Vec3f(0.0060f, 0.2790f, 0.6250f)  // leaf nodes
	};

	CoordBBox bbox;
	std::vector<vertex> vertices, colors;
	Vec3f wmin, wmax, color;

	for (FloatTree::NodeCIter node_iter = grid->tree().cbeginNode(); node_iter; ++node_iter) {
		const int level = node_iter.getLevel();

		if (level == 0 && !draw_leaves) {
			continue;
		}
		else if (level == 1 && !draw_level_2) {
			continue;
		}
		else if (level == 2 && !draw_level_1) {
			continue;
		}
		else if (level == 3 && !draw_root) {
			continue;
		}
		else {
			node_iter.getBoundingBox(bbox);

			const Vec3f min(bbox.min().x() - 0.5f, bbox.min().y() - 0.5f, bbox.min().z() - 0.5f);
			const Vec3f max(bbox.max().x() + 0.5f, bbox.max().y() + 0.5f, bbox.max().z() + 0.5f);

			wmin = grid->indexToWorld(min);
			wmax = grid->indexToWorld(max);

			color = node_color[std::max(3 - level, 0)];

			add_box(&vertices, &colors, NULL, wmin, wmax, color, false);
		}
	}

	glDisable(GL_CULL_FACE);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glEnable(GL_BLEND);

	glVertexPointer(3, GL_FLOAT, 0, &vertices[0]);
	glColorPointer(3, GL_FLOAT, 0, &colors[0]);
	glDrawArrays(GL_QUADS, 0, vertices.size());

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void OpenVDBPrimitive_draw_values(OpenVDBPrimitive *vdb_prim, float tolerance, float point_size, const bool draw_box, const int lod)
{
	using namespace openvdb;
	using namespace openvdb::math;

	FloatGrid::Ptr grid = gridPtrCast<FloatGrid>(vdb_prim->getGridPtr());
	std::vector<vertex> vertices, colors, normals;

	size_t i = 0;
	float fac = static_cast<float>(lod) * 0.01f;
	size_t num_voxels = grid->activeVoxelCount();
	float num_points = std::max(1.0f, floorf(num_voxels * fac));
	float lod_fl = floorf(num_voxels / num_points);

	vertices.reserve(num_points);
	colors.reserve(num_points);
	normals.reserve(num_points);

	if (draw_box) {
		CoordBBox bbox;
		Vec3f wmin, wmax;

		/* The following colors are meant to be the same as in the example images of
		 * "VDB: High-Resolution Sparse Volumes With Dynamic Topology",
		 * K. Museth, 2013 */
		Vec3f voxel_color[2] = {
		    Vec3f(0.523f, 0.0325175f, 0.0325175f), // positive values
		    Vec3f(0.92f, 0.92f, 0.92f), // negative values
		};

		Vec3f color;

		for (FloatGrid::ValueOnCIter iter = grid->cbeginValueOn(); iter; ++iter) {
			float value = iter.getValue();

			if (value < tolerance) {
				continue;
			}

			if (fmod((float)i, lod_fl) == 0.0f) {
				iter.getBoundingBox(bbox);

				const Vec3f min(bbox.min().x() - 0.5f, bbox.min().y() - 0.5f, bbox.min().z() - 0.5f);
				const Vec3f max(bbox.max().x() + 0.5f, bbox.max().y() + 0.5f, bbox.max().z() + 0.5f);

				wmin = grid->indexToWorld(min);
				wmax = grid->indexToWorld(max);

				color = isNegative(value) ? voxel_color[1] : voxel_color[0];
				add_box(&vertices, &colors, &normals, wmin, wmax, color, true);
			}

			i++;
		}
	}
	else {
		for (FloatGrid::ValueOnCIter iter = grid->cbeginValueOn(); iter; ++iter) {
			float value = iter.getValue();

			if (value < tolerance) {
				continue;
			}

			if (fmod((float)i, lod_fl) == 0.0f) {
				Vec3f point = grid->indexToWorld(iter.getCoord());
				Vec3f color(value);
				add_point(&vertices, &colors, point, color);
			}

			i++;
		}
	}

	glPointSize(point_size);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	if (draw_box)
		glEnableClientState(GL_NORMAL_ARRAY);

	glEnable(GL_LIGHTING);
	glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);
	glShadeModel(GL_SMOOTH);

	glVertexPointer(3, GL_FLOAT, 0, &vertices[0]);
	glColorPointer(3, GL_FLOAT, 0, &colors[0]);
	glNormalPointer(GL_FLOAT, 0, &normals[0]);
	glDrawArrays(draw_box ? GL_QUADS : GL_POINTS, 0, vertices.size());

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	if (draw_box)
		glDisableClientState(GL_NORMAL_ARRAY);

	glDisable(GL_COLOR_MATERIAL);
	glDisable(GL_LIGHTING);
}

} // namespace internal

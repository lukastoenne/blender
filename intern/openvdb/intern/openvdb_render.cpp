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
                      openvdb::Vec3f point, openvdb::Vec3f color)
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

static void add_box(std::vector<vertex> *vertices, std::vector<vertex> *colors,
                    openvdb::Vec3f min, openvdb::Vec3f max, openvdb::Vec3f color)
{
	using namespace openvdb;
	Vec3f point;

	point = Vec3f(min.x(), min.y(), min.z());
	add_point(vertices, colors, point, color);
	point = Vec3f(max.x(), min.y(), min.z());
	add_point(vertices, colors, point, color);
	point = Vec3f(max.x(), min.y(), max.z());
	add_point(vertices, colors, point, color);
	point = Vec3f(min.x(), min.y(), max.z());
	add_point(vertices, colors, point, color);

	point = Vec3f(min.x(), min.y(), min.z());
	add_point(vertices, colors, point, color);
	point = Vec3f(min.x(), max.y(), min.z());
	add_point(vertices, colors, point, color);
	point = Vec3f(min.x(), max.y(), max.z());
	add_point(vertices, colors, point, color);
	point = Vec3f(min.x(), min.y(), max.z());
	add_point(vertices, colors, point, color);

	point = Vec3f(max.x(), max.y(), min.z());
	add_point(vertices, colors, point, color);
	point = Vec3f(max.x(), min.y(), min.z());
	add_point(vertices, colors, point, color);
	point = Vec3f(max.x(), min.y(), max.z());
	add_point(vertices, colors, point, color);
	point = Vec3f(max.x(), max.y(), max.z());
	add_point(vertices, colors, point, color);

	point = Vec3f(max.x(), max.y(), min.z());
	add_point(vertices, colors, point, color);
	point = Vec3f(min.x(), max.y(), min.z());
	add_point(vertices, colors, point, color);
	point = Vec3f(min.x(), max.y(), max.z());
	add_point(vertices, colors, point, color);
	point = Vec3f(max.x(), max.y(), max.z());
	add_point(vertices, colors, point, color);
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
	    Vec3f(0.0060f, 0.2790f, 0.6250f), // leaf nodes
	    Vec3f(0.8710f, 0.3940f, 0.0191f), // intermediate internal node levels
	    Vec3f(0.0432f, 0.3300f, 0.0411f), // first internal node level
	    Vec3f(0.0450f, 0.0450f, 0.0450f)  // root node
	};

	CoordBBox bbox;
	std::vector<vertex> vertices, colors;
	Vec3f wmin, wmax, color;

	for (FloatTree::NodeCIter node_iter = grid->tree().cbeginNode(); node_iter; ++node_iter) {
		node_iter.getBoundingBox(bbox);

		const Vec3f min(bbox.min().x() - 0.5f, bbox.min().y() - 0.5f, bbox.min().z() - 0.5f);
		const Vec3f max(bbox.max().x() + 0.5f, bbox.max().y() + 0.5f, bbox.max().z() + 0.5f);

		wmin = grid->indexToWorld(min);
		wmax = grid->indexToWorld(max);

		const int level = node_iter.getLevel();

		if (level == 0) {
			if (!draw_leaves) {
				continue;
			}
			color = node_color[0];
		}
		else if (level == 1) {
			if (!draw_level_2) {
				continue;
			}
			color = node_color[1];
		}
		else if (level == 2) {
			if (!draw_level_1) {
				continue;
			}
			color = node_color[2];
		}
		else {
			if (!draw_root) {
				continue;
			}
			color = node_color[3];
		}

		add_box(&vertices, &colors, wmin, wmax, color);
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

void OpenVDBPrimitive_draw_values(OpenVDBPrimitive *vdb_prim, float tolerance, float point_size, const bool draw_box)
{
	using namespace openvdb;
	using namespace openvdb::math;

	FloatGrid::Ptr grid = gridPtrCast<FloatGrid>(vdb_prim->getGridPtr());
	std::vector<vertex> vertices, colors;

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

			iter.getBoundingBox(bbox);

			const Vec3f min(bbox.min().x() - 0.5f, bbox.min().y() - 0.5f, bbox.min().z() - 0.5f);
			const Vec3f max(bbox.max().x() + 0.5f, bbox.max().y() + 0.5f, bbox.max().z() + 0.5f);

			wmin = grid->indexToWorld(min);
			wmax = grid->indexToWorld(max);

			color = isNegative(value) ? voxel_color[1] : voxel_color[0];
			add_box(&vertices, &colors, wmin, wmax, color);
		}
	}
	else {
		for (FloatGrid::ValueOnCIter iter = grid->cbeginValueOn(); iter; ++iter) {
			float value = iter.getValue();

			if (value < tolerance) {
				continue;
			}

			Vec3f point = grid->indexToWorld(iter.getCoord());
			Vec3f color(value);
			add_point(&vertices, &colors, point, color);
		}
	}

	glPointSize(point_size);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnable(GL_LIGHTING);
	glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);
	glShadeModel(GL_SMOOTH);

	glVertexPointer(3, GL_FLOAT, 0, &vertices[0]);
	glColorPointer(3, GL_FLOAT, 0, &colors[0]);
	glDrawArrays(draw_box ? GL_QUADS : GL_POINTS, 0, vertices.size());

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisable(GL_COLOR_MATERIAL);
	glDisable(GL_LIGHTING);
}

}

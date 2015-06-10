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

#include "openvdb_primitive.h"

OpenVDBPrimitive::OpenVDBPrimitive()
{}

OpenVDBPrimitive::~OpenVDBPrimitive()
{}

openvdb::GridBase &OpenVDBPrimitive::getGrid()
{
    return *m_grid;
}

const openvdb::GridBase &OpenVDBPrimitive::getConstGrid() const
{
    return *m_grid;
}

openvdb::GridBase::Ptr OpenVDBPrimitive::getGridPtr()
{
    return m_grid;
}

openvdb::GridBase::ConstPtr OpenVDBPrimitive::getConstGridPtr() const
{
    return m_grid;
}

void OpenVDBPrimitive::setGrid(openvdb::GridBase::Ptr grid)
{
    m_grid = grid->copyGrid();
}

static openvdb::Mat4R convertMatrix(const float mat[4][4])
{
    return openvdb::Mat4R(
                mat[0][0], mat[0][1], mat[0][2], mat[0][3],
                mat[1][0], mat[1][1], mat[1][2], mat[1][3],
                mat[2][0], mat[2][1], mat[2][2], mat[2][3],
                mat[3][0], mat[3][1], mat[3][2], mat[3][3]);
}

/* A simple protection to avoid crashes for cases when something goes wrong for
 * some reason in the matrix creation. */
static openvdb::math::MapBase::Ptr createAffineMap(const float mat[4][4])
{
    using namespace openvdb::math;
    MapBase::Ptr transform;

    try {
        transform.reset(new AffineMap(convertMatrix(mat)));
    }
    catch (const openvdb::ArithmeticError &e) {
        std::cerr << e.what() << "\n";
        transform.reset(new AffineMap());
    }

    return transform;
}

void OpenVDBPrimitive::setTransform(const float mat[4][4])
{
    using namespace openvdb::math;

    Transform::Ptr transform = Transform::Ptr(new Transform(createAffineMap(mat)));
    m_grid->setTransform(transform);
}

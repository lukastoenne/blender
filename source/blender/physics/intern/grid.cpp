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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file grid.cpp
 *  \ingroup bph
 */

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_math.h"
}

#include "grid.h"

namespace BPH {

Grid::Grid() :
    cellsize(0.0f),
    inv_cellsize(0.0f),
    num_cells(0)
{
	zero_v3(offset);
	zero_v3_int(res);
}

Grid::~Grid()
{
}

void Grid::resize(float _cellsize, const float _offset[3], const int _res[3])
{
	cellsize = _cellsize;
	inv_cellsize = 1.0f / _cellsize;
	copy_v3_v3(offset, _offset);
	copy_v3_v3_int(res, _res);
	num_cells = _res[0] * _res[1] * _res[2];
	
	divergence = lVector(num_cells);
	pressure = lVector(num_cells);
}

void Grid::init()
{
	divergence.setZero();
}

void Grid::clear()
{
	pressure.setZero();
}

} /* namespace BPH */

#include "implicit.h"


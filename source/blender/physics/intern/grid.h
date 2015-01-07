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

#ifndef __BPH_GRID_H__
#define __BPH_GRID_H__

/** \file grid.h
 *  \ingroup bph
 */

#include "BLI_utildefines.h"

#include "eigen_utils.h"

namespace BPH {

typedef struct Grid {
	Grid();
	~Grid();
	
	void resize(float cellsize, const float offset[3], const int res[3]);
	
	void init();
	void clear();
	
	float cellsize, inv_cellsize;
	float offset[3];
	int res[3];
	int num_cells;
	
	lVector divergence;
	lVector pressure;
	
	struct SimDebugData *debug_data;
	float debug1, debug2;
	int debug3, debug4;
	
} Grid;

} /* namespace BPH */

#endif

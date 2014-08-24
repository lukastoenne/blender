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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <cstring>

#include "HAIR_debug.h"
#include "HAIR_volume.h"

HAIR_NAMESPACE_BEGIN

bool Debug::active = false;
#ifdef HAIR_DEBUG
ThreadMutex Debug::mutex;
Debug::Elements Debug::elements;
#endif

HAIR_SolverDebugVolume *Debug::volume(Volume *vol)
{
	HAIR_SolverDebugVolume *dvol = (HAIR_SolverDebugVolume *)MEM_callocN(sizeof(HAIR_SolverDebugVolume), "hair debug volume");
	
	dvol->size_x = vol->size_x();
	dvol->size_y = vol->size_y();
	dvol->size_z = vol->size_z();
	
	dvol->dimensions[0] = dvol->dimensions[1] = dvol->dimensions[2] = 10.0f;
	
	int totsize = vol->size_x() * vol->size_y() * vol->size_z();
	dvol->data = (HAIR_SolverDebugVoxel *)MEM_mallocN(sizeof(HAIR_SolverDebugVoxel) * totsize, "hair debug voxel data");
	for (int i = 0; i < totsize; ++i) {
		dvol->data[i].r = vol->randomstuff.data()[i];
	}
}

HAIR_NAMESPACE_END

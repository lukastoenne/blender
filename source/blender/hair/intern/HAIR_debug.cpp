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

extern "C" {
#include "DNA_texture_types.h"
}

#include "HAIR_debug.h"
#include "HAIR_volume.h"

HAIR_NAMESPACE_BEGIN

bool Debug::active = false;
#ifdef HAIR_DEBUG
ThreadMutex Debug::mutex;
Debug::Elements Debug::elements;
#endif

bool Debug::texture_volume(Volume *vol, VoxelData *vd)
{
	vd->resol[0] = vol->size_x();
	vd->resol[1] = vol->size_y();
	vd->resol[2] = vol->size_z();
	
	size_t totres = (size_t)vol->size_x() * (size_t)vol->size_y() * (size_t)vol->size_z();
	vd->data_type = TEX_VD_INTENSITY;
	if (totres > 0) {
		vd->dataset = (float *)MEM_mapallocN(sizeof(float) * (totres), "hair volume texture data");
		for (int i = 0; i < totres; ++i) {
			vd->dataset[i] = vol->randomstuff.data()[i];
		}
	}
	else
		vd->dataset = NULL;
	
	return true;
}

HAIR_NAMESPACE_END

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

#ifndef __HAIR_VOLUME_H__
#define __HAIR_VOLUME_H__

#include <vector>

#include "MEM_guardedalloc.h"

#include "HAIR_types.h"

HAIR_NAMESPACE_BEGIN

template <typename T>
struct VolumeAttribute {
	VolumeAttribute()
	{
		m_data = NULL;
		m_alloc_size = 0;
		m_size = 0;
	}
	
	VolumeAttribute(int size)
	{
		alloc(size);
	}

	~VolumeAttribute()
	{
		free();
	}
	
	T *data() { return m_data; }
	const T *data() const { return m_data; }
	
	int size() const { return m_size; }
	
	void resize(int size)
	{
		if (size > m_alloc_size) {
			free();
			alloc(size);
		}
		else {
			m_size = size;
		}
	}
	
	void shrink_to_fit()
	{
		if (m_size < m_alloc_size) {
			free();
			alloc(m_size);
		}
	}
	
protected:
	void alloc(int size)
	{
		m_data = size > 0 ? (T*)MEM_mallocN(sizeof(T) * size, "Hair Volume Attribute") : NULL;
		m_alloc_size = size;
		m_size = size;
	}
	
	void free()
	{
		if (m_data) {
			MEM_freeN(m_data);
			m_data = NULL;
		}
		m_alloc_size = 0;
		m_size = 0;
	}
	
private:
	T *m_data;
	int m_size, m_alloc_size;
};

typedef VolumeAttribute<float> VolumeAttributeFloat;
typedef VolumeAttribute<int> VolumeAttributeInt;
typedef VolumeAttribute<float3> VolumeAttributeFloat3;

struct Volume {
	Volume();
	
	void resize(int x, int y, int z);
	
	VolumeAttributeFloat randomstuff;
	
private:
	int size_x, size_y, size_z;
};

HAIR_NAMESPACE_END

#endif

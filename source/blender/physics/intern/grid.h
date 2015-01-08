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

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "eigen_utils.h"

namespace BPH {

/* external function for trilinear interpolation of arbitrary types,
 * with specialization for non-primitive vectors, etc.
 */
template <typename T>
BLI_INLINE T interp_trilinear(const T data[8], const float uvw[3], const float muvw[3])
{
	return muvw[2]*( muvw[1]*( muvw[0]*data[0] + uvw[0]*data[1] )   +
		              uvw[1]*( muvw[0]*data[2] + uvw[0]*data[3] ) ) +
		    uvw[2]*( muvw[1]*( muvw[0]*data[4] + uvw[0]*data[5] )   +
		              uvw[1]*( muvw[0]*data[6] + uvw[0]*data[7] ) );
}

template <>
float3 interp_trilinear<float3>(const float3 data[8], const float uvw[3], const float muvw[3])
{
	float x = muvw[2]*( muvw[1]*( muvw[0]*data[0].x + uvw[0]*data[1].x )   +
		                 uvw[1]*( muvw[0]*data[2].x + uvw[0]*data[3].x ) ) +
		       uvw[2]*( muvw[1]*( muvw[0]*data[4].x + uvw[0]*data[5].x )   +
		                 uvw[1]*( muvw[0]*data[6].x + uvw[0]*data[7].x ) );
	float y = muvw[2]*( muvw[1]*( muvw[0]*data[0].y + uvw[0]*data[1].y )   +
		                 uvw[1]*( muvw[0]*data[2].y + uvw[0]*data[3].y ) ) +
		       uvw[2]*( muvw[1]*( muvw[0]*data[4].y + uvw[0]*data[5].y )   +
		                 uvw[1]*( muvw[0]*data[6].y + uvw[0]*data[7].y ) );
	float z = muvw[2]*( muvw[1]*( muvw[0]*data[0].z + uvw[0]*data[1].z )   +
		                 uvw[1]*( muvw[0]*data[2].z + uvw[0]*data[3].z ) ) +
		       uvw[2]*( muvw[1]*( muvw[0]*data[4].z + uvw[0]*data[5].z )   +
		                 uvw[1]*( muvw[0]*data[6].z + uvw[0]*data[7].z ) );
	return float3(x, y, z);
}

template <typename T>
struct GridHash {
	GridHash() :
	    m_data(NULL),
	    m_size(0)
	{
		zero_v3_int(m_stride);
		zero_v3_int(m_res);
	}
	
	~GridHash()
	{
		if (m_data)
			MEM_freeN(m_data);
	}
	
	const int* resolution() const { return m_res; }
	int size() const { return m_size; }
	
	void resize(const int res[3])
	{
		if (m_data)
			MEM_freeN(m_data);
		
		copy_v3_v3_int(m_res, res);
		m_stride[0] = 1;
		m_stride[1] = res[0];
		m_stride[2] = res[0] * res[1];
		m_size      = res[0] * res[1] * res[2];
		
		m_data = (T *)MEM_mallocN(sizeof(T) * m_size, "grid hash data");
	}
	
	void clear()
	{
		memset(m_data, 0, sizeof(T) * m_size);
	}
	
	inline T* get(int x, int y, int z)
	{
		if (x >= 0 && x < m_res[0] && y >= 0 && y < m_res[1] && z >= 0 && z < m_res[2])
			return m_data + x + y * m_stride[1] + z * m_stride[2];
		else
			return NULL;
	}
	
	inline const T* get(int x, int y, int z) const
	{
		if (x >= 0 && x < m_res[0] && y >= 0 && y < m_res[1] && z >= 0 && z < m_res[2])
			return m_data + x + y * m_stride[1] + z * m_stride[2];
		else
			return NULL;
	}
	
	inline T& add(int x, int y, int z)
	{
		BLI_assert(x >= 0 && x < m_res[0] && y >= 0 && y < m_res[1] && z >= 0 && z < m_res[2]);
		return *(m_data + x + y * m_stride[1] + z * m_stride[2]);
	}
	
	inline const T& add(int x, int y, int z) const
	{
		BLI_assert(x >= 0 && x < m_res[0] && y >= 0 && y < m_res[1] && z >= 0 && z < m_res[2]);
		return *(m_data + x + y * m_stride[1] + z * m_stride[2]);
	}
	
	void from_eigen(lVector &r) const
	{
		BLI_assert(r.rows() == m_size);
		for (int i = 0; i < m_size; ++i) {
			m_data[i] = r.coeff(i);
		}
	}
	
	void to_eigen(lVector &r) const
	{
		BLI_assert(r.rows() == m_size);
		for (int i = 0; i < m_size; ++i) {
			r.coeffRef(i) = m_data[i];
		}
	}
	
	inline T interpolate(const float vec[3])
	{
		T data[8];
		float uvw[3], muvw[3];
		int stride[3] = { 1, m_res[0], m_res[0] * m_res[1] };
		
		int offset = interp_weights(vec, uvw);
		muvw[0] = 1.0f - uvw[0];
		muvw[1] = 1.0f - uvw[1];
		muvw[2] = 1.0f - uvw[2];
		
		data[0] = m_data[offset                                    ];
		data[1] = m_data[offset                         + stride[0]];
		data[2] = m_data[offset             + stride[1]            ];
		data[3] = m_data[offset             + stride[1] + stride[0]];
		data[4] = m_data[offset + stride[2]                        ];
		data[5] = m_data[offset + stride[2]             + stride[0]];
		data[6] = m_data[offset + stride[2] + stride[1]            ];
		data[7] = m_data[offset + stride[2] + stride[1] + stride[0]];
		
		return interp_trilinear(data, uvw, muvw);
	}
	
protected:
	
	inline int index_axis(const float vec[3], int axis)
	{
		return min_ii( max_ii( (int)vec[axis], 0), m_res[axis]-2 );
	}
	
	inline int offset(const float vec[3], const int res[3], const float gmin[3], float scale)
	{
		int i, j, k;
		i = index_axis(vec, 0);
		j = index_axis(vec, 1);
		k = index_axis(vec, 2);
		return i + (j + k*res[1])*res[0];
	}
	
	inline int interp_weights(const float vec[3], float uvw[3])
	{
		int i = index_axis(vec, 0);
		int j = index_axis(vec, 1);
		int k = index_axis(vec, 2);
		int offset = i + (j + k*m_res[1])*m_res[0];
		
		uvw[0] = vec[0] - (float)i;
		uvw[1] = vec[1] - (float)j;
		uvw[2] = vec[2] - (float)k;
		
		return offset;
	}
	
private:
	T *m_data; /* XXX TODO stub array for now, actually use a hash table here! */
	int m_res[3];
	int m_stride[3];
	int m_size;
};

typedef struct Grid {
	Grid();
	~Grid();
	
	void resize(float cellsize, const float offset[3], const int res[3]);
	
	void init();
	void clear();
	
	int set_inner_cells(GridHash<bool> &bounds, GridHash<float3> &normal, struct Object *ob) const;
	
	void calc_divergence(GridHash<float> &divergence, const GridHash<bool> &source, const GridHash<float3> &source_normal) const;
	void calc_gradient(GridHash<float3> &velocity, const GridHash<float> &pressure) const;
	void normalize(GridHash<float3> &velocity) const;
	
	void solve_pressure(GridHash<float> &pressure, const GridHash<float> &divergence) const;
	
	float cellsize, inv_cellsize;
	float offset[3];
	int res[3];
	int num_cells;
	
	struct SimDebugData *debug_data;
	float debug1, debug2;
	int debug3, debug4;
	
} Grid;

} /* namespace BPH */

#endif

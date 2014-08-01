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

#ifndef __HAIR_SMOOTHING_H__
#define __HAIR_SMOOTHING_H__

#include "HAIR_curve.h"
#include "HAIR_math.h"
#include "HAIR_memalloc.h"
#include "HAIR_types.h"

HAIR_NAMESPACE_BEGIN

template <typename T>
struct NullDefaultCtor {
	static const T null = T();
};

//template <typename T, typename OperationsT = StdArithmetic<T> >
template <typename T, typename NullT = NullDefaultCtor<T> >
struct SmoothingIterator {
	SmoothingIterator(float rest_length, float amount) :
	    num(1),
	    beta(min_ff(1.0f, amount > 0.0f ? 1.0f - expf(- rest_length / amount) : 1.0f)),
	    f1(2.0f*(1.0f-beta)),
	    f2((1.0f-beta)*(1.0f-beta)),
	    f3(beta*beta)
	{
	}
	
	T begin(const T &val0, const T &val1)
	{
		dval_pp = dval_p = val1 - val0;
		
		res_p = val0;
		
		val = val1;
		
		num = 2;
		
		return val0;
	}
	
	T next(const T &val_n)
	{
		T ndval_p = f1 * dval_p - f2 * dval_pp + f3 * (val_n - val);
		T nres = res_p + dval_p;
		
		dval_pp = dval_p;
		dval_p = ndval_p;
		
		res_p = nres;
		
		val = val_n;
		
		++num;
		
		return nres;
	}
	
	T val;
	T res_p;
	T dval_p, dval_pp;
	int num;
	const float beta, f1, f2, f3;

	HAIR_CXX_CLASS_ALLOC(SmoothingIterator)
};

struct FrameIterator {
	FrameIterator(float rest_length, float amount, int totpoints, const float3 &co0, const float3 &co1) :
	    m_loc_iter(rest_length, amount),\
	    m_totpoints(totpoints),
	    m_frame(Transform::Identity)
	{
		float3 smooth_co1 = m_loc_iter.begin(co0, co1);
		
		normalize_v3_v3(m_prev_dir, smooth_co1 - co0);
		m_prev_co = smooth_co1;
	}
	
	int cur() const { return m_loc_iter.num; }
	
	bool valid() const
	{
		/* note: totpoints is still allowed, so the iterator yields a frame
		 * for the last point (without reading from loc_iter though).
		 */
		return m_loc_iter.num <= m_totpoints;
	}
	
	void next(const float3 &co)
	{
		float3 dir;
		static const float epsilon = 1.0e-6;
		
		float3 smooth_co = m_loc_iter.next(co);
		
		normalize_v3_v3(dir, smooth_co - m_prev_co);
		
		float3 C = cross_v3_v3(m_prev_dir, dir);
		float D = dot_v3_v3(m_prev_dir, dir);
		/* test if angle too small for usable result
		 * XXX define epsilon better
		 */
		if (fabsf(D) < 1.0f - epsilon) {
			/* construct rotation from one segment to the next */
			float cos_phi_2 = sqrtf((1.0f + D) * 0.5f); /* cos(phi/2) -> quaternion w element */
			float3 axis = C / (2.0f * cos_phi_2); /* axis * sin(phi/2) -> quaternion (x,y,z) elements */
			float4 rot(axis.x, axis.y, axis.z, cos_phi_2);
			
			/* apply the local rotation to the frame axes */
			m_frame.normal = mul_qt_v3(rot, m_frame.normal);
			m_frame.tangent = mul_qt_v3(rot, m_frame.tangent);
			m_frame.cotangent = mul_qt_v3(rot, m_frame.cotangent);
		}
		
		m_prev_dir = dir;
		m_prev_co = smooth_co;
	}
	
	const Frame &frame() const { return m_frame; }
	
protected:
	SmoothingIterator<float3> m_loc_iter;
	int m_totpoints;
	float3 m_prev_dir, m_prev_co;
	
	Frame m_frame;
	
	HAIR_CXX_CLASS_ALLOC(SmoothingIterator)
};

HAIR_NAMESPACE_END

#endif

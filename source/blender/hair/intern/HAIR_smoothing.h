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

extern "C" {
#include "BLI_math.h"
}

#include "HAIR_curve.h"
#include "HAIR_debug.h"
#include "HAIR_math.h"
#include "HAIR_memalloc.h"
#include "HAIR_types.h"

HAIR_NAMESPACE_BEGIN

template <typename T>
struct NullDefaultCtor {
	static const T null = T();
};

/* WalkerT must have the following interface:
 * 
 * WalkerT()            : default constructor (uninitialized)
 * operator =           : assignment/copy operator
 * data_t               : type of the returned value
 * data_t read()        : return the next value
 * int size() const     : total number of elements
 */

template <typename WalkerT>
struct SmoothingIterator {
	typedef typename WalkerT::data_t data_t;
	
	SmoothingIterator()
	{}
	
	SmoothingIterator(const WalkerT &_walker, float rest_length, float amount) :
	    walker(_walker),
	    num(0),
	    tot(walker.size()),
	    beta(min_ff(1.0f, amount > 0.0f ? 1.0f - expf(- rest_length / amount) : 1.0f)),
	    f1(2.0f*(1.0f-beta)),
	    f2((1.0f-beta)*(1.0f-beta)),
	    f3(beta*beta)
	{
		data_t val0 = walker.read();
		data_t val1 = walker.read();
		
		dval_p = dval = val1 - val0;
		
		res = val0;
		
		val = val1;
	}
	
	bool valid() const { return num < tot; }
	
	int index() const { return num; }
	
	data_t get() const
	{
		return res;
	}
	
	void next()
	{
		data_t nres = res + dval;
		
		data_t val_n = walker.read();
		data_t ndval = f1 * dval - f2 * dval_p + f3 * (val_n - val);
		
		dval_p = dval;
		dval = ndval;
		
		res = nres;
		
		val = val_n;
		
		++num;
	}
	
protected:
	WalkerT walker;
	int num, tot;
	
	data_t val;
	data_t res;
	data_t dval, dval_p;
	
	float beta, f1, f2, f3;

	HAIR_CXX_CLASS_ALLOC(SmoothingIterator)
};

/* XXX optimizing the frame iterator could be quite rewarding in terms of performance ...
 * See "Parallel Transport Approach to Curve Framing" (Hanson et al. 1995)
 * ftp://cgi.soic.indiana.edu/pub/techreports/TR425.pdf
 */

template <typename WalkerT>
struct FrameIterator {
	FrameIterator()
	{}

	FrameIterator(const WalkerT &walker, float rest_length, float amount, const Frame &initial_frame) :
	    m_loc_iter(walker, rest_length, amount),
	    m_frame(initial_frame)
	{
		m_dir = initial_frame.normal;
	}
	
	int index() const { return m_loc_iter.index(); }
	bool valid() const { return m_loc_iter.valid(); }
	
	void next()
	{
		static const float epsilon = 1.0e-6;
		
		float3 prev_dir = m_dir;
		
		float3 prev_co = m_loc_iter.get();
		m_loc_iter.next();
		float3 co = m_loc_iter.get();
		normalize_v3_v3(m_dir, co - prev_co);
		
		float3 C = cross_v3_v3(prev_dir, m_dir);
		float D = dot_v3v3(prev_dir, m_dir);
		if (fabsf(D) > epsilon && fabsf(D) < 1.0f - epsilon) {
			/* half angle sine, cosine */
			D = sqrtf((1.0f + D) * 0.5f);
			C = C / D * 0.5f;
			/* construct rotation from one segment to the next */
			float4 rot(C.x, C.y, C.z, D);
			/* apply the local rotation to the frame axes */
			m_frame.normal = mul_qt_v3(rot, m_frame.normal);
			m_frame.tangent = mul_qt_v3(rot, m_frame.tangent);
			m_frame.cotangent = mul_qt_v3(rot, m_frame.cotangent);
		}
	}
	
	const Frame &frame() const { return m_frame; }
	
protected:
	SmoothingIterator<WalkerT> m_loc_iter;
	float3 m_dir;
	
	Frame m_frame;
	
	HAIR_CXX_CLASS_ALLOC(SmoothingIterator)
};

HAIR_NAMESPACE_END

#endif

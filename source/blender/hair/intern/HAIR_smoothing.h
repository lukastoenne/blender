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

#include "HAIR_math.h"
#include "HAIR_memalloc.h"

HAIR_NAMESPACE_BEGIN

#if 0
template <typename T>
struct StdArithmetic {
	static T sum(const T &a, const T &b) {
		return a + b;
	}
	
	static T difference(const T &a, const T &b) {
		return a - b;
	}
	
	static T scale(float fac, const T &a) {
		return fac * a;
	}
};
#endif

template <typename T>
struct NullDefaultCtor {
	static const T null = T();
};

//template <typename T, typename OperationsT = StdArithmetic<T> >
template <typename T, typename NullT = NullDefaultCtor<T> >
struct SmoothingIterator {
	SmoothingIterator(float rest_length, float amount) :
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

HAIR_NAMESPACE_END

#endif

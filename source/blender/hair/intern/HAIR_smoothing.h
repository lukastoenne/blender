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

//template <typename T, typename OperationsT = StdArithmetic<T> >
template <typename T>
struct SmoothingIterator {
	SmoothingIterator(float rest_length, float amount) :
	    beta(min_ff(1.0f, 1.0f - expf(- rest_length / amount))),
	    f1(2.0f*(1.0f-beta)),
	    f2(-(1.0f-beta)*(1.0f-beta)),
	    f3(beta*beta)
	{
	}
	
	void begin(const T &val0, const T &val1)
	{
		dval_ppp = dval_pp = dval_p = val1 - val0;
		
		res_p = val0;
		res = val1;
		
		num = 1;
	}
	
	const T& next(const T &val_)
	{
		dval_ppp = dval_pp;
		dval_pp = dval_p;
		dval_p = f1 * dval_pp + f2 * dval_ppp + f3 * (val_ - val_p);
		
		T tmp = res_p;
		res_p = res;
		res = tmp + dval_p;
		
		val_p = val_;
		
		++num;
		
		return res;
	}
	
	T val_p;
	T res, res_p;
	T dval_p, dval_pp, dval_ppp;
	int num;
	const float beta, f1, f2, f3;
};

HAIR_NAMESPACE_END

#endif

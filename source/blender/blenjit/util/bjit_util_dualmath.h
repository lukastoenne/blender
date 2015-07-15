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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BJIT_DUALMATH_H__
#define __BJIT_DUALMATH_H__

#include "math.h"

#include "bjit_util_math.h"

namespace bjit {

template <typename T>
struct Dual {
	typedef T Value;
	
	Dual()
	{}
	
	template <typename U>
	Dual(const Dual<U> &other) :
	    m_value(T(other.m_value)), m_dx(T(other.m_dx)), m_dy(T(other.m_dy)), T(m_dz(other.m_dz))
	{}
	
	Dual(const T &value) :
	    m_value(value), m_dx(0), m_dy(0), m_dz(0)
	{}
	
	Dual(const T &value, const T &dx, const T &dy, const T &dz) :
	    m_value(value), m_dx(dx), m_dy(dy), m_dz(dz)
	{}
	
	const T &value() const { return m_value; }
	const T &dx() const { return m_dx; }
	const T &dy() const { return m_dy; }
	const T &dz() const { return m_dz; }
	
	T &value() { return m_value; }
	T &dx() { return m_dx; }
	T &dy() { return m_dy; }
	T &dz() { return m_dz; }
	
	operator const T& () const {
		return m_value;
	}
	
	Dual<T>& operator += (const Dual<T> &other)
	{
		m_value += other.m_value;
		m_dx += other.m_dx;
		m_dy += other.m_dy;
		m_dz += other.m_dz;
		return *this;
	}
	
	Dual<T> operator + (const Dual<T> &other)
	{
		Dual<T> res(*this);
		res += other;
		return res;
	}
	
private:
	T m_value;
	T m_dx, m_dy, m_dz;
};

/* standard types */

typedef Dual<float> dual_f;
typedef Dual<vec3_t> dual_v3;

} /* namespace bjit */

#endif

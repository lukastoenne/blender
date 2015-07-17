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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __OPENVDB_UTIL_H__
#define __OPENVDB_UTIL_H__

#include <cstdio>
#include <string>

void catch_exception(int &ret);

double time_dt();

/* A utility class which prints the time elapsed during its lifetime, useful for
 * e.g. timing the overall execution time of a function */
class ScopeTimer {
	double m_start;
	std::string m_message;

public:
	ScopeTimer(const std::string &message)
	    : m_message(message)
	{
		m_start = time_dt();
	}

	~ScopeTimer()
	{
		printf("%s: %fs\n", m_message.c_str(), time_dt() - m_start);
	}
};

#ifndef NDEBUG
#	define Timer(x) \
		ScopeTimer func(x);
#else
#	define Timer(x)
#endif

#endif /* __OPENVDB_UTIL_H__ */

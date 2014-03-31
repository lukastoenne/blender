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
 * Original Author: Brecht van Lommel
 * Contributor(s): Lukas Toenne
 */

#ifndef __DEPSGRAPH_UTIL_STRING_H__
#define __DEPSGRAPH_UTIL_STRING_H__

#include <string>
#include <cstdarg>
#include <cstdio>

using std::string;

/* format string function working like snprintf
 * http://stackoverflow.com/a/3742999
 */
inline string string_format(const std::string fmt, ...)
{
	string s;
	int n, size=100;
	bool b=false;
	va_list marker;

	while (!b)
	{
		s.resize(size);
		va_start(marker, fmt);
		n = vsnprintf((char*)s.c_str(), size, fmt.c_str(), marker);
		va_end(marker);
		if ((n>0) && ((b=(n<size))==true)) s.resize(n); else size*=2;
	}
	return s;
}

#endif /* __DEPSGRAPH_UTIL_STRING_H__ */

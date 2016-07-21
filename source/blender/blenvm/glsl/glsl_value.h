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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __GLSL_VALUE_H__
#define __GLSL_VALUE_H__

/** \file blender/blenvm/glsl/glsl_value.h
 *  \ingroup glsl
 */

#include "util_string.h"

namespace blenvm {

struct GLSLValue {
	GLSLValue(const string &name);
	~GLSLValue();
	
	const string &name() const { return m_name; }
	
private:
	string m_name;
};

} /* namespace blenvm */

#endif /* __GLSL_VALUE_H__ */

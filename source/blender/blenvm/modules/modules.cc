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

#include "mod_base.h"
#include "mod_color.h"
#include "mod_math.h"

BVM_MOD_NAMESPACE_BEGIN

#if 0
inline int force_linking()
{
	int i = 0;
	i += (long int)VALUE_FLOAT;
	i += (long int)VALUE_FLOAT3;
	i += (long int)VALUE_FLOAT4;
	i += (long int)VALUE_INT;
	i += (long int)VALUE_MATRIX44;
	i += (long int)MIX_RGB3;
	i += (long int)MIX_RGB;
	
	return i;
}

static int f = force_linking();
#endif

BVM_MOD_NAMESPACE_END

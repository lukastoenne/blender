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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/util/bvm_util_map.h
 *  \ingroup bvm
 */

#ifndef __BVM_UTIL_MAP_H__
#define __BVM_UTIL_MAP_H__

#include <map>

#include "bvm_util_hash.h"

using std::map;
using std::pair;

#if defined(BVM_NO_UNORDERED_MAP)
#  include <map>
typedef std::map unordered_map;
#endif

#if defined(BVM_TR1_UNORDERED_MAP)
#  include <tr1/unordered_map>
using std::tr1::unordered_map;
#endif

#if defined(BVM_STD_UNORDERED_MAP)
#  include <unordered_map>
using std::unordered_map;
#endif

#if defined(BVM_STD_UNORDERED_MAP_IN_TR1_NAMESPACE)
#  include <unordered_map>
using std::tr1::unordered_map;
#endif

#if !defined(BVM_NO_UNORDERED_MAP) && !defined(BVM_TR1_UNORDERED_MAP) && \
    !defined(BVM_STD_UNORDERED_MAP) && !defined(BVM_STD_UNORDERED_MAP_IN_TR1_NAMESPACE)  // NOLINT
#  error One of: BVM_NO_UNORDERED_MAP, BVM_TR1_UNORDERED_MAP,\
 BVM_STD_UNORDERED_MAP, BVM_STD_UNORDERED_MAP_IN_TR1_NAMESPACE must be defined!  // NOLINT
#endif

#endif  /* __BVM_UTIL_MAP_H__ */

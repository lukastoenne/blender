# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2014, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Joshua Leung, Lukas Toenne
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	./intern
	./util
	../blenkernel
	../blenlib
	../bmesh
	../makesdna
	../makesrna
	../modifiers
	../../../intern/guardedalloc
)

set(INC_SYS
	${BOOST_INCLUDE_DIR}
)

set(SRC
	intern/depsgraph_build.cpp
	intern/depsgraph_core.cpp
	intern/depsgraph_debug.cpp
	intern/depsgraph_eval.cpp
	intern/depsgraph_query.cpp
	intern/depsgraph_queue.cpp
	intern/depsgraph_type_defines.cpp

	DEG_depsgraph.h
	DEG_depsgraph_build.h
	DEG_depsgraph_debug.h
	DEG_depsgraph_query.h
	intern/depsgraph_eval.h
	intern/depsgraph_intern.h
	intern/depsgraph_queue.h
	intern/depsgraph_types.h
	intern/stubs.h
	util/depsgraph_util_foreach.h
	util/depsgraph_util_map.h
	util/depsgraph_util_set.h
)

add_definitions(${BOOST_DEFINITIONS})

blender_add_lib(bf_depsgraph "${SRC}" "${INC}" "${INC_SYS}")

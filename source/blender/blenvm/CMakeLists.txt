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
# The Original Code is Copyright (C) 2015, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Lukas Toenne.
#
# ***** END GPL LICENSE BLOCK *****

add_subdirectory(bvm)
add_subdirectory(compile)

if(WITH_LLVM)
	add_subdirectory(llvm)
	add_definitions(-DWITH_LLVM)
endif()

set(INC
	.
	bvm
	compile
	llvm
	intern
	util
	../blenkernel
	../blenlib
	../depsgraph
	../makesdna
	../makesrna
	../render/extern/include
	../../../intern/guardedalloc
)

set(INC_SYS
)

set(SRC
	intern/bvm_api.cc
	intern/function.cc
	intern/function.h
	intern/function_cache.cc
	intern/function_cache.h

	util/util_opcode.h
	util/util_data_ptr.h
	util/util_debug.h
	util/util_hash.h
	util/util_map.h
	util/util_math.h
	util/util_string.h
	util/util_structs.h
	util/util_thread.h

	BVM_api.h
	BVM_types.h
)

TEST_UNORDERED_MAP_SUPPORT()
if(HAVE_STD_UNORDERED_MAP_HEADER)
	if(HAVE_UNORDERED_MAP_IN_STD_NAMESPACE)
		add_definitions(-DBVM_STD_UNORDERED_MAP)
	else()
		if(HAVE_UNORDERED_MAP_IN_TR1_NAMESPACE)
			add_definitions(-DBVM_STD_UNORDERED_MAP_IN_TR1_NAMESPACE)
		else()
			add_definitions(-DBVM_NO_UNORDERED_MAP)
			message(STATUS "Replacing unordered_map/set with map/set (warning: slower!)")
		endif()
	endif()
else()
	if(HAVE_UNORDERED_MAP_IN_TR1_NAMESPACE)
		add_definitions(-DBVM_TR1_UNORDERED_MAP)
	else()
		add_definitions(-DBVM_NO_UNORDERED_MAP)
		message(STATUS "Replacing unordered_map/set with map/set (warning: slower!)")
	endif()
endif()

blender_add_lib(bf_blenvm "${SRC}" "${INC}" "${INC_SYS}")

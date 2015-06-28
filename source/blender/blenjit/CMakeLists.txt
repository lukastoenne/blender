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

set(INC
	.
	../blenfont
	../blenlib
	../blenkernel
	../depsgraph
	../gpu
	../ikplugin
	../imbuf
	../makesdna
	../makesrna
	../bmesh
	../physics
	../../../intern/guardedalloc
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
	${ZLIB_INCLUDE_DIRS}
)

set(SRC
	intern/forcefield.cpp

	BJIT_forcefield.h
)

if(WITH_LLVM)
	list(APPEND SRC
#		intern/effect_llvm.cpp
	)
	list(APPEND INC_SYS
		${LLVM_INCLUDE_DIRS}
	)
	add_definitions(-DWITH_LLVM -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -std=c++0x)
endif()

blender_add_lib(bf_blenjit "${SRC}" "${INC}" "${INC_SYS}")

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
	..
	../compile
	../intern
	../modules
	../util
	../../blenkernel
	../../blenlib
	../../makesdna
	../../makesrna
	../../../../intern/guardedalloc
)

set(INC_SYS
	${LLVM_INCLUDE_DIRS}
)

set(SRC
	llvm_compiler.cc
	llvm_compiler_simple.cc
	llvm_compiler_dual.cc
	llvm_compiler.h
	llvm_engine.cc
	llvm_engine.h
	llvm_function.cc
	llvm_function.h
	llvm_headers.h
	llvm_modules.cc
	llvm_modules.h
	llvm_types.cc
	llvm_types.h
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${RTTI_DISABLE_FLAGS}")

add_definitions(-DWITH_LLVM -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -std=c++0x)

blender_add_lib(bf_blenvm_llvm "${SRC}" "${INC}" "${INC_SYS}")

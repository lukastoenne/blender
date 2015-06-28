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

set(LLVM_SRC
	modules/mod_forcefield.cpp
)

set(LLVM_HEADERS
	modules/mod_forcefield.h
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

# modules
if(WITH_LLVM)
	# XXX TODO
	set(LLVM_IR_COMPILER "clang")
	set(LLVM_IR_INSTALL_PATH "llvm")

	macro(BLENJIT_MODULE_ADD src)
		get_filename_component(llvm_name ${src} NAME_WE)
		set(llvm_ir "${llvm_name}.ll")

		add_custom_command(
			OUTPUT ${llvm_ir}
			COMMAND ${LLVM_IR_COMPILER}
					-emit-llvm
					-S ${CMAKE_CURRENT_SOURCE_DIR}/${src}
					-o ${CMAKE_CURRENT_BINARY_DIR}/${llvm_ir}
#					-I${CMAKE_CURRENT_SOURCE_DIR}/../util
#					-I${CMAKE_CURRENT_SOURCE_DIR}/svm
			DEPENDS ${LLVM_SRC})

		delayed_install("${CMAKE_CURRENT_BINARY_DIR}" "${llvm_ir}" ${LLVM_IR_INSTALL_PATH}/modules/)
		list(APPEND llvm_irs ${llvm_ir})
	endmacro()

	foreach(src ${LLVM_SRC})
		# Compile LLVM IR code
		BLENJIT_MODULE_ADD(${src})
	endforeach()

	add_custom_target(bf_blenjit_llvm_ir ALL DEPENDS ${llvm_irs})

	delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${LLVM_HEADERS}" ${LLVM_IR_INSTALL_PATH})
endif()

blender_add_lib(bf_blenjit "${SRC}" "${INC}" "${INC_SYS}")

if(WITH_LLVM)
	add_dependencies(bf_blenjit bf_blenjit_llvm_ir)
endif()

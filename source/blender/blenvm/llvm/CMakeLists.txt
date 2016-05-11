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
	llvm_codegen.cc
	llvm_codegen.h
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

set(LLVM_SRC
	../modules/modules.cc
)
set(LLVM_HEADERS
	../modules/mod_color.h
	../modules/mod_defines.h
	../modules/mod_math.h
	../modules/mod_value.h
)

if(NOT WITH_BLENVM_IRMODULES)
	list(APPEND SRC ${LLVM_SRC} ${LLVM_HEADERS})
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${RTTI_DISABLE_FLAGS}")

add_definitions(-DWITH_LLVM -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -std=c++0x)
add_definitions(
	-DBVM_MOD_NAMESPACE_BEGIN=namespace\ blenvm\ {\ namespace\ modules\ {
	-DBVM_MOD_NAMESPACE_END=}}
)

blender_add_lib(bf_blenvm_llvm "${SRC}" "${INC}" "${INC_SYS}")


# Modules

if(WITH_BLENVM_IRMODULES)
	# XXX TODO
	set(LLVM_IR_COMPILER "clang-3.5")
	set(LLVM_IR_INSTALL_PATH "scripts/llvm")
	set(llvm_irs)

	macro(BLENVM_LLVM_MODULE_ADD src)
		get_filename_component(llvm_name ${src} NAME_WE)
		set(llvm_ir "${llvm_name}.ll")

		add_custom_command(
			OUTPUT ${llvm_ir}
			COMMAND ${LLVM_IR_COMPILER}
					-S
					${CMAKE_CURRENT_SOURCE_DIR}/${src}
					-emit-llvm
					-std=c++0x
					-o ${CMAKE_CURRENT_BINARY_DIR}/${llvm_ir}
					-DBLENVM_RUNTIME
					-D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS
					-DBVM_MOD_NAMESPACE_BEGIN
					-DBVM_MOD_NAMESPACE_END
					-DBVM_MOD_ANNOTATE_FUNCTIONS
					-I${CMAKE_CURRENT_SOURCE_DIR}/../../blenlib
			DEPENDS ${LLVM_SRC} ${LLVM_HEADERS})
	
		delayed_install("${CMAKE_CURRENT_BINARY_DIR}" "${llvm_ir}" ${LLVM_IR_INSTALL_PATH}/modules/)
		list(APPEND llvm_irs ${llvm_ir})
	endmacro()

	add_definitions(-DWITH_BLENVM_IRMODULES)

	foreach(src ${LLVM_SRC})
		# Compile LLVM IR code
		BLENVM_LLVM_MODULE_ADD(${src})
	endforeach()

	add_custom_target(bf_blenvm_llvm_modules ALL DEPENDS ${llvm_irs})
	add_dependencies(bf_blenvm_llvm bf_blenvm_llvm_modules)

	delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${LLVM_HEADERS}" ${LLVM_IR_INSTALL_PATH})
endif()

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
# Contributor(s): Kevin Dietrich.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
    .
    intern
    ../blenkernel
    ../blenlib
    ../makesdna
    ../makesrna
    ../../../intern/guardedalloc
    ../../../intern/smoke/intern
)

set(INC_SYS

)

set(SRC
    openvdb_capi.h
)

if(WITH_OPENVDB)
    add_definitions(
        -DWITH_OPENVDB
		-DDDF_DEBUG=0
    )

    list(APPEND INC_SYS
        ${OPENEXR_INCLUDE_DIRS}
        ${OPENVDB_INCLUDE_DIRS}
    )

    list(APPEND SRC
		intern/openvdb_reader.cpp
		intern/openvdb_smoke_export.cpp
		intern/openvdb_writer.cpp
        openvdb_capi.cpp
		openvdb_util.cpp

        openvdb_intern.h
    )
endif()

blender_add_lib(bf_openvdb "${SRC}" "${INC}" "${INC_SYS}")

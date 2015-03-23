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
# The Original Code is Copyright (C) 2013, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	../
	../intern
	../util
	../../blenkernel
	../../blenlib
	../../makesdna
	../../makesrna
	../../../../intern/guardedalloc
)

set(INC_SYS
	${BOOST_INCLUDE_DIR}
	${ALEMBIC_INCLUDE_DIRS}
	${OPENEXR_INCLUDE_DIR}/OpenEXR
)

set(SRC
	abc_frame_mapper.cpp
	abc_frame_mapper.h
	abc_reader.cpp
	abc_reader.h
	abc_schema.h
	abc_writer.cpp
	abc_writer.h

	abc_cloth.cpp
	abc_cloth.h
	abc_dynamicpaint.cpp
	abc_dynamicpaint.h
	abc_mesh.cpp
	abc_mesh.h
	abc_particles.cpp
	abc_particles.h
	abc_rigidbody.cpp
	abc_rigidbody.h
	abc_smoke.cpp
	abc_smoke.h
	abc_softbody.cpp
	abc_softbody.h
)

add_definitions(-DWITH_ALEMBIC)

blender_add_lib(bf_pointcache_alembic "${SRC}" "${INC}" "${INC_SYS}")

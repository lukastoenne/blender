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
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	../include
	../sculpt_paint
	../../blenfont
	../../blenkernel
	../../blenlib
	../../bmesh
	../../gpu
	../../makesdna
	../../makesrna
	../../windowmanager
	../../../../intern/guardedalloc
	../../../../intern/glew-mx
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	hair_cursor.c
	hair_edit.c
	hair_mirror.c
	hair_ops.c
	hair_select.c
	hair_stroke.c
	hair_undo.c

	hair_intern.h
)

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_editor_hair "${SRC}" "${INC}" "${INC_SYS}")

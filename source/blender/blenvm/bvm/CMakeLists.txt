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
	../modules
	../util
	../../blenkernel
	../../blenlib
	../../bmesh
	../../imbuf
	../../makesdna
	../../makesrna
	../../../../intern/guardedalloc
)

set(INC_SYS
)

set(SRC
	bvm_eval.cc
	bvm_eval.h
	bvm_eval_common.h
	bvm_eval_curve.h
	bvm_eval_image.h
	bvm_eval_math.h
	bvm_eval_mesh.h
	bvm_eval_texture.h
	bvm_instruction_list.cc
	bvm_instruction_list.h
)

blender_add_lib(bf_blenvm_bvm "${SRC}" "${INC}" "${INC_SYS}")

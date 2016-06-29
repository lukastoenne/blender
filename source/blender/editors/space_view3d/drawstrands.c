/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/drawstrands.c
 *  \ingroup spview3d
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_strand_types.h"
#include "DNA_view3d_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_strands.h"

#include "BIF_gl.h"

#include "GPU_debug.h"
#include "GPU_shader.h"
#include "GPU_strands.h"

#include "view3d_intern.h"  // own include

void draw_strands(Strands *strands, Object *ob, RegionView3D *rv3d)
{
	GPUStrands *gpu_strands = GPU_strands_get(strands);
	
	GPU_strands_bind_uniforms(gpu_strands, ob->obmat, rv3d->viewmat);
	GPU_strands_bind(gpu_strands, rv3d->viewmat, rv3d->viewinv);
	
	GLuint vertex_buffer;
	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	const size_t numverts = 4;
	float verts[12] = {
	    0.0f, 0.0f, 0.0f,
	    1.0f, 0.0f, 0.0f,
	    0.0f, 1.0f, 0.0f,
	    1.0f, 1.0f, 0.0f,
	};
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * numverts, verts, GL_STATIC_DRAW);

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, NULL);

	glDrawArrays(GL_TRIANGLES, 0, numverts);

	glDisableClientState(GL_VERTEX_ARRAY);

	/* cleanup */
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &vertex_buffer);
	
	GPU_strands_unbind(gpu_strands);
}

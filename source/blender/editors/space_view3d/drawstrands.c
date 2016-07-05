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

#include "BKE_DerivedMesh.h"
#include "BKE_strands.h"

#include "BIF_gl.h"

#include "GPU_buffers.h"
#include "GPU_debug.h"
#include "GPU_shader.h"
#include "GPU_strands.h"

#include "view3d_intern.h"  // own include

void draw_strands(Strands *strands, StrandData *data, Object *ob, RegionView3D *rv3d,
                  bool show_controls, bool show_strands)
{
	GPUStrandsShader *gpu_shader = GPU_strand_shader_get(strands);
	
	if (show_controls) {
		GPU_strands_setup_edges(data);
		GPUDrawStrands *gds = data->gpu_buffer;
		if (gds->control_points && gds->control_edges) {
			GPU_buffer_draw_elements(gds->control_edges, GL_LINES, 0,
			                         (gds->totverts - gds->totcurves) * 2);
		}
		GPU_buffers_unbind();
	}
	
	if (show_strands) {
		GPU_strand_shader_bind_uniforms(gpu_shader, ob->obmat, rv3d->viewmat);
		GPU_strand_shader_bind(gpu_shader, rv3d->viewmat, rv3d->viewinv);
		
		GPU_strands_setup_roots(data);
		GPUDrawStrands *gds = data->gpu_buffer;
		if (gds->root_points) {
			struct GPUAttrib *attrib;
			int num_attrib;
			GPU_strand_shader_get_attributes(strands->gpu_shader, &attrib, &num_attrib);
			
			int elemsize = GPU_attrib_element_size(attrib, num_attrib);
			GPU_interleaved_attrib_setup(gds->root_points, attrib, num_attrib, elemsize, false);
			
			glDrawArrays(GL_POINTS, 0, gds->totroots * elemsize);
			
			GPU_interleaved_attrib_unbind();
		}
		GPU_strands_buffer_unbind();
		
		GPU_strand_shader_unbind(gpu_shader);
	}
}

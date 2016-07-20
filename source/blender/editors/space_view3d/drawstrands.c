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

#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_strand_types.h"
#include "DNA_view3d_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_editstrands.h"
#include "BKE_DerivedMesh.h"
#include "BKE_main.h"
#include "BKE_strands.h"

#include "bmesh.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "GPU_buffers.h"
#include "GPU_debug.h"
#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_select.h"
#include "GPU_shader.h"
#include "GPU_strands.h"

#include "ED_screen.h"
#include "ED_types.h"

#include "UI_resources.h"
#include "UI_interface_icons.h"

#include "view3d_intern.h"  // own include

static GPUStrands_ShaderModel get_shader_model(int smd_shader_model)
{
	switch (smd_shader_model) {
		case MOD_STRANDS_SHADER_CLASSIC_BLENDER: return GPU_STRAND_SHADER_CLASSIC_BLENDER;
		case MOD_STRANDS_SHADER_KAJIYA: return GPU_STRAND_SHADER_KAJIYA;
		case MOD_STRANDS_SHADER_MARSCHNER: return GPU_STRAND_SHADER_MARSCHNER;
	}
	
	BLI_assert(false && "Unhandled shader model enum value!");
	return 0;
}

static GPUStrands_FiberPrimitive get_fiber_primitive(int smd_fiber_primitive)
{
	switch (smd_fiber_primitive) {
		case MOD_STRANDS_FIBER_LINE: return GPU_STRANDS_FIBER_LINE;
		case MOD_STRANDS_FIBER_RIBBON: return GPU_STRANDS_FIBER_RIBBON;
	}
	
	BLI_assert(false && "Unhandled fiber primitive enum value!");
	return 0;
}

static int get_effects(int smd_effects)
{
	GPUStrands_Effects effects = 0;
	if (smd_effects & MOD_STRANDS_EFFECT_CLUMP)
		effects |= GPU_STRAND_EFFECT_CLUMP;
	if (smd_effects & MOD_STRANDS_EFFECT_CURL)
		effects |= GPU_STRAND_EFFECT_CURL;
	
	return effects;
}

static void bind_strands_shader(GPUStrandsShader *shader, RegionView3D *rv3d,
                                Object *ob, StrandsModifierData *smd, int debug_value)
{
	GPU_strand_shader_bind_uniforms(shader, ob->obmat, rv3d->viewmat);
	GPU_strand_shader_bind(shader, rv3d->viewmat, rv3d->viewinv,
	                       smd->ribbon_width,
	                       smd->clump_thickness, smd->clump_shape,
	                       smd->curl_thickness, smd->curl_shape, smd->curl_radius, smd->curl_length,
	                       debug_value, smd->debug_scale);
}

void draw_strands(Scene *scene, View3D *UNUSED(v3d), RegionView3D *rv3d,
                  Object *ob, StrandsModifierData *smd)
{
	Strands *strands = smd->strands;
	BMEditStrands *edit = smd->edit;
	DerivedMesh *root_dm = edit ? edit->root_dm : mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
	bool show_controls = smd->flag & MOD_STRANDS_SHOW_STRANDS;
	bool show_strands = smd->flag & MOD_STRANDS_SHOW_FIBERS;
	bool use_geomshader = smd->flag & MOD_STRANDS_USE_GEOMSHADER;
	GPUStrands_FiberPrimitive fiber_primitive = get_fiber_primitive(smd->fiber_primitive);
	bool enable_debugging = (smd->debug_value != 0);
	
	if (strands == NULL)
		return;
	
	GPUStrandsConverter *converter;
	if (edit)
		converter = BKE_editstrands_get_gpu_converter(edit, root_dm,
		                                              smd->subdiv, fiber_primitive, use_geomshader);
	else
		converter = BKE_strands_get_gpu_converter(strands, root_dm,
		                                          smd->subdiv, fiber_primitive, use_geomshader);
	
	if (smd->gpu_buffer == NULL)
		smd->gpu_buffer = GPU_strands_buffer_create(converter);
	GPUDrawStrands *buffer = smd->gpu_buffer;
	
	if (!strands->gpu_shader) {
		GPUStrandsShaderParams shader_params;
		shader_params.fiber_primitive = fiber_primitive;
		shader_params.effects = get_effects(smd->effects);
		shader_params.use_geomshader = use_geomshader;
		shader_params.shader_model = get_shader_model(smd->shader_model);
		
		/* XXX TODO not nice, we can potentially have multiple hair
		 * subtrees and it's not clear yet how these would be combined.
		 * For the time being just select the first and expect it to be the only one ...
		 */
		shader_params.nodes = NULL;
		if (ob->nodetree) {
			bNode *node;
			for (node = ob->nodetree->nodes.first; node; node = node->next) {
				if (STREQ(node->idname, "HairNode")) {
					shader_params.nodes = (bNodeTree *)node->id;
					break;
				}
			}
		}
		
		strands->gpu_shader = GPU_strand_shader_create(&shader_params);
	}
	GPUStrandsShader *shader = strands->gpu_shader;
	
	if (show_controls) {
		GPU_strands_setup_edges(buffer, converter);
		if (buffer->strand_points && buffer->strand_edges) {
			GPU_buffer_draw_elements(buffer->strand_edges, GL_LINES, 0,
			                         buffer->strand_totedges * 2);
		}
		GPU_buffers_unbind();
	}
	
	if (show_strands) {
		bind_strands_shader(shader, rv3d, ob, smd, 0);
		
		GPU_strands_setup_fibers(buffer, converter);
		if (use_geomshader) {
			if (buffer->fibers) {
				struct GPUAttrib *attrib;
				int num_attrib;
				GPU_strand_shader_get_fiber_attributes(shader, false, &attrib, &num_attrib);
				
				int elemsize = GPU_attrib_element_size(attrib, num_attrib);
				GPU_interleaved_attrib_setup(buffer->fibers, attrib, num_attrib, elemsize, false);
				
				glDrawArrays(GL_POINTS, 0, buffer->totfibers * elemsize);
				
				GPU_interleaved_attrib_unbind();
			}
		}
		else {
			if (buffer->fiber_points && buffer->fiber_indices) {
				struct GPUAttrib *attrib;
				int num_attrib;
				GPU_strand_shader_get_fiber_attributes(shader, false, &attrib, &num_attrib);
				
				int elemsize = GPU_attrib_element_size(attrib, num_attrib);
				GPU_interleaved_attrib_setup(buffer->fiber_points, attrib, num_attrib, elemsize, false);
				
				switch (fiber_primitive) {
					case GPU_STRANDS_FIBER_LINE:
						GPU_buffer_draw_elements(buffer->fiber_indices, GL_LINES, 0,
						                         buffer->fiber_totelems);
						break;
					case GPU_STRANDS_FIBER_RIBBON:
						GPU_buffer_draw_elements(buffer->fiber_indices, GL_TRIANGLE_STRIP, 0,
						                         buffer->fiber_totelems);
						break;
				}
				
				GPU_interleaved_attrib_unbind();
			}
		}
		GPU_strands_buffer_unbind();
		
		GPU_strand_shader_unbind(shader);
	}
	
	if (enable_debugging) {
		bind_strands_shader(shader, rv3d, ob, smd, smd->debug_value);
		
		GPU_strands_setup_fibers(buffer, converter);
		if (buffer->fiber_points && buffer->fiber_indices) {
			struct GPUAttrib *attrib;
			int num_attrib;
			GPU_strand_shader_get_fiber_attributes(shader, true, &attrib, &num_attrib);
			
			int elemsize = GPU_attrib_element_size(attrib, num_attrib);
			GPU_interleaved_attrib_setup(buffer->fiber_points, attrib, num_attrib, elemsize, false);
			
			glDrawArrays(GL_POINTS, 0, buffer->fiber_totverts * elemsize);
			
			GPU_interleaved_attrib_unbind();
		}
		GPU_strands_buffer_unbind();
		
		GPU_strand_shader_unbind(shader);
	}
	
	converter->free(converter);
}

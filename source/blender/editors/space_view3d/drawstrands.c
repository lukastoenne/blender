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

static int get_effects(int smd_effects)
{
	GPUStrands_Effects effects = 0;
	if (smd_effects & MOD_STRANDS_EFFECT_CLUMPING)
		effects |= GPU_STRAND_EFFECT_CLUMPING;
	if (smd_effects & MOD_STRANDS_EFFECT_CURL)
		effects |= GPU_STRAND_EFFECT_CURL;
	
	return effects;
}

static void bind_strands_shader(GPUStrandsShader *shader, RegionView3D *rv3d,
                                Object *ob, StrandsModifierData *smd)
{
	GPU_strand_shader_bind_uniforms(shader, ob->obmat, rv3d->viewmat);
	GPU_strand_shader_bind(shader, rv3d->viewmat, rv3d->viewinv,
	                       smd->clumping_factor, smd->clumping_shape);
}

void draw_strands(Scene *scene, View3D *UNUSED(v3d), RegionView3D *rv3d,
                  Object *ob, StrandsModifierData *smd)
{
	Strands *strands = smd->strands;
	bool show_controls = smd->flag & MOD_STRANDS_SHOW_STRANDS;
	bool show_strands = smd->flag & MOD_STRANDS_SHOW_FIBERS;
	bool use_geomshader = smd->flag & MOD_STRANDS_USE_GEOMSHADER;
	
	if (strands == NULL)
		return;
	
	GPUDrawStrandsParams params = {0};
	params.strands = smd->strands;
	params.root_dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
	params.subdiv = smd->subdiv;
	params.use_geomshader = use_geomshader;
	
	if (smd->gpu_buffer == NULL)
		smd->gpu_buffer = GPU_strands_buffer_create(&params);
	GPUDrawStrands *buffer = smd->gpu_buffer;
	GPUStrandsShader *shader = GPU_strand_shader_get(strands,
	                                                 get_shader_model(smd->shader_model),
	                                                 get_effects(smd->effects),
	                                                 use_geomshader);
	
	if (show_controls) {
		GPU_strands_setup_edges(buffer, &params);
		if (buffer->strand_points && buffer->strand_edges) {
			GPU_buffer_draw_elements(buffer->strand_edges, GL_LINES, 0,
			                         buffer->strand_totedges * 2);
		}
		GPU_buffers_unbind();
	}
	
	if (show_strands) {
		bind_strands_shader(shader, rv3d, ob, smd);
		
		GPU_strands_setup_fibers(buffer, &params);
		if (use_geomshader) {
			if (buffer->fibers) {
				struct GPUAttrib *attrib;
				int num_attrib;
				GPU_strand_shader_get_fiber_attributes(shader, &attrib, &num_attrib);
				
				int elemsize = GPU_attrib_element_size(attrib, num_attrib);
				GPU_interleaved_attrib_setup(buffer->fibers, attrib, num_attrib, elemsize, false);
				
				glDrawArrays(GL_POINTS, 0, buffer->totfibers * elemsize);
				
				GPU_interleaved_attrib_unbind();
			}
		}
		else {
			struct GPUAttrib *attrib;
			int num_attrib;
			GPU_strand_shader_get_fiber_attributes(shader, &attrib, &num_attrib);
			
			int elemsize = GPU_attrib_element_size(attrib, num_attrib);
			GPU_interleaved_attrib_setup(buffer->fiber_points, attrib, num_attrib, elemsize, false);
			
			GPU_buffer_draw_elements(buffer->fiber_edges, GL_LINES, 0,
			                         buffer->fiber_totedges * 2);
			
			GPU_interleaved_attrib_unbind();
		}
		GPU_strands_buffer_unbind();
		
		GPU_strand_shader_unbind(shader);
	}
}

/*************************/
/*** Edit Mode Drawing ***/

#if 0
typedef enum StrandsShadeMode {
	STRANDS_SHADE_FLAT,
	STRANDS_SHADE_HAIR,
} StrandsShadeMode;

typedef struct StrandsDrawInfo {
	bool has_zbuf;
	bool use_zbuf_select;
	
	StrandsShadeMode shade_mode;
	int select_mode;
	
	float col_base[4];
	float col_select[4];
} StrandsDrawInfo;

BLI_INLINE bool strands_use_normals(const StrandsDrawInfo *info)
{
	return ELEM(info->shade_mode, STRANDS_SHADE_HAIR);
}

static void init_draw_info(StrandsDrawInfo *info, View3D *v3d,
                           StrandsShadeMode shade_mode, int select_mode)
{
	info->has_zbuf = v3d->zbuf;
	info->use_zbuf_select = (v3d->flag & V3D_ZBUF_SELECT);
	
	info->shade_mode = shade_mode;
	info->select_mode = select_mode;
	
	/* get selection theme colors */
	UI_GetThemeColor4fv(TH_VERTEX, info->col_base);
	UI_GetThemeColor4fv(TH_VERTEX_SELECT, info->col_select);
}

static void set_opengl_state_strands(const StrandsDrawInfo *info)
{
	if (!info->use_zbuf_select)
		glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	
	if (ELEM(info->shade_mode, STRANDS_SHADE_HAIR)) {
		glEnable(GL_LIGHTING);
		glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
		glEnable(GL_COLOR_MATERIAL);
		glShadeModel(GL_SMOOTH);
	}
	else {
		glDisable(GL_LIGHTING);
	}
	
	glEnableClientState(GL_VERTEX_ARRAY);
	if (strands_use_normals(info))
		glEnableClientState(GL_NORMAL_ARRAY);
}

static void set_opengl_state_dots(const StrandsDrawInfo *info)
{
	if (!info->use_zbuf_select)
		glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	
	glDisable(GL_LIGHTING);
	
	glEnableClientState(GL_VERTEX_ARRAY);
	glPointSize(3.0);
}

static void restore_opengl_state(const StrandsDrawInfo *info)
{
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	
	glDisable(GL_BLEND);
	glDisable(GL_LIGHTING);
	glDisable(GL_COLOR_MATERIAL);
	glShadeModel(GL_FLAT);
	if (info->has_zbuf)
		glEnable(GL_DEPTH_TEST);
	glLineWidth(1.0f);
	glPointSize(1.0);
}

/* ------------------------------------------------------------------------- */
/* strands */

static void setup_gpu_buffers_strands(BMEditStrands *edit, const StrandsDrawInfo *info)
{
	const size_t size_v3 = sizeof(float) * 3;
	const size_t size_vertex = (strands_use_normals(info) ? 2*size_v3 : size_v3);
	BMesh *bm = edit->base.bm;
	
//	int totstrands = BM_strands_count(edit->bm);
	int totvert = bm->totvert;
	int totedge = bm->totedge;
	
	if (!edit->vertex_glbuf)
		glGenBuffers(1, &edit->vertex_glbuf);
	if (!edit->elem_glbuf)
		glGenBuffers(1, &edit->elem_glbuf);
	
	glBindBuffer(GL_ARRAY_BUFFER, edit->vertex_glbuf);
	glBufferData(GL_ARRAY_BUFFER, size_vertex * totvert, NULL, GL_DYNAMIC_DRAW);
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, edit->elem_glbuf);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * totedge * 2, NULL, GL_DYNAMIC_DRAW);
	
	glVertexPointer(3, GL_FLOAT, size_vertex, NULL);
	if (strands_use_normals(info))
		glNormalPointer(GL_FLOAT, size_vertex, (GLubyte *)NULL + size_v3);
}

static void unbind_gpu_buffers_strands(void)
{
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

static int write_gpu_buffers_strands(BMEditStrands *edit, const StrandsDrawInfo *info)
{
	const size_t size_v3 = sizeof(float) * 3;
	const size_t size_vertex = (strands_use_normals(info) ? 2*size_v3 : size_v3);
	BMesh *bm = edit->base.bm;
	
	GLubyte *vertex_data;
	unsigned int *elem_data;
	BMVert *root, *v, *vprev;
	BMIter iter, iter_strand;
	int index, indexprev, index_edge;
	int k;
	
	vertex_data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	elem_data = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
	if (!vertex_data || !elem_data)
		return 0;
	
	BM_mesh_elem_index_ensure(bm, BM_VERT);
	
	index_edge = 0;
	BM_ITER_STRANDS(root, &iter, bm, BM_STRANDS_OF_MESH) {
		BM_ITER_STRANDS_ELEM_INDEX(v, &iter_strand, root, BM_VERTS_OF_STRAND, k) {
			size_t offset_co;
			
			index = BM_elem_index_get(v);
			
			offset_co = index * size_vertex;
			copy_v3_v3((float *)(vertex_data + offset_co), v->co);
			
			if (k > 0) {
				if (strands_use_normals(info)) {
					size_t offset_nor = offset_co + size_v3;
					float nor[3];
					sub_v3_v3v3(nor, v->co, vprev->co);
					normalize_v3(nor);
					copy_v3_v3((float *)(vertex_data + offset_nor), nor);
					
					if (k == 1) {
						/* define root normal: same as first segment */
						size_t offset_root_nor = indexprev * size_vertex + size_v3;
						copy_v3_v3((float *)(vertex_data + offset_root_nor), nor);
					}
				}
				
				{
					elem_data[index_edge + 0] = indexprev;
					elem_data[index_edge + 1] = index;
					index_edge += 2;
				}
			}
			
			vprev = v;
			indexprev = index;
		}
	}
	
	glUnmapBuffer(GL_ARRAY_BUFFER);
	glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
	
	return index_edge;
}

/* ------------------------------------------------------------------------- */
/* dots */

static void setup_gpu_buffers_dots(BMEditStrands *edit, const StrandsDrawInfo *info, bool selected)
{
	const size_t size_v3 = sizeof(float) * 3;
	const size_t size_vertex = size_v3;
	BMesh *bm = edit->base.bm;
	
	BMVert *v;
	BMIter iter;
	int totvert;
	
	switch (info->select_mode) {
		case HAIR_SELECT_STRAND:
			totvert = 0;
			break;
		case HAIR_SELECT_VERTEX:
			totvert = selected ? bm->totvertsel : bm->totvert - bm->totvertsel;
			break;
		case HAIR_SELECT_TIP:
			totvert = 0;
			BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
				if (BM_elem_flag_test_bool(v, BM_ELEM_SELECT) != selected)
					continue;
				if (!BM_strands_vert_is_tip(v))
					continue;
				
				++totvert;
			}
			break;
	}
	
	if (totvert == 0)
		return;
	
	if (!edit->dot_glbuf)
		glGenBuffers(1, &edit->dot_glbuf);
	
	glBindBuffer(GL_ARRAY_BUFFER, edit->dot_glbuf);
	glBufferData(GL_ARRAY_BUFFER, size_vertex * totvert, NULL, GL_DYNAMIC_DRAW);
	
	glVertexPointer(3, GL_FLOAT, size_vertex, NULL);
}

static void unbind_gpu_buffers_dots(void)
{
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static int write_gpu_buffers_dots(BMEditStrands *edit, const StrandsDrawInfo *info, bool selected)
{
	const size_t size_v3 = sizeof(float) * 3;
	const size_t size_vertex = size_v3;
	BMesh *bm = edit->base.bm;
	
	GLubyte *vertex_data;
	BMVert *v;
	BMIter iter;
	int index_dot;
	
	if (info->select_mode == HAIR_SELECT_STRAND)
		return 0;
	
	vertex_data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	if (!vertex_data)
		return 0;
	
	BM_mesh_elem_index_ensure(bm, BM_VERT);
	
	index_dot = 0;
	switch (info->select_mode) {
		case HAIR_SELECT_STRAND:
			/* already exited, but keep the case for the compiler */
			break;
		case HAIR_SELECT_VERTEX:
			BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
				size_t offset_co;
				
				if (BM_elem_flag_test_bool(v, BM_ELEM_SELECT) != selected)
					continue;
				
				offset_co = index_dot * size_vertex;
				copy_v3_v3((float *)(vertex_data + offset_co), v->co);
				++index_dot;
			}
			break;
		case HAIR_SELECT_TIP:
			BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
				size_t offset_co;
				
				if (BM_elem_flag_test_bool(v, BM_ELEM_SELECT) != selected)
					continue;
				if (!BM_strands_vert_is_tip(v))
					continue;
				
				offset_co = index_dot * size_vertex;
				copy_v3_v3((float *)(vertex_data + offset_co), v->co);
				++index_dot;
			}
			break;
	}
	
	glUnmapBuffer(GL_ARRAY_BUFFER);
	
	return index_dot;
}

/* ------------------------------------------------------------------------- */

static void draw_dots(BMEditStrands *edit, const StrandsDrawInfo *info, bool selected)
{
	int totelem;
	
	if (selected)
		glColor3fv(info->col_select);
	else
		glColor3fv(info->col_base);
	
	setup_gpu_buffers_dots(edit, info, selected);
	totelem = write_gpu_buffers_dots(edit, info, selected);
	if (totelem > 0)
		glDrawArrays(GL_POINTS, 0, totelem);
}
#endif

void draw_strands_edit(Scene *scene, View3D *UNUSED(v3d), RegionView3D *rv3d,
                       Object *ob, StrandsModifierData *smd)
{
	BMEditStrands *edit = smd->edit;
	bool show_controls = smd->flag & MOD_STRANDS_SHOW_STRANDS;
	bool show_strands = smd->flag & MOD_STRANDS_SHOW_FIBERS;
	bool use_geomshader = smd->flag & MOD_STRANDS_USE_GEOMSHADER;
	
	if (smd->strands == NULL || edit == NULL)
		return;
	
	GPUDrawStrandsParams params = {0};
	params.strands = smd->strands;
	params.edit = edit;
	params.root_dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
	params.subdiv = smd->subdiv;
	params.use_geomshader = use_geomshader;
	
	if (smd->gpu_buffer == NULL)
		smd->gpu_buffer = GPU_strands_buffer_create(&params);
	GPUDrawStrands *buffer = smd->gpu_buffer;
	GPUStrandsShader *shader = GPU_strand_shader_get(smd->strands,
	                                                 get_shader_model(smd->shader_model),
	                                                 get_effects(smd->effects),
	                                                 use_geomshader);
	
	if (show_controls) {
		GPU_strands_setup_edges(buffer, &params);
		if (buffer->strand_points && buffer->strand_edges) {
			GPU_buffer_draw_elements(buffer->strand_edges, GL_LINES, 0, buffer->strand_totedges * 2);
		}
		GPU_buffers_unbind();
	}
	
	if (show_strands) {
		bind_strands_shader(shader, rv3d, ob, smd);
		
		GPU_strands_setup_fibers(buffer, &params);
		if (use_geomshader) {
			if (buffer->fibers) {
				struct GPUAttrib *attrib;
				int num_attrib;
				GPU_strand_shader_get_fiber_attributes(shader, &attrib, &num_attrib);
				
				int elemsize = GPU_attrib_element_size(attrib, num_attrib);
				GPU_interleaved_attrib_setup(buffer->fibers, attrib, num_attrib, elemsize, false);
				
				glDrawArrays(GL_POINTS, 0, buffer->totfibers * elemsize);
				
				GPU_interleaved_attrib_unbind();
			}
		}
		else {
			struct GPUAttrib *attrib;
			int num_attrib;
			GPU_strand_shader_get_fiber_attributes(shader, &attrib, &num_attrib);
			
			int elemsize = GPU_attrib_element_size(attrib, num_attrib);
			GPU_interleaved_attrib_setup(buffer->fiber_points, attrib, num_attrib, elemsize, false);
			
			GPU_buffer_draw_elements(buffer->fiber_edges, GL_LINES, 0,
			                         buffer->fiber_totedges * 2);
			
			GPU_interleaved_attrib_unbind();
		}
		GPU_strands_buffer_unbind();
		
		GPU_strand_shader_unbind(shader);
	}
	
#if 0
	HairEditSettings *settings = &scene->toolsettings->hair_edit;
	
	StrandsDrawInfo info;
	int totelem;
	
	init_draw_info(&info, v3d, STRANDS_SHADE_HAIR, settings->select_mode);
	
	set_opengl_state_strands(&info);
	setup_gpu_buffers_strands(edit, &info);
	totelem = write_gpu_buffers_strands(edit, &info);
	if (totelem > 0)
		glDrawElements(GL_LINES, totelem, GL_UNSIGNED_INT, NULL);
	unbind_gpu_buffers_strands();
	
	set_opengl_state_dots(&info);
	draw_dots(edit, &info, false);
	draw_dots(edit, &info, true);
	unbind_gpu_buffers_dots();
	
	restore_opengl_state(&info);
#endif
}

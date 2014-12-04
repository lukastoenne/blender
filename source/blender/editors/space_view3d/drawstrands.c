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
 * Contributor(s): Lukas Toenne.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/drawstrands.c
 *  \ingroup spview3d
 */

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BKE_edithair.h"
#include "BKE_main.h"

#include "bmesh.h"

#include "ED_screen.h"
#include "ED_types.h"

#include "UI_resources.h"
#include "UI_interface_icons.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_select.h"

#include "view3d_intern.h"

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
	
//	int totstrands = BM_strands_count(edit->bm);
	int totvert = edit->bm->totvert;
	int totedge = edit->bm->totedge;
	
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
	
	BM_mesh_elem_index_ensure(edit->bm, BM_VERT);
	
	index_edge = 0;
	BM_ITER_STRANDS(root, &iter, edit->bm, BM_STRANDS_OF_MESH) {
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
	BMesh *bm = edit->bm;
	
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
	
	GLubyte *vertex_data;
	BMVert *v;
	BMIter iter;
	int index_dot;
	
	if (info->select_mode == HAIR_SELECT_STRAND)
		return 0;
	
	vertex_data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	if (!vertex_data)
		return 0;
	
	BM_mesh_elem_index_ensure(edit->bm, BM_VERT);
	
	index_dot = 0;
	switch (info->select_mode) {
		case HAIR_SELECT_STRAND:
			/* already exited, but keep the case for the compiler */
			break;
		case HAIR_SELECT_VERTEX:
			BM_ITER_MESH(v, &iter, edit->bm, BM_VERTS_OF_MESH) {
				size_t offset_co;
				
				if (BM_elem_flag_test_bool(v, BM_ELEM_SELECT) != selected)
					continue;
				
				offset_co = index_dot * size_vertex;
				copy_v3_v3((float *)(vertex_data + offset_co), v->co);
				++index_dot;
			}
			break;
		case HAIR_SELECT_TIP:
			BM_ITER_MESH(v, &iter, edit->bm, BM_VERTS_OF_MESH) {
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

void draw_strands_edit_hair(Scene *scene, View3D *v3d, BMEditStrands *edit)
{
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
}

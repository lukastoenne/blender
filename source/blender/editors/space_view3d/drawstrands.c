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

static void setup_gpu_buffers(BMEditStrands *edit)
{
//	int totstrands = BM_strands_count(edit->bm);
	int totvert = edit->bm->totvert;
	int totedge = edit->bm->totedge;
	
	if (!edit->vertex_glbuf)
		glGenBuffers(1, &edit->vertex_glbuf);
	if (!edit->elem_glbuf)
		glGenBuffers(1, &edit->elem_glbuf);
	
	glBindBuffer(GL_ARRAY_BUFFER, edit->vertex_glbuf);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, edit->elem_glbuf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * totvert, NULL, GL_DYNAMIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * totedge * 2, NULL, GL_DYNAMIC_DRAW);
	
	glEnableClientState(GL_VERTEX_ARRAY);
	
	glVertexPointer(3, GL_FLOAT, 3 * sizeof(float), NULL);
}

static void release_gpu_buffers(void)
{
	glUnmapBuffer(GL_ARRAY_BUFFER);
	glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
}

void draw_strands_edit(Scene *UNUSED(scene), View3D *v3d, BMEditStrands *edit)
{
	const bool zbuf_select = v3d->flag & V3D_ZBUF_SELECT;
	float sel_col[3], nosel_col[3];
	
	float (*vertex_data)[3];
	unsigned int *elem_data;
	BMVert *v;
	BMEdge *e;
	BMIter iter;
	
	/* get selection theme colors */
	UI_GetThemeColor3fv(TH_VERTEX_SELECT, sel_col);
	UI_GetThemeColor3fv(TH_VERTEX, nosel_col);
	
	/* opengl setup */
	if (!zbuf_select)
		glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	
	//	glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
	//	glEnable(GL_COLOR_MATERIAL);
	//	glShadeModel(GL_SMOOTH);
		glDisable(GL_LIGHTING);
	
	setup_gpu_buffers(edit);
	
	/* initialize buffers */
	vertex_data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	elem_data = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
	
	BM_mesh_elem_index_ensure(edit->bm, BM_VERT);
	
	BM_ITER_MESH(v, &iter, edit->bm, BM_VERTS_OF_MESH) {
		int offset = BM_elem_index_get(v);
		copy_v3_v3(vertex_data[offset], v->co);
	}
	
	BM_ITER_MESH(e, &iter, edit->bm, BM_EDGES_OF_MESH) {
		int offset = BM_elem_index_get(e) * 2;
		elem_data[offset + 0] = BM_elem_index_get(e->v1);
		elem_data[offset + 1] = BM_elem_index_get(e->v2);
	}
	
	release_gpu_buffers();
	
	glDrawElements(GL_LINES, edit->bm->totedge * 2, GL_UNSIGNED_INT, NULL);
	
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	glDisable(GL_BLEND);
	glDisable(GL_LIGHTING);
	glShadeModel(GL_FLAT);
	if (v3d->zbuf)
		glEnable(GL_DEPTH_TEST);
	glLineWidth(1.0f);
	glPointSize(1.0);
}

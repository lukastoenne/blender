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

/** \file blender/gpu/intern/gpu_strands.c
 *  \ingroup gpu
 */

#include "MEM_guardedalloc.h"

#include "GPU_glew.h"

#include "BLI_dynstr.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "DNA_strand_types.h"

#include "BKE_DerivedMesh.h" /* XXX just because GPU_buffers.h needs a type from here */
#include "BKE_editstrands.h"
#include "BKE_strands.h"

#include "GPU_buffers.h"
#include "GPU_extensions.h"
#include "GPU_strands.h"

#include "bmesh.h"

typedef struct GPUFiber {
	/* object space location and orientation of the follicle */
	float co[3];
	float normal[3];
	float tangent[3];
	/* indices and weights for interpolating control strands */
	unsigned int control_index[4];
	float control_weight[4];
	/* parametric distance from the primary control strand */
	float root_distance[2];
} GPUFiber;

typedef struct GPUFiberVertex {
	/* index of the fiber curve (for texture lookup) */
	unsigned int fiber_index;
	/* curve parameter for interpolation */
	float curve_param;
} GPUFiberVertex;

typedef enum GPUStrandBufferType {
	GPU_STRAND_BUFFER_STRAND_VERTEX = 0,
	GPU_STRAND_BUFFER_STRAND_EDGE,
	GPU_STRAND_BUFFER_CONTROL_VERTEX,
	GPU_STRAND_BUFFER_CONTROL_CURVE,
	GPU_STRAND_BUFFER_CONTROL_NORMAL,
	GPU_STRAND_BUFFER_CONTROL_TANGENT,
	/* fiber buffers */
	GPU_STRAND_BUFFER_FIBER_VERTEX,
	GPU_STRAND_BUFFER_FIBER_EDGE,
	/* fiber curve attributes (buffer textures) */
	GPU_STRAND_BUFFER_FIBER_POSITION,
	GPU_STRAND_BUFFER_FIBER_CONTROL_INDEX,
	GPU_STRAND_BUFFER_FIBER_CONTROL_WEIGHT,
	/* fiber vertex buffer (geometry shader only) */
	GPU_STRAND_BUFFER_FIBER,
} GPUStrandBufferType;

static GLenum gpu_strands_buffer_gl_type(GPUStrandBufferType type)
{
	switch (type) {
		case GPU_STRAND_BUFFER_STRAND_VERTEX:
		case GPU_STRAND_BUFFER_CONTROL_VERTEX:
		case GPU_STRAND_BUFFER_CONTROL_NORMAL:
		case GPU_STRAND_BUFFER_CONTROL_TANGENT:
		case GPU_STRAND_BUFFER_FIBER:
		case GPU_STRAND_BUFFER_FIBER_VERTEX:
		case GPU_STRAND_BUFFER_FIBER_POSITION:
		case GPU_STRAND_BUFFER_FIBER_CONTROL_INDEX:
		case GPU_STRAND_BUFFER_FIBER_CONTROL_WEIGHT:
			return GL_ARRAY_BUFFER;
		case GPU_STRAND_BUFFER_STRAND_EDGE:
		case GPU_STRAND_BUFFER_CONTROL_CURVE:
		case GPU_STRAND_BUFFER_FIBER_EDGE:
			return GL_ELEMENT_ARRAY_BUFFER;
	}
	BLI_assert(false);
	return 0;
}

/* get the GPUDrawObject buffer associated with a type */
static GPUBuffer **gpu_strands_buffer_from_type(GPUDrawStrands *gds, GPUStrandBufferType type)
{
	switch (type) {
		case GPU_STRAND_BUFFER_STRAND_VERTEX:
			return &gds->strand_points;
		case GPU_STRAND_BUFFER_STRAND_EDGE:
			return &gds->strand_edges;
		case GPU_STRAND_BUFFER_CONTROL_CURVE:
			return &gds->control_curves;
		case GPU_STRAND_BUFFER_CONTROL_VERTEX:
			return &gds->control_points;
		case GPU_STRAND_BUFFER_CONTROL_NORMAL:
			return &gds->control_normals;
		case GPU_STRAND_BUFFER_CONTROL_TANGENT:
			return &gds->control_tangents;
		case GPU_STRAND_BUFFER_FIBER:
			return &gds->fibers;
		case GPU_STRAND_BUFFER_FIBER_VERTEX:
			return &gds->fiber_points;
		case GPU_STRAND_BUFFER_FIBER_EDGE:
			return &gds->fiber_indices;
		case GPU_STRAND_BUFFER_FIBER_POSITION:
			return &gds->fiber_position;
		case GPU_STRAND_BUFFER_FIBER_CONTROL_INDEX:
			return &gds->fiber_control_index;
		case GPU_STRAND_BUFFER_FIBER_CONTROL_WEIGHT:
			return &gds->fiber_control_weight;
	}
	BLI_assert(false);
	return 0;
}

static GPUBufferTexture *gpu_strands_buffer_texture_from_type(GPUDrawStrands *gds,
                                                              GPUStrandBufferType type, GLenum *format)
{
	switch (type) {
		case GPU_STRAND_BUFFER_CONTROL_CURVE:
			*format = GL_RG32UI;
			return &gds->control_curves_tex;
		case GPU_STRAND_BUFFER_CONTROL_VERTEX:
			*format = GL_RGB32F;
			return &gds->control_points_tex;
		case GPU_STRAND_BUFFER_CONTROL_NORMAL:
			*format = GL_RGB32F;
			return &gds->control_normals_tex;
		case GPU_STRAND_BUFFER_CONTROL_TANGENT:
			*format = GL_RGB32F;
			return &gds->control_tangents_tex;
		case GPU_STRAND_BUFFER_FIBER_POSITION:
			*format = GL_RGB32F;
			return &gds->fiber_position_tex;
		case GPU_STRAND_BUFFER_FIBER_CONTROL_INDEX:
			*format = GL_RGBA32UI;
			return &gds->fiber_control_index_tex;
		case GPU_STRAND_BUFFER_FIBER_CONTROL_WEIGHT:
			*format = GL_RGBA32F;
			return &gds->fiber_control_weight_tex;
		default:
			*format = 0;
			return NULL;
	}
}

/* get the amount of space to allocate for a buffer of a particular type */
static size_t gpu_strands_buffer_size_from_type(GPUDrawStrands *gpu_buffer, GPUStrandBufferType type)
{
	switch (type) {
		case GPU_STRAND_BUFFER_STRAND_VERTEX:
			return sizeof(float) * 3 * gpu_buffer->strand_totverts;
		case GPU_STRAND_BUFFER_STRAND_EDGE:
			return sizeof(int) * 2 * gpu_buffer->strand_totedges;
		case GPU_STRAND_BUFFER_CONTROL_CURVE:
			return sizeof(int) * 2 * gpu_buffer->control_totcurves;
		case GPU_STRAND_BUFFER_CONTROL_VERTEX:
			return sizeof(float) * 3 * gpu_buffer->control_totverts;
		case GPU_STRAND_BUFFER_CONTROL_NORMAL:
			return sizeof(float) * 3 * gpu_buffer->control_totverts;
		case GPU_STRAND_BUFFER_CONTROL_TANGENT:
			return sizeof(float) * 3 * gpu_buffer->control_totverts;
		case GPU_STRAND_BUFFER_FIBER:
			return sizeof(GPUFiber) * gpu_buffer->totfibers;
		case GPU_STRAND_BUFFER_FIBER_VERTEX:
			return sizeof(GPUFiberVertex) * gpu_buffer->fiber_totverts;
		case GPU_STRAND_BUFFER_FIBER_EDGE:
			return sizeof(int) * gpu_buffer->fiber_totelems;
		case GPU_STRAND_BUFFER_FIBER_POSITION:
			return sizeof(float) * 3 * gpu_buffer->totfibers;
		case GPU_STRAND_BUFFER_FIBER_CONTROL_INDEX:
			return sizeof(int) * 4 * gpu_buffer->totfibers;
		case GPU_STRAND_BUFFER_FIBER_CONTROL_WEIGHT:
			return sizeof(float) * 4 * gpu_buffer->totfibers;
	}
	BLI_assert(false);
	return 0;
}

static void gpu_strands_setup_buffer_texture(GPUBuffer *buffer, GLenum format, GPUBufferTexture *tex)
{
	if (!buffer)
		return;
	
	glGenTextures(1, &tex->id);
	glBindTexture(GL_TEXTURE_BUFFER, tex->id);
	
	glTexBuffer(GL_TEXTURE_BUFFER, format, buffer->id);
	
	glBindTexture(GL_TEXTURE_BUFFER, 0);
}

/* ******** */

static int strands_fiber_length(GPUStrandsConverter *conv, StrandFiber *fiber, int subdiv)
{
	float fnumverts = 0.0f;
	for (int k = 0; k < 4; ++k) {
		unsigned int index = fiber->control_index[k];
		if (index == STRAND_INDEX_NONE)
			continue;
		fnumverts += fiber->control_weight[k] * (float)conv->getNumStrandCurveVerts(conv, index);
	}
	int orig_num_verts = max_ii((int)ceil(fnumverts), 2);
	return BKE_strand_curve_cache_size(orig_num_verts, subdiv);
}

static void strands_count_fibers(GPUStrandsConverter *conv,
                                 unsigned int *r_totfibers, unsigned int *r_totverts, unsigned int *r_totelems)
{
	unsigned int totfibers = conv->getNumFibers(conv);
	
	unsigned int totverts = 0;
	unsigned int totelems = 0;
	StrandFiber *fiber = conv->getFiberArray(conv);
	switch (conv->fiber_primitive) {
		case GPU_STRANDS_FIBER_LINE:
			for (int i = 0; i < totfibers; ++i, ++fiber)
				totverts += strands_fiber_length(conv, fiber, conv->subdiv);
			/* GL_LINES
			 * Note: GL_LINE_STRIP does not support degenerate elements,
			 * so we have to use a line strip and render individual segments
			 */
			totelems = 2 * (totverts - totfibers);
			break;
		case GPU_STRANDS_FIBER_RIBBON:
			for (int i = 0; i < totfibers; ++i, ++fiber)
				totverts += strands_fiber_length(conv, fiber, conv->subdiv);
			totverts *= 2; /* ribbon has 2 edges */
			/* GL_TRIANGLE_STRIP
			 * Extra elements are degenerate triangles for splitting the strip
			 */
			totelems = totverts + 2 * (totfibers - 1);
			break;
	}
	
	if (r_totfibers) *r_totfibers = totfibers;
	if (r_totverts) *r_totverts = totverts;
	if (r_totelems) *r_totelems = totelems;
}

GPUDrawStrands *GPU_strands_buffer_create(GPUStrandsConverter *conv)
{
	GPUDrawStrands *gds = MEM_callocN(sizeof(GPUDrawStrands), "GPUDrawStrands");
	
	gds->strand_totverts = conv->getNumStrandVerts(conv);
	gds->control_totcurves = conv->getNumStrandCurves(conv);
	gds->strand_totedges = gds->strand_totverts - gds->control_totcurves;
	gds->control_totverts = BKE_strand_curve_cache_totverts(gds->strand_totverts,
	                                                        gds->control_totcurves,
	                                                        conv->subdiv);
	if (conv->use_geomshader) {
		gds->totfibers = conv->getNumFibers(conv);
		gds->fiber_totverts = 0;
		gds->fiber_totelems = 0;
	}
	else {
		strands_count_fibers(conv, &gds->totfibers, &gds->fiber_totverts, &gds->fiber_totelems);
	}
	
	return gds;
}

static void strands_copy_strand_vertex(void *userdata, int UNUSED(index), const float *co, float (*mat)[4])
{
	float (**varray)[3] = userdata;
	
	if (mat)
		mul_v3_m4v3(**varray, mat, co);
	else
		copy_v3_v3(**varray, co);
	++(*varray);
}

static void strands_copy_strand_edge(void *userdata, int index1, int index2)
{
	unsigned int (**varray)[2] = userdata;
	
	(**varray)[0] = index1;
	(**varray)[1] = index2;
	++(*varray);
}

static void strands_copy_curve(void *userdata, int verts_begin, int num_verts)
{
	unsigned int (**varray)[2] = userdata;
	
	(**varray)[0] = verts_begin;
	(**varray)[1] = num_verts;
	++(*varray);
}

static void strands_copy_curve_cache_vertex(void *userdata, const StrandCurveCache *cache, int num_verts)
{
	float (**varray)[3] = userdata;
	
	float (*vert)[3] = cache->verts;
	for (int v = 0; v < num_verts; ++v, ++vert) {
		copy_v3_v3(**varray, *vert);
		++(*varray);
	}
}

static void strands_copy_curve_cache_normal(void *userdata, const StrandCurveCache *cache, int num_verts)
{
	float (**varray)[3] = userdata;
	
	float (*vert)[3] = cache->normals;
	for (int v = 0; v < num_verts; ++v, ++vert) {
		copy_v3_v3(**varray, *vert);
		++(*varray);
	}
}

static void strands_copy_curve_cache_tangent(void *userdata, const StrandCurveCache *cache, int num_verts)
{
	float (**varray)[3] = userdata;
	
	float (*vert)[3] = cache->tangents;
	for (int v = 0; v < num_verts; ++v, ++vert) {
		copy_v3_v3(**varray, *vert);
		++(*varray);
	}
}

static void strands_copy_fiber_data(GPUStrandsConverter *conv, GPUFiber *varray)
{
	int totfibers = conv->getNumFibers(conv);
	StrandFiber *fiber = conv->getFiberArray(conv);
	
	for (int v = 0; v < totfibers; ++v, ++fiber) {
		BKE_strands_get_fiber_vectors(fiber, conv->root_dm, varray->co, varray->normal, varray->tangent);
		for (int k = 0; k < 4; ++k) {
			varray->control_index[k] = fiber->control_index[k];
			varray->control_weight[k] = fiber->control_weight[k];
		}
		copy_v2_v2(varray->root_distance, fiber->root_distance);
		++varray;
	}
}

static void strands_copy_fiber_line_vertex_data(GPUStrandsConverter *conv, GPUFiberVertex *varray)
{
	int totfibers = conv->getNumFibers(conv);
	StrandFiber *fiber = conv->getFiberArray(conv);
	for (int i = 0; i < totfibers; ++i, ++fiber) {
		int num_verts = strands_fiber_length(conv, fiber, conv->subdiv);
		
		for (int k = 0; k < num_verts; ++k) {
			varray->fiber_index = i;
			varray->curve_param = (float)k / (float)(num_verts - 1);
			++varray;
		}
	}
}

static void strands_copy_fiber_ribbon_vertex_data(GPUStrandsConverter *conv, GPUFiberVertex *varray)
{
	int totfibers = conv->getNumFibers(conv);
	StrandFiber *fiber = conv->getFiberArray(conv);
	for (int i = 0; i < totfibers; ++i, ++fiber) {
		int num_verts = strands_fiber_length(conv, fiber, conv->subdiv);
		
		for (int k = 0; k < num_verts; ++k) {
			/* two vertices for the two edges of the ribbon */
			
			varray->fiber_index = i;
			varray->curve_param = (float)k / (float)(num_verts - 1);
			++varray;
			
			varray->fiber_index = i;
			varray->curve_param = (float)k / (float)(num_verts - 1);
			++varray;
		}
	}
}

static void strands_copy_fiber_line_index_data(GPUStrandsConverter *conv, unsigned int (*varray)[2])
{
	int totfibers = conv->getNumFibers(conv);
	StrandFiber *fiber = conv->getFiberArray(conv);
	int verts_begin = 0;
	for (int i = 0; i < totfibers; ++i, ++fiber) {
		int num_verts = strands_fiber_length(conv, fiber, conv->subdiv);
		
		for (int k = 0; k < num_verts - 1; ++k) {
			(*varray)[0] = verts_begin + k;
			(*varray)[1] = verts_begin + k + 1;
			++varray;
		}
		
		verts_begin += num_verts;
	}
}

static void strands_copy_fiber_ribbon_index_data(GPUStrandsConverter *conv, unsigned int *varray)
{
	int totfibers = conv->getNumFibers(conv);
	StrandFiber *fiber = conv->getFiberArray(conv);
	int verts_begin = 0;
	for (int i = 0; i < totfibers; ++i, ++fiber) {
		int num_verts = strands_fiber_length(conv, fiber, conv->subdiv);
		
		if (i > 0 && verts_begin > 0) {
			/* repeat cap vertices to create degenerate triangles
				 * and separate fibers.
				 */
			*varray++ = verts_begin - 1;
			*varray++ = verts_begin;
		}
		
		for (int k = 0; k < num_verts; ++k) {
			*varray++ = verts_begin + k * 2;
			*varray++ = verts_begin + k * 2 + 1;
		}
		
		verts_begin += num_verts *2;
	}
}

static void strands_copy_fiber_position_data(GPUStrandsConverter *conv, float (*varray)[3])
{
	int totfibers = conv->getNumFibers(conv);
	StrandFiber *fiber = conv->getFiberArray(conv);
	for (int v = 0; v < totfibers; ++v, ++fiber) {
		float nor[3], tang[3];
		BKE_strands_get_fiber_vectors(fiber, conv->root_dm, *varray, nor, tang);
		++varray;
	}
}

static void strands_copy_fiber_control_index_data(GPUStrandsConverter *conv, unsigned int (*varray)[4])
{
	int totfibers = conv->getNumFibers(conv);
	StrandFiber *fiber = conv->getFiberArray(conv);
	for (int v = 0; v < totfibers; ++v, ++fiber) {
		for (int k = 0; k < 4; ++k)
			(*varray)[k] = fiber->control_index[k];
		++varray;
	}
}

static void strands_copy_fiber_control_weight_data(GPUStrandsConverter *conv, float (*varray)[4])
{
	int totfibers = conv->getNumFibers(conv);
	StrandFiber *fiber = conv->getFiberArray(conv);
	for (int v = 0; v < totfibers; ++v, ++fiber) {
		copy_v4_v4(*varray, fiber->control_weight);
		++varray;
	}
}

typedef struct StrandsBufferSetupInfo {
	GPUStrandsConverter *converter;
	GPUStrandBufferType type;
} StrandsBufferSetupInfo;

static void strands_copy_gpu_data(void *varray, void *vinfo)
{
	StrandsBufferSetupInfo *info = vinfo;
	GPUStrandsConverter *conv = info->converter;
	GPUStrandBufferType type = info->type;
	
	switch (type) {
		case GPU_STRAND_BUFFER_STRAND_VERTEX:
			conv->foreachStrandVertex(conv, strands_copy_strand_vertex, &varray);
			break;
		case GPU_STRAND_BUFFER_STRAND_EDGE:
			conv->foreachStrandEdge(conv, strands_copy_strand_edge, &varray);
			break;
		case GPU_STRAND_BUFFER_CONTROL_CURVE:
			conv->foreachCurve(conv, strands_copy_curve, &varray);
			break;
		case GPU_STRAND_BUFFER_CONTROL_VERTEX:
			conv->foreachCurveCache(conv, strands_copy_curve_cache_vertex, &varray);
			break;
		case GPU_STRAND_BUFFER_CONTROL_NORMAL:
			conv->foreachCurveCache(conv, strands_copy_curve_cache_normal, &varray);
			break;
		case GPU_STRAND_BUFFER_CONTROL_TANGENT:
			conv->foreachCurveCache(conv, strands_copy_curve_cache_tangent, &varray);
			break;
		case GPU_STRAND_BUFFER_FIBER:
			strands_copy_fiber_data(conv, (GPUFiber *)varray);
			break;
		case GPU_STRAND_BUFFER_FIBER_VERTEX:
			switch (conv->fiber_primitive) {
				case GPU_STRANDS_FIBER_LINE:
					strands_copy_fiber_line_vertex_data(conv, (GPUFiberVertex *)varray);
					break;
				case GPU_STRANDS_FIBER_RIBBON:
					strands_copy_fiber_ribbon_vertex_data(conv, (GPUFiberVertex *)varray);
					break;
			}
			break;
		case GPU_STRAND_BUFFER_FIBER_EDGE:
			switch (conv->fiber_primitive) {
				case GPU_STRANDS_FIBER_LINE:
					strands_copy_fiber_line_index_data(conv, (unsigned int (*)[2])varray);
					break;
				case GPU_STRANDS_FIBER_RIBBON:
					strands_copy_fiber_ribbon_index_data(conv, (unsigned int *)varray);
					break;
			}
			break;
		case GPU_STRAND_BUFFER_FIBER_POSITION:
			strands_copy_fiber_position_data(conv, (float (*)[3])varray);
			break;
		case GPU_STRAND_BUFFER_FIBER_CONTROL_INDEX:
			strands_copy_fiber_control_index_data(conv, (unsigned int (*)[4])varray);
			break;
		case GPU_STRAND_BUFFER_FIBER_CONTROL_WEIGHT:
			strands_copy_fiber_control_weight_data(conv, (float (*)[4])varray);
			break;
	}
}

static bool strands_setup_buffer_common(GPUDrawStrands *strands_buffer, GPUStrandsConverter *conv,
                                        GPUStrandBufferType type, bool update)
{
	GPUBuffer **buf;
	GPUBufferTexture *tex;
	
	buf = gpu_strands_buffer_from_type(strands_buffer, type);
	if (*buf == NULL || update) {
		GLenum target = gpu_strands_buffer_gl_type(type);
		size_t size = gpu_strands_buffer_size_from_type(strands_buffer, type);
		StrandsBufferSetupInfo info;
		info.converter = conv;
		info.type = type;
		
		*buf = GPU_buffer_setup(target, size, strands_copy_gpu_data, &info, *buf);
		
		GLenum tex_format;
		tex = gpu_strands_buffer_texture_from_type(strands_buffer, type, &tex_format);
		if (tex)
			gpu_strands_setup_buffer_texture(*buf, tex_format, tex);
	}
	
	return *buf != NULL;
}

void GPU_strands_setup_verts(GPUDrawStrands *strands_buffer, GPUStrandsConverter *conv)
{
	if (!strands_setup_buffer_common(strands_buffer, conv, GPU_STRAND_BUFFER_STRAND_VERTEX, false))
		return;
	
	GPU_enable_vertex_buffer(strands_buffer->strand_points, 0);
}

void GPU_strands_setup_edges(GPUDrawStrands *strands_buffer, GPUStrandsConverter *conv)
{
	if (!strands_setup_buffer_common(strands_buffer, conv, GPU_STRAND_BUFFER_STRAND_VERTEX, false))
		return;
	if (!strands_setup_buffer_common(strands_buffer, conv, GPU_STRAND_BUFFER_STRAND_EDGE, false))
		return;

	GPU_enable_vertex_buffer(strands_buffer->strand_points, 0);
	GPU_enable_element_buffer(strands_buffer->strand_edges);
}

void GPU_strands_setup_fibers(GPUDrawStrands *strands_buffer, GPUStrandsConverter *conv)
{
	if (!strands_setup_buffer_common(strands_buffer, conv, GPU_STRAND_BUFFER_CONTROL_CURVE, false))
		return;
	if (!strands_setup_buffer_common(strands_buffer, conv, GPU_STRAND_BUFFER_CONTROL_VERTEX, false))
		return;
	if (!strands_setup_buffer_common(strands_buffer, conv, GPU_STRAND_BUFFER_CONTROL_NORMAL, false))
		return;
	if (!strands_setup_buffer_common(strands_buffer, conv, GPU_STRAND_BUFFER_CONTROL_TANGENT, false))
		return;
	if (conv->use_geomshader) {
		if (!strands_setup_buffer_common(strands_buffer, conv, GPU_STRAND_BUFFER_FIBER, false))
			return;
		
		GPU_enable_vertex_buffer(strands_buffer->fibers, sizeof(GPUFiber));
	}
	else {
		if (!strands_setup_buffer_common(strands_buffer, conv, GPU_STRAND_BUFFER_FIBER_VERTEX, false))
			return;
		if (!strands_setup_buffer_common(strands_buffer, conv, GPU_STRAND_BUFFER_FIBER_EDGE, false))
			return;
		if (!strands_setup_buffer_common(strands_buffer, conv, GPU_STRAND_BUFFER_FIBER_POSITION, false))
			return;
		if (!strands_setup_buffer_common(strands_buffer, conv, GPU_STRAND_BUFFER_FIBER_CONTROL_INDEX, false))
			return;
		if (!strands_setup_buffer_common(strands_buffer, conv, GPU_STRAND_BUFFER_FIBER_CONTROL_WEIGHT, false))
			return;
		
		GPU_enable_vertex_buffer(strands_buffer->fiber_points, sizeof(GPUFiberVertex));
		GPU_enable_element_buffer(strands_buffer->fiber_indices);
	}

	if (strands_buffer->control_curves_tex.id != 0) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_BUFFER, strands_buffer->control_curves_tex.id);
	}
	if (strands_buffer->control_points_tex.id != 0) {
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_BUFFER, strands_buffer->control_points_tex.id);
	}
	if (strands_buffer->control_normals_tex.id != 0) {
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_BUFFER, strands_buffer->control_normals_tex.id);
	}
	if (strands_buffer->control_tangents_tex.id != 0) {
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_BUFFER, strands_buffer->control_tangents_tex.id);
	}
	if (strands_buffer->fiber_position_tex.id != 0) {
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_BUFFER, strands_buffer->fiber_position_tex.id);
	}
	if (strands_buffer->fiber_control_index_tex.id != 0) {
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_BUFFER, strands_buffer->fiber_control_index_tex.id);
	}
	if (strands_buffer->fiber_control_weight_tex.id != 0) {
		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_BUFFER, strands_buffer->fiber_control_weight_tex.id);
	}
}

void GPU_strands_buffer_unbind(void)
{
	GPU_interleaved_attrib_unbind();
	
	GPU_buffers_unbind();
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_BUFFER, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_BUFFER, 0);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_BUFFER, 0);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_BUFFER, 0);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_BUFFER, 0);
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_BUFFER, 0);
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_BUFFER, 0);
	/* reset, following draw code expects active texture 0 */
	glActiveTexture(GL_TEXTURE0);
}

void GPU_strands_buffer_invalidate(GPUDrawStrands *gpu_buffer, GPUStrands_Component components)
{
	if (components & GPU_STRANDS_COMPONENT_CONTROLS) {
		GPU_buffer_free(gpu_buffer->strand_points);
		GPU_buffer_free(gpu_buffer->strand_edges);
		GPU_buffer_free(gpu_buffer->control_points);
		GPU_buffer_free(gpu_buffer->control_normals);
		GPU_buffer_free(gpu_buffer->control_tangents);
		GPU_buffer_free(gpu_buffer->control_curves);
		gpu_buffer->strand_points = NULL;
		gpu_buffer->strand_edges = NULL;
		gpu_buffer->control_points = NULL;
		gpu_buffer->control_normals = NULL;
		gpu_buffer->control_tangents = NULL;
		gpu_buffer->control_curves = NULL;
	}
	if (components & GPU_STRANDS_COMPONENT_FIBER_ATTRIBUTES) {
		GPU_buffer_free(gpu_buffer->fiber_position);
		GPU_buffer_free(gpu_buffer->fiber_control_index);
		GPU_buffer_free(gpu_buffer->fiber_control_weight);
		gpu_buffer->fiber_position = NULL;
		gpu_buffer->fiber_control_index = NULL;
		gpu_buffer->fiber_control_weight = NULL;
	}
	if (components & GPU_STRANDS_COMPONENT_FIBERS) {
		GPU_buffer_free(gpu_buffer->fibers);
		GPU_buffer_free(gpu_buffer->fiber_points);
		GPU_buffer_free(gpu_buffer->fiber_indices);
		gpu_buffer->fibers = NULL;
		gpu_buffer->fiber_points = NULL;
		gpu_buffer->fiber_indices = NULL;
	}
}

void GPU_strands_buffer_free(GPUDrawStrands *gpu_buffer)
{
	if (gpu_buffer) {
#if 0 /* XXX crashes, maybe not needed for buffer textures? */
		if (gpu_buffer->control_curves_tex.id)
			glDeleteTextures(1, &gpu_buffer->control_curves_tex.id);
		if (gpu_buffer->control_points_tex.id)
			glDeleteTextures(1, &gpu_buffer->control_points_tex.id);
		...
#endif
		
		GPU_strands_buffer_invalidate(gpu_buffer, GPU_STRANDS_COMPONENT_ALL);
		
		MEM_freeN(gpu_buffer);
	}
}

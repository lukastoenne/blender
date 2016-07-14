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
	GPU_STRAND_BUFFER_FIBER_NORMAL,
	GPU_STRAND_BUFFER_FIBER_TANGENT,
	GPU_STRAND_BUFFER_FIBER_CONTROL_INDEX,
	GPU_STRAND_BUFFER_FIBER_CONTROL_WEIGHT,
	GPU_STRAND_BUFFER_FIBER_ROOT_DISTANCE,
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
		case GPU_STRAND_BUFFER_FIBER_NORMAL:
		case GPU_STRAND_BUFFER_FIBER_TANGENT:
		case GPU_STRAND_BUFFER_FIBER_CONTROL_INDEX:
		case GPU_STRAND_BUFFER_FIBER_CONTROL_WEIGHT:
		case GPU_STRAND_BUFFER_FIBER_ROOT_DISTANCE:
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
			return &gds->fiber_edges;
		case GPU_STRAND_BUFFER_FIBER_POSITION:
			return &gds->fiber_position;
		case GPU_STRAND_BUFFER_FIBER_NORMAL:
			return &gds->fiber_normal;
		case GPU_STRAND_BUFFER_FIBER_TANGENT:
			return &gds->fiber_tangent;
		case GPU_STRAND_BUFFER_FIBER_CONTROL_INDEX:
			return &gds->fiber_control_index;
		case GPU_STRAND_BUFFER_FIBER_CONTROL_WEIGHT:
			return &gds->fiber_control_weight;
		case GPU_STRAND_BUFFER_FIBER_ROOT_DISTANCE:
			return &gds->fiber_root_distance;
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
		case GPU_STRAND_BUFFER_FIBER_NORMAL:
			*format = GL_RGB32F;
			return &gds->fiber_normal_tex;
		case GPU_STRAND_BUFFER_FIBER_TANGENT:
			*format = GL_RGB32F;
			return &gds->fiber_tangent_tex;
		case GPU_STRAND_BUFFER_FIBER_CONTROL_INDEX:
			*format = GL_RGBA32UI;
			return &gds->fiber_control_index_tex;
		case GPU_STRAND_BUFFER_FIBER_CONTROL_WEIGHT:
			*format = GL_RGBA32F;
			return &gds->fiber_control_weight_tex;
		case GPU_STRAND_BUFFER_FIBER_ROOT_DISTANCE:
			*format = GL_RG32F;
			return &gds->fiber_root_distance_tex;
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
			return sizeof(int) * 2 * gpu_buffer->fiber_totedges;
		case GPU_STRAND_BUFFER_FIBER_POSITION:
			return sizeof(float) * 3 * gpu_buffer->totfibers;
		case GPU_STRAND_BUFFER_FIBER_NORMAL:
			return sizeof(float) * 3 * gpu_buffer->totfibers;
		case GPU_STRAND_BUFFER_FIBER_TANGENT:
			return sizeof(float) * 3 * gpu_buffer->totfibers;
		case GPU_STRAND_BUFFER_FIBER_CONTROL_INDEX:
			return sizeof(int) * 4 * gpu_buffer->totfibers;
		case GPU_STRAND_BUFFER_FIBER_CONTROL_WEIGHT:
			return sizeof(float) * 4 * gpu_buffer->totfibers;
		case GPU_STRAND_BUFFER_FIBER_ROOT_DISTANCE:
			return sizeof(float) * 2 * gpu_buffer->totfibers;
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

typedef struct BMStrandCurve
{
	BMVert *root;
	int verts_begin;
	int num_verts;
} BMStrandCurve;

static BMStrandCurve *editstrands_build_curves(BMesh *bm, unsigned int *r_totcurves)
{
	BMVert *root;
	BMIter iter;
	
	unsigned int totstrands = BM_strands_count(bm);
	BMStrandCurve *curves = MEM_mallocN(sizeof(BMStrandCurve) * totstrands, "BMStrandCurve");
	
	BMStrandCurve *curve = curves;
	BM_ITER_STRANDS(root, &iter, bm, BM_STRANDS_OF_MESH) {
		curve->root = root;
		curve->verts_begin = BM_elem_index_get(root);
		curve->num_verts = BM_strand_verts_count(root);
		
		++curve;
	}
	
	if (r_totcurves) *r_totcurves = totstrands;
	return curves;
}

static int strands_fiber_length(StrandFiber *fiber, StrandCurve *curves, unsigned int totcurves, int subdiv)
{
	float fnumverts = 0.0f;
	for (int k = 0; k < 4; ++k) {
		unsigned int index = fiber->control_index[k];
		if (index == STRAND_INDEX_NONE)
			continue;
		BLI_assert(index < totcurves);
		fnumverts += fiber->control_weight[k] * (float)curves[index].num_verts;
	}
	UNUSED_VARS(totcurves);
	int orig_num_verts = max_ii((int)ceil(fnumverts), 2);
	return BKE_strand_curve_cache_size(orig_num_verts, subdiv);
}

static void strands_count_fibers(GPUDrawStrandsParams *params,
                                 unsigned int *r_totfibers, unsigned int *r_totverts, unsigned int *r_totedges)
{
	Strands *strands = params->strands;
	
	unsigned int totfibers = strands->totfibers;
	
	unsigned int totverts = 0;
	StrandFiber *fiber = strands->fibers;
	for (int i = 0; i < totfibers; ++i, ++fiber)
		totverts += strands_fiber_length(fiber, strands->curves, strands->totcurves, params->subdiv);
	unsigned int totedges = totverts - totfibers;
	
	if (r_totfibers) *r_totfibers = totfibers;
	if (r_totverts) *r_totverts = totverts;
	if (r_totedges) *r_totedges = totedges;
}

static int editstrands_fiber_length(StrandFiber *fiber, BMStrandCurve *curves, int totcurves, int subdiv)
{
	float fnumverts = 0.0f;
	for (int k = 0; k < 4; ++k) {
		unsigned int index = fiber->control_index[k];
		if (index == STRAND_INDEX_NONE)
			continue;
		BLI_assert(index < totcurves);
		fnumverts += fiber->control_weight[k] * (float)curves[index].num_verts;
	}
	UNUSED_VARS(totcurves);
	int orig_num_verts = max_ii((int)ceil(fnumverts), 2);
	return BKE_strand_curve_cache_size(orig_num_verts, subdiv);
}

static void editstrands_count_fibers(GPUDrawStrandsParams *params,
                                     unsigned int *r_totfibers, unsigned int *r_totverts, unsigned int *r_totedges)
{
	BMEditStrands *edit = params->edit;
	BLI_assert(edit != NULL);
	BMesh *bm = edit->base.bm;
	
	unsigned int totfibers = edit->totfibers;
	
	unsigned int totcurves;
	BMStrandCurve *curves = editstrands_build_curves(bm, &totcurves);
	
	int totverts = 0;
	StrandFiber *fiber = edit->fibers;
	for (int i = 0; i < totfibers; ++i, ++fiber)
		totverts += editstrands_fiber_length(fiber, curves, totcurves, params->subdiv);
	unsigned int totedges = totverts - totfibers;
	
	MEM_freeN(curves);
	
	if (r_totfibers) *r_totfibers = totfibers;
	if (r_totverts) *r_totverts = totverts;
	if (r_totedges) *r_totedges = totedges;
}

GPUDrawStrands *GPU_strands_buffer_create(GPUDrawStrandsParams *params)
{
	GPUDrawStrands *gds = MEM_callocN(sizeof(GPUDrawStrands), "GPUDrawStrands");
	
	if (params->edit) {
		BMEditStrands *edit = params->edit;
		BMesh *bm = edit->base.bm;
		int totcurves = BM_strands_count(bm);
		gds->strand_totverts = bm->totvert;
		gds->strand_totedges = bm->totedge;
		gds->control_totcurves = totcurves;
		gds->control_totverts = BKE_strand_curve_cache_totverts(bm->totvert,
		                                                        totcurves,
		                                                        params->subdiv);
		if (params->use_geomshader) {
			gds->totfibers = edit->totfibers;
			gds->fiber_totverts = 0;
			gds->fiber_totedges = 0;
		}
		else {
			editstrands_count_fibers(params, &gds->totfibers, &gds->fiber_totverts, &gds->fiber_totedges);
		}
	}
	else {
		Strands *strands = params->strands;
		gds->strand_totverts = strands->totverts;
		gds->strand_totedges = strands->totverts - strands->totcurves;
		gds->control_totcurves = strands->totcurves;
		gds->control_totverts = BKE_strand_curve_cache_totverts(strands->totverts,
		                                                        strands->totcurves,
		                                                        params->subdiv);
		if (params->use_geomshader) {
			gds->totfibers = strands->totfibers;
			gds->fiber_totverts = 0;
			gds->fiber_totedges = 0;
		}
		else {
			strands_count_fibers(params, &gds->totfibers, &gds->fiber_totverts, &gds->fiber_totedges);
		}
	}
	
	return gds;
}

static void strands_copy_strand_vertex_data(GPUDrawStrandsParams *params, float (*varray)[3])
{
	if (params->edit) {
		BMesh *bm = params->edit->base.bm;
		BMIter iter;
		BMVert *vert;
		
		BM_ITER_MESH(vert, &iter, bm, BM_VERTS_OF_MESH) {
			copy_v3_v3(*varray++, vert->co);
		}
	}
	else {
		Strands *strands = params->strands;
		int totcurves = strands->totcurves, c;
		
		StrandCurve *curve = strands->curves;
		for (c = 0; c < totcurves; ++c, ++curve) {
			float rootmat[4][4];
			BKE_strands_get_matrix(curve, params->root_dm, rootmat);
			int verts_begin = curve->verts_begin, num_verts = curve->num_verts, v;
			BLI_assert(verts_begin < strands->totverts);
			BLI_assert(num_verts >= 2);
			
			StrandVertex *vert = strands->verts + verts_begin;
			for (v = 0; v < num_verts; ++v, ++vert) {
				mul_v3_m4v3(*varray++, rootmat, vert->co);
			}
		}
	}
}

static void strands_copy_strand_edge_data(GPUDrawStrandsParams *params, unsigned int (*varray)[2])
{
	if (params->edit) {
		BMesh *bm = params->edit->base.bm;
		BMIter iter;
		BMEdge *edge;
		
		BM_ITER_MESH(edge, &iter, bm, BM_EDGES_OF_MESH) {
			(*varray)[0] = BM_elem_index_get(edge->v1);
			(*varray)[1] = BM_elem_index_get(edge->v2);
			++varray;
		}
	}
	else {
		Strands *strands = params->strands;
		int totcurves = strands->totcurves, c;
		int totedges = 0;
		
		StrandCurve *curve = strands->curves;
		for (c = 0; c < totcurves; ++c, ++curve) {
			int verts_begin = curve->verts_begin, num_verts = curve->num_verts, v;
			BLI_assert(verts_begin < strands->totverts);
			BLI_assert(num_verts >= 2);
			
			for (v = 0; v < num_verts - 1; ++v) {
				(*varray)[0] = verts_begin + v;
				(*varray)[1] = verts_begin + v + 1;
				++varray;
				++totedges;
			}
		}
		BLI_assert(totedges == strands->totverts - totcurves);
	}
}

static void strands_copy_control_cache_attribute_data(StrandCurveCache *cache, int num_verts,
                                                      void **pvarray, GPUStrandBufferType type)
{
	void *varray = *pvarray;
	
	switch (type) {
		case GPU_STRAND_BUFFER_CONTROL_VERTEX: {
			float (*vert)[3] = cache->verts;
			for (int v = 0; v < num_verts; ++v, ++vert) {
				copy_v3_v3((float *)varray, *vert);
				varray = (float *)varray + 3;
			}
			break;
		}
		case GPU_STRAND_BUFFER_CONTROL_NORMAL: {
			float (*nor)[3] = cache->normals;
			for (int v = 0; v < num_verts; ++v, ++nor) {
				copy_v3_v3((float *)varray, *nor);
				varray = (float *)varray + 3;
			}
			break;
		}
		case GPU_STRAND_BUFFER_CONTROL_TANGENT: {
			float (*tang)[3] = cache->tangents;
			for (int v = 0; v < num_verts; ++v, ++tang) {
				copy_v3_v3((float *)varray, *tang);
				varray = (float *)varray + 3;
			}
			break;
		}
		default:
			BLI_assert(false);
			break;
	}
	
	*pvarray = varray;
}

static void strands_copy_control_attribute_data(GPUDrawStrandsParams *params, void *varray,
                                                GPUStrandBufferType type)
{
	if (params->edit) {
		BMesh *bm = params->edit->base.bm;
		
		StrandCurveCache *cache = BKE_strand_curve_cache_create_bm(bm, params->subdiv);
		
		BMIter iter;
		BMVert *root;
		BM_ITER_STRANDS(root, &iter, bm, BM_STRANDS_OF_MESH) {
			float rootmat[4][4];
			BKE_editstrands_get_matrix(params->edit, root, rootmat);
			
			int orig_num_verts = BM_strand_verts_count(root);
			int num_verts = BKE_strand_curve_cache_calc_bm(root, orig_num_verts, cache, rootmat, params->subdiv);
			BLI_assert(orig_num_verts >= 2);
			
			strands_copy_control_cache_attribute_data(cache, num_verts, &varray, type);
		}
		
		BKE_strand_curve_cache_free(cache);
	}
	else {
		Strands *strands = params->strands;
		
		StrandCurveCache *cache = BKE_strand_curve_cache_create(strands, params->subdiv);
		
		int totcurves = strands->totcurves;
		StrandCurve *curve = strands->curves;
		for (int c = 0; c < totcurves; ++c, ++curve) {
			float rootmat[4][4];
			BKE_strands_get_matrix(curve, params->root_dm, rootmat);
			
			int verts_begin = curve->verts_begin;
			int orig_num_verts = curve->num_verts;
			BLI_assert(verts_begin < strands->totverts);
			BLI_assert(orig_num_verts >= 2);
			
			int num_verts = BKE_strand_curve_cache_calc(strands->verts + verts_begin, orig_num_verts,
			                                            cache, rootmat, params->subdiv);
			
			strands_copy_control_cache_attribute_data(cache, num_verts, &varray, type);
		}
		
		BKE_strand_curve_cache_free(cache);
	}
}

static void strands_copy_control_curve_data(GPUDrawStrandsParams *params, unsigned int (*varray)[2])
{
	if (params->edit) {
		BMesh *bm = params->edit->base.bm;
		BMIter iter;
		BMVert *root;
		
		int verts_begin = 0;
		BM_ITER_STRANDS(root, &iter, bm, BM_STRANDS_OF_MESH) {
			int orig_num_verts = BM_strand_verts_count(root);
			int num_verts = BKE_strand_curve_cache_size(orig_num_verts, params->subdiv);
			
			(*varray)[0] = verts_begin;
			(*varray)[1] = num_verts;
			++varray;
			
			verts_begin += num_verts;
		}
	}
	else {
		Strands *strands = params->strands;
		int totcurves = strands->totcurves;
		
		int verts_begin = 0;
		StrandCurve *curve = strands->curves;
		for (int c = 0; c < totcurves; ++c, ++curve) {
			int num_verts = BKE_strand_curve_cache_size(curve->num_verts, params->subdiv);
			
			(*varray)[0] = verts_begin;
			(*varray)[1] = num_verts;
			++varray;
			
			verts_begin += num_verts;
		}
	}
}

static void strands_copy_fiber_data(GPUDrawStrandsParams *params, GPUFiber *varray)
{
	if (params->edit) {
		BMEditStrands *edit = params->edit;
		int totfibers = edit->totfibers;
		
		/* strand fiber points */
		StrandFiber *fiber = edit->fibers;
		for (int v = 0; v < totfibers; ++v, ++fiber) {
			BKE_strands_get_fiber_vectors(fiber, edit->root_dm, varray->co, varray->normal, varray->tangent);
			for (int k = 0; k < 4; ++k) {
				varray->control_index[k] = fiber->control_index[k];
				varray->control_weight[k] = fiber->control_weight[k];
			}
			copy_v2_v2(varray->root_distance, fiber->root_distance);
			++varray;
		}
	}
	else {
		Strands *strands = params->strands;
		int totfibers = strands->totfibers, v;
		
		/* strand fiber points */
		StrandFiber *fiber = strands->fibers;
		for (v = 0; v < totfibers; ++v, ++fiber) {
			BKE_strands_get_fiber_vectors(fiber, params->root_dm, varray->co, varray->normal, varray->tangent);
			for (int k = 0; k < 4; ++k) {
				varray->control_index[k] = fiber->control_index[k];
				varray->control_weight[k] = fiber->control_weight[k];
			}
			copy_v2_v2(varray->root_distance, fiber->root_distance);
			++varray;
		}
	}
}

static void strands_copy_fiber_vertex_data(GPUDrawStrandsParams *params, GPUFiberVertex *varray)
{
	if (params->edit) {
		BMEditStrands *edit = params->edit;
		BMesh *bm = edit->base.bm;
		int totfibers = edit->totfibers;
		
		unsigned int totcurves;
		BMStrandCurve *curves = editstrands_build_curves(bm, &totcurves);
		
		StrandFiber *fiber = edit->fibers;
		for (int i = 0; i < totfibers; ++i, ++fiber) {
			int num_verts = editstrands_fiber_length(fiber, curves, totcurves, params->subdiv);
			
			for (int k = 0; k < num_verts; ++k) {
				varray->fiber_index = i;
				varray->curve_param = (float)k / (float)(num_verts - 1);
				++varray;
			}
		}
		
		MEM_freeN(curves);
	}
	else {
		Strands *strands = params->strands;
		int totfibers = strands->totfibers;
		
		StrandFiber *fiber = strands->fibers;
		for (int i = 0; i < totfibers; ++i, ++fiber) {
			int num_verts = strands_fiber_length(fiber, strands->curves, strands->totcurves, params->subdiv);
			
			for (int k = 0; k < num_verts; ++k) {
				varray->fiber_index = i;
				varray->curve_param = (float)k / (float)(num_verts - 1);
				++varray;
			}
		}
	}
}

static void strands_copy_fiber_edge_data(GPUDrawStrandsParams *params, unsigned int (*varray)[2])
{
	if (params->edit) {
		BMEditStrands *edit = params->edit;
		BMesh *bm = edit->base.bm;
		int totfibers = edit->totfibers;
		
		unsigned int totcurves;
		BMStrandCurve *curves = editstrands_build_curves(bm, &totcurves);
		
		StrandFiber *fiber = edit->fibers;
		int verts_begin = 0;
		for (int i = 0; i < totfibers; ++i, ++fiber) {
			int num_verts = editstrands_fiber_length(fiber, curves, totcurves, params->subdiv);
			
			for (int k = 0; k < num_verts - 1; ++k) {
				(*varray)[0] = verts_begin + k;
				(*varray)[1] = verts_begin + k + 1;
				++varray;
			}
			
			verts_begin += num_verts;
		}
		
		MEM_freeN(curves);
	}
	else {
		Strands *strands = params->strands;
		int totfibers = strands->totfibers;
		
		StrandFiber *fiber = strands->fibers;
		int verts_begin = 0;
		for (int i = 0; i < totfibers; ++i, ++fiber) {
			int num_verts = strands_fiber_length(fiber, strands->curves, strands->totcurves, params->subdiv);
			
			for (int k = 0; k < num_verts - 1; ++k) {
				(*varray)[0] = verts_begin + k;
				(*varray)[1] = verts_begin + k + 1;
				++varray;
			}
			
			verts_begin += num_verts;
		}
	}
}

static void strands_copy_fiber_array_attribute_data(StrandFiber *fibers, int totfibers, DerivedMesh *root_dm, GPUStrandBufferType type, void *varray)
{
	StrandFiber *fiber = fibers;
	for (int v = 0; v < totfibers; ++v, ++fiber) {
		switch (type) {
			case GPU_STRAND_BUFFER_FIBER_POSITION: {
				float nor[3], tang[3];
				BKE_strands_get_fiber_vectors(fiber, root_dm, (float *)varray, nor, tang);
				varray = (float *)varray + 3;
				break;
			}
			case GPU_STRAND_BUFFER_FIBER_NORMAL: {
				float co[3], tang[3];
				BKE_strands_get_fiber_vectors(fiber, root_dm, co, (float *)varray, tang);
				varray = (float *)varray + 3;
				break;
			}
			case GPU_STRAND_BUFFER_FIBER_TANGENT: {
				float co[3], nor[3];
				BKE_strands_get_fiber_vectors(fiber, root_dm, co, nor, (float *)varray);
				varray = (float *)varray + 3;
				break;
			}
			case GPU_STRAND_BUFFER_FIBER_CONTROL_INDEX: {
				for (int k = 0; k < 4; ++k)
					((int *)varray)[k] = fiber->control_index[k];
				varray = (int *)varray + 4;
				break;
			}
			case GPU_STRAND_BUFFER_FIBER_CONTROL_WEIGHT: {
				copy_v4_v4((float *)varray, fiber->control_weight);
				varray = (float *)varray + 4;
				break;
			}
			case GPU_STRAND_BUFFER_FIBER_ROOT_DISTANCE: {
				copy_v2_v2((float *)varray, fiber->root_distance);
				varray = (float *)varray + 2;
				break;
			}
			default:
				BLI_assert(false);
				break;
		}
	}
}

static void strands_copy_fiber_attribute_data(GPUDrawStrandsParams *params, GPUStrandBufferType type, void *varray)
{
	if (params->edit) {
		BMEditStrands *edit = params->edit;
		strands_copy_fiber_array_attribute_data(edit->fibers, edit->totfibers, edit->root_dm, type, varray);
	}
	else {
		Strands *strands = params->strands;
		strands_copy_fiber_array_attribute_data(strands->fibers, strands->totfibers, params->root_dm, type, varray);
	}
}

typedef struct StrandsBufferSetupInfo {
	GPUDrawStrandsParams *params;
	GPUStrandBufferType type;
} StrandsBufferSetupInfo;

static void strands_copy_gpu_data(void *varray, void *vinfo)
{
	StrandsBufferSetupInfo *info = vinfo;
	GPUDrawStrandsParams *params = info->params;
	GPUStrandBufferType type = info->type;
	
	switch (type) {
		case GPU_STRAND_BUFFER_STRAND_VERTEX:
			strands_copy_strand_vertex_data(params, (float (*)[3])varray);
			break;
		case GPU_STRAND_BUFFER_STRAND_EDGE:
			strands_copy_strand_edge_data(params, (unsigned int (*)[2])varray);
			break;
		case GPU_STRAND_BUFFER_CONTROL_VERTEX:
		case GPU_STRAND_BUFFER_CONTROL_NORMAL:
		case GPU_STRAND_BUFFER_CONTROL_TANGENT:
			strands_copy_control_attribute_data(params, varray, type);
			break;
		case GPU_STRAND_BUFFER_CONTROL_CURVE:
			strands_copy_control_curve_data(params, (unsigned int (*)[2])varray);
			break;
		case GPU_STRAND_BUFFER_FIBER:
			strands_copy_fiber_data(params, (GPUFiber *)varray);
			break;
		case GPU_STRAND_BUFFER_FIBER_VERTEX:
			strands_copy_fiber_vertex_data(params, (GPUFiberVertex *)varray);
			break;
		case GPU_STRAND_BUFFER_FIBER_EDGE:
			strands_copy_fiber_edge_data(params, (unsigned int (*)[2])varray);
			break;
		case GPU_STRAND_BUFFER_FIBER_POSITION:
		case GPU_STRAND_BUFFER_FIBER_NORMAL:
		case GPU_STRAND_BUFFER_FIBER_TANGENT:
		case GPU_STRAND_BUFFER_FIBER_CONTROL_INDEX:
		case GPU_STRAND_BUFFER_FIBER_CONTROL_WEIGHT:
		case GPU_STRAND_BUFFER_FIBER_ROOT_DISTANCE:
			strands_copy_fiber_attribute_data(params, type, varray);
			break;
	}
}

static bool strands_setup_buffer_common(GPUDrawStrands *strands_buffer, GPUDrawStrandsParams *params, GPUStrandBufferType type, bool update)
{
	GPUBuffer **buf;
	GPUBufferTexture *tex;
	
	buf = gpu_strands_buffer_from_type(strands_buffer, type);
	if (*buf == NULL || update) {
		GLenum target = gpu_strands_buffer_gl_type(type);
		size_t size = gpu_strands_buffer_size_from_type(strands_buffer, type);
		StrandsBufferSetupInfo info;
		info.params = params;
		info.type = type;
		
		*buf = GPU_buffer_setup(target, size, strands_copy_gpu_data, &info, *buf);
		
		GLenum tex_format;
		tex = gpu_strands_buffer_texture_from_type(strands_buffer, type, &tex_format);
		if (tex)
			gpu_strands_setup_buffer_texture(*buf, tex_format, tex);
	}
	
	return *buf != NULL;
}

void GPU_strands_setup_verts(GPUDrawStrands *strands_buffer, GPUDrawStrandsParams *params)
{
	if (!strands_setup_buffer_common(strands_buffer, params, GPU_STRAND_BUFFER_STRAND_VERTEX, false))
		return;
	
	GPU_enable_vertex_buffer(strands_buffer->strand_points, 0);
}

void GPU_strands_setup_edges(GPUDrawStrands *strands_buffer, GPUDrawStrandsParams *params)
{
	if (!strands_setup_buffer_common(strands_buffer, params, GPU_STRAND_BUFFER_STRAND_VERTEX, false))
		return;
	if (!strands_setup_buffer_common(strands_buffer, params, GPU_STRAND_BUFFER_STRAND_EDGE, false))
		return;

	GPU_enable_vertex_buffer(strands_buffer->strand_points, 0);
	GPU_enable_element_buffer(strands_buffer->strand_edges);
}

void GPU_strands_setup_fibers(GPUDrawStrands *strands_buffer, GPUDrawStrandsParams *params)
{
	if (!strands_setup_buffer_common(strands_buffer, params, GPU_STRAND_BUFFER_CONTROL_CURVE, false))
		return;
	if (!strands_setup_buffer_common(strands_buffer, params, GPU_STRAND_BUFFER_CONTROL_VERTEX, false))
		return;
	if (!strands_setup_buffer_common(strands_buffer, params, GPU_STRAND_BUFFER_CONTROL_NORMAL, false))
		return;
	if (!strands_setup_buffer_common(strands_buffer, params, GPU_STRAND_BUFFER_CONTROL_TANGENT, false))
		return;
	if (params->use_geomshader) {
		if (!strands_setup_buffer_common(strands_buffer, params, GPU_STRAND_BUFFER_FIBER, false))
			return;
		
		GPU_enable_vertex_buffer(strands_buffer->fibers, sizeof(GPUFiber));
	}
	else {
		if (!strands_setup_buffer_common(strands_buffer, params, GPU_STRAND_BUFFER_FIBER_VERTEX, false))
			return;
		if (!strands_setup_buffer_common(strands_buffer, params, GPU_STRAND_BUFFER_FIBER_EDGE, false))
			return;
		if (!strands_setup_buffer_common(strands_buffer, params, GPU_STRAND_BUFFER_FIBER_POSITION, false))
			return;
		if (!strands_setup_buffer_common(strands_buffer, params, GPU_STRAND_BUFFER_FIBER_NORMAL, false))
			return;
		if (!strands_setup_buffer_common(strands_buffer, params, GPU_STRAND_BUFFER_FIBER_TANGENT, false))
			return;
		if (!strands_setup_buffer_common(strands_buffer, params, GPU_STRAND_BUFFER_FIBER_CONTROL_INDEX, false))
			return;
		if (!strands_setup_buffer_common(strands_buffer, params, GPU_STRAND_BUFFER_FIBER_CONTROL_WEIGHT, false))
			return;
		if (!strands_setup_buffer_common(strands_buffer, params, GPU_STRAND_BUFFER_FIBER_ROOT_DISTANCE, false))
			return;
		
		GPU_enable_vertex_buffer(strands_buffer->fiber_points, sizeof(GPUFiberVertex));
		GPU_enable_element_buffer(strands_buffer->fiber_edges);
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
	if (strands_buffer->fiber_normal_tex.id != 0) {
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_BUFFER, strands_buffer->fiber_normal_tex.id);
	}
	if (strands_buffer->fiber_tangent_tex.id != 0) {
		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_BUFFER, strands_buffer->fiber_tangent_tex.id);
	}
	if (strands_buffer->fiber_control_index_tex.id != 0) {
		glActiveTexture(GL_TEXTURE7);
		glBindTexture(GL_TEXTURE_BUFFER, strands_buffer->fiber_control_index_tex.id);
	}
	if (strands_buffer->fiber_control_weight_tex.id != 0) {
		glActiveTexture(GL_TEXTURE8);
		glBindTexture(GL_TEXTURE_BUFFER, strands_buffer->fiber_control_weight_tex.id);
	}
	if (strands_buffer->fiber_root_distance_tex.id != 0) {
		glActiveTexture(GL_TEXTURE9);
		glBindTexture(GL_TEXTURE_BUFFER, strands_buffer->fiber_root_distance_tex.id);
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
	glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_BUFFER, 0);
	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_BUFFER, 0);
	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_BUFFER, 0);
	/* reset, following draw code expects active texture 0 */
	glActiveTexture(GL_TEXTURE0);
}

void GPU_strands_buffer_invalidate(GPUDrawStrands *gpu_buffer, GPUStrandsComponent components)
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
		GPU_buffer_free(gpu_buffer->fiber_normal);
		GPU_buffer_free(gpu_buffer->fiber_tangent);
		GPU_buffer_free(gpu_buffer->fiber_control_index);
		GPU_buffer_free(gpu_buffer->fiber_control_weight);
		GPU_buffer_free(gpu_buffer->fiber_root_distance);
		gpu_buffer->fiber_position = NULL;
		gpu_buffer->fiber_normal = NULL;
		gpu_buffer->fiber_tangent = NULL;
		gpu_buffer->fiber_control_index = NULL;
		gpu_buffer->fiber_control_weight = NULL;
		gpu_buffer->fiber_root_distance = NULL;
	}
	if (components & GPU_STRANDS_COMPONENT_FIBERS) {
		GPU_buffer_free(gpu_buffer->fibers);
		GPU_buffer_free(gpu_buffer->fiber_points);
		GPU_buffer_free(gpu_buffer->fiber_edges);
		gpu_buffer->fibers = NULL;
		gpu_buffer->fiber_points = NULL;
		gpu_buffer->fiber_edges = NULL;
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

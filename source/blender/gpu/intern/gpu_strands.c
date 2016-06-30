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

#include "BLI_dynstr.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "DNA_strand_types.h"

#include "BKE_strands.h"

#include "GPU_extensions.h"
#include "GPU_strands.h"
#include "GPU_shader.h"

struct GPUStrandsShader {
	bool bound;
	
	GPUShader *shader;
	
	char *fragmentcode;
	char *geometrycode;
	char *vertexcode;
};

extern char datatoc_gpu_shader_basic_vert_glsl[];
extern char datatoc_gpu_shader_basic_frag_glsl[];
extern char datatoc_gpu_shader_basic_geom_glsl[];

static char *codegen_vertex(void)
{
#if 0
	char *code;
	
	DynStr *ds = BLI_dynstr_new();
	
	code = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);
	
	return code;
#else
	return BLI_strdup(datatoc_gpu_shader_basic_vert_glsl);
#endif
}

static char *codegen_fragment(void)
{
#if 0
	char *code;
	
	DynStr *ds = BLI_dynstr_new();
	
	code = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);
	
	return code;
#else
	return BLI_strdup(datatoc_gpu_shader_basic_frag_glsl);
#endif
}

static char *codegen_geometry(void)
{
#if 0
	char *code;
	
	DynStr *ds = BLI_dynstr_new();
	
	code = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);
	
	return code;
#else
	return BLI_strdup(datatoc_gpu_shader_basic_geom_glsl);
#endif
}

GPUStrandsShader *GPU_strand_shader_get(struct Strands *strands)
{
	if (strands->gpu_shader != NULL)
		return strands->gpu_shader;
	
	GPUStrandsShader *gpu_shader = MEM_callocN(sizeof(GPUStrandsShader), "GPUStrands");
	
	/* TODO */
	char *fragmentcode = codegen_fragment();
	char *vertexcode = codegen_vertex();
	char *geometrycode = false ? codegen_geometry() : NULL;
	
	int flags = GPU_SHADER_FLAGS_NONE;
	
	GPUShader *shader = GPU_shader_create_ex(
	                        vertexcode,
	                        fragmentcode,
	                        geometrycode,
	                        NULL,
	                        NULL,
	                        0,
	                        0,
	                        0,
	                        flags);
	
	/* failed? */
	if (shader) {
		gpu_shader->shader = shader;
		gpu_shader->vertexcode = vertexcode;
		gpu_shader->fragmentcode = fragmentcode;
		gpu_shader->geometrycode = geometrycode;
	}
	else {
		if (vertexcode)
			MEM_freeN(vertexcode);
		if (fragmentcode)
			MEM_freeN(fragmentcode);
		if (geometrycode)
			MEM_freeN(geometrycode);
	}
	
	strands->gpu_shader = gpu_shader;
	return gpu_shader;
}

void GPU_strand_shader_free(struct GPUStrandsShader *gpu_shader)
{
	if (gpu_shader->shader)
		GPU_shader_free(gpu_shader->shader);
	
	if (gpu_shader->fragmentcode)
		MEM_freeN(gpu_shader->fragmentcode);
	if (gpu_shader->vertexcode)
		MEM_freeN(gpu_shader->vertexcode);
	if (gpu_shader->geometrycode)
		MEM_freeN(gpu_shader->geometrycode);
	
	MEM_freeN(gpu_shader);
}

void GPU_strand_shader_bind(GPUStrandsShader *gpu_shader,
                      float viewmat[4][4], float viewinv[4][4])
{
	if (!gpu_shader->shader)
		return;

	GPU_shader_bind(gpu_shader->shader);
	
	UNUSED_VARS(viewmat, viewinv);
}

void GPU_strand_shader_bind_uniforms(GPUStrandsShader *gpu_shader,
                               float obmat[4][4], float viewmat[4][4])
{
	if (!gpu_shader->shader)
		return;
	
	UNUSED_VARS(obmat, viewmat);
}

void GPU_strand_shader_unbind(GPUStrandsShader *gpu_shader)
{
	gpu_shader->bound = 0;
	GPU_shader_unbind();
}

bool GPU_strand_shader_bound(GPUStrandsShader *gpu_shader)
{
	return gpu_shader->bound;
}

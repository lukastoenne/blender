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
#include "BKE_strands.h"

#include "GPU_buffers.h" /* XXX just for GPUAttrib, can that type be moved? */
#include "GPU_extensions.h"
#include "GPU_strands.h"
#include "GPU_shader.h"


struct GPUStrandsShader {
	bool bound;
	
	GPUShader *shader;
	GPUAttrib attributes[GPU_MAX_ATTRIB];
	int num_attributes;
	
	char *fragmentcode;
	char *geometrycode;
	char *vertexcode;
};

extern char datatoc_gpu_shader_strand_frag_glsl[];
extern char datatoc_gpu_shader_strand_geom_glsl[];
extern char datatoc_gpu_shader_strand_vert_glsl[];
extern char datatoc_gpu_shader_strand_effects_glsl[];
extern char datatoc_gpu_shader_strand_util_glsl[];

static char *codegen_vertex(void)
{
	char *code;
	
	DynStr *ds = BLI_dynstr_new();
	
	BLI_dynstr_append(ds, datatoc_gpu_shader_strand_util_glsl);
	BLI_dynstr_append(ds, datatoc_gpu_shader_strand_effects_glsl);
	BLI_dynstr_append(ds, datatoc_gpu_shader_strand_vert_glsl);
	
	code = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);
	
	return code;
}

static char *codegen_geometry(void)
{
	char *code;
	
	DynStr *ds = BLI_dynstr_new();
	BLI_dynstr_append(ds, datatoc_gpu_shader_strand_util_glsl);
	BLI_dynstr_append(ds, datatoc_gpu_shader_strand_effects_glsl);
	BLI_dynstr_append(ds, datatoc_gpu_shader_strand_geom_glsl);
	
	code = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);
	
	return code;
}

static char *codegen_fragment(void)
{
	char *code;
	
	DynStr *ds = BLI_dynstr_new();
	
	BLI_dynstr_append(ds, datatoc_gpu_shader_strand_util_glsl);
	BLI_dynstr_append(ds, datatoc_gpu_shader_strand_frag_glsl);
	
	code = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);
	
	return code;
}

GPUStrandsShader *GPU_strand_shader_get(struct Strands *strands,
                                        GPUStrands_ShaderModel shader_model,
                                        int effects,
                                        bool use_geometry_shader)
{
	if (strands->gpu_shader != NULL)
		return strands->gpu_shader;
	
	GPUStrandsShader *gpu_shader = MEM_callocN(sizeof(GPUStrandsShader), "GPUStrands");
	
	/* TODO */
	char *vertexcode = codegen_vertex();
	char *geometrycode = use_geometry_shader ? codegen_geometry() : NULL;
	char *fragmentcode = codegen_fragment();
	
	int flags = GPU_SHADER_FLAGS_NONE;
	
#define MAX_DEFINES 1024
	char defines[MAX_DEFINES];
	char *defines_cur = defines;
	*defines_cur = '\0';
	
	if (use_geometry_shader) {
		defines_cur += BLI_snprintf(defines_cur, MAX_DEFINES - (defines_cur - defines),
		                            "#define USE_GEOMSHADER\n");
	}
	switch (shader_model) {
		case GPU_STRAND_SHADER_CLASSIC_BLENDER:
			defines_cur += BLI_snprintf(defines_cur, MAX_DEFINES - (defines_cur - defines),
			                            "#define SHADING_CLASSIC_BLENDER\n");
			break;
		case GPU_STRAND_SHADER_KAJIYA:
			defines_cur += BLI_snprintf(defines_cur, MAX_DEFINES - (defines_cur - defines),
			                            "#define SHADING_KAJIYA\n");
			break;
		case GPU_STRAND_SHADER_MARSCHNER:
			defines_cur += BLI_snprintf(defines_cur, MAX_DEFINES - (defines_cur - defines),
			                            "#define SHADING_MARSCHNER\n");
			break;
	}
	if (effects & GPU_STRAND_EFFECT_CLUMP)
		defines_cur += BLI_snprintf(defines_cur, MAX_DEFINES - (defines_cur - defines),
		                            "#define USE_EFFECT_CLUMPING\n");
	if (effects & GPU_STRAND_EFFECT_CURL)
		defines_cur += BLI_snprintf(defines_cur, MAX_DEFINES - (defines_cur - defines),
		                            "#define USE_EFFECT_CURL\n");
		
	
#undef MAX_DEFINES
	
	GPUShader *shader = GPU_shader_create_ex(
	                        vertexcode,
	                        fragmentcode,
	                        geometrycode,
	                        NULL,
	                        defines,
	                        0,
	                        0,
	                        0,
	                        flags);
	
	/* failed? */
	if (shader) {
		gpu_shader->shader = shader;
		gpu_shader->vertexcode = vertexcode;
		gpu_shader->geometrycode = geometrycode;
		gpu_shader->fragmentcode = fragmentcode;
		
		GPU_shader_bind(gpu_shader->shader);
		GPU_shader_uniform_int(shader, GPU_shader_get_uniform(shader, "control_curves"), 0);
		GPU_shader_uniform_int(shader, GPU_shader_get_uniform(shader, "control_points"), 1);
		GPU_shader_uniform_int(shader, GPU_shader_get_uniform(shader, "control_normals"), 2);
		GPU_shader_uniform_int(shader, GPU_shader_get_uniform(shader, "control_tangents"), 3);
		GPU_shader_uniform_int(shader, GPU_shader_get_uniform(shader, "fiber_position"), 4);
		GPU_shader_uniform_int(shader, GPU_shader_get_uniform(shader, "fiber_normal"), 5);
		GPU_shader_uniform_int(shader, GPU_shader_get_uniform(shader, "fiber_tangent"), 6);
		GPU_shader_uniform_int(shader, GPU_shader_get_uniform(shader, "fiber_control_index"), 7);
		GPU_shader_uniform_int(shader, GPU_shader_get_uniform(shader, "fiber_control_weight"), 8);
		GPU_shader_uniform_int(shader, GPU_shader_get_uniform(shader, "fiber_root_distance"), 9);
		GPU_shader_unbind();
		
		GPUAttrib *attr = gpu_shader->attributes;
		
		if (use_geometry_shader) {
			/* position */
			attr->index = -1; /* no explicit attribute, we use gl_Vertex for this */
			attr->info_index = -1;
			attr->type = GL_FLOAT;
			attr->size = 3;
			++attr;
			
			/* normal */
			attr->index = GPU_shader_get_attribute(gpu_shader->shader, "normal");
			attr->info_index = -1;
			attr->type = GL_FLOAT;
			attr->size = 3;
			++attr;
			
			/* tangent */
			attr->index = GPU_shader_get_attribute(gpu_shader->shader, "tangent");
			attr->info_index = -1;
			attr->type = GL_FLOAT;
			attr->size = 3;
			++attr;
			
			/* control_index */
			attr->index = GPU_shader_get_attribute(gpu_shader->shader, "control_index");
			attr->info_index = -1;
			attr->type = GL_UNSIGNED_INT;
			attr->size = 4;
			++attr;
			
			/* control_weight */
			attr->index = GPU_shader_get_attribute(gpu_shader->shader, "control_weight");
			attr->info_index = -1;
			attr->type = GL_FLOAT;
			attr->size = 4;
			++attr;
			
			/* root_distance */
			attr->index = GPU_shader_get_attribute(gpu_shader->shader, "root_distance");
			attr->info_index = -1;
			attr->type = GL_FLOAT;
			attr->size = 2;
			++attr;
		}
		else {
			/* fiber_index */
			attr->index = GPU_shader_get_attribute(gpu_shader->shader, "fiber_index");
			attr->info_index = -1;
			attr->type = GL_UNSIGNED_INT;
			attr->size = 1;
			++attr;
			
			/* curve_param */
			attr->index = GPU_shader_get_attribute(gpu_shader->shader, "curve_param");
			attr->info_index = -1;
			attr->type = GL_FLOAT;
			attr->size = 1;
			++attr;
		}
		
		gpu_shader->num_attributes = (int)(attr - gpu_shader->attributes);
	}
	else {
		if (vertexcode)
			MEM_freeN(vertexcode);
		if (geometrycode)
			MEM_freeN(geometrycode);
		if (fragmentcode)
			MEM_freeN(fragmentcode);
	}
	
	strands->gpu_shader = gpu_shader;
	return gpu_shader;
}

void GPU_strand_shader_free(struct GPUStrandsShader *gpu_shader)
{
	if (gpu_shader->shader)
		GPU_shader_free(gpu_shader->shader);
	
	if (gpu_shader->vertexcode)
		MEM_freeN(gpu_shader->vertexcode);
	if (gpu_shader->geometrycode)
		MEM_freeN(gpu_shader->geometrycode);
	if (gpu_shader->fragmentcode)
		MEM_freeN(gpu_shader->fragmentcode);
	
	MEM_freeN(gpu_shader);
}

void GPU_strand_shader_bind(GPUStrandsShader *strand_shader,
                      float viewmat[4][4], float viewinv[4][4],
                      float clump_thickness, float clump_shape,
                      float curl_thickness, float curl_shape, float curl_radius, float curl_length,
                      int debug_value)
{
	GPUShader *shader = strand_shader->shader;
	if (!shader)
		return;

	GPU_shader_bind(shader);
	glUniform1f(GPU_shader_get_uniform(shader, "clump_thickness"), clump_thickness);
	glUniform1f(GPU_shader_get_uniform(shader, "clump_shape"), clump_shape);
	glUniform1f(GPU_shader_get_uniform(shader, "curl_thickness"), curl_thickness);
	glUniform1f(GPU_shader_get_uniform(shader, "curl_shape"), curl_shape);
	glUniform1f(GPU_shader_get_uniform(shader, "curl_radius"), curl_radius);
	glUniform1f(GPU_shader_get_uniform(shader, "curl_length"), curl_length);
	glUniform1i(GPU_shader_get_uniform(shader, "debug_value"), debug_value);

	strand_shader->bound = true;

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
	gpu_shader->bound = false;
	GPU_shader_unbind();
}

bool GPU_strand_shader_bound(GPUStrandsShader *gpu_shader)
{
	return gpu_shader->bound;
}

void GPU_strand_shader_get_fiber_attributes(GPUStrandsShader *gpu_shader,
                                            GPUAttrib **r_attrib, int *r_num)
{
	if (r_attrib) *r_attrib = gpu_shader->attributes;
	if (r_num) *r_num = gpu_shader->num_attributes;
}

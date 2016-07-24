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

#include "BVM_api.h"

#include "GPU_buffers.h" /* XXX just for GPUAttrib, can that type be moved? */
#include "GPU_extensions.h"
#include "GPU_strands.h"
#include "GPU_shader.h"

#include "intern/gpu_bvm_nodes.h"

#define MAX_DEFINES 1024

struct GPUStrandsShader {
	bool bound;
	
	GPUShader *shader;
	GPUAttrib attributes[GPU_MAX_ATTRIB];
	int num_attributes;
	char *fragmentcode;
	char *geometrycode;
	char *vertexcode;
	
	GPUShader *debug_shader;
	GPUAttrib debug_attributes[GPU_MAX_ATTRIB];
	int num_debug_attributes;
	char *debug_fragmentcode;
	char *debug_geometrycode;
	char *debug_vertexcode;
};

extern char datatoc_gpu_shader_strand_frag_glsl[];
extern char datatoc_gpu_shader_strand_geom_glsl[];
extern char datatoc_gpu_shader_strand_vert_glsl[];
extern char datatoc_gpu_shader_strand_debug_frag_glsl[];
extern char datatoc_gpu_shader_strand_debug_geom_glsl[];
extern char datatoc_gpu_shader_strand_debug_vert_glsl[];
extern char datatoc_gpu_shader_strand_util_glsl[];

static void get_defines(GPUStrandsShaderParams *params, char *defines)
{
	switch (params->shader_model) {
		case GPU_STRAND_SHADER_CLASSIC_BLENDER:
			strcat(defines, "#define SHADING_CLASSIC_BLENDER\n");
			break;
		case GPU_STRAND_SHADER_KAJIYA:
			strcat(defines, "#define SHADING_KAJIYA\n");
			break;
		case GPU_STRAND_SHADER_MARSCHNER:
			strcat(defines, "#define SHADING_MARSCHNER\n");
			break;
	}
	switch (params->fiber_primitive) {
		case GPU_STRANDS_FIBER_LINE:
			strcat(defines, "#define FIBER_LINE\n");
			break;
		case GPU_STRANDS_FIBER_RIBBON:
			strcat(defines, "#define FIBER_RIBBON\n");
			break;
	}
	
	if (params->effects & GPU_STRAND_EFFECT_CLUMP)
		strcat(defines, "#define USE_EFFECT_CLUMPING\n");
	if (params->effects & GPU_STRAND_EFFECT_CURL)
		strcat(defines, "#define USE_EFFECT_CURL\n");
	
	if (params->use_geomshader) {
		strcat(defines, "#define USE_GEOMSHADER\n");
	}
}

static void set_texture_uniforms(GPUShader *shader)
{
	GPU_shader_bind(shader);
	GPU_shader_uniform_int(shader, GPU_shader_get_uniform(shader, "samplers.control_curves"), 0);
	GPU_shader_uniform_int(shader, GPU_shader_get_uniform(shader, "samplers.control_points"), 1);
	GPU_shader_uniform_int(shader, GPU_shader_get_uniform(shader, "samplers.control_normals"), 2);
	GPU_shader_uniform_int(shader, GPU_shader_get_uniform(shader, "samplers.control_tangents"), 3);
	GPU_shader_uniform_int(shader, GPU_shader_get_uniform(shader, "samplers.fiber_position"), 4);
	GPU_shader_uniform_int(shader, GPU_shader_get_uniform(shader, "samplers.fiber_control_index"), 5);
	GPU_shader_uniform_int(shader, GPU_shader_get_uniform(shader, "samplers.fiber_control_weight"), 6);
	GPU_shader_unbind();
}

static void get_shader_attributes(GPUShader *shader, bool use_geomshader,
                                  GPUAttrib *r_attributes, int *r_num_attributes)
{
	GPUAttrib *attr = r_attributes;
	
	if (use_geomshader) {
		/* position */
		attr->index = -1; /* no explicit attribute, we use gl_Vertex for this */
		attr->info_index = -1;
		attr->type = GL_FLOAT;
		attr->size = 3;
		++attr;
		
		/* normal */
		attr->index = GPU_shader_get_attribute(shader, "normal");
		attr->info_index = -1;
		attr->type = GL_FLOAT;
		attr->size = 3;
		++attr;
		
		/* tangent */
		attr->index = GPU_shader_get_attribute(shader, "tangent");
		attr->info_index = -1;
		attr->type = GL_FLOAT;
		attr->size = 3;
		++attr;
		
		/* control_index */
		attr->index = GPU_shader_get_attribute(shader, "control_index");
		attr->info_index = -1;
		attr->type = GL_UNSIGNED_INT;
		attr->size = 4;
		++attr;
		
		/* control_weight */
		attr->index = GPU_shader_get_attribute(shader, "control_weight");
		attr->info_index = -1;
		attr->type = GL_FLOAT;
		attr->size = 4;
		++attr;
		
		/* root_distance */
		attr->index = GPU_shader_get_attribute(shader, "root_distance");
		attr->info_index = -1;
		attr->type = GL_FLOAT;
		attr->size = 2;
		++attr;
	}
	else {
		/* fiber_index */
		attr->index = GPU_shader_get_attribute(shader, "fiber_index");
		attr->info_index = -1;
		attr->type = GL_UNSIGNED_INT;
		attr->size = 1;
		++attr;
		
		/* curve_param */
		attr->index = GPU_shader_get_attribute(shader, "curve_param");
		attr->info_index = -1;
		attr->type = GL_FLOAT;
		attr->size = 1;
		++attr;
	}
	
	*r_num_attributes = (int)(attr - r_attributes);
}

static char *codegen(const char *basecode, const char *nodecode)
{
	char *code;
	
	if (!basecode)
		return NULL;
	
	DynStr *ds = BLI_dynstr_new();
	
	BLI_dynstr_append(ds, datatoc_gpu_shader_strand_util_glsl);
	
	if (nodecode)
		BLI_dynstr_append(ds, nodecode);
	BLI_dynstr_append(ds, basecode);
	
	code = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);
	
	return code;
}

GPUStrandsShader *GPU_strand_shader_create(GPUStrandsShaderParams *params)
{
	bool use_geometry_shader = params->use_geomshader;
	
	char *vertex_basecode = datatoc_gpu_shader_strand_vert_glsl;
	char *geometry_basecode = use_geometry_shader ? datatoc_gpu_shader_strand_geom_glsl : NULL;
	char *fragment_basecode = datatoc_gpu_shader_strand_frag_glsl;
	char *vertex_debug_basecode = datatoc_gpu_shader_strand_debug_vert_glsl;
	char *geometry_debug_basecode = datatoc_gpu_shader_strand_debug_geom_glsl;
	char *fragment_debug_basecode = datatoc_gpu_shader_strand_debug_frag_glsl;
	
	char *nodecode;
	if (params->nodes) {
		nodecode = BVM_gen_hair_deform_function_glsl(params->nodes, "displace_vertex");
	}
	else {
		nodecode = BLI_strdup("\n"
		                      "void displace_vertex(in vec3 location_V, in vec3 location_DX, in vec3 location_DY,"
		                      "                     in float parameter_V, in float parameter_DX, in float parameter_DY,"
		                      "                     in float curvelength_V, in float curvelength_DX, in float curvelength_DY,"
		                      "                     in mat4 target,"
		                      "                     out vec3 offset_V, out vec3 offset_DX, out vec3 offset_DY)"
		                      "{ offset_V = vec3(0.0); offset_DX = vec3(0.0); offset_DY = vec3(0.0); }\n");
	}
	
	GPUStrandsShader *gpu_shader = MEM_callocN(sizeof(GPUStrandsShader), "GPUStrands");
	
	int flags = GPU_SHADER_FLAGS_NONE;
	char defines[MAX_DEFINES] = "";
	get_defines(params, defines);
	
	/* Main shader */
	char *vertexcode = codegen(vertex_basecode, nodecode);
	char *geometrycode = codegen(geometry_basecode, NULL);
	char *fragmentcode = codegen(fragment_basecode, NULL);
	GPUShader *shader = GPU_shader_create_ex(vertexcode, fragmentcode, geometrycode, glsl_bvm_nodes_library,
	                                         defines, 0, 0, 0, flags);
	if (shader) {
		gpu_shader->shader = shader;
		gpu_shader->vertexcode = vertexcode;
		gpu_shader->geometrycode = geometrycode;
		gpu_shader->fragmentcode = fragmentcode;
		
		set_texture_uniforms(shader);
		get_shader_attributes(shader, use_geometry_shader, gpu_shader->attributes, &gpu_shader->num_attributes);
	}
	else {
		if (vertexcode)
			MEM_freeN(vertexcode);
		if (geometrycode)
			MEM_freeN(geometrycode);
		if (fragmentcode)
			MEM_freeN(fragmentcode);
	}
	
	/* Debug shader */
	char *debug_vertexcode = codegen(vertex_debug_basecode, nodecode);
	char *debug_geometrycode = codegen(geometry_debug_basecode, NULL);
	char *debug_fragmentcode = codegen(fragment_debug_basecode, NULL);
	GPUShader *debug_shader = GPU_shader_create_ex(debug_vertexcode, debug_fragmentcode, debug_geometrycode, glsl_bvm_nodes_library,
	                                         defines, 0, 0, 0, flags);
	if (debug_shader) {
		gpu_shader->debug_shader = debug_shader;
		gpu_shader->debug_vertexcode = debug_vertexcode;
		gpu_shader->debug_geometrycode = debug_geometrycode;
		gpu_shader->debug_fragmentcode = debug_fragmentcode;
		
		set_texture_uniforms(debug_shader);
		get_shader_attributes(debug_shader, false, gpu_shader->debug_attributes, &gpu_shader->num_debug_attributes);
	}
	else {
		if (debug_vertexcode)
			MEM_freeN(debug_vertexcode);
		if (debug_geometrycode)
			MEM_freeN(debug_geometrycode);
		if (debug_fragmentcode)
			MEM_freeN(debug_fragmentcode);
	}
	
	/* gets included in shader code */
	if (nodecode)
		MEM_freeN(nodecode);
	
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
	
	if (gpu_shader->debug_shader)
		GPU_shader_free(gpu_shader->debug_shader);
	if (gpu_shader->debug_vertexcode)
		MEM_freeN(gpu_shader->debug_vertexcode);
	if (gpu_shader->debug_geometrycode)
		MEM_freeN(gpu_shader->debug_geometrycode);
	if (gpu_shader->debug_fragmentcode)
		MEM_freeN(gpu_shader->debug_fragmentcode);
	
	MEM_freeN(gpu_shader);
}

void GPU_strand_shader_bind(GPUStrandsShader *strand_shader,
                      float viewmat[4][4], float viewinv[4][4],
                      float ribbon_width,
                      float clump_thickness, float clump_shape,
                      float curl_thickness, float curl_shape, float curl_radius, float curl_length,
                      int debug_value, float debug_scale)
{
	GPUShader *shader = (debug_value == 0) ? strand_shader->shader : strand_shader->debug_shader;
	if (!shader)
		return;

	GPU_shader_bind(shader);
	glUniform1f(GPU_shader_get_uniform(shader, "ribbon_width"), ribbon_width);
	glUniform1f(GPU_shader_get_uniform(shader, "clump_thickness"), clump_thickness);
	glUniform1f(GPU_shader_get_uniform(shader, "clump_shape"), clump_shape);
	glUniform1f(GPU_shader_get_uniform(shader, "curl_thickness"), curl_thickness);
	glUniform1f(GPU_shader_get_uniform(shader, "curl_shape"), curl_shape);
	glUniform1f(GPU_shader_get_uniform(shader, "curl_radius"), curl_radius);
	glUniform1f(GPU_shader_get_uniform(shader, "curl_length"), curl_length);
	glUniform1i(GPU_shader_get_uniform(shader, "debug_mode"), debug_value != 0);
	glUniform1i(GPU_shader_get_uniform(shader, "debug_value"), debug_value);
	glUniform1f(GPU_shader_get_uniform(shader, "debug_scale"), debug_scale);

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

void GPU_strand_shader_get_fiber_attributes(GPUStrandsShader *gpu_shader, bool debug,
                                            GPUAttrib **r_attrib, int *r_num)
{
	if (!debug) {
		if (r_attrib) *r_attrib = gpu_shader->attributes;
		if (r_num) *r_num = gpu_shader->num_attributes;
	}
	else {
		if (r_attrib) *r_attrib = gpu_shader->debug_attributes;
		if (r_num) *r_num = gpu_shader->num_debug_attributes;
	}
}

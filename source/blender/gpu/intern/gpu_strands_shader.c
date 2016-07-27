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

static void get_defines(const GPUStrandsShaderParams *params, char *defines)
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
	
	if (params->use_geomshader) {
		strcat(defines, "#define USE_GEOMSHADER\n");
	}
}

/* XXX variant of GPUPass (plus attributes),
 * all this will need reconsideration in 2.8 viewport code.
 */
typedef struct GPUStrandsPass {
	struct GPUShader *shader;
	char *vertexcode;
	char *geometrycode;
	char *fragmentcode;
	
	GPUAttrib attributes[GPU_MAX_ATTRIB];
	int num_attributes;
} GPUStrandsPass;

typedef struct GPUAttribDecl {
	const char *name;
	int type;
	int size;
} GPUAttribDecl;

typedef struct GPUTextureDecl {
	const char *name;
} GPUTextureDecl;

static bool create_pass(GPUStrandsPass *pass, const GPUStrandsShaderParams *params,
                        char *vertexcode, char *geometrycode, char *fragmentcode,
                        const GPUAttribDecl *attrib_decls, const GPUTextureDecl *tex_decls)
{
	int flags = GPU_SHADER_FLAGS_NONE;
	char defines[MAX_DEFINES] = "";
	get_defines(params, defines);
	
	GPUShader *shader = GPU_shader_create_ex(vertexcode, fragmentcode, geometrycode, glsl_bvm_nodes_library,
	                                         defines, 0, 0, 0, flags);
	if (shader) {
		pass->shader = shader;
		pass->vertexcode = vertexcode;
		pass->geometrycode = geometrycode;
		pass->fragmentcode = fragmentcode;
		
		{
			const GPUAttribDecl *decl = attrib_decls;
			GPUAttrib *attr = pass->attributes;
			pass->num_attributes = 0;
			while (decl->name != NULL) {
				attr->index = GPU_shader_get_attribute(shader, decl->name);
				attr->info_index = -1;
				attr->type = decl->type;
				attr->size = decl->size;
				
				++decl;
				++attr;
				++pass->num_attributes;
			}
		}
		
		{
			GPU_shader_bind(shader);
			const GPUTextureDecl *decl = tex_decls;
			int i = 0;
			while (decl->name != NULL) {
				GPU_shader_uniform_int(shader, GPU_shader_get_uniform(shader, decl->name), i);
				
				++decl;
				++i;
			}
			GPU_shader_unbind();
		}
		
		return true;
	}
	else {
		if (vertexcode)
			MEM_freeN(vertexcode);
		if (geometrycode)
			MEM_freeN(geometrycode);
		if (fragmentcode)
			MEM_freeN(fragmentcode);
		return false;
	}
}

static void free_pass(GPUStrandsPass *pass)
{
	if (pass) {
		if (pass->shader)
			GPU_shader_free(pass->shader);
		if (pass->vertexcode)
			MEM_freeN(pass->vertexcode);
		if (pass->geometrycode)
			MEM_freeN(pass->geometrycode);
		if (pass->fragmentcode)
			MEM_freeN(pass->fragmentcode);
	}
}

struct GPUStrandsShader {
	bool bound;
	
	GPUStrandsPass shading_pass;
	GPUStrandsPass debug_pass;
};

extern char datatoc_gpu_shader_strand_frag_glsl[];
extern char datatoc_gpu_shader_strand_geom_glsl[];
extern char datatoc_gpu_shader_strand_vert_glsl[];
extern char datatoc_gpu_shader_strand_debug_frag_glsl[];
extern char datatoc_gpu_shader_strand_debug_geom_glsl[];
extern char datatoc_gpu_shader_strand_debug_vert_glsl[];
extern char datatoc_gpu_shader_strand_util_glsl[];

static const GPUAttribDecl fiber_geomshader_attributes[] = {
    { "", GL_FLOAT, 3 }, /* no explicit attribute, we use gl_Vertex for this */
    { "normal", GL_FLOAT, 3 },
    { "tangent", GL_FLOAT, 3 },
    { "control_index", GL_UNSIGNED_INT, 4 },
    { "control_weight", GL_FLOAT, 4 },
    { "root_distance", GL_FLOAT, 2 },
    { NULL, 0, 0 }
};

static const GPUAttribDecl fiber_attributes[] = {
    { "fiber_index", GL_UNSIGNED_INT, 1 },
    { "curve_param", GL_FLOAT, 1 },
    { NULL, 0, 0 }
};

static const GPUTextureDecl fiber_textures[] = {
    { "samplers.control_curves" },
    { "samplers.control_points" },
    { "samplers.control_normals" },
    { "samplers.control_tangents" },
    { "samplers.fiber_position" },
    { "samplers.fiber_control_index" },
    { "samplers.fiber_control_weight" },
    { NULL }
};

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
	
	/* Main shader */
	const GPUAttribDecl *attributes = use_geometry_shader ? fiber_geomshader_attributes : fiber_attributes;
	create_pass(&gpu_shader->shading_pass, params,
	            codegen(datatoc_gpu_shader_strand_vert_glsl, nodecode),
	            use_geometry_shader ? codegen(datatoc_gpu_shader_strand_geom_glsl, NULL) : NULL,
	            codegen(datatoc_gpu_shader_strand_frag_glsl, NULL),
	            attributes, fiber_textures);
	
	/* Debug shader */
	create_pass(&gpu_shader->debug_pass, params,
	            codegen(datatoc_gpu_shader_strand_debug_vert_glsl, nodecode),
	            codegen(datatoc_gpu_shader_strand_debug_geom_glsl, NULL),
	            codegen(datatoc_gpu_shader_strand_debug_frag_glsl, NULL),
	            fiber_attributes, fiber_textures);
	
	/* gets included in shader code */
	if (nodecode)
		MEM_freeN(nodecode);
	
	return gpu_shader;
}

void GPU_strand_shader_free(struct GPUStrandsShader *gpu_shader)
{
	free_pass(&gpu_shader->shading_pass);
	free_pass(&gpu_shader->debug_pass);
	
	MEM_freeN(gpu_shader);
}

void GPU_strand_shader_bind(GPUStrandsShader *strand_shader,
                      float viewmat[4][4], float viewinv[4][4],
                      float ribbon_width,
                      int debug_value, float debug_scale)
{
	GPUStrandsPass *pass = (debug_value == 0) ? &strand_shader->shading_pass : &strand_shader->debug_pass;
	if (!pass->shader)
		return;

	GPU_shader_bind(pass->shader);
	glUniform1f(GPU_shader_get_uniform(pass->shader, "ribbon_width"), ribbon_width);
	glUniform1i(GPU_shader_get_uniform(pass->shader, "debug_mode"), debug_value != 0);
	glUniform1i(GPU_shader_get_uniform(pass->shader, "debug_value"), debug_value);
	glUniform1f(GPU_shader_get_uniform(pass->shader, "debug_scale"), debug_scale);

	strand_shader->bound = true;

	UNUSED_VARS(viewmat, viewinv);
}

void GPU_strand_shader_bind_uniforms(GPUStrandsShader *gpu_shader,
                                     float obmat[4][4], float viewmat[4][4])
{
	if (!gpu_shader->shading_pass.shader)
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
	GPUStrandsPass *pass = (!debug) ? &gpu_shader->shading_pass : &gpu_shader->debug_pass;
	if (r_attrib) *r_attrib = pass->attributes;
	if (r_num) *r_num = pass->num_attributes;
}

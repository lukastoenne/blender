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

typedef enum GPUStrandAttributes {
	GPU_STRAND_ATTRIB_POSITION,
	GPU_STRAND_ATTRIB_GUIDE_INDEX,
	GPU_STRAND_ATTRIB_GUIDE_WEIGHT,
	
	NUM_GPU_STRAND_ATTRIB /* must be last */
} GPUStrandAttributes;

struct GPUStrandsShader {
	bool bound;
	
	GPUShader *shader;
	GPUAttrib attributes[NUM_GPU_STRAND_ATTRIB];
	
	char *fragmentcode;
	char *geometrycode;
	char *vertexcode;
};

const char *vertex_shader = STRINGIFY(
	in uvec3 control_index;
	in vec3 control_weight;
	
	void main()
	{
		vec4 co = gl_ModelViewMatrix * (gl_Vertex + vec4(control_weight.xyz, 0.0));
		gl_Position = gl_ProjectionMatrix * co;
	}
);

const char *fragment_shader = STRINGIFY(
	out vec4 outColor;
	
	void main()
	{
		outColor = vec4(1.0, 0.0, 1.0, 1.0);
	}
);

const char *geometry_shader = STRINGIFY(
	layout(points) in;
	layout(line_strip, max_vertices = 2) out;
	
	uniform samplerBuffer guides;
	
	void main()
	{
		gl_Position = gl_in[0].gl_Position + vec4(-0.1, 0.0, 0.0, 0.0);
		EmitVertex();
		
		gl_Position = gl_in[0].gl_Position + vec4(0.1, 0.0, 0.0, 0.0);
		EmitVertex();
		
		EndPrimitive();
	}
);

static char *codegen_vertex(void)
{
#if 0
	char *code;
	
	DynStr *ds = BLI_dynstr_new();
	
	code = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);
	
	return code;
#else
	return BLI_strdup(vertex_shader);
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
	return BLI_strdup(fragment_shader);
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
	return BLI_strdup(geometry_shader);
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
	char *geometrycode = codegen_geometry();
	
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
		
		GPUAttrib *attr;
		
		attr = &gpu_shader->attributes[GPU_STRAND_ATTRIB_POSITION];
		attr->index = -1; /* no explicit attribute, we use gl_Vertex for this */
		attr->info_index = -1;
		attr->type = GL_FLOAT;
		attr->size = 3;
		
		attr = &gpu_shader->attributes[GPU_STRAND_ATTRIB_GUIDE_INDEX];
		attr->index = GPU_shader_get_attribute(gpu_shader->shader, "control_index");
		attr->info_index = -1;
		attr->type = GL_UNSIGNED_INT;
		attr->size = 4;
		
		attr = &gpu_shader->attributes[GPU_STRAND_ATTRIB_GUIDE_WEIGHT];
		attr->index = GPU_shader_get_attribute(gpu_shader->shader, "control_weight");
		attr->info_index = -1;
		attr->type = GL_FLOAT;
		attr->size = 4;
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

void GPU_strand_shader_get_attributes(GPUStrandsShader *gpu_shader,
                                      GPUAttrib **r_attrib, int *r_num)
{
	if (r_attrib) *r_attrib = gpu_shader->attributes;
	if (r_num) *r_num = NUM_GPU_STRAND_ATTRIB;
}

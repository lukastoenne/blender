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
	GPU_STRAND_ATTRIB_CONTROL_INDEX,
	GPU_STRAND_ATTRIB_CONTROL_WEIGHT,
	
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
	in uvec4 control_index;
	in vec4 control_weight;
	
	out uvec4 v_control_index;
	out vec4 v_control_weight;

	out vec3 vColor;
	
	void main()
	{
//		vec4 co = gl_ModelViewMatrix * gl_Vertex;
//		gl_Position = gl_ProjectionMatrix * co;
		gl_Position = gl_Vertex;
		
		v_control_index = control_index;
		v_control_weight = control_weight;
		vColor = vec3(float(control_index.x)/float(10), 0.0, 0.0);
	}
);

const char *geometry_shader = STRINGIFY(
	layout(points) in;
	layout(line_strip, max_vertices = 64) out;
	
	in vec3 vColor[];
	
	in uvec4 v_control_index[];
	in vec4 v_control_weight[];

	out vec3 fColor;

	uniform isamplerBuffer control_curves;
	uniform samplerBuffer control_points;
	
	void main()
	{
		vec4 root = gl_in[0].gl_Position;
		
		int index0 = int(v_control_index[0].x);
		ivec4 curve0 = texelFetch(control_curves, index0);
		int vert_begin0 = int(curve0.x);
		int num_verts0 = int(curve0.y);
		vec4 root0 = texelFetch(control_points, vert_begin0);
		vec4 offset0 = root - root0;
		
//		fColor = vColor[0];
		fColor = vec3(float(num_verts0)/float(10), 0.0, 0.0);
		
		gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * root;
		EmitVertex();
		
//		gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * (root + vec4(0.1, 0.0, 0.0, 0.0));
//		gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * (root + vec4(float(index0 % 10 + 1) * 0.01, 0.0, 0.0, 0.0));
//		gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * vec4(index0, 0.0, 0.0, 1.0);
//		gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * vec4(v_control_weight[0].xyz, 1.0);
//		EmitVertex();
		
		gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * root0;
		EmitVertex();
		
/*		for (int i = 1; i < num_verts; ++i) {
			vec4 loc = texelFetch(control_points, vert_begin + i);
			
			gl_Position = vec4((loc + offset).xyz, 1.0);
			EmitVertex();
		}*/
		
	    EndPrimitive();
	}
);
	
	const char *fragment_shader = STRINGIFY(
		in vec3 fColor;
		
		out vec4 outColor;
		
		void main()
		{
			outColor = vec4(fColor, 1.0);
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

GPUStrandsShader *GPU_strand_shader_get(struct Strands *strands)
{
	if (strands->gpu_shader != NULL)
		return strands->gpu_shader;
	
	GPUStrandsShader *gpu_shader = MEM_callocN(sizeof(GPUStrandsShader), "GPUStrands");
	
	/* TODO */
	char *vertexcode = codegen_vertex();
	char *geometrycode = codegen_geometry();
	char *fragmentcode = codegen_fragment();
	
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
		gpu_shader->geometrycode = geometrycode;
		gpu_shader->fragmentcode = fragmentcode;
		
		GPU_shader_bind(gpu_shader->shader);
		GPU_shader_uniform_int(shader, GPU_shader_get_uniform(shader, "control_curves"), 0);
		GPU_shader_uniform_int(shader, GPU_shader_get_uniform(shader, "control_points"), 1);
		GPU_shader_unbind();
		
		GPUAttrib *attr;
		
		attr = &gpu_shader->attributes[GPU_STRAND_ATTRIB_POSITION];
		attr->index = -1; /* no explicit attribute, we use gl_Vertex for this */
		attr->info_index = -1;
		attr->type = GL_FLOAT;
		attr->size = 3;
		
		attr = &gpu_shader->attributes[GPU_STRAND_ATTRIB_CONTROL_INDEX];
		attr->index = GPU_shader_get_attribute(gpu_shader->shader, "control_index");
		attr->info_index = -1;
		attr->type = GL_UNSIGNED_INT;
		attr->size = 4;
		
		attr = &gpu_shader->attributes[GPU_STRAND_ATTRIB_CONTROL_WEIGHT];
		attr->index = GPU_shader_get_attribute(gpu_shader->shader, "control_weight");
		attr->info_index = -1;
		attr->type = GL_FLOAT;
		attr->size = 4;
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

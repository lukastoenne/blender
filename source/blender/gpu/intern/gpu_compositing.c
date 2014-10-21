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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Antony Riakiotakis.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_compositing.c
 *  \ingroup gpu
 *
 * System that manages framebuffer compositing.
 */

#include "BLI_sys_types.h"
#include "BLI_rect.h"
#include "BLI_math.h"

#include "DNA_vec_types.h"
#include "DNA_view3d_types.h"

#include "GPU_extensions.h"
#include "GPU_compositing.h"

#include "GL/glew.h"

#include "MEM_guardedalloc.h"

static const float fullscreencos[4][2] = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};
static const float fullscreenuvs[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};

struct GPUFX {
	/* we borrow the term gbuffer from deferred rendering however this is just a regular 
	 * depth/color framebuffer. Could be extended later though */
	GPUFrameBuffer *gbuffer;
	
	/* texture bound to the first color attachment of the gbuffer */
	GPUTexture *color_buffer;

	/* texture bound to the depth attachment of the gbuffer */
	GPUTexture *depth_buffer;
	
	/* dimensions of the gbuffer */
	int gbuffer_dim[2];
	
	/* or-ed flags of enabled effects */
	int effects;
};


/* generate a new FX compositor */
GPUFX *GPU_create_fx_compositor(void)
{
	GPUFX *fx = MEM_callocN(sizeof(GPUFX), "GPUFX compositor");
	
	return fx;
}


static void cleanup_fx_gl_data(GPUFX *fx, bool do_fbo)
{
	if (fx->color_buffer) {
		GPU_framebuffer_texture_detach(fx->gbuffer, fx->color_buffer);
		GPU_texture_free(fx->color_buffer);
		fx->color_buffer = NULL;
	}
	if (fx->depth_buffer) {
		GPU_framebuffer_texture_detach(fx->gbuffer, fx->depth_buffer);
		GPU_texture_free(fx->depth_buffer);
		fx->depth_buffer = NULL;
	}		

	if (fx->gbuffer && do_fbo) {
		GPU_framebuffer_free(fx->gbuffer);
		fx->gbuffer = NULL;
	}
}

/* destroy a text compositor */
void GPU_destroy_fx_compositor(GPUFX *fx)
{
	cleanup_fx_gl_data(fx, true);
	MEM_freeN(fx);
}




bool GPU_initialize_fx_passes(GPUFX *fx, rcti *rect, rcti *scissor_rect, int fxflags)
{
	int w = BLI_rcti_size_x(rect) + 1, h = BLI_rcti_size_y(rect) + 1;
	char err_out[256];

	fx->effects = 0;
	
	if (!fx->gbuffer) 
		fx->gbuffer = GPU_framebuffer_create();
	
	if (!fx->gbuffer) 
		return false;
	
	/* check if color buffers need recreation */
	if (!fx->color_buffer || !fx->depth_buffer || w != fx->gbuffer_dim[0] || h != fx->gbuffer_dim[1]) {
		cleanup_fx_gl_data(fx, false);
		
		if (!(fx->color_buffer = GPU_texture_create_2D(w, h, NULL, err_out))) {
			printf(".256%s\n", err_out);
		}
		
		if (!(fx->depth_buffer = GPU_texture_create_depth(w, h, err_out))) {
			printf("%.256s\n", err_out);
		}
		
		/* in case of failure, cleanup */
		if (!fx->color_buffer || !fx->depth_buffer) {
			cleanup_fx_gl_data(fx, true);
			return false;
		}
		
		fx->gbuffer_dim[0] = w;
		fx->gbuffer_dim[1] = h;		
	}
	
	/* bind the buffers */
	
	/* first depth buffer, because system assumes read/write buffers */
	if(!GPU_framebuffer_texture_attach(fx->gbuffer, fx->depth_buffer, err_out))
		printf("%.256s\n", err_out);
	
	if(!GPU_framebuffer_texture_attach(fx->gbuffer, fx->color_buffer, err_out))
		printf("%.256s\n", err_out);
	
	GPU_framebuffer_texture_bind(fx->gbuffer, fx->color_buffer, 
								 GPU_texture_opengl_width(fx->color_buffer), GPU_texture_opengl_height(fx->color_buffer));

	/* enable scissor test. It's needed to ensure sculpting works correctly */
	if (scissor_rect) {
		int w_sc = BLI_rcti_size_x(scissor_rect) + 1;
		int h_sc = BLI_rcti_size_y(scissor_rect) + 1;
		glPushAttrib(GL_SCISSOR_BIT);
		glEnable(GL_SCISSOR_TEST);
		glScissor(scissor_rect->xmin - rect->xmin, scissor_rect->ymin - rect->ymin, 
				  w_sc, h_sc);
	}
	fx->effects = fxflags;
	
	return true;
}


bool GPU_fx_do_composite_pass(GPUFX *fx, struct View3D *v3d, struct RegionView3D *rv3d) {
	GPUShader *fx_shader;
	int numslots = 0, i;
	
	if (fx->effects == 0)
		return false;
	
	/* first, unbind the render-to-texture framebuffer */
	GPU_framebuffer_texture_unbind(fx->gbuffer, fx->color_buffer);
	glPopAttrib();
	GPU_framebuffer_restore();
	
	/* full screen FX pass */
	
	/* first we need to blur the color pass */
	
	
	/* set up the shader */
	fx_shader = GPU_shader_get_builtin_fx_shader(GPU_SHADER_FX_DEPTH_OF_FIELD);
	if (fx_shader) {
		/* view vectors for the corners of the view frustum. Can be used to recreate the world space position easily */
		float ssao_viewvecs[3][4] = {
		    {-1.0f, -1.0f, -1.0f, 1.0f},
		    {1.0f, -1.0f, -1.0f, 1.0f},
		    {-1.0f, 1.0f, -1.0f, 1.0f}
		};
		int i;
		int screendim_uniform, color_uniform, depth_uniform, dof_uniform, blurred_uniform;
		int ssao_uniform, ssao_color_uniform, ssao_viewvecs_uniform;
		float fac = v3d->dof_fstop * v3d->dof_aperture;
		float dof_params[2] = {v3d->dof_aperture * fabs(fac / (v3d->dof_focal_distance - fac)), 
							   v3d->dof_focal_distance};
		float ssao_params[4] = {v3d->ssao_distance_max, v3d->ssao_darkening, 0.0f, 0.0f};
		float screen_dim[2] = {fx->gbuffer_dim[0], fx->gbuffer_dim[1]};
		
		float invproj[4][4];

		/* invert the view matrix */
		invert_m4_m4(invproj, rv3d->winmat);

		/* convert the view vectors to view space */
		for (i = 0; i < 3; i++) {
			mul_m4_v4(invproj, ssao_viewvecs[i]);
			/* normalized trick see http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
			mul_v3_fl(ssao_viewvecs[i], 1.0f / ssao_viewvecs[i][3]);
			mul_v3_fl(ssao_viewvecs[i], 1.0f / ssao_viewvecs[i][2]);
		}

		/* we need to store the differences */
		ssao_viewvecs[1][0] -= ssao_viewvecs[0][0];
		ssao_viewvecs[1][1] = ssao_viewvecs[2][1] - ssao_viewvecs[0][1];

		dof_uniform = GPU_shader_get_uniform(fx_shader, "dof_params");
		ssao_uniform = GPU_shader_get_uniform(fx_shader, "ssao_params");
		ssao_color_uniform = GPU_shader_get_uniform(fx_shader, "ssao_color");
		blurred_uniform = GPU_shader_get_uniform(fx_shader, "blurredcolorbuffer");
		screendim_uniform = GPU_shader_get_uniform(fx_shader, "screendim");
		color_uniform = GPU_shader_get_uniform(fx_shader, "colorbuffer");
		depth_uniform = GPU_shader_get_uniform(fx_shader, "depthbuffer");
		ssao_viewvecs_uniform = GPU_shader_get_uniform(fx_shader, "ssao_viewvecs");
		
		GPU_shader_bind(fx_shader);
		
		GPU_shader_uniform_vector(fx_shader, screendim_uniform, 2, 1, screen_dim);
		GPU_shader_uniform_vector(fx_shader, dof_uniform, 2, 1, dof_params);
		GPU_shader_uniform_vector(fx_shader, ssao_uniform, 4, 1, ssao_params);
		GPU_shader_uniform_vector(fx_shader, ssao_color_uniform, 4, 1, v3d->ssao_color);
		GPU_shader_uniform_vector(fx_shader, ssao_viewvecs_uniform, 4, 3, ssao_viewvecs[0]);

		GPU_texture_bind(fx->color_buffer, numslots++);
		GPU_shader_uniform_texture(fx_shader, blurred_uniform, fx->color_buffer);
		/* generate mipmaps for the color buffer */
//		glGenerateMipmapEXT(GL_TEXTURE_2D);
//		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
//		glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, 2.0);
		
		GPU_texture_bind(fx->color_buffer, numslots++);
		GPU_shader_uniform_texture(fx_shader, color_uniform, fx->color_buffer);
		
		GPU_texture_bind(fx->depth_buffer, numslots++);
		GPU_depth_texture_mode(fx->depth_buffer, false);
		GPU_shader_uniform_texture(fx_shader, depth_uniform, fx->depth_buffer);
	}
	/* set up quad buffer */
	glVertexPointer(2, GL_FLOAT, 0, fullscreencos);
	glTexCoordPointer(2, GL_FLOAT, 0, fullscreenuvs);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);		
	
	/* set invalid color in case shader fails */
	glColor3f(1.0, 0.0, 1.0);
	
	/* draw */
	glDisable(GL_DEPTH_TEST);
	glDrawArrays(GL_QUADS, 0, 4);
	
	/* disable bindings */
	GPU_texture_unbind(fx->color_buffer);
	GPU_depth_texture_mode(fx->depth_buffer, true);
	GPU_texture_unbind(fx->depth_buffer);

	/* same texture may be bound to more than one slot. Use this to explicitly disable texturing everywhere */
	for (i = numslots; i > 0; i--) {
		glActiveTexture(GL_TEXTURE0 + i - 1);
		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_TEXTURE_2D);
	}
	
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	
	GPU_shader_unbind();
	
	return true;
}

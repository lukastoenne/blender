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
#include "BLI_rand.h"

#include "DNA_vec_types.h"
#include "DNA_view3d_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_camera_types.h"
#include "DNA_gpu_types.h"

#include "GPU_extensions.h"
#include "GPU_compositing.h"

#include "GL/glew.h"

#include "MEM_guardedalloc.h"

static const float fullscreencos[4][2] = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};
static const float fullscreenuvs[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};

static float ssao_sample_directions[16][2];
static bool init = false;

struct GPUFX {
	/* we borrow the term gbuffer from deferred rendering however this is just a regular 
	 * depth/color framebuffer. Could be extended later though */
	GPUFrameBuffer *gbuffer;
	
	/* texture bound to the first color attachment of the gbuffer */
	GPUTexture *color_buffer;

	/* second texture used for ping-pong compositing */
	GPUTexture *color_buffer_sec;

	/* all those buffers below have to coexist. Fortunately they are all quarter sized (1/16th of memory) of original framebuffer */
	float dof_near_w;
	float dof_near_h;

	/* texture used for near coc and color blurring calculation */
	GPUTexture *dof_near_coc_buffer;
	/* blurred near coc buffer. */
	GPUTexture *dof_near_coc_blurred_buffer;
	/* final near coc buffer. */
	GPUTexture *dof_near_coc_final_buffer;

	/* texture bound to the depth attachment of the gbuffer */
	GPUTexture *depth_buffer;

	/* texture used for jittering for various effects */
	GPUTexture *jitter_buffer;

	/* dimensions of the gbuffer */
	int gbuffer_dim[2];
	
	GPUFXOptions options;

	/* or-ed flags of enabled effects */
	int effects;

	/* number of passes, needed to detect if ping pong buffer allocation is needed */
	int num_passes;

	/* we have a stencil, restore the previous state */
	bool restore_stencil;
};


/* generate a new FX compositor */
GPUFX *GPU_create_fx_compositor(void)
{
	GPUFX *fx = MEM_callocN(sizeof(GPUFX), "GPUFX compositor");
	
	return fx;
}

static void cleanup_fx_dof_buffers(GPUFX *fx)
{
	if (fx->dof_near_coc_blurred_buffer) {
		GPU_texture_free(fx->dof_near_coc_blurred_buffer);
		fx->dof_near_coc_blurred_buffer = NULL;
	}
	if (fx->dof_near_coc_buffer) {
		GPU_texture_free(fx->dof_near_coc_buffer);
		fx->dof_near_coc_buffer = NULL;
	}
	if (fx->dof_near_coc_final_buffer) {
		GPU_texture_free(fx->dof_near_coc_final_buffer);
		fx->dof_near_coc_final_buffer = NULL;
	}
}

static void cleanup_fx_gl_data(GPUFX *fx, bool do_fbo)
{
	if (fx->color_buffer) {
		GPU_framebuffer_texture_detach(fx->color_buffer);
		GPU_texture_free(fx->color_buffer);
		fx->color_buffer = NULL;
	}

	if (fx->color_buffer_sec) {
		GPU_framebuffer_texture_detach(fx->color_buffer_sec);
		GPU_texture_free(fx->color_buffer_sec);
		fx->color_buffer_sec = NULL;
	}

	if (fx->depth_buffer) {
		GPU_framebuffer_texture_detach(fx->depth_buffer);
		GPU_texture_free(fx->depth_buffer);
		fx->depth_buffer = NULL;
	}		

	cleanup_fx_dof_buffers(fx);

	if (fx->jitter_buffer && do_fbo) {
		GPU_texture_free(fx->jitter_buffer);
		fx->jitter_buffer = NULL;
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

static GPUTexture * create_jitter_texture (void)
{
	float jitter [64 * 64][2];
	int i;

	for (i = 0; i < 64 * 64; i++) {
		jitter[i][0] = BLI_frand();
		jitter[i][1] = BLI_frand();
		normalize_v2(jitter[i]);
	}

	return GPU_texture_create_2D_procedural(64, 64, &jitter[0][0], NULL);
}

static void create_sample_directions(void)
{
	int i;
	float dir[4][2] = {{1.0f, 0.0f},
	                  {0.0f, 1.0f},
	                  {-1.0f, 0.0f},
	                  {0.0f, -1.0f}};

	for (i = 0; i < 4; i++) {
		copy_v2_v2(ssao_sample_directions[i], dir[i]);
	}

	for (i = 0; i < 4; i++) {
		float mat[2][2];
		rotate_m2(mat, M_PI/4.0);

		copy_v2_v2(ssao_sample_directions[i + 4], ssao_sample_directions[i]);
		mul_m2v2(mat, ssao_sample_directions[i + 4]);
	}

	for (i = 0; i < 8; i++) {
		float mat[2][2];
		rotate_m2(mat, M_PI/8.0);
		copy_v2_v2(ssao_sample_directions[i + 8], ssao_sample_directions[i]);
		mul_m2v2(mat, ssao_sample_directions[i + 8]);
	}

	init = true;
}

bool GPU_initialize_fx_passes(GPUFX *fx, rcti *rect, rcti *scissor_rect, int fxflags, GPUFXOptions *options)
{
	int w = BLI_rcti_size_x(rect) + 1, h = BLI_rcti_size_y(rect) + 1;
	char err_out[256];
	int num_passes = 0;

	fx->effects = 0;

	if (!options) {
		cleanup_fx_gl_data(fx, true);
		return false;
	}

	/* disable effects if no options passed for them */
	if (!options->dof_options) {
		fxflags &= ~GPU_FX_DEPTH_OF_FIELD;
	}
	if (!options->ssao_options) {
		fxflags &= ~GPU_FX_SSAO;
	}

	if (!fxflags) {
		cleanup_fx_gl_data(fx, true);
		return false;
	}
	
	fx->num_passes = 0;
	/* dof really needs a ping-pong buffer to work */
	if (fxflags & GPU_FX_DEPTH_OF_FIELD) {
		num_passes++;
	}
	if (fxflags & GPU_FX_SSAO)
		num_passes++;

	if (!fx->gbuffer) 
		fx->gbuffer = GPU_framebuffer_create();
	
	/* try creating the jitter texture */
	if (!fx->jitter_buffer)
		fx->jitter_buffer = create_jitter_texture();

	if (!fx->gbuffer) 
		return false;
	
	/* check if color buffers need recreation */
	if (!fx->color_buffer || !fx->depth_buffer || w != fx->gbuffer_dim[0] || h != fx->gbuffer_dim[1]) {
		cleanup_fx_gl_data(fx, false);
		
		if (!(fx->color_buffer = GPU_texture_create_2D(w, h, NULL, err_out))) {
			printf(".256%s\n", err_out);
			cleanup_fx_gl_data(fx, true);
			return false;
		}
		
		if (!(fx->depth_buffer = GPU_texture_create_depth(w, h, err_out))) {
			printf("%.256s\n", err_out);
			cleanup_fx_gl_data(fx, true);
			return false;
		}
	}
	
	/* create textures for dof effect */
	if (fxflags & GPU_FX_DEPTH_OF_FIELD) {
		if (!fx->dof_near_coc_buffer || !fx->dof_near_coc_blurred_buffer || !fx->dof_near_coc_final_buffer) {
			fx->dof_near_w = w / 4;
			fx->dof_near_h = h / 4;

			if (!(fx->dof_near_coc_buffer = GPU_texture_create_2D(fx->dof_near_w, fx->dof_near_h, NULL, err_out))) {
				printf("%.256s\n", err_out);
				cleanup_fx_gl_data(fx, true);
				return false;
			}
			if (!(fx->dof_near_coc_blurred_buffer = GPU_texture_create_2D(fx->dof_near_w, fx->dof_near_h, NULL, err_out))) {
				printf("%.256s\n", err_out);
				cleanup_fx_gl_data(fx, true);
				return false;
			}
			if (!(fx->dof_near_coc_final_buffer = GPU_texture_create_2D(fx->dof_near_w, fx->dof_near_h, NULL, err_out))) {
				printf("%.256s\n", err_out);
				cleanup_fx_gl_data(fx, true);
				return false;
			}
		}
	}
	else {
		/* cleanup unnecessary buffers */
		cleanup_fx_dof_buffers(fx);
	}

	/* we need to pass data between shader stages, allocate an extra color buffer */
	if (num_passes > 1) {
		if(!fx->color_buffer_sec) {
			if (!(fx->color_buffer_sec = GPU_texture_create_2D(w, h, NULL, err_out))) {
				printf(".256%s\n", err_out);
				cleanup_fx_gl_data(fx, true);
				return false;
			}
		}
	}
	else {
		if (fx->color_buffer_sec) {
			GPU_framebuffer_texture_detach(fx->color_buffer_sec);
			GPU_texture_free(fx->color_buffer_sec);
			fx->color_buffer_sec = NULL;
		}
	}

	/* bind the buffers */
	
	/* first depth buffer, because system assumes read/write buffers */
	if(!GPU_framebuffer_texture_attach(fx->gbuffer, fx->depth_buffer, 0, err_out))
		printf("%.256s\n", err_out);
	
	if(!GPU_framebuffer_texture_attach(fx->gbuffer, fx->color_buffer, 0, err_out))
		printf("%.256s\n", err_out);
	
	if(!GPU_framebuffer_check_valid(fx->gbuffer, err_out))
		printf("%.256s\n", err_out);
	
	GPU_texture_bind_as_framebuffer(fx->color_buffer);

	/* enable scissor test. It's needed to ensure sculpting works correctly */
	if (scissor_rect) {
		int w_sc = BLI_rcti_size_x(scissor_rect) + 1;
		int h_sc = BLI_rcti_size_y(scissor_rect) + 1;
		glPushAttrib(GL_SCISSOR_BIT);
		glEnable(GL_SCISSOR_TEST);
		glScissor(scissor_rect->xmin - rect->xmin, scissor_rect->ymin - rect->ymin, 
				  w_sc, h_sc);
		fx->restore_stencil = true;
	}
	else {
		fx->restore_stencil = false;
	}

	fx->effects = fxflags;

	if (options)
		fx->options = *options;
	fx->gbuffer_dim[0] = w;
	fx->gbuffer_dim[1] = h;

	fx->num_passes = num_passes;

	return true;
}


bool GPU_fx_do_composite_pass(GPUFX *fx, float projmat[4][4], bool is_persp, struct Scene *scene, struct GPUOffScreen *ofs) {
	GPUTexture *src, *target;
	int numslots = 0;
	float invproj[4][4];
	int i;
	/* number of passes left. when there are no more passes, the result is passed to the frambuffer */
	int passes_left = fx->num_passes;
	/* view vectors for the corners of the view frustum. Can be used to recreate the world space position easily */
	float viewvecs[3][4] = {
	    {-1.0f, -1.0f, -1.0f, 1.0f},
	    {1.0f, -1.0f, -1.0f, 1.0f},
	    {-1.0f, 1.0f, -1.0f, 1.0f}
	};

	if (fx->effects == 0)
		return false;
	
	/* first, unbind the render-to-texture framebuffer */
	GPU_framebuffer_texture_detach(fx->color_buffer);
	GPU_framebuffer_texture_detach(fx->depth_buffer);

	if (fx->restore_stencil)
		glPopAttrib();

	src = fx->color_buffer;
	target = fx->color_buffer_sec;

	/* set up quad buffer */
	glVertexPointer(2, GL_FLOAT, 0, fullscreencos);
	glTexCoordPointer(2, GL_FLOAT, 0, fullscreenuvs);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	/* full screen FX pass */

	/* invert the view matrix */
	invert_m4_m4(invproj, projmat);

	/* convert the view vectors to view space */
	for (i = 0; i < 3; i++) {
		mul_m4_v4(invproj, viewvecs[i]);
		/* normalized trick see http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
		mul_v3_fl(viewvecs[i], 1.0f / viewvecs[i][3]);
		if (is_persp)
			mul_v3_fl(viewvecs[i], 1.0f / viewvecs[i][2]);
		viewvecs[i][3] = 1.0;
	}

	/* we need to store the differences */
	viewvecs[1][0] -= viewvecs[0][0];
	viewvecs[1][1] = viewvecs[2][1] - viewvecs[0][1];

	/* calculate a depth offset as well */
	if (!is_persp) {
		float vec_far[] = {-1.0f, -1.0f, 1.0f, 1.0f};
		mul_m4_v4(invproj, vec_far);
		mul_v3_fl(vec_far, 1.0f / vec_far[3]);
		viewvecs[1][2] = vec_far[2] - viewvecs[0][2];
	}

	/* ssao pass */
	if (fx->effects & GPU_FX_SSAO) {
		GPUShader *ssao_shader;
		ssao_shader = GPU_shader_get_builtin_fx_shader(GPU_SHADER_FX_SSAO, is_persp);
		if (ssao_shader) {
			GPUSSAOOptions *options = fx->options.ssao_options;
			int color_uniform, depth_uniform;
			int ssao_uniform, ssao_color_uniform, viewvecs_uniform, ssao_sample_params_uniform;
			int ssao_jitter_uniform, ssao_direction_uniform;
			float ssao_params[4] = {options->ssao_distance_max, options->ssao_darkening, options->ssao_attenuation, 0.0f};
			float sample_params[4];

			if (!init)
				create_sample_directions();

			switch (options->ssao_ray_sample_mode) {
				case 0:
					sample_params[0] = 4;
					sample_params[1] = 4;
					break;
				case 1:
					sample_params[0] = 8;
					sample_params[1] = 5;
					break;
				case 2:
					sample_params[0] = 16;
					sample_params[1] = 10;
					break;
			}

			/* multiplier so we tile the random texture on screen */
			sample_params[2] = fx->gbuffer_dim[0] / 64.0;
			sample_params[3] = fx->gbuffer_dim[1] / 64.0;

			ssao_uniform = GPU_shader_get_uniform(ssao_shader, "ssao_params");
			ssao_color_uniform = GPU_shader_get_uniform(ssao_shader, "ssao_color");
			color_uniform = GPU_shader_get_uniform(ssao_shader, "colorbuffer");
			depth_uniform = GPU_shader_get_uniform(ssao_shader, "depthbuffer");
			viewvecs_uniform = GPU_shader_get_uniform(ssao_shader, "viewvecs");
			ssao_sample_params_uniform = GPU_shader_get_uniform(ssao_shader, "ssao_sample_params");
			ssao_jitter_uniform = GPU_shader_get_uniform(ssao_shader, "jitter_tex");
			ssao_direction_uniform = GPU_shader_get_uniform(ssao_shader, "sample_directions");

			GPU_shader_bind(ssao_shader);

			GPU_shader_uniform_vector(ssao_shader, ssao_uniform, 4, 1, ssao_params);
			GPU_shader_uniform_vector(ssao_shader, ssao_color_uniform, 4, 1, options->ssao_color);
			GPU_shader_uniform_vector(ssao_shader, viewvecs_uniform, 4, 3, viewvecs[0]);
			GPU_shader_uniform_vector(ssao_shader, ssao_sample_params_uniform, 4, 1, sample_params);
			GPU_shader_uniform_vector(ssao_shader, ssao_direction_uniform, 2, 16, ssao_sample_directions[0]);

			GPU_texture_bind(src, numslots++);
			GPU_shader_uniform_texture(ssao_shader, color_uniform, src);

			GPU_texture_bind(fx->depth_buffer, numslots++);
			GPU_depth_texture_mode(fx->depth_buffer, false, true);
			GPU_shader_uniform_texture(ssao_shader, depth_uniform, fx->depth_buffer);

			GPU_texture_bind(fx->jitter_buffer, numslots++);
			GPU_shader_uniform_texture(ssao_shader, ssao_jitter_uniform, fx->jitter_buffer);

			/* set invalid color in case shader fails */
			glColor3f(1.0, 0.0, 1.0);

			/* draw */
			if (passes_left-- == 1) {
				GPU_framebuffer_texture_unbind(fx->gbuffer, NULL);
				if (ofs) {
					GPU_offscreen_bind(ofs, false);
				}
				else
					GPU_framebuffer_restore();
			}
			else {
				/* bind the ping buffer to the color buffer */
				GPU_framebuffer_texture_attach(fx->gbuffer, target, 0, NULL);
			}
			glDisable(GL_DEPTH_TEST);
			glDrawArrays(GL_QUADS, 0, 4);

			/* disable bindings */
			GPU_texture_unbind(src);
			GPU_depth_texture_mode(fx->depth_buffer, true, false);
			GPU_texture_unbind(fx->depth_buffer);

			/* may not be attached, in that case this just returns */
			if (target) {
				GPU_framebuffer_texture_detach(target);
				if (ofs) {
					GPU_offscreen_bind(ofs, false);
				}
				else {
					GPU_framebuffer_restore();
				}
			}

			/* swap here, after src/target have been unbound */
			SWAP(GPUTexture *, target, src);
			numslots = 0;
		}
	}

	/* second pass, dof */
	if (fx->effects & GPU_FX_DEPTH_OF_FIELD) {
		GPUDOFOptions *options = fx->options.dof_options;
		GPUShader *dof_shader_pass1, *dof_shader_pass2, *dof_shader_pass3, *dof_shader_pass4, *dof_shader_pass5;
		float dof_params[4];
		float scale = scene->unit.system ? scene->unit.scale_length : 1.0f;
		float scale_camera = 0.001f / scale;
		float aperture = 2.0f * scale_camera * options->dof_focal_length / options->dof_fstop; // * v3d->dof_aperture;

		dof_params[0] = aperture * fabs(scale_camera * options->dof_focal_length / (options->dof_focus_distance - scale_camera * options->dof_focal_length));
		dof_params[1] = options->dof_focus_distance;
		dof_params[2] = fx->gbuffer_dim[0] / (scale_camera * options->dof_sensor);
		dof_params[3] = 0.0f;

		/* DOF effect has many passes but most of them are performed on a texture whose dimensions are 4 times less than the original
		 * (16 times lower than original screen resolution). Technique used is not very exact but should be fast enough and is based
		 * on "Practical Post-Process Depth of Field" see http://http.developer.nvidia.com/GPUGems3/gpugems3_ch28.html */
		dof_shader_pass1 = GPU_shader_get_builtin_fx_shader(GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_ONE, is_persp);
		dof_shader_pass2 = GPU_shader_get_builtin_fx_shader(GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_TWO, is_persp);
		dof_shader_pass3 = GPU_shader_get_builtin_fx_shader(GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_THREE, is_persp);
		dof_shader_pass4 = GPU_shader_get_builtin_fx_shader(GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_FOUR, is_persp);
		dof_shader_pass5 = GPU_shader_get_builtin_fx_shader(GPU_SHADER_FX_DEPTH_OF_FIELD_PASS_FIVE, is_persp);

		/* error occured, restore framebuffers and return */
		if (!dof_shader_pass1 || !dof_shader_pass2 || !dof_shader_pass3 || !dof_shader_pass4 || !dof_shader_pass5) {
			GPU_framebuffer_texture_unbind(fx->gbuffer, NULL);
			GPU_framebuffer_restore();
			return false;
		}

		/* pass first, first level of blur in low res buffer */
		{
			int invrendertargetdim_uniform, color_uniform, depth_uniform, dof_uniform;
			int viewvecs_uniform;

			float invrendertargetdim[2] = {1.0f / fx->gbuffer_dim[0], 1.0f / fx->gbuffer_dim[1]};

			dof_uniform = GPU_shader_get_uniform(dof_shader_pass1, "dof_params");
			invrendertargetdim_uniform = GPU_shader_get_uniform(dof_shader_pass1, "invrendertargetdim");
			color_uniform = GPU_shader_get_uniform(dof_shader_pass1, "colorbuffer");
			depth_uniform = GPU_shader_get_uniform(dof_shader_pass1, "depthbuffer");
			viewvecs_uniform = GPU_shader_get_uniform(dof_shader_pass1, "viewvecs");

			GPU_shader_bind(dof_shader_pass1);

			GPU_shader_uniform_vector(dof_shader_pass1, dof_uniform, 4, 1, dof_params);
			GPU_shader_uniform_vector(dof_shader_pass1, invrendertargetdim_uniform, 2, 1, invrendertargetdim);
			GPU_shader_uniform_vector(dof_shader_pass1, viewvecs_uniform, 4, 3, viewvecs[0]);

			GPU_texture_bind(src, numslots++);
			GPU_shader_uniform_texture(dof_shader_pass1, color_uniform, src);

			GPU_texture_bind(fx->depth_buffer, numslots++);
			GPU_depth_texture_mode(fx->depth_buffer, false, true);
			GPU_shader_uniform_texture(dof_shader_pass1, depth_uniform, fx->depth_buffer);

			/* target is the downsampled coc buffer */
			GPU_framebuffer_texture_attach(fx->gbuffer, fx->dof_near_coc_buffer, 0, NULL);
			/* binding takes care of setting the viewport to the downsampled size */
			GPU_texture_bind_as_framebuffer(fx->dof_near_coc_buffer);

			glDisable(GL_DEPTH_TEST);
			glDrawArrays(GL_QUADS, 0, 4);
			/* disable bindings */
			GPU_texture_unbind(src);
			GPU_depth_texture_mode(fx->depth_buffer, true, false);
			GPU_texture_unbind(fx->depth_buffer);

			GPU_framebuffer_texture_detach(fx->dof_near_coc_buffer);
			numslots = 0;
		}

		/* second pass, gaussian blur the downsampled image */
		{
			int invrendertargetdim_uniform, color_uniform, depth_uniform, dof_uniform;
			int viewvecs_uniform;
			float invrendertargetdim[2] = {1.0f / GPU_texture_opengl_width(fx->dof_near_coc_blurred_buffer),
			                               1.0f / GPU_texture_opengl_height(fx->dof_near_coc_blurred_buffer)};
			float tmp = invrendertargetdim[0];
			invrendertargetdim[0] = 0.0f;

			dof_params[2] = GPU_texture_opengl_width(fx->dof_near_coc_blurred_buffer) / (scale_camera * options->dof_sensor);

			dof_uniform = GPU_shader_get_uniform(dof_shader_pass2, "dof_params");
			invrendertargetdim_uniform = GPU_shader_get_uniform(dof_shader_pass2, "invrendertargetdim");
			color_uniform = GPU_shader_get_uniform(dof_shader_pass2, "colorbuffer");
			depth_uniform = GPU_shader_get_uniform(dof_shader_pass2, "depthbuffer");
			viewvecs_uniform = GPU_shader_get_uniform(dof_shader_pass2, "viewvecs");

			/* Blurring vertically */
			GPU_shader_bind(dof_shader_pass2);

			GPU_shader_uniform_vector(dof_shader_pass2, dof_uniform, 4, 1, dof_params);
			GPU_shader_uniform_vector(dof_shader_pass2, invrendertargetdim_uniform, 2, 1, invrendertargetdim);
			GPU_shader_uniform_vector(dof_shader_pass2, viewvecs_uniform, 4, 3, viewvecs[0]);

			GPU_texture_bind(fx->depth_buffer, numslots++);
			GPU_depth_texture_mode(fx->depth_buffer, false, true);
			GPU_shader_uniform_texture(dof_shader_pass2, depth_uniform, fx->depth_buffer);

			GPU_texture_bind(fx->dof_near_coc_buffer, numslots++);
			GPU_shader_uniform_texture(dof_shader_pass2, color_uniform, fx->dof_near_coc_buffer);

			/* use final buffer as a temp here */
			GPU_framebuffer_texture_attach(fx->gbuffer, fx->dof_near_coc_final_buffer, 0, NULL);

			/* Drawing quad */
			glDrawArrays(GL_QUADS, 0, 4);

			/* *unbind/detach */
			GPU_texture_unbind(fx->dof_near_coc_buffer);
			GPU_framebuffer_texture_detach(fx->dof_near_coc_final_buffer);

			/* Blurring horizontally */
			invrendertargetdim[0] = tmp;
			invrendertargetdim[1] = 0.0f;
			GPU_shader_uniform_vector(dof_shader_pass2, invrendertargetdim_uniform, 2, 1, invrendertargetdim);

			GPU_texture_bind(fx->dof_near_coc_final_buffer, numslots++);
			GPU_shader_uniform_texture(dof_shader_pass2, color_uniform, fx->dof_near_coc_final_buffer);

			GPU_framebuffer_texture_attach(fx->gbuffer, fx->dof_near_coc_blurred_buffer, 0, NULL);
			glDrawArrays(GL_QUADS, 0, 4);

			/* *unbind/detach */
			GPU_depth_texture_mode(fx->depth_buffer, true, false);
			GPU_texture_unbind(fx->depth_buffer);

			GPU_texture_unbind(fx->dof_near_coc_final_buffer);
			GPU_framebuffer_texture_detach(fx->dof_near_coc_blurred_buffer);

			dof_params[2] = fx->gbuffer_dim[0] / (scale_camera * options->dof_sensor);

			numslots = 0;
		}

		/* third pass, calculate near coc */
		{
			int near_coc_downsampled, near_coc_blurred;

			near_coc_downsampled = GPU_shader_get_uniform(dof_shader_pass3, "colorbuffer");
			near_coc_blurred = GPU_shader_get_uniform(dof_shader_pass3, "blurredcolorbuffer");

			GPU_shader_bind(dof_shader_pass3);

			GPU_texture_bind(fx->dof_near_coc_buffer, numslots++);
			GPU_shader_uniform_texture(dof_shader_pass3, near_coc_downsampled, fx->dof_near_coc_buffer);

			GPU_texture_bind(fx->dof_near_coc_blurred_buffer, numslots++);
			GPU_shader_uniform_texture(dof_shader_pass3, near_coc_blurred, fx->dof_near_coc_blurred_buffer);

			GPU_framebuffer_texture_attach(fx->gbuffer, fx->dof_near_coc_final_buffer, 0, NULL);

			glDisable(GL_DEPTH_TEST);
			glDrawArrays(GL_QUADS, 0, 4);
			/* disable bindings */
			GPU_texture_unbind(fx->dof_near_coc_buffer);
			GPU_texture_unbind(fx->dof_near_coc_blurred_buffer);

			/* unbinding here restores the size to the original */
			GPU_framebuffer_texture_detach(fx->dof_near_coc_final_buffer);

			numslots = 0;
		}

		/* fourth pass blur final coc once to eliminate discontinuities */
		{
			int near_coc_downsampled;
			int invrendertargetdim_uniform;
			float invrendertargetdim[2] = {1.0f / GPU_texture_opengl_width(fx->dof_near_coc_blurred_buffer),
			                               1.0f / GPU_texture_opengl_height(fx->dof_near_coc_blurred_buffer)};

			near_coc_downsampled = GPU_shader_get_uniform(dof_shader_pass4, "colorbuffer");
			invrendertargetdim_uniform = GPU_shader_get_uniform(dof_shader_pass4, "invrendertargetdim");

			GPU_shader_bind(dof_shader_pass4);

			GPU_texture_bind(fx->dof_near_coc_final_buffer, numslots++);
			GPU_shader_uniform_texture(dof_shader_pass4, near_coc_downsampled, fx->dof_near_coc_final_buffer);
			GPU_shader_uniform_vector(dof_shader_pass4, invrendertargetdim_uniform, 2, 1, invrendertargetdim);

			GPU_framebuffer_texture_attach(fx->gbuffer, fx->dof_near_coc_buffer, 0, NULL);

			glDisable(GL_DEPTH_TEST);
			glDrawArrays(GL_QUADS, 0, 4);
			/* disable bindings */
			GPU_texture_unbind(fx->dof_near_coc_final_buffer);

			/* unbinding here restores the size to the original */
			GPU_framebuffer_texture_unbind(fx->gbuffer, fx->dof_near_coc_buffer);
			GPU_framebuffer_texture_detach(fx->dof_near_coc_buffer);

			numslots = 0;
		}

		/* final pass, merge blurred layers according to final calculated coc */
		{
			int medium_blurred_uniform, high_blurred_uniform, original_uniform, depth_uniform, dof_uniform;
			int invrendertargetdim_uniform, viewvecs_uniform;
			float invrendertargetdim[2] = {1.0f / fx->gbuffer_dim[0], 1.0f / fx->gbuffer_dim[1]};

			medium_blurred_uniform = GPU_shader_get_uniform(dof_shader_pass5, "mblurredcolorbuffer");
			high_blurred_uniform = GPU_shader_get_uniform(dof_shader_pass5, "blurredcolorbuffer");
			dof_uniform = GPU_shader_get_uniform(dof_shader_pass5, "dof_params");
			invrendertargetdim_uniform = GPU_shader_get_uniform(dof_shader_pass5, "invrendertargetdim");
			original_uniform = GPU_shader_get_uniform(dof_shader_pass5, "colorbuffer");
			depth_uniform = GPU_shader_get_uniform(dof_shader_pass5, "depthbuffer");
			viewvecs_uniform = GPU_shader_get_uniform(dof_shader_pass5, "viewvecs");

			GPU_shader_bind(dof_shader_pass5);

			GPU_shader_uniform_vector(dof_shader_pass5, dof_uniform, 4, 1, dof_params);
			GPU_shader_uniform_vector(dof_shader_pass5, invrendertargetdim_uniform, 2, 1, invrendertargetdim);
			GPU_shader_uniform_vector(dof_shader_pass5, viewvecs_uniform, 4, 3, viewvecs[0]);

			GPU_texture_bind(src, numslots++);
			GPU_shader_uniform_texture(dof_shader_pass5, original_uniform, src);

			GPU_texture_bind(fx->dof_near_coc_blurred_buffer, numslots++);
			GPU_shader_uniform_texture(dof_shader_pass5, high_blurred_uniform, fx->dof_near_coc_blurred_buffer);

			GPU_texture_bind(fx->dof_near_coc_buffer, numslots++);
			GPU_shader_uniform_texture(dof_shader_pass5, medium_blurred_uniform, fx->dof_near_coc_buffer);

			GPU_texture_bind(fx->depth_buffer, numslots++);
			GPU_depth_texture_mode(fx->depth_buffer, false, true);
			GPU_shader_uniform_texture(dof_shader_pass5, depth_uniform, fx->depth_buffer);

			/* if this is the last pass, prepare for rendering on the frambuffer */
			if (passes_left-- == 1) {
				GPU_framebuffer_texture_unbind(fx->gbuffer, NULL);
				if (ofs) {
					GPU_offscreen_bind(ofs, false);
				}
				else
					GPU_framebuffer_restore();
			}
			else {
				/* bind the ping buffer to the color buffer */
				GPU_framebuffer_texture_attach(fx->gbuffer, target, 0, NULL);
			}
			glDisable(GL_DEPTH_TEST);
			glDrawArrays(GL_QUADS, 0, 4);
			/* disable bindings */
			GPU_texture_unbind(fx->dof_near_coc_buffer);
			GPU_texture_unbind(fx->dof_near_coc_blurred_buffer);
			GPU_texture_unbind(src);
			GPU_depth_texture_mode(fx->depth_buffer, true, false);
			GPU_texture_unbind(fx->depth_buffer);

			/* may not be attached, in that case this just returns */
			if (target) {
				GPU_framebuffer_texture_detach(target);
				if (ofs) {
					GPU_offscreen_bind(ofs, false);
				}
				else {
					GPU_framebuffer_restore();
				}
			}

			SWAP(GPUTexture *, target, src);
			numslots = 0;
		}
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	
	GPU_shader_unbind();

	return true;
}

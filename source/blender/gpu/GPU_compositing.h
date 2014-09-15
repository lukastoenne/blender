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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Antony Riakiotakis.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file GPU_compositing.h
 *  \ingroup gpu
 */

#ifndef __GPU_COMPOSITING_H__
#define __GPU_COMPOSITING_H__

/* opaque handle for framebuffer compositing effects (defined in gpu_compositing.c )*/
typedef struct GPUFX GPUFX;

struct View3D;
struct rcti;

/**** Public API *****/

typedef enum GPUFXShaderEffect {
	GPU_SHADER_FX_SSAO           =	(1<<0),
	GPU_SHADER_FX_DEPTH_OF_FIELD =	(1<<1),
	GPU_SHADER_FX_MAX            =	(1<<2),
} GPUFXShaderEffect;

#define MAX_FX_SHADERS (GPU_SHADER_FX_MAX - 1)

/* generate a new FX compositor */
GPUFX *GPU_create_fx_compositor(void);

/* destroy a text compositor */
void GPU_destroy_fx_compositor(GPUFX *fx);

/* initialize a framebuffer with size taken from the viewport */
bool GPU_initialize_fx_passes(GPUFX *fx, struct rcti *rect, int fxflags);

/* do compositing on the fx passes that have been initialized */
bool GPU_fx_do_composite_pass(GPUFX *fx, struct View3D *v3d);

#endif // __GPU_COMPOSITING_H__

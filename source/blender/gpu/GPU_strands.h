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

/** \file GPU_strands.h
 *  \ingroup gpu
 */

#ifndef __GPU_STRANDS_H__
#define __GPU_STRANDS_H__

#ifdef __cplusplus
extern "C" {
#endif

struct Strands;

typedef struct GPUStrands GPUStrands;

GPUStrands *GPU_strands_get(struct Strands *strands);

void GPU_strands_free(struct GPUStrands *gpu_strands);

void GPU_strands_bind(
        GPUStrands *gpu_strands,
        float viewmat[4][4], float viewinv[4][4]);
void GPU_strands_bind_uniforms(
        GPUStrands *gpu_strands,
        float obmat[4][4], float viewmat[4][4]);
void GPU_strands_unbind(GPUStrands *gpu_strands);
bool GPU_strands_bound(GPUStrands *gpu_strands);

#ifdef __cplusplus
}
#endif

#endif /*__GPU_MATERIAL_H__*/

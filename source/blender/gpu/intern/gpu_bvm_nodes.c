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

/** \file blender/gpu/intern/gpu_bvm_nodes.c
 *  \ingroup gpu
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "intern/gpu_bvm_nodes.h"

extern char datatoc_gpu_shader_bvm_nodes_base_glsl[];
extern char datatoc_gpu_shader_bvm_nodes_math_glsl[];

char *glsl_bvm_nodes_library = NULL;

static char *codegen_libcode(void)
{
#define NUMPARTS 2
	const char *parts[NUMPARTS] = {
	    datatoc_gpu_shader_bvm_nodes_base_glsl,
	    datatoc_gpu_shader_bvm_nodes_math_glsl,
	};
	size_t len[NUMPARTS];
	size_t totlen = 1; /* includes terminator char */
	for (int i = 0; i < NUMPARTS; ++i) {
		len[i] = strlen(parts[i]);
		totlen += len[i];
	}
	
	char *buf = MEM_mallocN(sizeof(char) * totlen, "strand shader libcode");
	char *s = buf;
	for (int i = 0; i < NUMPARTS; ++i) {
		strcpy(s, parts[i]);
		s += len[i];
	}
	
	return buf;
#undef NUMPARTS
}

void gpu_bvm_nodes_init(void)
{
	glsl_bvm_nodes_library = codegen_libcode();
}

void gpu_bvm_nodes_exit(void)
{
	if (glsl_bvm_nodes_library)
		MEM_freeN(glsl_bvm_nodes_library);
}

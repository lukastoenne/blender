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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Antony Riakiotakis.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __GPU_RENDERER_H__
#define __GPU_RENDERER_H__

#include "DNA_listBase.h"

struct Object;

/* batches per material sorted by some criterion, ie texture image changes */
typedef struct SubBatch {
	int start;
	int len;
	void *data; /* extra data relevant to the subbatch, such as image */
} SubBatch;


/* stores data related to the object that can be used for rendering */
typedef struct MaterialBatch {
	int start;
	int len;
	ListBase subbatches;
	struct Object *ob;
	void *data;
} MaterialBatch;


/* new type of material */
typedef struct GPUMaterialX {
	int datarequest; /* type of data requested from objects */
} GPUMaterialX;

void GPU_renderer_material_draw(struct ListBase *materials);

#endif // __GPU_RENDERER_H__

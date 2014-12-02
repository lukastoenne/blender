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

/** \file blender/editors/hair/hair_edit.c
 *  \ingroup edhair
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "DNA_brush_types.h"
#include "DNA_scene_types.h"

#include "BKE_edithair.h"

#include "bmesh.h"

#include "hair_intern.h"

typedef void (*VertexToolCb)(HairToolData *data, BMVert *v, float factor);

BLI_INLINE float hair_tool_filter_vertex(HairToolData *data, BMVert *v)
{
	return 1.0f; // TODO
}

static int hair_tool_apply_vertex(HairToolData *data, VertexToolCb cb)
{
	const float threshold = 0.0f; /* XXX could be useful, is it needed? */
	
	BMVert *v;
	BMIter iter;
	int tot = 0;
	
	BM_ITER_MESH(v, &iter, data->edit->bm, BM_VERTS_OF_MESH) {
		float factor = hair_tool_filter_vertex(data, v);
		if (factor > threshold) {
			cb(data, v, factor);
			++tot;
		}
	}
	
	return tot;
}

/* ------------------------------------------------------------------------- */

static void hair_vertex_comb(HairToolData *data, BMVert *v, float factor)
{
	madd_v3_v3fl(v->co, data->delta, factor);
}

bool hair_brush_step(HairToolData *data)
{
	Brush *brush = data->settings->brush;
	BrushHairTool hair_tool = brush->hair_tool;
	int tot = 0;
	
	switch (hair_tool) {
		case HAIR_TOOL_COMB:    tot = hair_tool_apply_vertex(data, hair_vertex_comb);       break;
		case HAIR_TOOL_CUT:     break;
		case HAIR_TOOL_LENGTH:  break;
		case HAIR_TOOL_PUFF:    break;
		case HAIR_TOOL_ADD:     break;
		case HAIR_TOOL_SMOOTH:  break;
		case HAIR_TOOL_WEIGHT:  break;
	}
	
	return tot > 0;
}

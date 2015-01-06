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

/** \file blender/blenkernel/intern/hair_flow.c
 *  \ingroup bph
 */

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_effect.h"
}

#include "BPH_strands.h"

#include "implicit.h"
#include "eigen_utils.h"

struct HairFlowData {
};

HairFlowData *BPH_strands_solve_hair_flow(Scene *scene, Object *ob)
{
	return NULL;
}

void BPH_strands_free_hair_flow(HairFlowData *data)
{
}

void BPH_strands_sample_hair_flow(Object *ob, BMEditStrands *edit, HairFlowData *data)
{
	
}

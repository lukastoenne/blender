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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/mesh_sample.c
 *  \ingroup bke
 *
 * Sample a mesh surface or volume and evaluate samples on deformed meshes.
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_rand.h"

#include "BKE_mesh_sample.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"

#include "BLI_strict_flags.h"

/* Evaluate */


/* Iterators */

#if 0
static void mesh_sample_surface_array_iterator_next(MSurfaceSampleArrayIterator *iter)
{
	++iter->cur;
	--iter->remaining;
}

static bool mesh_sample_surface_array_iterator_valid(MSurfaceSampleArrayIterator *iter)
{
	return (iter->remaining > 0);
}

static void mesh_sample_surface_array_iterator_free(MSurfaceSampleArrayIterator *iter)
{
}

void BKE_mesh_sample_surface_array_begin(MSurfaceSampleArrayIterator *iter, MSurfaceSample *array, int totarray)
{
	iter->cur = array;
	iter->remaining = totarray;
	
	iter->base.next = 
}
#endif


/* Sampling */

static mesh_sample_surface_uniform(const MSurfaceSampleInfo *info, MSurfaceSample *sample, RNG *rng)
{
	sample->orig_face = BLI_rng_get_int(rng) % info->dm->getNumTessFaces(info->dm);
	sample->orig_weights = 
}

void BKE_mesh_sample_surface_array(const MSurfaceSampleInfo *info, MSurfaceSample *samples, int totsample)
{
	RNG *rng = BLI_rng_new(info->seed);
	
}

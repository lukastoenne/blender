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

/** \file blender/editors/hair/edithair.c
 *  \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_edithair.h"

HairEditData *BKE_edithair_create(void)
{
	HairEditData *hedit = MEM_callocN(sizeof(HairEditData), "hair edit data");
	return hedit;
}

HairEditData *BKE_edithair_copy(HairEditData *hedit)
{
	HairEditData *thedit = MEM_dupallocN(hedit);
	
	if (hedit->curves) {
		thedit->curves = MEM_dupallocN(hedit->curves);
	}
	
	if (hedit->verts) {
		thedit->verts = MEM_dupallocN(hedit->verts);
	}
	
	return thedit;
}

void BKE_edithair_free(HairEditData *hedit)
{
	if (hedit->curves) {
		MEM_freeN(hedit->curves);
	}
	
	if (hedit->verts) {
		MEM_freeN(hedit->verts);
	}
	
	MEM_freeN(hedit);
}

void BKE_edithair_clear(HairEditData *hedit)
{
	if (hedit->curves) {
		MEM_freeN(hedit->curves);
		hedit->curves = NULL;
	}
	hedit->totcurves = 0;
	hedit->alloc_curves = 0;
	
	if (hedit->verts) {
		MEM_freeN(hedit->verts);
		hedit->verts = NULL;
	}
	hedit->totverts = 0;
	hedit->alloc_verts = 0;
}

void BKE_edithair_reserve(HairEditData *hedit, int alloc_curves, int alloc_verts, bool shrink)
{
	if (!hedit)
		return;
	
	if ((alloc_curves > hedit->alloc_curves) || (alloc_curves < hedit->alloc_curves && shrink)) {
		size_t size_curves = sizeof(HairEditCurve) * alloc_curves;
		if (hedit->curves)
			hedit->curves = MEM_recallocN_id(hedit->curves, size_curves, "hair edit curves");
		else
			hedit->curves = MEM_callocN(size_curves, "hair edit curves");
		hedit->alloc_curves = alloc_curves;
		CLAMP_MAX(hedit->totcurves, alloc_curves);
	}
	
	if ((alloc_verts > hedit->alloc_verts) || (alloc_verts < hedit->alloc_verts && shrink)) {
		size_t size_verts = sizeof(HairEditVertex) * alloc_verts;
		if (hedit->verts)
			hedit->verts = MEM_recallocN_id(hedit->verts, size_verts, "hair edit verts");
		else
			hedit->verts = MEM_callocN(size_verts, "hair edit verts");
		hedit->alloc_verts = alloc_verts;
		CLAMP_MAX(hedit->totverts, alloc_verts);
	}
}

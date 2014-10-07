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

#include "MEM_guardedalloc.h"

#include "hair_intern.h"

HairEditData *hair_edit_create(int totcurves, int totverts)
{
	HairEditData *hedit = MEM_callocN(sizeof(HairEditData), "hair edit data");
	
	hedit->curves = MEM_callocN(sizeof(HairEditCurve) * totcurves, "hair edit curves");
	hedit->totcurves = totcurves;
	
	hedit->verts = MEM_callocN(sizeof(HairEditVertex) * totverts, "hair edit verts");
	hedit->totverts = totverts;
	
	return hedit;
}

HairEditData *hair_edit_copy(HairEditData *hedit)
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

void hair_edit_free(HairEditData *hedit)
{
	if (hedit->curves) {
		MEM_freeN(hedit->curves);
	}
	
	if (hedit->verts) {
		MEM_freeN(hedit->verts);
	}
	
	MEM_freeN(hedit);
}

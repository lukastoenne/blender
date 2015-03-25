/*
 * Copyright 2015, Blender Foundation.
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
 */

#include "MEM_guardedalloc.h"

#include "BKE_strands.h"

Strands *BKE_strands_new(int curves, int verts)
{
	Strands *s = MEM_mallocN(sizeof(Strands), "strands");
	
	s->totcurves = curves;
	s->curves = MEM_mallocN(sizeof(StrandsCurve) * curves, "strand curves");
	
	s->totverts = verts;
	s->verts = MEM_mallocN(sizeof(StrandsVertex) * verts, "strand vertices");
	
	return s;
}

void BKE_strands_free(Strands *strands)
{
	if (strands) {
		if (strands->curves)
			MEM_freeN(strands->curves);
		if (strands->verts)
			MEM_freeN(strands->verts);
		MEM_freeN(strands);
	}
}

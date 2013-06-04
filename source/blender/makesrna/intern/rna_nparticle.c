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
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_nparticle.c
 *  \ingroup RNA
 */

#include "DNA_nparticle_types.h"

#include "RNA_define.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

#include "BKE_nparticle.h"


#else

static void rna_def_nparticle_buffer(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "NParticleBuffer", NULL);
	RNA_def_struct_ui_text(srna, "Particle Buffer", "Container for particles");
}

void RNA_def_nparticle(BlenderRNA *brna)
{
	rna_def_nparticle_buffer(brna);
}

#endif

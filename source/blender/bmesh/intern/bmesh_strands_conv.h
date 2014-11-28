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
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_STRANDS_CONV_H__
#define __BMESH_STRANDS_CONV_H__

/** \file blender/bmesh/intern/bmesh_strands_conv.h
 *  \ingroup bmesh
 */

struct BMesh;
struct Mesh;
struct ParticleSystem;

extern const char *CD_PSYS_MASS;
extern const char *CD_PSYS_WEIGHT;

void BM_strands_cd_validate(struct BMesh *bm);
void BM_strands_cd_flag_ensure(struct BMesh *bm, struct ParticleSystem *psys, const char cd_flag);
void BM_strands_cd_flag_apply(struct BMesh *bm, const char cd_flag);
char BM_strands_cd_flag_from_bmesh(struct BMesh *bm);

int BM_strands_count_psys_keys(struct ParticleSystem *psys);
void BM_strands_bm_from_psys(struct BMesh *bm, struct ParticleSystem *psys,
                             const bool set_key, int act_key_nr);
void BM_strands_bm_to_psys(struct BMesh *bm, struct ParticleSystem *psys);

#define BMALLOC_TEMPLATE_FROM_PSYS(psys) { (CHECK_TYPE_INLINE(psys, ParticleSystem *), \
	BM_strands_count_psys_keys(psys)), (BM_strands_count_psys_keys(psys) - (psys)->totpart), 0, 0 }

#endif /* __BMESH_STRANDS_CONV_H__ */

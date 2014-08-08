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
 * Contributor(s): Blender Foundation,
 *                 Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_HAIR_H__
#define __BKE_HAIR_H__

/** \file BKE_hair.h
 *  \ingroup bke
 */

struct HairSystem;
struct HairCurve;
struct HairPoint;
struct HairDebugData;

struct HairSystem *BKE_hairsys_new(void);
void BKE_hairsys_free(struct HairSystem *hsys);
struct HairSystem *BKE_hairsys_copy(struct HairSystem *hsys);

struct HairCurve *BKE_hair_curve_add(struct HairSystem *hsys);
struct HairCurve *BKE_hair_curve_add_multi(struct HairSystem *hsys, int num);
void BKE_hair_curve_remove(struct HairSystem *hsys, struct HairCurve *hair);

struct HairPoint *BKE_hair_point_append(struct HairSystem *hsys, struct HairCurve *hair);
struct HairPoint *BKE_hair_point_append_multi(struct HairSystem *hsys, struct HairCurve *hair, int num);
struct HairPoint *BKE_hair_point_insert(struct HairSystem *hsys, struct HairCurve *hair, int pos);
struct HairPoint *BKE_hair_point_insert_multi(struct HairSystem *hsys, struct HairCurve *hair, int pos, int num);
void BKE_hair_point_remove(struct HairSystem *hsys, struct HairCurve *hair, struct HairPoint *point);
void BKE_hair_point_remove_position(struct HairSystem *hsys, struct HairCurve *hair, int pos);

void BKE_hair_debug_data_free(struct HairDebugData *debug_data);

#endif

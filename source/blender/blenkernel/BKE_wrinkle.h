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

/** \file BKE_wrinkle.h
 *  \ingroup blenkernel
 *  \brief Supporting functions for the wrinkle modifier
 */

#ifndef __BKE_WRINKLE_H__
#define __BKE_WRINKLE_H__

struct DerivedMesh;
struct Object;
struct WrinkleModifierData;
struct WrinkleMapSettings;

struct WrinkleMapSettings *BKE_wrinkle_map_add(struct WrinkleModifierData *wmd);
void BKE_wrinkle_map_remove(struct WrinkleModifierData *wmd, struct WrinkleMapSettings *map);
void BKE_wrinkle_maps_clear(struct WrinkleModifierData *wmd);
void BKE_wrinkle_map_move(struct WrinkleModifierData *wmd, int from_index, int to_index);

void BKE_wrinkle_apply(struct Object *ob, struct WrinkleModifierData *wmd, struct DerivedMesh *dm, const float (*orco)[3]);

void BKE_wrinkle_coeff_calc(struct Object *ob, struct WrinkleModifierData *wmd);
void BKE_wrinkle_coeff_free(struct WrinkleModifierData *wmd);
bool BKE_wrinkle_has_coeff(struct WrinkleModifierData *wmd);

#endif /* __BKE_WRINKLE_H__ */

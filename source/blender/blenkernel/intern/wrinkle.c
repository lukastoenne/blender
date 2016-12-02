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

/** \file wrinkle.c
 *  \ingroup blenkernel
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"

#include "DNA_modifier_types.h"
#include "DNA_texture_types.h"

#include "BKE_library.h"
#include "BKE_wrinkle.h"

static WrinkleMapSettings *wrinkle_map_create(Tex *texture)
{
	WrinkleMapSettings *map = MEM_callocN(sizeof(WrinkleMapSettings), "WrinkleMapSettings");
	
	if (texture) {
		map->texture = texture;
		id_us_plus(&texture->id);
	}
	
	return map;
}

static void wrinkle_map_free(WrinkleMapSettings *map)
{
	if (map->texture) {
		id_us_min(&map->texture->id);
	}
	
	MEM_freeN(map);
}

WrinkleMapSettings *BKE_wrinkle_map_add(WrinkleModifierData *wmd)
{
	WrinkleMapSettings *map = wrinkle_map_create(NULL);
	
	BLI_addtail(&wmd->wrinkle_maps, map);
	return map;
}

void BKE_wrinkle_map_remove(WrinkleModifierData *wmd, WrinkleMapSettings *map)
{
	BLI_assert(BLI_findindex(&wmd->wrinkle_maps, map) != -1);
	
	BLI_remlink(&wmd->wrinkle_maps, map);
	
	wrinkle_map_free(map);
}

void BKE_wrinkle_maps_clear(WrinkleModifierData *wmd)
{
	WrinkleMapSettings *map_next;
	for (WrinkleMapSettings *map = wmd->wrinkle_maps.first; map; map = map_next) {
		map_next = map->next;
		
		wrinkle_map_free(map);
	}
	BLI_listbase_clear(&wmd->wrinkle_maps);
}

void BKE_wrinkle_map_move(WrinkleModifierData *wmd, int from_index, int to_index)
{
	BLI_assert(from_index >= 0 && from_index < BLI_listbase_count(&wmd->wrinkle_maps));
	BLI_assert(to_index >= 0 && to_index < BLI_listbase_count(&wmd->wrinkle_maps));
	
	WrinkleMapSettings *map = BLI_findlink(&wmd->wrinkle_maps, from_index);
	WrinkleMapSettings *map_next = BLI_findlink(&wmd->wrinkle_maps, to_index);
	if (to_index >= from_index)
		map_next = map_next->next;
	
	BLI_remlink(&wmd->wrinkle_maps, map);
	BLI_insertlinkbefore(&wmd->wrinkle_maps, map_next, map);
}

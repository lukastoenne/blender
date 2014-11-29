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

/** \file blender/editors/hair/hair_ops.c
 *  \ingroup edhair
 */

#include "BLI_utildefines.h"

#include "DNA_object_types.h"

#include "BKE_context.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_physics.h"

#include "hair_intern.h"
#include "paint_intern.h"

void ED_operatortypes_hair(void)
{
	WM_operatortype_append(HAIR_OT_hair_edit_toggle);
	
	WM_operatortype_append(HAIR_OT_stroke);
}

static int hair_poll(bContext *C)
{
	if (hair_edit_toggle_poll(C))
		if (CTX_data_active_object(C)->mode & OB_MODE_HAIR_EDIT)
			return true;
	
	return false;
}

void ED_keymap_hair(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;
	
	keymap = WM_keymap_find(keyconf, "Hair", 0, 0);
	keymap->poll = hair_poll;
	
	kmi = WM_keymap_add_item(keymap, "HAIR_OT_brush", LEFTMOUSE, KM_PRESS, 0,        0);
	RNA_enum_set(kmi->ptr, "mode", BRUSH_STROKE_NORMAL);
}

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

static void ed_keymap_hair_brush_switch(wmKeyMap *keymap, const char *mode)
{
	wmKeyMapItem *kmi;
	int i;
	/* index 0-9 (zero key is tenth), shift key for index 10-19 */
	for (i = 0; i < 20; i++) {
		kmi = WM_keymap_add_item(keymap, "BRUSH_OT_active_index_set",
		                         ZEROKEY + ((i + 1) % 10), KM_PRESS, i < 10 ? 0 : KM_SHIFT, 0);
		RNA_string_set(kmi->ptr, "mode", mode);
		RNA_int_set(kmi->ptr, "index", i);
	}
}

static void ed_keymap_hair_brush_size(wmKeyMap *keymap, const char *UNUSED(path))
{
	wmKeyMapItem *kmi;

	kmi = WM_keymap_add_item(keymap, "BRUSH_OT_scale_size", LEFTBRACKETKEY, KM_PRESS, 0, 0);
	RNA_float_set(kmi->ptr, "scalar", 0.9);

	kmi = WM_keymap_add_item(keymap, "BRUSH_OT_scale_size", RIGHTBRACKETKEY, KM_PRESS, 0, 0);
	RNA_float_set(kmi->ptr, "scalar", 10.0 / 9.0); // 1.1111....
}

static void ed_keymap_hair_brush_radial_control(wmKeyMap *keymap, const char *settings, RCFlags flags)
{
	wmKeyMapItem *kmi;
	/* only size needs to follow zoom, strength shows fixed size circle */
	int flags_nozoom = flags & (~RC_ZOOM);
	int flags_noradial_secondary = flags & (~(RC_SECONDARY_ROTATION | RC_ZOOM));

	kmi = WM_keymap_add_item(keymap, "WM_OT_radial_control", FKEY, KM_PRESS, 0, 0);
	set_brush_rc_props(kmi->ptr, settings, "size", "use_unified_size", flags);

	kmi = WM_keymap_add_item(keymap, "WM_OT_radial_control", FKEY, KM_PRESS, KM_SHIFT, 0);
	set_brush_rc_props(kmi->ptr, settings, "strength", "use_unified_strength", flags_nozoom);

	if (flags & RC_WEIGHT) {
		kmi = WM_keymap_add_item(keymap, "WM_OT_radial_control", WKEY, KM_PRESS, 0, 0);
		set_brush_rc_props(kmi->ptr, settings, "weight", "use_unified_weight", flags_nozoom);
	}

	if (flags & RC_ROTATION) {
		kmi = WM_keymap_add_item(keymap, "WM_OT_radial_control", FKEY, KM_PRESS, KM_CTRL, 0);
		set_brush_rc_props(kmi->ptr, settings, "texture_slot.angle", NULL, flags_noradial_secondary);
	}

	if (flags & RC_SECONDARY_ROTATION) {
		kmi = WM_keymap_add_item(keymap, "WM_OT_radial_control", FKEY, KM_PRESS, KM_CTRL | KM_ALT, 0);
		set_brush_rc_props(kmi->ptr, settings, "mask_texture_slot.angle", NULL, flags_nozoom);
	}
}

void ED_keymap_hair(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;
	
	keymap = WM_keymap_find(keyconf, "Hair", 0, 0);
	keymap->poll = hair_poll;
	
	kmi = WM_keymap_add_item(keymap, "HAIR_OT_stroke", LEFTMOUSE, KM_PRESS, 0,        0);
	
	ed_keymap_hair_brush_switch(keymap, "hair_edit");
	ed_keymap_hair_brush_size(keymap, "tool_settings.hair_edit.brush.size");
	ed_keymap_hair_brush_radial_control(keymap, "hair_edit", 0);
}

# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

import bpy
from bpy.types import UIList


class OBJECT_UL_wrinkle_modifier_maps(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index, flt_flag):
        wrinkle_map = item

        if wrinkle_map.type == 'SHAPEKEY':
            icon = 'SHAPEKEY_DATA'
            text = wrinkle_map.shapekey
        elif wrinkle_map.type == 'TEXTURE':
            icon = 'TEXTURE'
            text = wrinkle_map.texture.name if wrinkle_map.texture else ""

        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.label(text=text, icon=icon)
        elif self.layout_type in {'GRID'}:
            layout.alignment = 'CENTER'
            layout.label(text="", icon=icon)


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)

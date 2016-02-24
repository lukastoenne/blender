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

# <pep8-80 compliant>

import bpy
from bpy.types import Header, Menu
from bl_ui.properties_grease_pencil_common import (
        GreasePencilDrawingToolsPanel,
        GreasePencilStrokeEditPanel,
        GreasePencilStrokeSculptPanel,
        GreasePencilDataPanel
        )


class SPREADSHEET_HT_header(Header):
    bl_space_type = 'SPREADSHEET'

    def draw(self, context):
        layout = self.layout
        space = context.space_data

        row = layout.row(align=True)
        row.template_header()


class SPREADSHEET_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout
        space = context.space_data

        # ...

        layout.separator()
        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area", text="Toggle Maximize Area")
        layout.operator("screen.screen_full_area").use_hide_panels = True


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)

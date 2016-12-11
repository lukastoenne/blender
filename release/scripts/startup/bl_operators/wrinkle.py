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
from bpy.types import Operator
from bpy.props import *

class WrinkleOperator():
    @classmethod
    def poll(cls, context):
        return hasattr(context, "modifier")


class WrinkleCoeffsCalculate(WrinkleOperator, Operator):
    """Calculate wrinkle map coefficients"""
    bl_idname = "modifier.wrinkle_coeffs_calculate"
    bl_label = "Calculate Coefficients"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        md = context.modifier
        md.calculate_coefficients()
        return {'FINISHED'}


class WrinkleCoeffsClear(WrinkleOperator, Operator):
    """Clear wrinkle map coefficients"""
    bl_idname = "modifier.wrinkle_coeffs_clear"
    bl_label = "Clear Coefficients"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        md = context.modifier
        md.clear_coefficients()
        return {'FINISHED'}


class WrinkleMapAdd(WrinkleOperator, Operator):
    """Add a new wrinkle map"""
    bl_idname = "modifier.wrinkle_map_add"
    bl_label = "Add Wrinkle Map"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        md = context.modifier

        wrinkle_map = md.wrinkle_maps.new()

        return {'FINISHED'}


class WrinkleMapRemove(WrinkleOperator, Operator):
    """Remove selected wrinkle map"""
    bl_idname = "modifier.wrinkle_map_remove"
    bl_label = "Remove Wrinkle Map"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        md = context.modifier

        md.wrinkle_maps.remove(md.active_wrinkle_map)
        md.active_wrinkle_map_index = min(md.active_wrinkle_map_index, len(md.wrinkle_maps)-1)

        return {'FINISHED'}


class WrinkleMapMove(WrinkleOperator, Operator):
    """Move a wrinkle map up or down"""
    bl_idname = "modifier.wrinkle_map_move"
    bl_label = "Move Wrinkle Map"
    bl_options = {'REGISTER', 'UNDO'}

    _direction_items = [
        ('UP', "Up", ""),
        ('DOWN', "Down", ""),
        ]

    direction = EnumProperty(name="Direction", items=_direction_items)

    def execute(self, context):
        md = context.modifier

        num = len(md.wrinkle_maps)
        from_index = md.active_wrinkle_map_index
        to_index = from_index+1 if self.direction == 'DOWN' else from_index-1
        if (from_index >= 0 and from_index < num and to_index >= 0 and to_index < num):
            md.wrinkle_maps.move(from_index, to_index)
            md.active_wrinkle_map_index = to_index

        return {'FINISHED'}

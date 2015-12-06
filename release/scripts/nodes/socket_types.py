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
from bpy.types import NodeSocket
from bpy.props import *

###############################################################################
# Socket Types

class GeometrySocket(NodeSocket):
    '''Geometry data socket'''
    bl_idname = 'GeometrySocket'
    bl_label = 'Geometry'

    is_placeholder = BoolProperty(name="Is Placeholder",
                                  default=False)

    def draw(self, context, layout, node, text):
        layout.label(text)

    def draw_color(self, context, node):
        alpha = 0.4 if self.is_placeholder else 1.0
        return (1.0, 0.4, 0.216, alpha)

###############################################################################

bvm_type_items = [
    ("FLOAT", "Float", "Floating point number", 0, 0),
    ("INT", "Int", "Integer number", 0, 1),
    ("VECTOR", "Vector", "3D vector", 0, 2),
    ("COLOR", "Color", "RGBA color", 0, 3),
    ("MESH", "Mesh", "Mesh data", 0, 4),
    ]

def bvm_type_to_socket(base_type):
    types = {
        "FLOAT" : bpy.types.NodeSocketFloat,
        "INT" : bpy.types.NodeSocketInt,
        "VECTOR" : bpy.types.NodeSocketVector,
        "COLOR" : bpy.types.NodeSocketColor,
        "MESH" : bpy.types.GeometrySocket,
        }
    return types.get(base_type, None)

def socket_type_to_bvm(socket):
    if isinstance(socket, bpy.types.NodeSocketFloat):
        return 'FLOAT'
    elif isinstance(socket, bpy.types.NodeSocketVector):
        return 'FLOAT3'
    elif isinstance(socket, bpy.types.NodeSocketColor):
        return 'FLOAT4'
    elif isinstance(socket, bpy.types.NodeSocketInt):
        return 'INT'
    elif isinstance(socket, bpy.types.GeometrySocket):
        return 'MESH'

def convert_sockets(compiler, from_typedesc, to_typedesc):
    from_type = from_typedesc.base_type
    to_type = to_typedesc.base_type

    if to_type == from_type:
        node = compiler.add_node("PASS_%s" % to_type)
        return {node.inputs[0]}, node.outputs[0]

    if to_type == 'FLOAT':
        if from_type == 'INT':
            node = compiler.add_node("INT_TO_FLOAT")
            return {node.inputs[0]}, node.outputs[0]
        elif from_type == 'FLOAT3':
            node = compiler.add_node("SET_FLOAT3")
            return {node.inputs[0], node.inputs[1], node.inputs[2]}, node.outputs[0]
        elif from_type == 'FLOAT4':
            node = compiler.add_node("SET_FLOAT4")
            return {node.inputs[0], node.inputs[1], node.inputs[2], node.inputs[3]}, node.outputs[0]
    
    elif to_type == 'FLOAT3':
        pass
    
    elif to_type == 'FLOAT4':
        pass
    
    elif to_type == 'INT':
        if from_type == 'FLOAT':
            node = compiler.add_node("FLOAT_TO_INT")
            return {node.inputs[0]}, node.outputs[0]
    
    elif to_type == 'MATRIX44':
        pass

    return set(), None

###############################################################################

def register():
    bpy.utils.register_module(__name__)

def unregister():
    bpy.utils.unregister_module(__name__)

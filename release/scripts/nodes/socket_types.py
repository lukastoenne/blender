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

class TransformSocket(NodeSocket):
    '''Affine 3D transformation'''
    bl_idname = 'TransformSocket'
    bl_label = 'Transform'

    def draw(self, context, layout, node, text):
        layout.label(text)

    def draw_color(self, context, node):
        return (0.59, 0.00, 0.67, 1.00)

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

class DupliSocket(NodeSocket):
    '''Dupli instances socket'''
    bl_idname = 'DupliSocket'
    bl_label = 'Duplis'

    is_placeholder = BoolProperty(name="Is Placeholder",
                                  default=False)

    def draw(self, context, layout, node, text):
        layout.label(text)

    def draw_color(self, context, node):
        alpha = 0.4 if self.is_placeholder else 1.0
        return (1.0, 0.4, 0.216, alpha)

###############################################################################

socket_type_items = [
    ("FLOAT", "Float", "Floating point number", 0, 0),
    ("INT", "Int", "Integer number", 0, 1),
    ("VECTOR", "Vector", "3D vector", 0, 2),
    ("COLOR", "Color", "RGBA color", 0, 3),
    ("MESH", "Mesh", "Mesh data", 0, 4),
    ("DUPLIS", "Duplis", "Dupli instances", 0, 5),
    ("TRANSFORM", "Transform", "Affine transformation", 0, 6),
    ]

def socket_type_to_rna(base_type):
    types = {
        "FLOAT" : bpy.types.NodeSocketFloat,
        "INT" : bpy.types.NodeSocketInt,
        "VECTOR" : bpy.types.NodeSocketVector,
        "COLOR" : bpy.types.NodeSocketColor,
        "MESH" : bpy.types.GeometrySocket,
        "DUPLIS" : bpy.types.DupliSocket,
        "TRANSFORM" : bpy.types.TransformSocket,
        }
    return types.get(base_type, None)

def rna_to_bvm_type(cls):
    if issubclass(cls, bpy.types.NodeSocketFloat):
        return 'FLOAT'
    elif issubclass(cls, bpy.types.NodeSocketVector):
        return 'FLOAT3'
    elif issubclass(cls, bpy.types.NodeSocketColor):
        return 'FLOAT4'
    elif issubclass(cls, bpy.types.NodeSocketInt):
        return 'INT'
    elif issubclass(cls, bpy.types.TransformSocket):
        return 'MATRIX44'
    elif issubclass(cls, bpy.types.GeometrySocket):
        return 'MESH'
    elif issubclass(cls, bpy.types.DupliSocket):
        return 'DUPLIS'

def socket_type_to_bvm_type(base_type):
    return rna_to_bvm_type(socket_type_to_rna(base_type))

# determines if a conversion is necessary and possible
# and returns a new input socket to link
def convert_sockets(compiler, from_socket, to_socket):
    from_type = from_socket.typedesc.base_type
    to_type = to_socket.typedesc.base_type

    if from_type == to_type:
        return from_socket

    def int_to_float(from_socket):
        node = compiler.add_node("INT_TO_FLOAT")
        compiler.link(from_socket, node.inputs[0])
        return node.outputs[0]
    def float_to_int(from_socket):
        node = compiler.add_node("FLOAT_TO_INT")
        compiler.link(from_socket, node.inputs[0])
        return node.outputs[0]
    def get_elem_float3(from_socket, index):
        node = compiler.add_node("GET_ELEM_FLOAT3")
        node.inputs[0].set_value(index)
        compiler.link(from_socket, node.inputs[1])
        return node.outputs[0]
    def get_elem_float4(from_socket, index):
        node = compiler.add_node("GET_ELEM_FLOAT4")
        node.inputs[0].set_value(index)
        compiler.link(from_socket, node.inputs[1])
        return node.outputs[0]
    def set_float3(from_socket):
        node = compiler.add_node("SET_FLOAT3")
        compiler.link(from_socket, node.inputs[0])
        compiler.link(from_socket, node.inputs[1])
        compiler.link(from_socket, node.inputs[2])
        return node.outputs[0]
    def set_float4(from_socket):
        node = compiler.add_node("SET_FLOAT4")
        compiler.link(from_socket, node.inputs[0])
        compiler.link(from_socket, node.inputs[1])
        compiler.link(from_socket, node.inputs[2])
        compiler.link(from_socket, node.inputs[3])
        return node.outputs[0]
    def float3_to_float4(from_socket):
        node = compiler.add_node("SET_FLOAT4")
        compiler.link(get_elem_float3(from_socket, 0), node.inputs[0])
        compiler.link(get_elem_float3(from_socket, 1), node.inputs[1])
        compiler.link(get_elem_float3(from_socket, 2), node.inputs[2])
        node.inputs[3].set_value(1.0)
        return node.outputs[0]
    def float4_to_float3(from_socket):
        node = compiler.add_node("SET_FLOAT3")
        compiler.link(get_elem_float4(from_socket, 0), node.inputs[0])
        compiler.link(get_elem_float4(from_socket, 1), node.inputs[1])
        compiler.link(get_elem_float4(from_socket, 2), node.inputs[2])
        return node.outputs[0]

    if to_type == 'FLOAT':
        if from_type == 'INT':
            return int_to_float(from_socket)
        elif from_type == 'FLOAT3':
            return get_elem_float3(from_socket, 0)
        elif from_type == 'FLOAT4':
            return get_elem_float4(from_socket, 0)
    
    elif to_type == 'FLOAT3':
        if from_type == 'FLOAT':
            return set_float3(from_socket)
        elif from_type == 'FLOAT4':
            return float4_to_float3(from_socket)
        elif from_type == 'INT':
            return set_float3(int_to_float(from_socket))
    
    elif to_type == 'FLOAT4':
        if from_type == 'FLOAT':
            return set_float4(from_socket)
        elif from_type == 'FLOAT3':
            return float3_to_float4(from_socket)
        elif from_type == 'INT':
            return set_float4(int_to_float(from_socket))
    
    elif to_type == 'INT':
        if from_type == 'FLOAT':
            return float_to_int(from_socket)
        if from_type == 'FLOAT3':
            return float_to_int(get_elem_float3(from_socket, 0))
        if from_type == 'FLOAT4':
            return float_to_int(get_elem_float4(from_socket, 0))
    
    elif to_type == 'MATRIX44':
        pass

###############################################################################

def register():
    bpy.utils.register_module(__name__)

def unregister():
    bpy.utils.unregister_module(__name__)

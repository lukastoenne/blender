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

class AnySocket(NodeSocket):
    '''Any type socket'''
    bl_idname = 'AnySocket'
    bl_label = 'Any'

    is_placeholder = BoolProperty(name="Is Placeholder",
                                  default=False)

    def draw(self, context, layout, node, text):
        layout.label(text)

    def draw_color(self, context, node):
        alpha = 0.4 if self.is_placeholder else 1.0
        return (0.95, 0.95, 0.95, alpha)

###############################################################################

# combined info for each socket type:
# identifier, UI name, description, bvm type, RNA struct type
_socket_type_info = [
    ("FLOAT", "Float", "Floating point number", 'FLOAT', bpy.types.NodeSocketFloat),
    ("INT", "Int", "Integer number", 'INT', bpy.types.NodeSocketInt),
    ("VECTOR", "Vector", "3D vector", 'FLOAT3', bpy.types.NodeSocketVector),
    ("COLOR", "Color", "RGBA color", 'FLOAT4', bpy.types.NodeSocketColor),
    ("MESH", "Mesh", "Mesh data", 'MESH', GeometrySocket),
    ("DUPLIS", "Duplis", "Dupli instances", 'DUPLIS', DupliSocket),
    ("TRANSFORM", "Transform", "Affine transformation", 'MATRIX44', TransformSocket),
    ]

socket_type_items = [(s[0], s[1], s[2], 0, i) for i,s in enumerate(_socket_type_info)]

def socket_type_to_rna(socket_type):
    for s in _socket_type_info:
        if s[0] == socket_type:
            return s[4]
    return None

def socket_type_to_bvm_type(socket_type):
    for s in _socket_type_info:
        if s[0] == socket_type:
            return s[3]
    return ''

def rna_to_socket_type(cls):
    for s in _socket_type_info:
        if issubclass(cls, s[4]):
            return s[0]
    return ''

def rna_to_bvm_type(cls):
    for s in _socket_type_info:
        if issubclass(cls, s[4]):
            return s[3]
    return ''

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

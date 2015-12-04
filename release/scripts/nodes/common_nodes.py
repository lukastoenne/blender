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
import nodeitems_utils
from bpy.types import Operator, ObjectNode, NodeTree, Node, NodeSocket
from bpy.props import *
from node_compiler import NodeCompiler, NodeWrapper, InputWrapper, OutputWrapper, StringDict

class NodeTreeBase():
    def bvm_compile(self, context, graph):
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

        comp = NodeCompiler(context, graph)

        input_map = dict()
        output_map = dict()

        for bnode in self.nodes:
            if not bnode.is_registered_node_type():
                continue
            if not hasattr(bnode, "compile"):
                continue

            # proxies for inputs/outputs
            bnode_inputs = StringDict()
            for binput in bnode.inputs:
                itype = socket_type_to_bvm(binput)
                
                proxy = comp.add_node("PASS_%s" % itype)
                bnode_inputs[binput.identifier] = proxy.outputs[0]
                input_map[(bnode, binput)] = proxy.inputs[0]

                if hasattr(binput, "default_value"):
                    proxy.inputs[0].set_value(binput.default_value)
            
            bnode_outputs = StringDict()
            for boutput in bnode.outputs:
                otype = socket_type_to_bvm(boutput)
                
                proxy = comp.add_node("PASS_%s" % otype)
                bnode_outputs[boutput.identifier] = proxy.inputs[0]
                output_map[(bnode, boutput)] = proxy.outputs[0]

            comp.bnode = bnode
            comp.bnode_inputs = bnode_inputs
            comp.bnode_outputs = bnode_outputs
            bnode.compile(comp)

        comp.bnode = None
        comp.bnode_inputs = None
        comp.bnode_outputs = None

        for blink in self.links:
            if not blink.is_valid:
                continue

            src = (blink.from_node, blink.from_socket)
            dst = (blink.to_node, blink.to_socket)
            comp.link(output_map[src], input_map[dst])

###############################################################################
# Generic Nodes

class CommonNodeBase():
    @classmethod
    def poll(cls, ntree):
        return isinstance(ntree, NodeTreeBase)

class IterationNode(CommonNodeBase, ObjectNode):
    '''Iteration number'''
    bl_idname = 'ObjectIterationNode'
    bl_label = 'Iteration'

    def init(self, context):
        self.outputs.new('NodeSocketInt', "N")

    def compile(self, compiler):
        node = compiler.add_node("ITERATION", self.name)
        compiler.map_output(0, node.outputs[0])

class MathNode(CommonNodeBase, ObjectNode):
    '''Math '''
    bl_idname = 'ObjectMathNode'
    bl_label = 'Math'

    _mode_items = [
        ('ADD_FLOAT', 'Add', '', 'NONE', 0),
        ('SUB_FLOAT', 'Subtract', '', 'NONE', 1),
        ('MUL_FLOAT', 'Multiply', '', 'NONE', 2),
        ('DIV_FLOAT', 'Divide', '', 'NONE', 3),
        ('SINE', 'Sine', '', 'NONE', 4),
        ('COSINE', 'Cosine', '', 'NONE', 5),
        ('TANGENT', 'Tangent', '', 'NONE', 6),
        ('ARCSINE', 'Arcsine', '', 'NONE', 7),
        ('ARCCOSINE', 'Arccosine', '', 'NONE', 8),
        ('ARCTANGENT', 'Arctangent', '', 'NONE', 9),
        ('POWER', 'Power', '', 'NONE', 10),
        ('LOGARITHM', 'Logarithm', '', 'NONE', 11),
        ('MINIMUM', 'Minimum', '', 'NONE', 12),
        ('MAXIMUM', 'Maximum', '', 'NONE', 13),
        ('ROUND', 'Round', '', 'NONE', 14),
        ('LESS_THAN', 'Less Than', '', 'NONE', 15),
        ('GREATER_THAN', 'Greater Than', '', 'NONE', 16),
        ('MODULO', 'Modulo', '', 'NONE', 17),
        ('ABSOLUTE', 'Absolute', '', 'NONE', 18),
        ('CLAMP', 'Clamp', '', 'NONE', 19),
    ]
    mode = EnumProperty(name="Mode",
                        items=_mode_items)

    def draw_buttons(self, context, layout):
        layout.prop(self, "mode")

    def init(self, context):
        self.inputs.new('NodeSocketFloat', "Value")
        self.inputs.new('NodeSocketFloat', "Value")
        self.outputs.new('NodeSocketFloat', "Value")

    def compile(self, compiler):
        node = compiler.add_node(self.mode, self.name+"N")

        is_binary = self.mode in {'ADD_FLOAT', 'SUB_FLOAT', 'MUL_FLOAT', 'DIV_FLOAT', 
                                  'POWER', 'LOGARITHM', 'MINIMUM', 'MAXIMUM',
                                  'LESS_THAN', 'GREATER_THAN', 'MODULO'}

        if is_binary:
            # binary mode
            compiler.map_input(0, node.inputs[0])
            compiler.map_input(1, node.inputs[1])
        else:
            # unary mode
            socket_a = self.inputs[0]
            socket_b = self.inputs[1]
            linked_a = (not socket_a.hide) and socket_a.is_linked
            linked_b = (not socket_a.hide) and socket_a.is_linked
            if linked_a or (not linked_b):
                compiler.map_input(0, node.inputs[0])
            else:
                compiler.map_input(1, node.inputs[0])

        compiler.map_output(0, node.outputs[0])


class VectorMathNode(CommonNodeBase, ObjectNode):
    '''Vector Math '''
    bl_idname = 'ObjectVectorMathNode'
    bl_label = 'Vector Math'

    _mode_items = [
        ('ADD_FLOAT3', 'Add', '', 'NONE', 0),
        ('SUB_FLOAT3', 'Subtract', '', 'NONE', 1),
        ('AVERAGE_FLOAT3', 'Average', '', 'NONE', 2),
        ('DOT_FLOAT3', 'Dot Product', '', 'NONE', 3),
        ('CROSS_FLOAT3', 'Cross Product', '', 'NONE', 4),
        ('NORMALIZE_FLOAT3', 'Normalize', '', 'NONE', 5),
    ]
    mode = EnumProperty(name="Mode",
                        items=_mode_items)

    def draw_buttons(self, context, layout):
        layout.prop(self, "mode")

    def init(self, context):
        self.inputs.new('NodeSocketVector', "Vector")
        self.inputs.new('NodeSocketVector', "Vector")
        self.outputs.new('NodeSocketVector', "Vector")
        self.outputs.new('NodeSocketFloat', "Value")

    def compile(self, compiler):
        node = compiler.add_node(self.mode, self.name+"N")

        is_binary = self.mode in {'ADD_FLOAT3', 'SUB_FLOAT3', 'AVERAGE_FLOAT3',
                                  'DOT_FLOAT3', 'CROSS_FLOAT3'}
        has_vector_out = self.mode in {'ADD_FLOAT3', 'SUB_FLOAT3', 'AVERAGE_FLOAT3',
                                       'CROSS_FLOAT3', 'NORMALIZE_FLOAT3'}
        has_value_out = self.mode in {'DOT_FLOAT3', 'NORMALIZE_FLOAT3'}

        if is_binary:
            # binary node
            compiler.map_input(0, node.inputs[0])
            compiler.map_input(1, node.inputs[1])
        else:
            # unary node
            socket_a = self.inputs[0]
            socket_b = self.inputs[1]
            linked_a = (not socket_a.hide) and socket_a.is_linked
            linked_b = (not socket_a.hide) and socket_a.is_linked
            if linked_a or (not linked_b):
                compiler.map_input(0, node.inputs[0])
            else:
                compiler.map_input(1, node.inputs[0])

        if has_vector_out and has_value_out:
            compiler.map_output(0, node.outputs[0])
            compiler.map_output(1, node.outputs[1])
        elif has_vector_out:
            compiler.map_output(0, node.outputs[0])
        elif has_value_out:
            compiler.map_output(1, node.outputs[0])


class SeparateVectorNode(CommonNodeBase, ObjectNode):
    '''Separate vector into elements'''
    bl_idname = 'ObjectSeparateVectorNode'
    bl_label = 'Separate Vector'

    def init(self, context):
        self.inputs.new('NodeSocketVector', "Vector")
        self.outputs.new('NodeSocketFloat', "X")
        self.outputs.new('NodeSocketFloat', "Y")
        self.outputs.new('NodeSocketFloat', "Z")

    def compile(self, compiler):
        node = compiler.add_node("GET_ELEM_FLOAT3", self.name+"X")
        node.inputs["index"].set_value(0)
        compiler.map_input(0, node.inputs["value"])
        compiler.map_output(0, node.outputs["value"])
        
        node = compiler.add_node("GET_ELEM_FLOAT3", self.name+"Y")
        node.inputs["index"].set_value(1)
        compiler.map_input(0, node.inputs["value"])
        compiler.map_output(1, node.outputs["value"])
        
        node = compiler.add_node("GET_ELEM_FLOAT3", self.name+"Z")
        node.inputs["index"].set_value(2)
        compiler.map_input(0, node.inputs["value"])
        compiler.map_output(2, node.outputs["value"])


class CombineVectorNode(CommonNodeBase, ObjectNode):
    '''Combine vector from component values'''
    bl_idname = 'ObjectCombineVectorNode'
    bl_label = 'Combine Vector'

    def init(self, context):
        self.inputs.new('NodeSocketFloat', "X")
        self.inputs.new('NodeSocketFloat', "Y")
        self.inputs.new('NodeSocketFloat', "Z")
        self.outputs.new('NodeSocketVector', "Vector")

    def compile(self, compiler):
        node = compiler.add_node("SET_FLOAT3", self.name+"N")
        compiler.map_input(0, node.inputs[0])
        compiler.map_input(1, node.inputs[1])
        compiler.map_input(2, node.inputs[2])
        compiler.map_output(0, node.outputs[0])

###############################################################################

def register():
    bpy.utils.register_module(__name__)

def unregister():
    bpy.utils.unregister_module(__name__)

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

import bpy, contextlib
import nodeitems_utils
from bpy.types import Operator, ObjectNode, BVMEvalGlobals
from bpy.props import *
from node_compiler import NodeCompiler, NodeWrapper, InputWrapper, OutputWrapper


def enum_property_copy(bpy_type, name):
    prop = bpy_type.bl_rna.properties[name]
    items = [(i.identifier, i.name, i.description, i.icon, i.value) for i in prop.enum_items]
    return EnumProperty(name=prop.name,
                        description=prop.description,
                        default=prop.default,
                        items=items)

def enum_property_value_prop(name):
    def fget(self):
        return self.bl_rna.properties[name].enum_items[getattr(self, name)].value
    return property(fget=fget)


###############################################################################


class NodeTreeBase():
    def bvm_compile_dependencies(self, depsnode):
        # own changes require recompile
        depsnode.add_nodetree_relation(self, 'PARAMETERS')

        for node in self.nodes:
            node.compile_dependencies(depsnode)

    def bvm_eval_dependencies(self, depsnode):
        for node in self.nodes:
            node.eval_dependencies(depsnode)

    def bvm_compile(self, graph):
        compiler = NodeCompiler(graph)
        self.compile_nodes(compiler)

    def compile_nodes(self, compiler):
        input_map = dict()
        output_map = dict()

        for bnode in self.nodes:
            if not bnode.is_registered_node_type():
                continue
            if not hasattr(bnode, "compile"):
                continue

            compiler.push(bnode, input_map, output_map)
            bnode.compile(compiler)
            compiler.pop()

        for blink in self.links:
            if not blink.is_valid:
                continue

            src = (blink.from_node, blink.from_socket)
            dst = (blink.to_node, blink.to_socket)
            compiler.link(output_map[src], input_map[dst])

class NodeBase():
    # XXX used to prevent reentrant updates due to RNA calls
    # this should be fixed in future by avoiding low-level update recursion on the RNA side
    is_updating = BoolProperty(options={'HIDDEN'})

    @contextlib.contextmanager
    def update_lock(self):
        self.is_updating = True
        try:
            yield
        finally:
            self.is_updating = False

    def compile_dependencies(self, depsnode):
        pass

    def eval_dependencies(self, depsnode):
        pass


###############################################################################


class DynamicSocketListNode:
    def add_extender(self, socketlist, sockettype):
        socket = socketlist.new(sockettype, "")
        socket.is_placeholder = True
        return socket

    def update_socket_list(self, socketlist, sockettype, insert=None):
        ntree = self.id_data

        # build map of connected inputs
        input_links = dict()
        for link in ntree.links:
            if link.to_node == self:
                input_links[link.to_socket] = (link, link.from_socket)

        # remove unconnected sockets
        for socket in socketlist:
            if socket not in input_links and socket != insert:
                socketlist.remove(socket)
            else:
                socket.is_placeholder = False

        # shift sockets to make room for a new link
        if insert is not None:
            socketlist.new(sockettype, "")
            nsocket = socketlist[-1]
            for socket in reversed(socketlist[:-1]):
                link, from_socket = input_links.get(socket, (None, None))
                if link is not None:
                    ntree.links.remove(link)
                    ntree.links.new(from_socket, nsocket)
                nsocket = socket
                if socket == insert:
                    break

        self.add_extender(socketlist, sockettype)

    def compile_socket_list(self, compiler, socketlist, passtype, jointype, valuetype):
        ntree = self.id_data

        # list of connected inputs
        used_inputs = set()
        for link in ntree.links:
            if link.to_node == self:
                used_inputs.add(link.to_socket)
        # make a sorted index list
        used_inputs = [ i for i,socket in enumerate(socketlist) if socket in used_inputs ]

        if len(used_inputs) > 0:
            node = compiler.add_node(passtype)
            compiler.map_input(used_inputs[0], node.inputs[0])
            result = node.outputs[0]
        
            for index in used_inputs[1:]:
                node = compiler.add_node(jointype)
                compiler.link(result, node.inputs[0])
                compiler.map_input(index, node.inputs[1])
                result = node.outputs[0]

        else:
            node = compiler.add_node(valuetype)
            result = node.outputs[0]

        return result


###############################################################################
# Generic Nodes

class CommonNodeBase(NodeBase):
    @classmethod
    def poll(cls, ntree):
        return isinstance(ntree, NodeTreeBase)

class RangeNode(CommonNodeBase, ObjectNode):
    '''Generate a range of numbers'''
    bl_idname = 'ObjectRangeNode'
    bl_label = 'Range'

    start = IntProperty(name="Start", default=0)
    end = IntProperty(name="End", default=10)
    step = IntProperty(name="Step", default=1)

    def draw_buttons(self, context, layout):
        row = layout.row(align=True)
        row.prop(self, "start")
        row.prop(self, "end")
        layout.prop(self, "step")

    def init(self, context):
        self.outputs.new('NodeSocketInt', "Value")

    def compile(self, compiler):
        node = compiler.add_node("RANGE_INT")
        node.inputs[1].set_value(self.start)
        node.inputs[2].set_value(self.end)
        node.inputs[3].set_value(self.step)
        compiler.map_output(0, node.outputs[0])

class ValueFloatNode(CommonNodeBase, ObjectNode):
    '''Floating point number'''
    bl_idname = 'ObjectValueFloatNode'
    bl_label = 'Float'

    value = FloatProperty(name="Value", default=0.0)

    def draw_buttons(self, context, layout):
        layout.prop(self, "value", text="")

    def init(self, context):
        self.outputs.new('NodeSocketFloat', "Value")

    def compile(self, compiler):
        node = compiler.add_proxy("FLOAT")
        node.inputs[0].set_value(self.value)
        compiler.map_output(0, node.outputs[0])

class ValueIntNode(CommonNodeBase, ObjectNode):
    '''Integer number'''
    bl_idname = 'ObjectValueIntNode'
    bl_label = 'Integer'

    value = IntProperty(name="Value", default=0)

    def draw_buttons(self, context, layout):
        layout.prop(self, "value", text="")

    def init(self, context):
        self.outputs.new('NodeSocketInt', "Value")

    def compile(self, compiler):
        node = compiler.add_proxy("INT")
        node.inputs[0].set_value(self.value)
        compiler.map_output(0, node.outputs[0])

class ValueVectorNode(CommonNodeBase, ObjectNode):
    '''3D vector'''
    bl_idname = 'ObjectValueVectorNode'
    bl_label = 'Vector'

    value = FloatVectorProperty(name="Value", size=3, default=(0.0, 0.0, 0.0))

    def draw_buttons(self, context, layout):
        col = layout.column(align=True)
        col.prop(self, "value", text="")

    def init(self, context):
        self.outputs.new('NodeSocketVector', "Value")

    def compile(self, compiler):
        node = compiler.add_proxy("FLOAT3")
        node.inputs[0].set_value(self.value)
        compiler.map_output(0, node.outputs[0])

class ValueColorNode(CommonNodeBase, ObjectNode):
    '''RGBA color'''
    bl_idname = 'ObjectValueColorNode'
    bl_label = 'Color'

    value = FloatVectorProperty(name="Value", size=4, subtype='COLOR',
                                default=(0.0, 0.0, 0.0, 1.0), min=0.0, max=1.0)

    def draw_buttons(self, context, layout):
        layout.template_color_picker(self, "value", value_slider=True)

    def init(self, context):
        self.outputs.new('NodeSocketColor', "Value")

    def compile(self, compiler):
        node = compiler.add_proxy("FLOAT4")
        node.inputs[0].set_value(self.value)
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
        ('SQRT', 'Square Root', '', 'NONE', 20),
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
    '''Vector Math'''
    bl_idname = 'ObjectVectorMathNode'
    bl_label = 'Vector Math'

    _mode_items = [
        ('ADD_FLOAT3', 'Add', '', 'NONE', 0),
        ('SUB_FLOAT3', 'Subtract', '', 'NONE', 1),
        ('MUL_FLOAT3', 'Multiply', '', 'NONE', 2),
        ('DIV_FLOAT3', 'Divide', '', 'NONE', 3),
        ('AVERAGE_FLOAT3', 'Average', '', 'NONE', 4),
        ('DOT_FLOAT3', 'Dot Product', '', 'NONE', 5),
        ('CROSS_FLOAT3', 'Cross Product', '', 'NONE', 6),
        ('NORMALIZE_FLOAT3', 'Normalize', '', 'NONE', 7),
        ('LENGTH_FLOAT3', 'Vector Length', '', 'NONE', 8),
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

        is_binary = self.mode in {'ADD_FLOAT3', 'SUB_FLOAT3', 'MUL_FLOAT3', 'DIV_FLOAT3',
                                  'AVERAGE_FLOAT3', 'DOT_FLOAT3', 'CROSS_FLOAT3'}
        has_vector_out = self.mode in {'ADD_FLOAT3', 'SUB_FLOAT3', 'MUL_FLOAT3', 'DIV_FLOAT3',
                                       'AVERAGE_FLOAT3', 'CROSS_FLOAT3', 'NORMALIZE_FLOAT3'}
        has_value_out = self.mode in {'DOT_FLOAT3', 'NORMALIZE_FLOAT3', 'LENGTH_FLOAT3'}

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

class TranslationTransformNode(CommonNodeBase, ObjectNode):
    '''Create translation from a vector'''
    bl_idname = 'ObjectTranslationTransformNode'
    bl_label = 'Translation Transform'

    def init(self, context):
        self.inputs.new('TransformSocket', "")
        self.inputs.new('NodeSocketVector', "Vector")
        self.outputs.new('TransformSocket', "")

    def compile(self, compiler):
        node = compiler.add_node("LOC_TO_MATRIX44")
        compiler.map_input(1, node.inputs[0])
        
        node_mul = compiler.add_node("MUL_MATRIX44")
        compiler.map_input(0, node_mul.inputs[0])
        compiler.link(node.outputs[0], node_mul.inputs[1])
        compiler.map_output(0, node_mul.outputs[0])


class GetTranslationNode(CommonNodeBase, ObjectNode):
    '''Get translation vector from a transform'''
    bl_idname = 'ObjectGetTranslationNode'
    bl_label = 'Get Translation'

    def init(self, context):
        self.inputs.new('TransformSocket', "")
        self.outputs.new('NodeSocketVector', "Vector")

    def compile(self, compiler):
        node = compiler.add_node("MATRIX44_TO_LOC")
        compiler.map_input(0, node.inputs[0])
        compiler.map_output(0, node.outputs[0])


_euler_order_items = [
    ('XYZ', "XYZ", "", 0, 1),
    ('XZY', "XZY", "", 0, 2),
    ('YXZ', "YXZ", "", 0, 3),
    ('YZX', "YZX", "", 0, 4),
    ('ZXY', "ZXY", "", 0, 5),
    ('ZYX', "ZYX", "", 0, 6),
    ]
_prop_euler_order = EnumProperty(name="Euler Order", items=_euler_order_items, default='XYZ')

class EulerTransformNode(CommonNodeBase, ObjectNode):
    '''Create rotation from Euler angles'''
    bl_idname = 'ObjectEulerTransformNode'
    bl_label = 'Euler Transform'

    euler_order = _prop_euler_order
    euler_order_value = enum_property_value_prop('euler_order')

    def draw_buttons(self, context, layout):
        layout.prop(self, "euler_order")

    def init(self, context):
        self.inputs.new('TransformSocket', "")
        self.inputs.new('NodeSocketVector', "Euler Angles")
        self.outputs.new('TransformSocket', "")

    def compile(self, compiler):
        node = compiler.add_node("EULER_TO_MATRIX44")
        node.inputs[0].set_value(self.euler_order_value)
        compiler.map_input(1, node.inputs[1])
        
        node_mul = compiler.add_node("MUL_MATRIX44")
        compiler.map_input(0, node_mul.inputs[0])
        compiler.link(node.outputs[0], node_mul.inputs[1])
        compiler.map_output(0, node_mul.outputs[0])


class GetEulerNode(CommonNodeBase, ObjectNode):
    '''Get euler angles from a transform'''
    bl_idname = 'ObjectGetEulerNode'
    bl_label = 'Get Euler Angles'

    euler_order = _prop_euler_order
    euler_order_value = enum_property_value_prop('euler_order')

    def draw_buttons(self, context, layout):
        layout.prop(self, "euler_order")

    def init(self, context):
        self.inputs.new('TransformSocket', "")
        self.outputs.new('NodeSocketVector', "Euler Angles")

    def compile(self, compiler):
        node = compiler.add_node("MATRIX44_TO_EULER")
        node.inputs[0].set_value(self.euler_order_value)
        compiler.map_input(0, node.inputs[1])
        compiler.map_output(0, node.outputs[0])


class AxisAngleTransformNode(CommonNodeBase, ObjectNode):
    '''Create rotation from axis and angle'''
    bl_idname = 'ObjectAxisAngleTransformNode'
    bl_label = 'Axis/Angle Transform'

    def init(self, context):
        self.inputs.new('TransformSocket', "")
        self.inputs.new('NodeSocketVector', "Axis")
        self.inputs.new('NodeSocketFloat', "Angle")
        self.outputs.new('TransformSocket', "")

    def compile(self, compiler):
        node = compiler.add_node("AXISANGLE_TO_MATRIX44")
        compiler.map_input(1, node.inputs[0])
        compiler.map_input(2, node.inputs[1])
        
        node_mul = compiler.add_node("MUL_MATRIX44")
        compiler.map_input(0, node_mul.inputs[0])
        compiler.link(node.outputs[0], node_mul.inputs[1])
        compiler.map_output(0, node_mul.outputs[0])


class GetAxisAngleNode(CommonNodeBase, ObjectNode):
    '''Get axis and angle from a transform'''
    bl_idname = 'ObjectGetAxisAngleNode'
    bl_label = 'Get Axis/Angle'

    def init(self, context):
        self.inputs.new('TransformSocket', "")
        self.outputs.new('NodeSocketVector', "Axis")
        self.outputs.new('NodeSocketFloat', "Angle")

    def compile(self, compiler):
        node = compiler.add_node("MATRIX44_TO_AXISANGLE")
        compiler.map_input(0, node.inputs[0])
        compiler.map_output(0, node.outputs[0])
        compiler.map_output(1, node.outputs[1])


class ScaleTransformNode(CommonNodeBase, ObjectNode):
    '''Create transform from a scaling vector'''
    bl_idname = 'ObjectScaleTransformNode'
    bl_label = 'Scale Transform'

    def init(self, context):
        self.inputs.new('TransformSocket', "")
        self.inputs.new('NodeSocketVector', "Scale")
        self.outputs.new('TransformSocket', "")

    def compile(self, compiler):
        node = compiler.add_node("SCALE_TO_MATRIX44")
        compiler.map_input(1, node.inputs[0])
        
        node_mul = compiler.add_node("MUL_MATRIX44")
        compiler.map_input(0, node_mul.inputs[0])
        compiler.link(node.outputs[0], node_mul.inputs[1])
        compiler.map_output(0, node_mul.outputs[0])


class GetScaleNode(CommonNodeBase, ObjectNode):
    '''Get scale from a transform'''
    bl_idname = 'ObjectGetScaleNode'
    bl_label = 'Get Scale'

    def init(self, context):
        self.inputs.new('TransformSocket', "")
        self.outputs.new('NodeSocketVector', "Scale")

    def compile(self, compiler):
        node = compiler.add_node("MATRIX44_TO_SCALE")
        compiler.map_input(0, node.inputs[0])
        compiler.map_output(0, node.outputs[0])


class RandomNode(CommonNodeBase, ObjectNode):
    '''Produce random values'''
    bl_idname = 'ObjectRandomNode'
    bl_label = 'Random'

    def _enable_input(self, index):
        for i, s in enumerate(self.inputs):
            s.enabled = (i == index)

    seed = IntProperty(name="Seed", default=0, subtype='UNSIGNED')

    _mode_items = [
        ('INT', 'Integer', 'Use integer input', 0, 0),
        ('FLOAT', 'Float', 'Use floating point input', 0, 1),
        ]
    def _mode_update(self, context):
        if self.mode == 'INT':
            self._enable_input(0)
        elif self.mode == 'FLOAT':
            self._enable_input(1)
    mode = EnumProperty(name="Mode", description="Type of input value", items=_mode_items,
                        default='INT', update=_mode_update)

    def draw_buttons(self, context, layout):
        layout.prop(self, "mode")
        layout.prop(self, "seed")

    def init(self, context):
        self.inputs.new('NodeSocketInt', "Value")
        self.inputs.new('NodeSocketFloat', "Value")
        self.outputs.new('NodeSocketInt', "Random")
        self.outputs.new('NodeSocketFloat', "Random")
        self._enable_input(0)

    def compile(self, compiler):
        if self.mode == 'INT':
            node = compiler.add_node("INT_TO_RANDOM")
            compiler.map_input(0, node.inputs[1])
        elif self.mode == 'FLOAT':
            node = compiler.add_node("FLOAT_TO_RANDOM")
            compiler.map_input(1, node.inputs[1])
        node.inputs[0].set_value(self.seed)
        compiler.map_output(0, node.outputs[0])
        compiler.map_output(1, node.outputs[1])


class TextureCloudsNode(CommonNodeBase, ObjectNode):
    '''Clouds texture'''
    bl_idname = 'ObjectTextureCloudsNode'
    bl_label = 'Clouds'

    depth = IntProperty(name="Depth", default=2)
    noise_basis = enum_property_copy(bpy.types.CloudsTexture, 'noise_basis')
    noise_basis_value = enum_property_value_prop('noise_basis')
    noise_hard = enum_property_copy(bpy.types.CloudsTexture, 'noise_type')
    noise_hard_value = enum_property_value_prop('noise_hard')

    def draw_buttons(self, context, layout):
        layout.prop(self, "depth")
        layout.prop(self, "noise_basis")
        layout.prop(self, "noise_hard")

    def init(self, context):
        self.inputs.new('NodeSocketVector', "Position")
        self.inputs.new('NodeSocketFloat', "Size")
        self.outputs.new('NodeSocketFloat', "Intensity")
        self.outputs.new('NodeSocketColor', "Color")
        self.outputs.new('NodeSocketVector', "Normal")

    def compile(self, compiler):
        node = compiler.add_node("TEX_PROC_CLOUDS")
        compiler.map_input(0, node.inputs["position"])
        compiler.map_input(1, node.inputs["size"])
        node.inputs["depth"].set_value(self.depth)
        node.inputs["noise_basis"].set_value(self.noise_basis_value)
        node.inputs["noise_hard"].set_value(self.noise_hard_value)
        compiler.map_output(0, node.outputs["intensity"])
        compiler.map_output(1, node.outputs["color"])
        compiler.map_output(2, node.outputs["normal"])


class TextureWoodNode(CommonNodeBase, ObjectNode):
    '''Wood texture'''
    bl_idname = 'ObjectTextureWoodNode'
    bl_label = 'Wood'

    noise_basis = enum_property_copy(bpy.types.WoodTexture, 'noise_basis')
    noise_basis_value = enum_property_value_prop('noise_basis')
    noise_basis_2 = enum_property_copy(bpy.types.WoodTexture, 'noise_basis_2')
    noise_basis_2_value = enum_property_value_prop('noise_basis_2')
    noise_hard = enum_property_copy(bpy.types.WoodTexture, 'noise_type')
    noise_hard_value = enum_property_value_prop('noise_hard')
    wood_type = enum_property_copy(bpy.types.WoodTexture, 'wood_type')
    wood_type_value = enum_property_value_prop('wood_type')

    def draw_buttons(self, context, layout):
        layout.prop(self, "noise_basis")
        layout.prop(self, "noise_basis_2")
        layout.prop(self, "noise_hard")
        layout.prop(self, "wood_type")

    def init(self, context):
        self.inputs.new('NodeSocketVector', "Position")
        self.inputs.new('NodeSocketFloat', "Size")
        self.inputs.new('NodeSocketFloat', "Turbulence")
        self.outputs.new('NodeSocketFloat', "Intensity")
        self.outputs.new('NodeSocketVector', "Normal")

    def compile(self, compiler):
        node = compiler.add_node("TEX_PROC_WOOD")
        compiler.map_input(0, node.inputs["position"])
        compiler.map_input(1, node.inputs["size"])
        compiler.map_input(2, node.inputs["turbulence"])
        node.inputs["noise_basis"].set_value(self.noise_basis_value)
        node.inputs["noise_basis_2"].set_value(self.noise_basis_2_value)
        node.inputs["noise_hard"].set_value(self.noise_hard_value)
        node.inputs["wood_type"].set_value(self.wood_type_value)
        compiler.map_output(0, node.outputs["intensity"])
        compiler.map_output(1, node.outputs["normal"])


class TextureMusgraveNode(CommonNodeBase, ObjectNode):
    '''Musgrave texture'''
    bl_idname = 'ObjectTextureMusgraveNode'
    bl_label = 'Musgrave'

    noise_basis = enum_property_copy(bpy.types.MusgraveTexture, 'noise_basis')
    noise_basis_value = enum_property_value_prop('noise_basis')
    musgrave_type = enum_property_copy(bpy.types.MusgraveTexture, 'musgrave_type')
    musgrave_type_value = enum_property_value_prop('musgrave_type')

    def draw_buttons(self, context, layout):
        layout.prop(self, "noise_basis")
        layout.prop(self, "musgrave_type")

    def init(self, context):
        self.inputs.new('NodeSocketVector', "Position")
        self.inputs.new('NodeSocketFloat', "Size")
        self.inputs.new('NodeSocketFloat', "Dimension")
        self.inputs.new('NodeSocketFloat', "Lacunarity")
        self.inputs.new('NodeSocketFloat', "Octaves")
        self.inputs.new('NodeSocketFloat', "Intensity")
        self.inputs.new('NodeSocketFloat', "Offset")
        self.inputs.new('NodeSocketFloat', "Gain")
        self.outputs.new('NodeSocketFloat', "Intensity")
        self.outputs.new('NodeSocketVector', "Normal")

    def compile(self, compiler):
        node = compiler.add_node("TEX_PROC_MUSGRAVE")
        compiler.map_input(0, node.inputs["position"])
        compiler.map_input(1, node.inputs["size"])
        compiler.map_input(2, node.inputs["dimension"])
        compiler.map_input(3, node.inputs["lacunarity"])
        compiler.map_input(4, node.inputs["octaves"])
        compiler.map_input(5, node.inputs["intensity"])
        compiler.map_input(6, node.inputs["offset"])
        compiler.map_input(7, node.inputs["gain"])
        node.inputs["noise_basis"].set_value(self.noise_basis_value)
        node.inputs["musgrave_type"].set_value(self.musgrave_type_value)
        compiler.map_output(0, node.outputs["intensity"])
        compiler.map_output(1, node.outputs["normal"])


class TextureMarbleNode(CommonNodeBase, ObjectNode):
    '''Marble texture'''
    bl_idname = 'ObjectTextureMarbleNode'
    bl_label = 'Marble'

    depth = IntProperty(name="Depth", default=2)
    noise_basis = enum_property_copy(bpy.types.MarbleTexture, 'noise_basis')
    noise_basis_value = enum_property_value_prop('noise_basis')
    noise_basis_2 = enum_property_copy(bpy.types.MarbleTexture, 'noise_basis_2')
    noise_basis_2_value = enum_property_value_prop('noise_basis_2')
    noise_hard = enum_property_copy(bpy.types.MarbleTexture, 'noise_type')
    noise_hard_value = enum_property_value_prop('noise_hard')
    marble_type = enum_property_copy(bpy.types.MarbleTexture, 'marble_type')
    marble_type_value = enum_property_value_prop('marble_type')

    def draw_buttons(self, context, layout):
        layout.prop(self, "depth")
        layout.prop(self, "noise_basis")
        layout.prop(self, "noise_basis_2")
        layout.prop(self, "noise_hard")
        layout.prop(self, "marble_type")

    def init(self, context):
        self.inputs.new('NodeSocketVector', "Position")
        self.inputs.new('NodeSocketFloat', "Size")
        self.inputs.new('NodeSocketFloat', "Nabla")
        self.inputs.new('NodeSocketFloat', "Turbulence")
        self.outputs.new('NodeSocketFloat', "Intensity")
        self.outputs.new('NodeSocketVector', "Normal")

    def compile(self, compiler):
        node = compiler.add_node("TEX_PROC_MARBLE")
        compiler.map_input(0, node.inputs["position"])
        compiler.map_input(1, node.inputs["size"])
        compiler.map_input(2, node.inputs["nabla"])
        compiler.map_input(3, node.inputs["turbulence"])
        node.inputs["depth"].set_value(self.depth)
        node.inputs["noise_basis"].set_value(self.noise_basis_value)
        node.inputs["noise_basis_2"].set_value(self.noise_basis_2_value)
        node.inputs["noise_hard"].set_value(self.noise_hard_value)
        node.inputs["marble_type"].set_value(self.marble_type_value)
        compiler.map_output(0, node.outputs["intensity"])
        compiler.map_output(1, node.outputs["normal"])


class TextureStucciNode(CommonNodeBase, ObjectNode):
    '''Stucci texture'''
    bl_idname = 'ObjectTextureStucciNode'
    bl_label = 'Stucci'

    noise_basis = enum_property_copy(bpy.types.StucciTexture, 'noise_basis')
    noise_basis_value = enum_property_value_prop('noise_basis')
    noise_hard = enum_property_copy(bpy.types.StucciTexture, 'noise_type')
    noise_hard_value = enum_property_value_prop('noise_hard')
    stucci_type = enum_property_copy(bpy.types.StucciTexture, 'stucci_type')
    stucci_type_value = enum_property_value_prop('stucci_type')

    def draw_buttons(self, context, layout):
        layout.prop(self, "noise_basis")
        layout.prop(self, "noise_hard")
        layout.prop(self, "stucci_type")

    def init(self, context):
        self.inputs.new('NodeSocketVector', "Position")
        self.inputs.new('NodeSocketFloat', "Size")
        self.inputs.new('NodeSocketFloat', "Turbulence")
        self.outputs.new('NodeSocketFloat', "Intensity")
        self.outputs.new('NodeSocketVector', "Normal")

    def compile(self, compiler):
        node = compiler.add_node("TEX_PROC_STUCCI")
        compiler.map_input(0, node.inputs["position"])
        compiler.map_input(1, node.inputs["size"])
        compiler.map_input(2, node.inputs["turbulence"])
        node.inputs["noise_basis"].set_value(self.noise_basis_value)
        node.inputs["noise_hard"].set_value(self.noise_hard_value)
        node.inputs["stucci_type"].set_value(self.stucci_type_value)
        compiler.map_output(0, node.outputs["intensity"])
        compiler.map_output(1, node.outputs["normal"])


class TextureDistNoiseNode(CommonNodeBase, ObjectNode):
    '''DistNoise texture'''
    bl_idname = 'ObjectTextureDistNoiseNode'
    bl_label = 'Distorted Noise'

    noise_basis = enum_property_copy(bpy.types.DistortedNoiseTexture, 'noise_basis')
    noise_basis_value = enum_property_value_prop('noise_basis')
    noise_dist = enum_property_copy(bpy.types.DistortedNoiseTexture, 'noise_distortion')
    noise_dist_value = enum_property_value_prop('noise_dist')

    def draw_buttons(self, context, layout):
        layout.prop(self, "noise_dist")
        layout.prop(self, "noise_basis")

    def init(self, context):
        self.inputs.new('NodeSocketVector', "Position")
        self.inputs.new('NodeSocketFloat', "Size")
        self.inputs.new('NodeSocketFloat', "Nabla")
        self.inputs.new('NodeSocketFloat', "Amount")
        self.outputs.new('NodeSocketFloat', "Intensity")
        self.outputs.new('NodeSocketVector', "Normal")

    def compile(self, compiler):
        node = compiler.add_node("TEX_PROC_DISTNOISE")
        compiler.map_input(0, node.inputs["position"])
        compiler.map_input(1, node.inputs["size"])
        compiler.map_input(2, node.inputs["nabla"])
        compiler.map_input(3, node.inputs["dist_amount"])
        node.inputs["noise_dist"].set_value(self.noise_dist_value)
        node.inputs["noise_basis"].set_value(self.noise_basis_value)
        compiler.map_output(0, node.outputs["intensity"])
        compiler.map_output(1, node.outputs["normal"])


class TextureMagicNode(CommonNodeBase, ObjectNode):
    '''Magic texture'''
    bl_idname = 'ObjectTextureMagicNode'
    bl_label = 'Magic'

    depth = IntProperty(name="Depth", default=2)

    def draw_buttons(self, context, layout):
        layout.prop(self, "depth")

    def init(self, context):
        self.inputs.new('NodeSocketVector', "Position")
        self.inputs.new('NodeSocketFloat', "Turbulence")
        self.outputs.new('NodeSocketFloat', "Intensity")
        self.outputs.new('NodeSocketColor', "Color")
        self.outputs.new('NodeSocketVector', "Normal")

    def compile(self, compiler):
        node = compiler.add_node("TEX_PROC_MAGIC")
        compiler.map_input(0, node.inputs["position"])
        compiler.map_input(1, node.inputs["turbulence"])

        node.inputs["depth"].set_value(self.depth)
        compiler.map_output(0, node.outputs["intensity"])
        compiler.map_output(1, node.outputs["color"])
        compiler.map_output(2, node.outputs["normal"])


class TextureVoronoiNode(CommonNodeBase, ObjectNode):
    '''Voronoi texture'''
    bl_idname = 'ObjectTextureVoronoiNode'
    bl_label = 'Voronoi'

    distance_metric = enum_property_copy(bpy.types.VoronoiTexture, 'distance_metric')
    distance_metric_value = enum_property_value_prop('distance_metric')
    color_type = enum_property_copy(bpy.types.VoronoiTexture, 'color_mode')
    color_type_value = enum_property_value_prop('color_type')

    def draw_buttons(self, context, layout):
        layout.prop(self, "distance_metric")
        layout.prop(self, "color_type")

    def init(self, context):
        self.inputs.new('NodeSocketVector', "Position")
        self.inputs.new('NodeSocketFloat', "Size")
        self.inputs.new('NodeSocketFloat', "Minkowski Exponent")
        self.inputs.new('NodeSocketFloat', "Weight 1")
        self.inputs.new('NodeSocketFloat', "Weight 2")
        self.inputs.new('NodeSocketFloat', "Weight 3")
        self.inputs.new('NodeSocketFloat', "Weight 4")
        self.outputs.new('NodeSocketFloat', "Intensity")
        self.outputs.new('NodeSocketColor', "Color")
        self.outputs.new('NodeSocketVector', "Normal")

    def compile(self, compiler):
        node = compiler.add_node("TEX_PROC_VORONOI")
        # XXX TODO use of scale factors is inconsistent in BLI ...
        #compiler.map_input(0, node.inputs["position"])
        #compiler.map_input(1, node.inputs["noise_size"])
        node_scale = compiler.add_node("DIV_FLOAT3")
        compiler.map_input(0, node_scale.inputs[0])
        compiler.map_input(1, node_scale.inputs[1])
        compiler.link(node_scale.outputs[0], node.inputs["position"])

        compiler.map_input(2, node.inputs["minkowski_exponent"])
        compiler.map_input(3, node.inputs["w1"])
        compiler.map_input(4, node.inputs["w2"])
        compiler.map_input(5, node.inputs["w3"])
        compiler.map_input(6, node.inputs["w4"])
        node.inputs["color_type"].set_value(self.color_type_value)
        node.inputs["distance_metric"].set_value(self.distance_metric_value)
        compiler.map_output(0, node.outputs["intensity"])
        compiler.map_output(1, node.outputs["color"])
        compiler.map_output(2, node.outputs["normal"])

###############################################################################

class ImageSampleNode(CommonNodeBase, ObjectNode):
    '''Sample an image'''
    bl_idname = 'ImageSampleNode'
    bl_label = 'Image Sample'

    bl_id_property_type = 'IMAGE'

    def draw_buttons(self, context, layout):
        layout.template_ID(self, "id")

    def eval_dependencies(self, depsnode):
        ima = self.id
        if ima:
            depsnode.add_image_relation(ima, 'PARAMETERS')

    def init(self, context):
        self.inputs.new('NodeSocketVector', "UV")
        self.outputs.new('NodeSocketColor', "Color")

    def compile(self, compiler):
        if self.id is None:
            return
        ima = compiler.get_id_key(self.id)
        node = compiler.add_node("IMAGE_SAMPLE")
        node.inputs[0].set_value(ima)
        compiler.map_input(0, node.inputs[1])
        compiler.map_output(0, node.outputs[0])

###############################################################################

def register():
    bpy.utils.register_module(__name__)

def unregister():
    bpy.utils.unregister_module(__name__)

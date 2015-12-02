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
from nodeitems_utils import NodeCategory, NodeItem
from mathutils import *
from collections import OrderedDict

###############################################################################
# Socket Types

class GeometrySocket(NodeSocket):
    '''Geometry data socket'''
    bl_idname = 'GeometrySocket'
    bl_label = 'Geometry'

    def draw(self, context, layout, node, text):
        layout.label(text)

    def draw_color(self, context, node):
        return (1.0, 0.4, 0.216, 1.0)


###############################################################################


# our own base class with an appropriate poll function,
# so the categories only show up in our own tree type
class ObjectNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        tree = context.space_data.edit_tree
        return tree and tree.bl_idname == 'ObjectNodeTree'

class GeometryNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        tree = context.space_data.edit_tree
        return tree and tree.bl_idname == 'GeometryNodeTree'

class ForceFieldNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        tree = context.space_data.edit_tree
        return tree and tree.bl_idname == 'ForceFieldNodeTree'

node_categories = [
    ObjectNodeCategory("COMPONENTS", "Components", items=[
        NodeItem("GeometryNode"),
        NodeItem("ForceFieldNode"),
        ]),
    
    GeometryNodeCategory("GEO_INPUT", "Input", items=[
        NodeItem("ObjectIterationNode"),
        NodeItem("GeometryMeshLoadNode"),
        ]),
    GeometryNodeCategory("GEO_OUTPUT", "Output", items=[
        NodeItem("GeometryOutputNode"),
        ]),
    GeometryNodeCategory("GEO_MODIFIER", "Modifier", items=[
        NodeItem("GeometryMeshArrayNode"),
        ]),
    GeometryNodeCategory("GEO_CONVERTER", "Converter", items=[
        NodeItem("ObjectSeparateVectorNode"),
        NodeItem("ObjectCombineVectorNode"),
        ]),
    GeometryNodeCategory("GEO_MATH", "Math", items=[
        NodeItem("ObjectMathNode"),
        NodeItem("ObjectVectorMathNode"),
        ]),
    
    ForceFieldNodeCategory("FORCE_INPUT", "Input", items=[
        NodeItem("ForcePointDataNode"),
        ]),
    ForceFieldNodeCategory("FORCE_OUTPUT", "Output", items=[
        NodeItem("ForceOutputNode"),
        ]),
    ForceFieldNodeCategory("FORCE_CONVERTER", "Converter", items=[
        NodeItem("ObjectSeparateVectorNode"),
        NodeItem("ObjectCombineVectorNode"),
        ]),
    ForceFieldNodeCategory("FORCE_MATH", "Math", items=[
        NodeItem("ObjectMathNode"),
        NodeItem("ObjectVectorMathNode"),
        ]),
    ForceFieldNodeCategory("FORCE_GEOMETRY", "Geometry", items=[
        NodeItem("ForceClosestPointNode"),
        ]),
    ]

###############################################################################

# Utility dict type that works like RNA collections,
# i.e. accepts both string keys and integer indices
class StringDict(OrderedDict):
    def __getitem__(self, key):
        if isinstance(key, int):
            return super().__getitem__(list(super().keys())[key])
        else:
            return super().__getitem__(key)

    def __contains__(self, key):
        if isinstance(key, int):
            return super().__contains__(list(super().keys())[key])
        else:
            return super().__contains__(key)

# Wrapper classes to make constructing node graphs as convenient as possible
# RNA does not allow collections of temporary (node,socket) pairs,
# so we use python wrappers to pass them around as a single object

class InputWrapper:
    def __init__(self, gnode, ginput):
        self.gnode = gnode
        self.ginput = ginput

    def link(self, from_output):
        self.gnode.set_input_link(self.ginput, from_output.gnode, from_output.goutput)

    def set_value(self, value):
        base_type = self.ginput.typedesc.base_type
        if base_type == 'FLOAT':
            self.gnode.set_value_float(self.ginput, value)
        elif base_type == 'FLOAT3':
            self.gnode.set_value_float3(self.ginput, value)
        elif base_type == 'FLOAT4':
            self.gnode.set_value_float4(self.ginput, value)
        elif base_type == 'INT':
            self.gnode.set_value_int(self.ginput, value)
        elif base_type == 'MATRIX44':
            self.gnode.set_value_matrix44(self.ginput, value)

class OutputWrapper:
    def __init__(self, gnode, goutput):
        self.gnode = gnode
        self.goutput = goutput

class NodeWrapper:
    def __init__(self, gnode):
        self.gnode = gnode
        self.inputs = StringDict([ (i.name, InputWrapper(self.gnode, i)) for i in self.gnode.inputs ])
        self.outputs = StringDict([ (o.name, OutputWrapper(self.gnode, o)) for o in self.gnode.outputs ])

# Compiler class for converting nodes
class NodeCompiler:
    def __init__(self, context, graph):
        self.context = context
        self.graph = graph
        self.bnode = None
        self.bnode_inputs = None
        self.bnode_outputs = None

    def add_node(self, type, name=""):
        node = self.graph.add_node(type, name)
        if node is None:
            raise Exception("Can not add node of type %r" % type)
        return NodeWrapper(node)

    def graph_output(self, name):
        out_node, out_socket = self.graph.get_output(name)
        return InputWrapper(out_node, out_node.inputs[out_socket])

    def map_input(self, key, socket):
        if key not in self.bnode_inputs:
            raise KeyError("Input %r not found in node %r" % (key, self.bnode))
        socket.link(self.bnode_inputs[key])

    def map_output(self, key, socket):
        if key not in self.bnode_outputs:
            raise KeyError("Output %r not found in node %r" % (key, self.bnode))
        self.bnode_outputs[key].link(socket)

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
            input_map[dst].link(output_map[src])

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

class ObjectNodeTree(NodeTreeBase, NodeTree):
    '''Object component nodes'''
    bl_idname = 'ObjectNodeTree'
    bl_label = 'Object Nodes'
    bl_icon = 'OBJECT_DATA'

    @classmethod
    def get_from_context(cls, context):
        ob = context.object
        if ob:
            return ob.node_tree, ob.node_tree, ob
        else:
            return None, None, None


class ObjectNodeBase():
    @classmethod
    def poll(cls, ntree):
        return ntree.bl_idname == 'ObjectNodeTree'


class GeometryNode(ObjectNodeBase, ObjectNode):
    '''Geometry'''
    bl_idname = 'GeometryNode'
    bl_label = 'Geometry'
    bl_icon = 'MESH_DATA'

    bl_id_property_type = 'NODETREE'
    @classmethod
    def bl_id_property_poll(cls, ntree):
        return ntree.bl_idname == 'GeometryNodeTree'

    def draw_buttons(self, context, layout):
        layout.template_ID(self, "id", new="object_nodes.geometry_nodes_new")

    def compile(self, compiler):
        pass


class ForceFieldNode(ObjectNodeBase, ObjectNode):
    '''Force Field'''
    bl_idname = 'ForceFieldNode'
    bl_label = 'Force Field'
    bl_icon = 'FORCE_FORCE'

    bl_id_property_type = 'NODETREE'
    @classmethod
    def bl_id_property_poll(cls, ntree):
        return ntree.bl_idname == 'ForceFieldNodeTree'

    def draw_buttons(self, context, layout):
        layout.template_ID(self, "id", new="object_nodes.force_field_nodes_new")

    def compile(self, compiler):
        pass

###############################################################################

class GeometryNodeTree(NodeTreeBase, NodeTree):
    '''Geometry nodes'''
    bl_idname = 'GeometryNodeTree'
    bl_label = 'Geometry Nodes'
    bl_icon = 'MESH_DATA'

    # does not show up in the editor header
    @classmethod
    def poll(cls, context):
        return False


class GeometryNodeBase():
    @classmethod
    def poll(cls, ntree):
        return ntree.bl_idname == 'GeometryNodeTree'


class GeometryOutputNode(GeometryNodeBase, ObjectNode):
    '''Geometry output'''
    bl_idname = 'GeometryOutputNode'
    bl_label = 'Output'

    def init(self, context):
        self.inputs.new('GeometrySocket', "")

    def compile(self, compiler):
        compiler.map_input(0, compiler.graph_output("mesh"))


class GeometryMeshLoadNode(GeometryNodeBase, ObjectNode):
    '''Mesh object data'''
    bl_idname = 'GeometryMeshLoadNode'
    bl_label = 'Mesh'

    def init(self, context):
        self.outputs.new('GeometrySocket', "")

    def compile(self, compiler):
        node = compiler.add_node("MESH_LOAD", self.name)
        compiler.map_output(0, node.outputs[0])


class GeometryMeshArrayNode(GeometryNodeBase, ObjectNode):
    '''Make a number of transformed copies of a mesh'''
    bl_idname = 'GeometryMeshArrayNode'
    bl_label = 'Array'

    def init(self, context):
        self.inputs.new('GeometrySocket', "")
        self.inputs.new('NodeSocketInt', "Count")
        self.inputs.new('NodeSocketVector', "Offset")
        self.outputs.new('GeometrySocket', "")

    def compile(self, compiler):
        node = compiler.add_node("MESH_ARRAY", self.name+"MOD")
        node_tfm = compiler.add_node("LOCROTSCALE_TO_MATRIX44", self.name+"TFM")

        node.inputs["transform"].link(node_tfm.outputs[0])

        compiler.map_input(0, node.inputs["mesh_in"])
        compiler.map_input(1, node.inputs["count"])
        compiler.map_input(2, node_tfm.inputs["loc"])
        compiler.map_output(0, node.outputs["mesh_out"])


###############################################################################


class ForceFieldNodeTree(NodeTreeBase, NodeTree):
    '''Force field nodes'''
    bl_idname = 'ForceFieldNodeTree'
    bl_label = 'Force Field Nodes'
    bl_icon = 'FORCE_FORCE'

    # does not show up in the editor header
    @classmethod
    def poll(cls, context):
        return False


class ForceNodeBase():
    @classmethod
    def poll(cls, ntree):
        return ntree.bl_idname == 'ForceFieldNodeTree'


class ForceOutputNode(ForceNodeBase, ObjectNode):
    '''Force Output'''
    bl_idname = 'ForceOutputNode'
    bl_label = 'Output'
    bl_icon = 'FORCE_FORCE'

    def init(self, context):
        self.inputs.new('NodeSocketVector', "Force")
        self.inputs.new('NodeSocketVector', "Impulse")

    def compile(self, compiler):
        compiler.map_input(0, compiler.graph_output("force"))
        compiler.map_input(1, compiler.graph_output("impulse"))


class PointDataNode(ForceNodeBase, ObjectNode):
    '''Input data of physical points'''
    bl_idname = 'ForcePointDataNode'
    bl_label = 'Point Data'

    def init(self, context):
        self.outputs.new('NodeSocketVector', "Position")
        self.outputs.new('NodeSocketVector', "Velocity")

    def compile(self, compiler):
        node = compiler.add_node("POINT_POSITION", self.name+"P")
        compiler.map_output(0, node.outputs[0])
        node = compiler.add_node("POINT_VELOCITY", self.name+"V")
        compiler.map_output(1, node.outputs[0])


class ForceClosestPointNode(ForceNodeBase, ObjectNode):
    '''Closest point on the effector mesh'''
    bl_idname = 'ForceClosestPointNode'
    bl_label = 'Closest Point'
    bl_icon = 'FORCE_FORCE'

    def init(self, context):
        self.inputs.new('NodeSocketVector', "Vector")
        self.outputs.new('NodeSocketVector', "Position")
        self.outputs.new('NodeSocketVector', "Normal")
        self.outputs.new('NodeSocketVector', "Tangent")

    def compile(self, compiler):
        node = compiler.add_node("EFFECTOR_CLOSEST_POINT", self.name+"N")
        obnode = compiler.add_node("EFFECTOR_OBJECT", self.name+"O")
        
        node.inputs["object"].link(obnode.outputs[0])

        compiler.map_input(0, node.inputs["vector"])
        compiler.map_output(0, node.outputs["position"])
        compiler.map_output(1, node.outputs["normal"])
        compiler.map_output(2, node.outputs["tangent"])

###############################################################################

class ObjectNodesNew(Operator):
    """Create new object node tree"""
    bl_idname = "object_nodes.object_nodes_new"
    bl_label = "New"
    bl_options = {'REGISTER', 'UNDO'}

    name = StringProperty(
            name="Name",
            )

    def execute(self, context):
    	return bpy.ops.node.new_node_tree(type='ObjectNodeTree', name="ObjectNodes")


class ObjectNodeEdit(Operator):
    """Open a node for editing"""
    bl_idname = "object_nodes.node_edit"
    bl_label = "Edit"
    bl_options = {'REGISTER', 'UNDO'}

    exit = BoolProperty(name="Exit", description="Exit current node tree", default=False)

    @staticmethod
    def get_node(context):
        if hasattr(context, "node"):
            return context.node
        else:
            return context.active_node

    @classmethod
    def poll(cls, context):
        space = context.space_data
        if space.type != 'NODE_EDITOR':
            return False
        return True

    def execute(self, context):
        space = context.space_data
        node = self.get_node(context)
        has_tree = node and hasattr(node, "id") and node.id and isinstance(node.id, bpy.types.NodeTree)
        exit = self.exit or not has_tree

        if exit:
            space.path.pop()
        else:
            space.path.append(node.id, node)

        return {'FINISHED'}


class GeometryNodesNew(Operator):
    """Create new geometry node tree"""
    bl_idname = "object_nodes.geometry_nodes_new"
    bl_label = "New"
    bl_options = {'REGISTER', 'UNDO'}

    name = StringProperty(
            name="Name",
            )

    def execute(self, context):
        return bpy.ops.node.new_node_tree(type='GeometryNodeTree', name="GeometryNodes")


class ForceFieldNodesNew(Operator):
    """Create new force field node tree"""
    bl_idname = "object_nodes.force_field_nodes_new"
    bl_label = "New"
    bl_options = {'REGISTER', 'UNDO'}

    name = StringProperty(
            name="Name",
            )

    def execute(self, context):
        return bpy.ops.node.new_node_tree(type='ForceFieldNodeTree', name="ForceFieldNodes")


###############################################################################

keymaps = []

def register():
    # XXX HACK, needed to have access to the operator, registration needs cleanup once moved out of operators
    bpy.utils.register_class(ObjectNodeEdit)

    nodeitems_utils.register_node_categories("OBJECT_NODES", node_categories)

    # create keymap
    wm = bpy.context.window_manager
    km = wm.keyconfigs.default.keymaps.new(name="Node Generic", space_type='NODE_EDITOR')
    
    kmi = km.keymap_items.new(bpy.types.OBJECT_NODES_OT_node_edit.bl_idname, 'TAB', 'PRESS')
    
    kmi = km.keymap_items.new(bpy.types.OBJECT_NODES_OT_node_edit.bl_idname, 'TAB', 'PRESS', ctrl=True)
    kmi.properties.exit = True
    
    keymaps.append(km)

def unregister():
    nodeitems_utils.unregister_node_categories("OBJECT_NODES")

    # remove keymap
    wm = bpy.context.window_manager
    for km in keymaps:
        wm.keyconfigs.default.keymaps.remove(km)
    keymaps.clear()

register()

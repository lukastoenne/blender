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
from common_nodes import NodeTreeBase

###############################################################################

# our own base class with an appropriate poll function,
# so the categories only show up in our own tree type
class ForceFieldNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        tree = context.space_data.edit_tree
        return tree and tree.bl_idname == 'ForceFieldNodeTree'

node_categories = [
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
        
        compiler.link(obnode.outputs[0], node.inputs["object"])

        compiler.map_input(0, node.inputs["vector"])
        compiler.map_output(0, node.outputs["position"])
        compiler.map_output(1, node.outputs["normal"])
        compiler.map_output(2, node.outputs["tangent"])

###############################################################################

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

def register():
    bpy.utils.register_module(__name__)

    nodeitems_utils.register_node_categories("FORCEFIELD_NODES", node_categories)

def unregister():
    nodeitems_utils.unregister_node_categories("FORCEFIELD_NODES")

    bpy.utils.unregister_module(__name__)

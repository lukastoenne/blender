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
from bpy.types import Operator, ObjectNode, NodeTree, Node
from bpy.props import *
from nodeitems_utils import NodeCategory, NodeItem
from mathutils import *
from common_nodes import NodeTreeBase, NodeBase, DynamicSocketListNode, enum_property_copy, enum_property_value_prop
import group_nodes

###############################################################################


# our own base class with an appropriate poll function,
# so the categories only show up in our own tree type
class HairNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        tree = context.space_data.edit_tree
        return tree and tree.bl_idname == 'HairNodeTree'

###############################################################################

class HairNodeTree(NodeTreeBase, NodeTree):
    '''Hair nodes'''
    bl_idname = 'HairNodeTree'
    bl_label = 'Hair Nodes'
    bl_icon = 'MESH_DATA'

    # does not show up in the editor header
    @classmethod
    def poll(cls, context):
        return False

    def init_default(self):
        pass


class HairNodeBase(NodeBase):
    @classmethod
    def poll(cls, ntree):
        return ntree.bl_idname == 'HairNodeTree'

###############################################################################

class HairNodesNew(Operator):
    """Create new hair node tree"""
    bl_idname = "object_nodes.hair_nodes_new"
    bl_label = "New"
    bl_options = {'REGISTER', 'UNDO'}

    name = StringProperty(
            name="Name",
            default="HairNodes",
            )

    @classmethod
    def make_node_tree(cls, name="HairNodes"):
        ntree = bpy.data.node_groups.new(name, HairNodeTree.bl_idname)
        if ntree:
            ntree.init_default()
        return ntree

    def execute(self, context):
        node = getattr(context, "node", None)
        ntree = self.make_node_tree(self.name)
        if ntree is None:
            return {'CANCELLED'}
        if node:
            node.id = ntree
        return {'FINISHED'}

###############################################################################

class HairInputNode(HairNodeBase, ObjectNode):
    '''Hair inputs'''
    bl_idname = 'HairInputNode'
    bl_label = 'Hair'

    def init(self, context):
        self.outputs.new('NodeSocketVector', "Location")
        self.outputs.new('NodeSocketFloat', "Parameter")
        self.outputs.new('TransformSocket', "Target")

    def compile(self, compiler):
        compiler.map_output(0, compiler.graph_input("location"))
        compiler.map_output(1, compiler.graph_input("parameter"))
        compiler.map_output(2, compiler.graph_input("target"))

class HairDeformNode(HairNodeBase, ObjectNode):
    '''Hair displacement result'''
    bl_idname = 'HairDeformNode'
    bl_label = 'Hair Displacement'

    def init(self, context):
        self.inputs.new('NodeSocketVector', "Target")

    def compile(self, compiler):
        compiler.map_input(0, compiler.graph_output("offset"))

###############################################################################

def register():
    bpy.utils.register_module(__name__)
    gnode, ginput, goutput = group_nodes.make_node_group_types(
        "Hair", HairNodeTree, HairNodeBase)

    node_categories = [
        HairNodeCategory("GEO_INPUT", "Input", items=[
            NodeItem("HairInputNode"),
            NodeItem(ginput.bl_idname),
            NodeItem("ObjectValueFloatNode"),
            NodeItem("ObjectValueIntNode"),
            NodeItem("ObjectValueVectorNode"),
            NodeItem("ObjectValueColorNode"),
            ]),
        HairNodeCategory("GEO_OUTPUT", "Output", items=[
            NodeItem("HairDeformNode"),
            NodeItem(goutput.bl_idname),
            ]),
        HairNodeCategory("GEO_CONVERTER", "Converter", items=[
            NodeItem("ObjectSeparateVectorNode"),
            NodeItem("ObjectCombineVectorNode"),
            NodeItem("ObjectSeparateMatrixNode"),
            NodeItem("ObjectCombineMatrixNode"),
            ]),
        HairNodeCategory("GEO_MATH", "Math", items=[
            NodeItem("ObjectMathNode"),
            NodeItem("ObjectVectorMathNode"),
            NodeItem("ObjectMatrixMathNode"),
            NodeItem("ObjectTransformVectorNode"),
            NodeItem("ObjectTranslationTransformNode"),
            NodeItem("ObjectEulerTransformNode"),
            NodeItem("ObjectAxisAngleTransformNode"),
            NodeItem("ObjectScaleTransformNode"),
            NodeItem("ObjectGetTranslationNode"),
            NodeItem("ObjectGetEulerNode"),
            NodeItem("ObjectGetAxisAngleNode"),
            NodeItem("ObjectGetScaleNode"),
            NodeItem("ObjectRandomNode"),
            ]),
        HairNodeCategory("GEO_TEXTURE", "Texture", items=[
            NodeItem("ImageSampleNode"),
            NodeItem("ObjectTextureCloudsNode"),
            NodeItem("ObjectTextureDistNoiseNode"),
            NodeItem("ObjectTextureGaborNoiseNode"),
            NodeItem("ObjectTextureMagicNode"),
            NodeItem("ObjectTextureMarbleNode"),
            NodeItem("ObjectTextureMusgraveNode"),
            NodeItem("ObjectTextureStucciNode"),
            NodeItem("ObjectTextureVoronoiNode"),
            NodeItem("ObjectTextureWoodNode"),
            ]),
        HairNodeCategory("GEO_CURVE", "Curve", items=[
            NodeItem("CurvePathNode"),
            ]),
        group_nodes.GroupNodeCategory("HAIR", gnode, ginput, goutput),
        ]
    nodeitems_utils.register_node_categories("HAIR_NODES", node_categories)

def unregister():
    nodeitems_utils.unregister_node_categories("HAIR_NODES")

    bpy.utils.unregister_module(__name__)

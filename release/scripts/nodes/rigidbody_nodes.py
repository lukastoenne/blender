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
from bpy.types import PropertyGroup, Operator, ObjectNode, NodeTree, Node
from bpy.props import *
from nodeitems_utils import NodeCategory, NodeItem
from mathutils import *
from common_nodes import NodeTreeBase, NodeBase, DynamicSocketListNode, enum_property_copy, enum_property_value_prop
import group_nodes

###############################################################################


# our own base class with an appropriate poll function,
# so the categories only show up in our own tree type
class RigidBodyNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        tree = context.space_data.edit_tree
        return tree and tree.bl_idname == 'RigidBodyNodeTree'

###############################################################################

class RigidBodyNodeTree(NodeTreeBase, NodeTree):
    '''Smoke simulation nodes'''
    bl_idname = 'RigidBodyNodeTree'
    bl_label = 'Rigid Body Nodes'
    bl_icon = 'PHYSICS'

    # does not show up in the editor header
    @classmethod
    def poll(cls, context):
        return False

    def init_default(self):
        #out = self.nodes.new(OutputNode.bl_idname)
        #out.location = (100, 20)
        pass


class RigidBodyNodeBase(NodeBase):
    @classmethod
    def poll(cls, ntree):
        return ntree.bl_idname == 'RigidBodyNodeTree'

###############################################################################


###############################################################################

class RigidBodyNodesNew(Operator):
    """Create new rigid body simulation node tree"""
    bl_idname = "object_nodes.rigidbody_nodes_new"
    bl_label = "New"
    bl_options = {'REGISTER', 'UNDO'}

    name = StringProperty(
            name="Name",
            default="RigidBodyNodes",
            )

    @classmethod
    def make_node_tree(cls, name="RigidBodyNodes"):
        ntree = bpy.data.node_groups.new(name, RigidBodyNodeTree.bl_idname)
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

def register():
    gnode, ginput, goutput = group_nodes.make_node_group_types(
        "RigidBody", RigidBodyNodeTree, RigidBodyNodeBase)
    bpy.utils.register_module(__name__)

    node_categories = [
        RigidBodyNodeCategory("SMO_INPUT", "Input", items=[
            NodeItem(ginput.bl_idname),
            NodeItem("ObjectValueFloatNode"),
            NodeItem("ObjectValueIntNode"),
            NodeItem("ObjectValueVectorNode"),
            NodeItem("ObjectValueColorNode"),
            ]),
        RigidBodyNodeCategory("SMO_OUTPUT", "Output", items=[
            NodeItem(goutput.bl_idname),
            ]),
        RigidBodyNodeCategory("SMO_CONVERTER", "Converter", items=[
            NodeItem("ObjectSeparateVectorNode"),
            NodeItem("ObjectCombineVectorNode"),
            ]),
        RigidBodyNodeCategory("SMO_MATH", "Math", items=[
            NodeItem("ObjectMathNode"),
            NodeItem("ObjectVectorMathNode"),
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
        RigidBodyNodeCategory("SMO_TEXTURE", "Texture", items=[
            NodeItem("ObjectTextureCloudsNode"),
            NodeItem("ObjectTextureDistNoiseNode"),
            NodeItem("ObjectTextureMagicNode"),
            NodeItem("ObjectTextureMarbleNode"),
            NodeItem("ObjectTextureMusgraveNode"),
            NodeItem("ObjectTextureStucciNode"),
            NodeItem("ObjectTextureVoronoiNode"),
            NodeItem("ObjectTextureWoodNode"),
            ]),
        group_nodes.GroupNodeCategory("SMO", gnode, ginput, goutput),
        ]
    nodeitems_utils.register_node_categories("RIGIDBODY_NODES", node_categories)

def unregister():
    nodeitems_utils.unregister_node_categories("RIGIDBODY_NODES")

    bpy.utils.unregister_module(__name__)

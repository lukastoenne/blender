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

# our own base class with an appropriate poll function,
# so the categories only show up in our own tree type
class ObjectNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        return context.space_data.tree_type == 'ObjectNodeTree'

node_categories = [
    ObjectNodeCategory("COMPONENTS", "Components", items=[
        NodeItem("ForceFieldNode"),
        ]),
    ]

###############################################################################

class ObjectNodeTree(NodeTree):
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

    @classmethod
    def register(cls):
        global node_categories
        nodeitems_utils.register_node_categories("OBJECT_NODES", node_categories)

    @classmethod
    def unregister(cls):
        nodeitems_utils.unregister_node_categories("OBJECT_NODES")


class ObjectNodeBase():
    @classmethod
    def poll(cls, ntree):
        return ntree.bl_idname == 'ObjectNodeTree'


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
        layout.operator("object_nodes.node_edit")

###############################################################################

class ForceFieldNodeTree(NodeTree):
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


class ForceForceNode(ForceNodeBase, ObjectNode):
    '''Force'''
    bl_idname = 'ForceForceNode'
    bl_label = 'Force'
    bl_icon = 'FORCE_FORCE'

    def init(self, context):
        self.inputs.new('NodeSocketFloat', "Strength")

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
        
        return cls.get_node(context) is not None

    def execute(self, context):
        space = context.space_data
        node = self.get_node(context)
        has_tree = hasattr(node, "id") and node.id and isinstance(node.id, bpy.types.NodeTree)
        exit = self.exit or not has_tree

        if exit:
            space.path.pop()
        else:
            space.path.append(node.id, node)

        return {'FINISHED'}


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

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
from bpy.types import Operator, NodeTree, Node, NodeSocket, ObjectNode
from bpy.props import *

def make_node_group_types(prefix, treetype, node_base):
    ntree_idname = treetype.bl_idname
    groupnode_idname = '%sGroupNode' % prefix

    class NodeGroupNew(Operator):
        """Create new object node tree"""
        bl_idname = "object_nodes.%s_nodegroup_new" % prefix.lower()
        bl_label = "New"
        bl_options = {'REGISTER', 'UNDO'}
        bl_ntree_idname = ntree_idname

        name = StringProperty(
                name="Name",
                )

        def execute(self, context):
            return bpy.ops.node.new_node_tree(type=self.bl_ntree_idname, name="%s Node Group" % prefix)

    def internal_group_nodes(ntree, visited=None):
        if ntree is None:
            return
        if visited is None:
            visited = set()
        elif ntree in visited:
            return
        visited.add(ntree)
        
        for node in ntree.nodes:
            if not isinstance(node, GroupNode):
                continue
            yield node
            for inode in internal_group_nodes(node.id, visited):
                yield inode

    class GroupNode(node_base, ObjectNode):
        '''Group of nodes that can be used in other trees'''
        bl_idname = groupnode_idname
        bl_label = 'Group'
        bl_ntree_idname = ntree_idname

        bl_id_property_type = 'NODETREE'
        def bl_id_property_poll(self, ntree):
            if not isinstance(ntree, treetype):
                return False
            for node in internal_group_nodes(ntree):
                if node == self:
                    return False
            return True

        def poll_instance(self, ntree):
            if hasattr(super(), "poll_instance") and not super().poll_instance(ntree):
                return False
            if self.id == ntree:
                return False
            for node in internal_group_nodes(self.id):
                if node.id == ntree:
                    return False
            return True

        def draw_buttons(self, context, layout):
            layout.template_ID(self, "id", new="object_nodes.geometry_nodes_new")

        def compile(self, compiler):
            # TODO
            pass

    class GroupInputNode(node_base, ObjectNode):
        '''Inputs of the node group inside the tree'''
        bl_idname = '%sGroupInputNode' % prefix
        bl_label = 'Group Inputs'

        def compile(self, compiler):
            # TODO
            pass

    class GroupOutputNode(node_base, ObjectNode):
        '''Outputs of the node group inside the tree'''
        bl_idname = '%sGroupOutputNode' % prefix
        bl_label = 'Group Outputs'

        def compile(self, compiler):
            # TODO
            pass

    bpy.utils.register_class(NodeGroupNew)
    bpy.utils.register_class(GroupNode)
    bpy.utils.register_class(GroupInputNode)
    bpy.utils.register_class(GroupOutputNode)

    return GroupNode, GroupInputNode, GroupOutputNode

###############################################################################

def register():
    pass

def unregister():
    pass

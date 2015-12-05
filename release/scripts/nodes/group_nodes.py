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
from bpy.types import Operator, Panel, UIList, NodeTree, Node, NodeSocket, ObjectNode, PropertyGroup, BVMTypeDesc
from bpy.props import *

###############################################################################
# Group Interface

def make_node_group_interface(prefix, treetype):
    _base_type_items = [ (t.identifier, t.name, t.description, t.icon, t.value) \
                         for t in BVMTypeDesc.bl_rna.properties['base_type'].enum_items ]
    _in_out_items = [('IN', "In", "Input"), ('OUT', "Out", "Output")]

    prop_name = StringProperty(name="Name", default="Value")
    prop_base_type = EnumProperty(name="Base Type", items=_base_type_items, default='FLOAT')
    prop_in_out = EnumProperty(name="In/Out", items=_in_out_items, default='IN')

    # XXX PropertyGroup does not have a bl_idname,
    # so we can only construct a dynamic type name
    # through the python 'type()' function
    d = { "name" : prop_name,
          "base_type" : prop_base_type,
        }
    item_type = type("%sNodeGroupItem" % prefix, (PropertyGroup,), d)

    bpy.utils.register_class(item_type)

    treetype.inputs = CollectionProperty(type=item_type)
    treetype.outputs = CollectionProperty(type=item_type)

    class OperatorBase():
        @classmethod
        def poll(cls, context):
            space = context.space_data
            if space.type != 'NODE_EDITOR':
                return False
            ntree = space.edit_tree
            if not (ntree and isinstance(ntree, treetype)):
                return False
            return True

    class NodeGroupItemAdd(OperatorBase, Operator):
        """Add a node group interface socket"""
        bl_idname = "object_nodes.%s_nodegroup_item_add" % prefix.lower()
        bl_label = "Add Socket"
        bl_options = {'REGISTER', 'UNDO'}

        name = prop_name
        base_type = prop_base_type
        in_out = prop_in_out

        def execute(self, context):
            ntree = context.space_data.edit_tree
            sockets = ntree.inputs if self.in_out == 'IN' else ntree.outputs
            item = sockets.add()
            item.name = self.name
            item.base_type = self.base_type
            return {'FINISHED'}

    class NodeGroupItemRemove(OperatorBase, Operator):
        """Remove a node group interface socket"""
        bl_idname = "object_nodes.%s_nodegroup_item_remove" % prefix.lower()
        bl_label = "Remove Socket"
        bl_options = {'REGISTER', 'UNDO'}

        index = IntProperty(name="Index")

        def execute(self, context):
            ntree = context.space_data.edit_tree
            ntree.inputs.remove(self.index)
            ntree.outputs.remove(self.index)
            return {'FINISHED'}

    bpy.utils.register_class(NodeGroupItemAdd)
    bpy.utils.register_class(NodeGroupItemRemove)

    class NodeGroupItemList(UIList):
        bl_idname = "OBJECT_NODES_UL_%s_nodegroup_inputs" % prefix.lower()

        def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index, flt_flag):
            layout.prop(item, "name", text="", emboss=False)
            layout.prop(item, "base_type", text="")
            props = layout.operator(NodeGroupItemRemove.bl_idname, text="", icon='X')
            props.index = index

    bpy.utils.register_class(NodeGroupItemList)

    class NodeGroupInterfacePanel(Panel):
        """Interface setup of a node group tree"""
        bl_label = "Interface"
        bl_idname = "OBJECT_NODES_PT_%s_nodegroup_interface" % prefix.lower()
        bl_space_type = 'NODE_EDITOR'
        bl_region_type = 'UI'

        @classmethod
        def poll(cls, context):
            space = context.space_data
            if space.type != 'NODE_EDITOR':
                return False
            ntree = space.edit_tree
            if not (ntree and isinstance(ntree, treetype)):
                return False
            return True

        def draw(self, context):
            ntree = context.space_data.edit_tree
            layout = self.layout

            row = layout.row()

            col = row.column(align=True)
            col.template_list(NodeGroupItemList.bl_idname, "inputs", ntree, "inputs", ntree, "active_input")
            props = col.operator(NodeGroupItemAdd.bl_idname, text="", icon='ZOOMIN')
            props.in_out = 'IN'

            col = row.column(align=True)
            col.template_list(NodeGroupItemList.bl_idname, "outputs", ntree, "outputs", ntree, "active_output")
            props = col.operator(NodeGroupItemAdd.bl_idname, text="", icon='ZOOMIN')
            props.in_out = 'OUT'

    bpy.utils.register_class(NodeGroupInterfacePanel)

###############################################################################

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

    make_node_group_interface(prefix, treetype)

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

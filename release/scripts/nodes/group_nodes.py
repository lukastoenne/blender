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

_base_type_items = [
    ("FLOAT", "Float", "Floating point number", 0, 0),
    ("INT", "Int", "Integer number", 0, 1),
    ("VECTOR", "Vector", "3D vector", 0, 2),
    ("COLOR", "Color", "RGBA color", 0, 3),
    ("MESH", "Mesh", "Mesh data", 0, 4),
    ]

def _base_type_to_socket(base_type):
    types = {
        "FLOAT" : bpy.types.NodeSocketFloat,
        "INT" : bpy.types.NodeSocketInt,
        "VECTOR" : bpy.types.NodeSocketVector,
        "COLOR" : bpy.types.NodeSocketColor,
        "MESH" : bpy.types.GeometrySocket,
        }
    return types.get(base_type, None)

def make_node_group_interface(prefix, treetype, tree_items_update):
    _in_out_items = [('IN', "In", "Input"), ('OUT', "Out", "Output")]

    prop_name = StringProperty(name="Name", default="Value", update=tree_items_update)
    prop_base_type = EnumProperty(name="Base Type", items=_base_type_items, default='FLOAT', update=tree_items_update)
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
    def add_input(self, name, base_type):
        item = self.inputs.add()
        item.name = name
        item.base_type = base_type
        tree_items_update(self)
    def add_output(self, name, base_type):
        item = self.outputs.add()
        item.name = name
        item.base_type = base_type
        tree_items_update(self)
    def remove_input(self, index):
        self.inputs.remove(index)
        tree_items_update(self)
    def remove_output(self, index):
        self.outputs.remove(index)
        tree_items_update(self)
    treetype.add_input = add_input
    treetype.add_output = add_output
    treetype.remove_input = remove_input
    treetype.remove_output = remove_output

    # -------------------------------------------------------------------------

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
            if self.in_out == 'IN':
                ntree.add_input(self.name, self.base_type)
            if self.in_out == 'OUT':
                ntree.add_output(self.name, self.base_type)
            return {'FINISHED'}

    class NodeGroupItemRemove(OperatorBase, Operator):
        """Remove a node group interface socket"""
        bl_idname = "object_nodes.%s_nodegroup_item_remove" % prefix.lower()
        bl_label = "Remove Socket"
        bl_options = {'REGISTER', 'UNDO'}

        index = IntProperty(name="Index")
        in_out = prop_in_out

        def execute(self, context):
            ntree = context.space_data.edit_tree
            if self.in_out == 'IN':
                ntree.remove_input(self.index)
            if self.in_out == 'OUT':
                ntree.remove_output(self.index)
            return {'FINISHED'}

    bpy.utils.register_class(NodeGroupItemAdd)
    bpy.utils.register_class(NodeGroupItemRemove)

    # -------------------------------------------------------------------------

    class NodeGroupInputList(UIList):
        bl_idname = "OBJECT_NODES_UL_%s_nodegroup_inputs" % prefix.lower()

        def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index, flt_flag):
            layout.prop(item, "name", text="", emboss=False)
            layout.prop(item, "base_type", text="")
            props = layout.operator(NodeGroupItemRemove.bl_idname, text="", icon='X')
            props.index = index
            props.in_out = 'IN'

    class NodeGroupOutputList(UIList):
        bl_idname = "OBJECT_NODES_UL_%s_nodegroup_outputs" % prefix.lower()

        def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index, flt_flag):
            layout.prop(item, "name", text="", emboss=False)
            layout.prop(item, "base_type", text="")
            props = layout.operator(NodeGroupItemRemove.bl_idname, text="", icon='X')
            props.index = index
            props.in_out = 'OUT'

    bpy.utils.register_class(NodeGroupInputList)
    bpy.utils.register_class(NodeGroupOutputList)

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
            col.template_list(NodeGroupInputList.bl_idname, "inputs", ntree, "inputs", ntree, "active_input")
            props = col.operator(NodeGroupItemAdd.bl_idname, text="", icon='ZOOMIN')
            props.in_out = 'IN'

            col = row.column(align=True)
            col.template_list(NodeGroupOutputList.bl_idname, "outputs", ntree, "outputs", ntree, "active_output")
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

    # updates all affected nodes when the interface changes
    def tree_items_update(self, context=bpy.context):
        gtree = self.id_data
        for node in gtree.nodes:
            if isinstance(node, GroupInputNode) or isinstance(node, GroupOutputNode):
                node.update()
        for ntree in bpy.data.node_groups:
            if not isinstance(ntree, treetype):
                continue
            for node in ntree.nodes:
                if isinstance(node, GroupNode):
                    if node.id == gtree:
                        node.update()

    def node_sockets_sync(sockets, items):
        free_sockets = set(s for s in sockets)
        free_items = set(i for i in items)
        
        def find_match(s):
            for i in free_items:
                if i.name == s.name and isinstance(s, _base_type_to_socket(i.base_type)):
                    return i

        def socket_index(s):
            for k, ts in enumerate(sockets):
                if ts == s:
                    return k

        # match up current sockets with items
        match = dict()
        for s in sockets:
            i = find_match(s)
            if i is not None:
                free_items.remove(i)
                free_sockets.remove(s)
                match[i] = s

        # nothing to do if perfect match
        if not (free_items or free_sockets):
            return

        # remove unmatched sockets
        for s in free_sockets:
            sockets.remove(s)

        # fix socket list
        for k, i in enumerate(items):
            s = match.get(i, None)
            if s is None:
                # add socket for unmatched item
                stype = _base_type_to_socket(i.base_type)
                s = sockets.new(stype.bl_rna.identifier, i.name)

            index = socket_index(s)
            if index != k:
                sockets.move(index, k)

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

        # XXX used to prevent reentrant updates due to RNA calls
        # this should be fixed in future by avoiding low-level update recursion on the RNA side
        is_updating_nodegroup = BoolProperty(options={'HIDDEN'})

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

        def update(self):
            if not self.is_updating_nodegroup:
                self.is_updating_nodegroup = True
                gtree = self.id
                if gtree:
                    node_sockets_sync(self.inputs, gtree.inputs)
                    node_sockets_sync(self.outputs, gtree.outputs)
                self.is_updating_nodegroup = False

        def compile(self, compiler):
            # TODO
            pass

    class GroupInputNode(node_base, ObjectNode):
        '''Inputs of the node group inside the tree'''
        bl_idname = '%sGroupInputNode' % prefix
        bl_label = 'Group Inputs'

        # XXX used to prevent reentrant updates due to RNA calls
        # this should be fixed in future by avoiding low-level update recursion on the RNA side
        is_updating_nodegroup = BoolProperty(options={'HIDDEN'})

        def update(self):
            if not self.is_updating_nodegroup:
                self.is_updating_nodegroup = True
                node_sockets_sync(self.outputs, self.id_data.inputs)
                self.is_updating_nodegroup = False
            pass

        def compile(self, compiler):
            # TODO
            pass

    class GroupOutputNode(node_base, ObjectNode):
        '''Outputs of the node group inside the tree'''
        bl_idname = '%sGroupOutputNode' % prefix
        bl_label = 'Group Outputs'

        # XXX used to prevent reentrant updates due to RNA calls
        # this should be fixed in future by avoiding low-level update recursion on the RNA side
        is_updating_nodegroup = BoolProperty(options={'HIDDEN'})

        def update(self):
            if not self.is_updating_nodegroup:
                self.is_updating_nodegroup = True
                node_sockets_sync(self.inputs, self.id_data.outputs)
                self.is_updating_nodegroup = False
            pass

        def compile(self, compiler):
            # TODO
            pass

    make_node_group_interface(prefix, treetype, tree_items_update)

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

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
from socket_types import socket_type_items, socket_type_to_rna, socket_type_to_bvm_type, rna_to_socket_type
from nodeitems_utils import NodeCategory as NodeCategoryBase, NodeItem, NodeItemCustom

###############################################################################
# Group Interface

def make_node_group_interface(prefix, treetype, tree_items_update):
    _in_out_items = [('IN', "In", "Input"), ('OUT', "Out", "Output")]

    prop_name = StringProperty(name="Name", default="Value", update=tree_items_update)
    prop_base_type = EnumProperty(name="Base Type", items=socket_type_items, default='FLOAT', update=tree_items_update)
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

# common identifying class to find group nodes
class GroupNodeBase():
    pass

def internal_group_trees(ntree, visited=None):
    if ntree is None:
        return
    if visited is None:
        visited = set()
    elif ntree in visited:
        return
    visited.add(ntree)

    yield ntree

    for node in ntree.nodes:
        if not isinstance(node, GroupNodeBase):
            continue
        for itree in internal_group_trees(node.id, visited):
            yield itree

def ancestor_trees(root_tree, all_trees):
    ancestors = set()
    if root_tree is None:
        return ancestors

    visited = set()
    def visit(ntree):
        global indent
        if ntree is None:
            return False
        if ntree in visited:
            return ntree in ancestors
        visited.add(ntree)

        for node in ntree.nodes:
            if not isinstance(node, GroupNodeBase):
                continue
            elif node.id == root_tree or visit(node.id):
                ancestors.add(ntree)
                return True
        return False

    for ntree in all_trees:
        visit(ntree)

    return ancestors


def make_node_group_types(prefix, treetype, node_base):
    ntree_idname = treetype.bl_rna.identifier
    groupnode_idname = '%sGroupNode' % prefix

    class NodeGroupNew(Operator):
        """Create new object node tree"""
        bl_idname = "object_nodes.%s_nodegroup_new" % prefix.lower()
        bl_label = "New"
        bl_options = {'REGISTER', 'UNDO'}
        bl_ntree_idname = ntree_idname

        name = StringProperty(
                name="Name",
                default="%s Node Group" % prefix,
                )

        @classmethod
        def init_default(cls, ntree):
            gin = ntree.nodes.new(GroupInputNode.bl_idname)
            gin.location = (-100 - gin.bl_width_default, 20)
            gout = ntree.nodes.new(GroupOutputNode.bl_idname)
            gout.location = (100, 20)

        def execute(self, context):
            node = getattr(context, "node", None)
            ntree = bpy.data.node_groups.new(self.name, self.bl_ntree_idname)
            if ntree is None:
                return {'CANCELLED'}
            
            self.init_default(ntree)
            if node:
                node.id = ntree
            return {'FINISHED'}

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
                if i.name == s.name and isinstance(s, socket_type_to_rna(i.base_type)):
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
                stype = socket_type_to_rna(i.base_type)
                s = sockets.new(stype.bl_rna.identifier, i.name)

            index = socket_index(s)
            if index != k:
                sockets.move(index, k)

    class GroupNode(node_base, ObjectNode, GroupNodeBase):
        '''Group of nodes that can be used in other trees'''
        bl_idname = groupnode_idname
        bl_label = 'Group'
        bl_ntree_idname = ntree_idname
        bl_id_property_type = 'NODETREE'

        def bl_id_property_poll(self, ntree):
            if not isinstance(ntree, treetype):
                return False
            parent_tree = self.id_data
            for itree in internal_group_trees(ntree):
                if itree == parent_tree:
                    return False
            return True

        def bl_id_property_update(self, context):
            self.update()

        def poll_instance(self, ntree):
            if hasattr(super(), "poll_instance") and not super().poll_instance(ntree):
                return False
            if self.id == ntree:
                return False
            for itree in internal_group_trees(self.id):
                if itree == ntree:
                    return False
            return True

        def draw_buttons(self, context, layout):
            layout.template_ID(self, "id", new=NodeGroupNew.bl_idname)

        def compile_dependencies(self, depsnode):
            ntree = self.id
            if ntree:
                # changes to the group tree require own recompile
                depsnode.add_nodetree_relation(ntree, 'PARAMETERS')
                # add internal dependencies of the group
                ntree.bvm_compile_dependencies(depsnode)

        def eval_dependencies(self, depsnode):
            ntree = self.id
            if ntree:
                ntree.bvm_eval_dependencies(depsnode)

        def update(self):
            if self.is_updating:
                return
            with self.update_lock():
                gtree = self.id
                node_sockets_sync(self.inputs, gtree.inputs if gtree else [])
                node_sockets_sync(self.outputs, gtree.outputs if gtree else [])

        def compile(self, compiler):
            if self.id is not None:
                self.id.compile_nodes(compiler)

    class GroupInputNode(node_base, ObjectNode):
        '''Inputs of the node group inside the tree'''
        bl_idname = '%sGroupInputNode' % prefix
        bl_label = 'Group Inputs'

        def update(self):
            if self.is_updating:
                return
            with self.update_lock():
                node_sockets_sync(self.outputs, self.id_data.inputs)

        def compile(self, compiler):
            gtree = self.id_data
            for i, item in enumerate(gtree.inputs):
                proxy = compiler.add_proxy(socket_type_to_bvm_type(item.base_type))
                compiler.map_output(i, proxy.outputs[0])
                compiler.map_input_external(i, proxy.inputs[0])

    class GroupOutputNode(node_base, ObjectNode):
        '''Outputs of the node group inside the tree'''
        bl_idname = '%sGroupOutputNode' % prefix
        bl_label = 'Group Outputs'

        def update(self):
            if self.is_updating:
                return
            with self.update_lock():
                node_sockets_sync(self.inputs, self.id_data.outputs)

        def compile(self, compiler):
            gtree = self.id_data
            for i, item in enumerate(gtree.outputs):
                proxy = compiler.add_proxy(socket_type_to_bvm_type(item.base_type))
                compiler.map_input(i, proxy.inputs[0])
                compiler.map_output_external(i, proxy.outputs[0])

    make_node_group_interface(prefix, treetype, tree_items_update)

    bpy.utils.register_class(NodeGroupNew)
    bpy.utils.register_class(GroupNode)
    bpy.utils.register_class(GroupInputNode)
    bpy.utils.register_class(GroupOutputNode)

    return GroupNode, GroupInputNode, GroupOutputNode

###############################################################################

def GroupNodeCategory(prefix, gnode, ginput, goutput):
    ntree_idname = gnode.bl_ntree_idname

    def copy_node_attributes(dst, src):
        copy_attrs = ['color', 'height', 'hide', 'label',
                      'location', 'id', 'mute', 'name',
                      'show_options', 'show_preview', 'show_texture', 'use_custom_color',
                      'width', 'width_hidden']
        for attr in copy_attrs:
            if not hasattr(dst, attr):
                continue
            if dst.is_property_readonly(attr):
                continue
            setattr(dst, attr, getattr(src, attr))

        for key, value in src.items():
            dst[key] = value

    def copy_nodes(nodes, to_ntree):
        new_nodes = dict()
        new_sockets = dict()
        # copy nodes and local attributes
        for node in nodes:
            inode = to_ntree.nodes.new(node.bl_idname)
            copy_node_attributes(inode, node)

            # map old to new
            new_nodes[node] = inode
            for old, new in zip(node.inputs, inode.inputs):
                new_sockets[old] = new
            for old, new in zip(node.outputs, inode.outputs):
                new_sockets[old] = new
        # parent node must be mapped to new nodes
        for node, inode in new_nodes.items():
            if node.parent:
                iparent = new_nodes.get(node.parent, None)
                if iparent:
                    inode.parent = iparent

        return new_nodes, new_sockets

    def node_bounds(nodes):
        if nodes:
            bbmin = (min(node.location[0] for node in nodes),
                     min(node.location[1] - node.height for node in nodes))
            bbmax = (max(node.location[0] + node.width for node in nodes),
                     max(node.location[1] for node in nodes))
        else:
            bbmin = (0.0, 0.0)
            bbmax = (0.0, 0.0)
        return bbmin, bbmax

    class NodeGroupMake(Operator):
        """Make a node group from selected nodes"""
        bl_idname = "object_nodes.%s_nodegroup_make" % prefix.lower()
        bl_label = "Make Group"
        bl_options = {'REGISTER', 'UNDO'}
        bl_ntree_idname = ntree_idname

        name = StringProperty(
                name="Name",
                default="Node Group",
                )

        @classmethod
        def poll(cls, context):
            space = context.space_data
            if not space or space.type != 'NODE_EDITOR':
                return False
            ntree = space.edit_tree
            if not ntree or not gnode.poll(ntree):
                return False
            return True

        def cache_selection(self, ntree):
            selected_nodes = [node for node in ntree.nodes if node.select]
            internal_links = []
            input_links = []
            output_links = []
            for link in ntree.links:
                if link.is_hidden or not link.is_valid:
                    continue
                sel_from = link.from_node.select
                sel_to = link.to_node.select
                if not sel_from and not sel_to:
                    continue
                elif sel_from and sel_to:
                    internal_links.append(link)
                elif sel_from:
                    output_links.append(link)
                elif sel_to:
                    input_links.append(link)
            return selected_nodes, internal_links, input_links, output_links

        def layout_group(self, group_node, new_nodes, input_node, output_node):
            bbmin, bbmax = node_bounds(new_nodes)
            center = (0.5*(bbmin[0] + bbmax[0]), 0.5*(bbmin[1] + bbmax[1]))
            for node in new_nodes.values():
                node.location[0] -= center[0]
                node.location[1] -= center[1]
            offsetx = 0.5*(bbmax[0] - bbmin[0])
            input_node.location[0] = -offsetx - input_node.bl_width_default - 50
            input_node.location[1] = 0.5*input_node.bl_height_default
            output_node.location[0] = offsetx + 50
            output_node.location[1] = 0.5*output_node.bl_height_default

            group_node.location[0] = center[0] - 0.5*group_node.bl_width_default
            group_node.location[1] = center[1] + 0.5*group_node.bl_height_default

        def execute(self, context):
            ntree = context.space_data.edit_tree

            # Warning! this has to happen right at the start
            # because creating a node will make it selected by default!
            selected_nodes, internal_links, input_links, output_links = self.cache_selection(ntree)

            groupnode = ntree.nodes.new(gnode.bl_idname)
            if groupnode is None:
                return {'CANCELLED'}
            grouptree = bpy.data.node_groups.new(self.name, self.bl_ntree_idname)
            if grouptree is None:
                return {'CANCELLED'}
            input_node = grouptree.nodes.new(ginput.bl_idname)
            output_node = grouptree.nodes.new(goutput.bl_idname)
            groupnode.id = grouptree
            
            # copy nodes and attributes
            new_nodes, new_sockets = copy_nodes(selected_nodes, grouptree)

            # move nodes to sensible locations
            self.layout_group(groupnode, new_nodes, input_node, output_node)

            # define the group interface
            io_inputs = dict()
            io_outputs = dict()
            for link in input_links:
                if link.from_socket not in io_inputs:
                    io_inputs[link.from_socket] = {link.to_socket,}
                else:
                    io_inputs[link.from_socket].add(link.to_socket)
            for link in output_links:
                if link.from_socket not in io_outputs:
                    io_outputs[link.from_socket] = {link.to_socket,}
                else:
                    io_outputs[link.from_socket].add(link.to_socket)

            # reconstruct links
            for link in internal_links:
                ifrom_socket = new_sockets[link.from_socket]
                ito_socket = new_sockets[link.to_socket]
                grouptree.links.new(ifrom_socket, ito_socket)
            for io, targets in io_inputs.items():
                grouptree.add_input(io.name, rna_to_socket_type(type(io)))
                # XXX this shouldn't be necessary, but node updates are terrible
                groupnode.update()
                io_extern = groupnode.inputs[-1]
                io_intern = input_node.outputs[-1]
                
                ntree.links.new(io, io_extern)
                for s in targets:
                    grouptree.links.new(io_intern, new_sockets[s])
            for io, targets in io_outputs.items():
                grouptree.add_output(io.name, rna_to_socket_type(type(io)))
                # XXX this shouldn't be necessary, but node updates are terrible
                groupnode.update()
                io_extern = groupnode.outputs[-1]
                io_intern = output_node.inputs[-1]

                grouptree.links.new(new_sockets[io], io_intern)
                for s in targets:
                    ntree.links.new(io_extern, s)

            # delete replaced nodes
            for node in selected_nodes:
                ntree.nodes.remove(node)

            # clean up selection
            for node in grouptree.nodes:
                node.select = False
            grouptree.nodes.active = None
            for node in ntree.nodes:
                node.select = (node == groupnode)
            ntree.nodes.active = groupnode

            return {'FINISHED'}
    
    # menu entry for node group tools
    def group_tools_draw(self, layout, context):
        # TODO C node operators won't work for our nodes
        #layout.operator("node.group_make")
        #layout.operator("node.group_ungroup")
        layout.separator()

    def node_group_items(context):
        if context is None:
            return
        space = context.space_data
        if not space:
            return
        ntree = space.edit_tree
        if not ntree:
            return

        yield NodeItemCustom(draw=group_tools_draw)

        all_trees = context.blend_data.node_groups
        ancestors = ancestor_trees(ntree, all_trees)
        free_trees = [t for t in all_trees if (t.bl_rna.identifier == gnode.bl_ntree_idname) and (t != ntree) and (t not in ancestors)]

        for ntree in free_trees:
            yield NodeItem(gnode.bl_idname,
                           label=ntree.name,
                           settings={"id": "bpy.data.node_groups[%r]" % ntree.name})

    class NodeCategory(NodeCategoryBase):
        def __init__(self):
            super().__init__("%s_GROUPS" % prefix, "Group", items=node_group_items)

        @classmethod
        def poll(cls, context):
            ntree = context.space_data.edit_tree
            if not ntree:
                return False
            return gnode.poll(ntree)

    bpy.utils.register_class(NodeGroupMake)

    # create keymap
    wm = bpy.context.window_manager
    km = wm.keyconfigs.default.keymaps.new(name="Node Generic", space_type='NODE_EDITOR')
    kmi = km.keymap_items.new(NodeGroupMake.bl_idname, 'G', 'PRESS', ctrl=True)
    keymaps.append(km)

    return NodeCategory()

###############################################################################

keymaps = []

def register():
    pass

def unregister():
    # remove keymap
    wm = bpy.context.window_manager
    for km in keymaps:
        wm.keyconfigs.default.keymaps.remove(km)
    keymaps.clear()

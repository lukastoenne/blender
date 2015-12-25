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

from common_nodes import NodeTreeBase, NodeBase
from geometry_nodes import GeometryNodesNew
from instancing_nodes import InstancingNodesNew
from forcefield_nodes import ForceFieldNodesNew

###############################################################################

# our own base class with an appropriate poll function,
# so the categories only show up in our own tree type
class ObjectNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        tree = context.space_data.edit_tree
        return tree and tree.bl_idname == 'ObjectNodeTree'

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


class ObjectNodeBase(NodeBase):
    @classmethod
    def poll(cls, ntree):
        return ntree.bl_idname == 'ObjectNodeTree'


class GeometryNode(ObjectNodeBase, ObjectNode):
    '''Geometry'''
    bl_idname = 'GeometryNode'
    bl_label = 'Geometry'
    bl_icon = 'MESH_DATA'

    bl_id_property_type = 'NODETREE'
    def bl_id_property_poll(self, ntree):
        return ntree.bl_idname == 'GeometryNodeTree'

    def compile_dependencies(self, depsnode):
        ntree = self.id
        if ntree:
            ntree.bvm_compile_dependencies(depsnode)

    def eval_dependencies(self, depsnode):
        ntree = self.id
        if ntree:
            ntree.bvm_eval_dependencies(depsnode)

    def draw_buttons(self, context, layout):
        layout.context_pointer_set("node", self)
        layout.template_ID(self, "id", new=GeometryNodesNew.bl_idname)

    def init(self, context):
        pass

    def compile(self, compiler):
        pass


class InstancingNode(ObjectNodeBase, ObjectNode):
    '''Instancing'''
    bl_idname = 'InstancingNode'
    bl_label = 'Instancing'
    bl_icon = 'EMPTY_DATA'

    bl_id_property_type = 'NODETREE'
    def bl_id_property_poll(self, ntree):
        return ntree.bl_idname == 'InstancingNodeTree'

    def compile_dependencies(self, depsnode):
        ntree = self.id
        if ntree:
            ntree.bvm_compile_dependencies(depsnode)

    def eval_dependencies(self, depsnode):
        ntree = self.id
        if ntree:
            ntree.bvm_eval_dependencies(depsnode)

    def draw_buttons(self, context, layout):
        layout.context_pointer_set("node", self)
        layout.template_ID(self, "id", new="object_nodes.instancing_nodes_new")

    def compile(self, compiler):
        pass


class ForceFieldNode(ObjectNodeBase, ObjectNode):
    '''Force Field'''
    bl_idname = 'ForceFieldNode'
    bl_label = 'Force Field'
    bl_icon = 'FORCE_FORCE'

    bl_id_property_type = 'NODETREE'
    def bl_id_property_poll(self, ntree):
        return ntree.bl_idname == 'ForceFieldNodeTree'

    def compile_dependencies(self, depsnode):
        ntree = self.id
        if ntree:
            ntree.bvm_compile_dependencies(depsnode)

    def eval_dependencies(self, depsnode):
        ntree = self.id
        if ntree:
            ntree.bvm_eval_dependencies(depsnode)

    def draw_buttons(self, context, layout):
        layout.context_pointer_set("node", self)
        layout.template_ID(self, "id", new="object_nodes.force_field_nodes_new")

    def compile(self, compiler):
        pass

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
            return getattr(context, "active_node", None)

    @classmethod
    def poll(cls, context):
        space = context.space_data
        if space.type != 'NODE_EDITOR':
            return False
        treetype = getattr(bpy.types, space.tree_type)
        if not issubclass(treetype, NodeTreeBase):
            return False
        return True

    def execute(self, context):
        space = context.space_data
        node = self.get_node(context)
        has_tree = node and node.id and isinstance(node.id, NodeTreeBase)
        exit = self.exit or not has_tree

        if exit:
            space.path.pop()
        else:
            space.path.append(node.id, node)

        return {'FINISHED'}

###############################################################################

keymaps = []

def register():
    bpy.utils.register_module(__name__)

    node_categories = [
        ObjectNodeCategory("COMPONENTS", "Components", items=[
            NodeItem("GeometryNode",
                     settings={"id": "bpy.types.%s.make_node_tree()" % (GeometryNodesNew.bl_rna.identifier)}),
            NodeItem("ForceFieldNode",
                     settings={"id": "bpy.types.%s.make_node_tree()" % (ForceFieldNodesNew.bl_rna.identifier)}),
            NodeItem("InstancingNode",
                     settings={"id": "bpy.types.%s.make_node_tree()" % (InstancingNodesNew.bl_rna.identifier)}),
            ]),
        ]
    nodeitems_utils.register_node_categories("OBJECT_NODES", node_categories)

    # create keymap
    wm = bpy.context.window_manager
    km = wm.keyconfigs.default.keymaps.new(name="Node Generic", space_type='NODE_EDITOR')
    
    kmi = km.keymap_items.new(bpy.types.OBJECT_NODES_OT_node_edit.bl_idname, 'TAB', 'PRESS')
    kmi.properties.exit = False
    
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

    bpy.utils.unregister_module(__name__)

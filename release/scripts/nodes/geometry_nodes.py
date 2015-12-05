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
import group_nodes

###############################################################################
# Socket Types

class GeometrySocket(NodeSocket):
    '''Geometry data socket'''
    bl_idname = 'GeometrySocket'
    bl_label = 'Geometry'

    is_placeholder = BoolProperty(name="Is Placeholder",
                                  default=False)

    def draw(self, context, layout, node, text):
        layout.label(text)

    def draw_color(self, context, node):
        alpha = 0.4 if self.is_placeholder else 1.0
        return (1.0, 0.4, 0.216, alpha)


###############################################################################


# our own base class with an appropriate poll function,
# so the categories only show up in our own tree type
class GeometryNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        tree = context.space_data.edit_tree
        return tree and tree.bl_idname == 'GeometryNodeTree'

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


class GeometryMeshCombineNode(GeometryNodeBase, ObjectNode):
    '''Combine multiple meshes into one'''
    bl_idname = 'GeometryMeshCombineNode'
    bl_label = 'Combine Meshes'

    def add_extender(self):
        socket = self.inputs.new('GeometrySocket', "")
        socket.is_placeholder = True
        return socket

    def init(self, context):
        self.add_extender()
        self.outputs.new('GeometrySocket', "")

    def update_inputs(self, insert=None):
        ntree = self.id_data

        # build map of connected inputs
        input_links = dict()
        for link in ntree.links:
            if link.to_node == self:
                input_links[link.to_socket] = (link, link.from_socket)

        # remove unconnected sockets
        for socket in self.inputs:
            if socket not in input_links and socket != insert:
                self.inputs.remove(socket)
            else:
                socket.is_placeholder = False

        # shift sockets to make room for a new link
        if insert is not None:
            self.inputs.new('GeometrySocket', "")
            nsocket = self.inputs[-1]
            for socket in reversed(self.inputs[:-1]):
                link, from_socket = input_links.get(socket, (None, None))
                if link is not None:
                    ntree.links.remove(link)
                    ntree.links.new(from_socket, nsocket)
                nsocket = socket
                if socket == insert:
                    break

        self.add_extender()

    def update(self):
        self.update_inputs()

    def insert_link(self, link):
        self.update_inputs(link.to_socket)

    def compile(self, compiler):
        ntree = self.id_data

        # list of connected inputs
        used_inputs = set()
        for link in ntree.links:
            if link.to_node == self:
                used_inputs.add(link.to_socket)
        # make a sorted index list
        used_inputs = [ i for i,socket in enumerate(self.inputs) if socket in used_inputs ]

        if len(used_inputs) > 0:
            node = compiler.add_node("PASS_MESH")
            compiler.map_input(used_inputs[0], node.inputs[0])
            result = node.outputs[0]
        
            for index in used_inputs[1:]:
                node = compiler.add_node("MESH_COMBINE")
                compiler.link(result, node.inputs[0])
                compiler.map_input(index, node.inputs[1])
                
                result = node.outputs[0]

            compiler.map_output(0, result)


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

        compiler.link(node_tfm.outputs[0], node.inputs["transform"])

        compiler.map_input(0, node.inputs["mesh_in"])
        compiler.map_input(1, node.inputs["count"])
        compiler.map_input(2, node_tfm.inputs["loc"])
        compiler.map_output(0, node.outputs["mesh_out"])

###############################################################################

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

###############################################################################

def register():
    bpy.utils.register_module(__name__)
    gnode, ginput, goutput = group_nodes.make_node_group_types("Geometry", GeometryNodeTree, GeometryNodeBase)

    node_categories = [
        GeometryNodeCategory("GEO_INPUT", "Input", items=[
            NodeItem("ObjectIterationNode"),
            NodeItem("GeometryMeshLoadNode"),
            NodeItem(ginput.bl_idname),
            ]),
        GeometryNodeCategory("GEO_OUTPUT", "Output", items=[
            NodeItem("GeometryOutputNode"),
            NodeItem(goutput.bl_idname),
            ]),
        GeometryNodeCategory("GEO_MODIFIER", "Modifier", items=[
            NodeItem("GeometryMeshArrayNode"),
            ]),
        GeometryNodeCategory("GEO_CONVERTER", "Converter", items=[
            NodeItem("ObjectSeparateVectorNode"),
            NodeItem("ObjectCombineVectorNode"),
            NodeItem("GeometryMeshCombineNode"),
            ]),
        GeometryNodeCategory("GEO_MATH", "Math", items=[
            NodeItem("ObjectMathNode"),
            NodeItem("ObjectVectorMathNode"),
            ]),
        GeometryNodeCategory("GEO_GROUP", "Group", items=[
            NodeItem(gnode.bl_idname),
            ]),
        ]
    nodeitems_utils.register_node_categories("GEOMETRY_NODES", node_categories)

def unregister():
    nodeitems_utils.unregister_node_categories("GEOMETRY_NODES")

    bpy.utils.unregister_module(__name__)

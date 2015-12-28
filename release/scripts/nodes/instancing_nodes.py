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
class InstancingNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        tree = context.space_data.edit_tree
        return tree and tree.bl_idname == 'InstancingNodeTree'

###############################################################################

class InstancingNodeTree(NodeTreeBase, NodeTree):
    '''Instancing nodes'''
    bl_idname = 'InstancingNodeTree'
    bl_label = 'Instancing Nodes'
    bl_icon = 'EMPTY_DATA'

    # does not show up in the editor header
    @classmethod
    def poll(cls, context):
        return False

    def init_default(self):
        out = self.nodes.new(OutputNode.bl_idname)
        out.location = (100, 20)


class InstancingNodeBase(NodeBase):
    @classmethod
    def poll(cls, ntree):
        return ntree.bl_idname == 'InstancingNodeTree'


def compile_object_transform(compiler, ob):
    node = compiler.add_node("OBJECT_TRANSFORM")
    compiler.link(ob, node.inputs[0])
    return node.outputs[0]

def compile_matrix_inverse(compiler, mat):
    node = compiler.add_node("INVERT_MATRIX44")
    compiler.link(mat, node.inputs[0])
    return node.outputs[0]

def compile_matrix_multiply(compiler, mat1, mat2):
    node = compiler.add_node("MUL_MATRIX44")
    compiler.link(mat1, node.inputs[0])
    compiler.link(mat2, node.inputs[1])
    return node.outputs[0]

def compile_object(compiler, ptr):
    key = compiler.get_id_key(ptr)
    node = compiler.add_node("OBJECT_LOOKUP")
    node.inputs[0].set_value(key)
    return node.outputs[0]


class OutputNode(InstancingNodeBase, ObjectNode, DynamicSocketListNode):
    '''Dupli output'''
    bl_idname = 'InstancingOutputNode'
    bl_label = 'Output'

    def init(self, context):
        if self.is_updating:
            return
        with self.update_lock():
            self.update_socket_list(self.inputs, 'DupliSocket')

    def update(self):
        if self.is_updating:
            return
        with self.update_lock():
            self.update_socket_list(self.inputs, 'DupliSocket')

    def insert_link(self, link):
        if self.is_updating:
            return
        with self.update_lock():
            self.update_socket_list(self.inputs, 'DupliSocket', insert=link.to_socket)

    def compile(self, compiler):
        result = self.compile_socket_list(compiler, self.inputs, "PASS_DUPLIS", "DUPLIS_COMBINE", "VALUE_DUPLIS")
        compiler.link(result, compiler.graph_output("dupli.result"))


class MakeDupliNode(InstancingNodeBase, ObjectNode):
    '''Make object instance'''
    bl_idname = 'InstancingMakeDupliNode'
    bl_label = 'Make Dupli'

    bl_id_property_type = 'OBJECT'

    def draw_buttons(self, context, layout):
        layout.template_ID(self, "id")

    def eval_dependencies(self, depsnode):
        dob = self.id
        if dob:
            # XXX not quite ideal: we need to define a "dependency"
            # because this puts the dupli object into the globals dict,
            # even though the duplis don't actually need to be re-evaluated
            # when this object changes
            depsnode.add_object_relation(dob, 'GEOMETRY')

    def init(self, context):
        self.inputs.new('TransformSocket', "Transform")
        self.inputs.new('NodeSocketInt', "Index")
        self.inputs.new('NodeSocketInt', "Hide")
        socket = self.inputs.new('NodeSocketInt', "Recursive")
        socket.default_value = 1
        self.outputs.new('DupliSocket', "")

    def compile(self, compiler):
        ob = compile_object(compiler, self.id)

        node = compiler.add_node("MAKE_DUPLI")
        compiler.link(ob, node.inputs["object"])
        compiler.map_input(0, node.inputs["transform"])
        compiler.map_input(1, node.inputs["index"])
        compiler.map_input(2, node.inputs["hide"])
        compiler.map_input(3, node.inputs["recursive"])
        compiler.map_output(0, node.outputs[0])

###############################################################################

class InstancingNodesNew(Operator):
    """Create new Instancing node tree"""
    bl_idname = "object_nodes.instancing_nodes_new"
    bl_label = "New"
    bl_options = {'REGISTER', 'UNDO'}

    name = StringProperty(
            name="Name",
            default="DupliNodes",
            )

    @classmethod
    def make_node_tree(cls, name="DupliNodes"):
        ntree = bpy.data.node_groups.new(name, InstancingNodeTree.bl_idname)
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
        "Instancing", InstancingNodeTree, InstancingNodeBase)
    bpy.utils.register_module(__name__)

    node_categories = [
        InstancingNodeCategory("INS_INPUT", "Input", items=[
            NodeItem("ObjectIterationNode"),
            NodeItem(ginput.bl_idname),
            NodeItem("ObjectValueFloatNode"),
            NodeItem("ObjectValueIntNode"),
            NodeItem("ObjectValueVectorNode"),
            NodeItem("ObjectValueColorNode"),
            ]),
        InstancingNodeCategory("INS_OUTPUT", "Output", items=[
            NodeItem("InstancingOutputNode"),
            NodeItem(goutput.bl_idname),
            ]),
        InstancingNodeCategory("INS_DUPLIS", "Duplis", items=[
            NodeItem("InstancingMakeDupliNode"),
            ]),
        InstancingNodeCategory("INS_CONVERTER", "Converter", items=[
            NodeItem("ObjectSeparateVectorNode"),
            NodeItem("ObjectCombineVectorNode"),
            ]),
        InstancingNodeCategory("INS_MATH", "Math", items=[
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
        InstancingNodeCategory("INS_TEXTURE", "Texture", items=[
            NodeItem("ObjectTextureCloudsNode"),
            NodeItem("ObjectTextureVoronoiNode"),
            ]),
        group_nodes.GroupNodeCategory("INS", gnode, ginput, goutput),
        ]
    nodeitems_utils.register_node_categories("INSTANCING_NODES", node_categories)

def unregister():
    nodeitems_utils.unregister_node_categories("INSTANCING_NODES")

    bpy.utils.unregister_module(__name__)

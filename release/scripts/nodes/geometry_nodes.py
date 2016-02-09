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

    def init_default(self):
        mesh = self.nodes.new(GeometryMeshLoadNode.bl_idname)
        mesh.location = (-100 - mesh.bl_width_default, 20)
        out = self.nodes.new(GeometryOutputNode.bl_idname)
        out.location = (100, 20)

        self.links.new(mesh.outputs[0], out.inputs[0])


class GeometryNodeBase(NodeBase):
    @classmethod
    def poll(cls, ntree):
        return ntree.bl_idname == 'GeometryNodeTree'


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

# typical inputs of a modifier node:
# (object, transform, inverse transform)
def compile_modifier_inputs(compiler, obptr):
    ptfm = compile_object_transform(compiler, compiler.graph_input("modifier.object"))
    pitfm = compile_matrix_inverse(compiler, ptfm)

    ob = compile_object(compiler, obptr)
    obtfm = compile_object_transform(compiler, ob)

    tfm = compile_matrix_multiply(compiler, pitfm, obtfm)
    itfm = compile_matrix_inverse(compiler, tfm)

    return ob, tfm, itfm


class GeometryOutputNode(GeometryNodeBase, ObjectNode, DynamicSocketListNode):
    '''Geometry output'''
    bl_idname = 'GeometryOutputNode'
    bl_label = 'Output'

    def init(self, context):
        if self.is_updating:
            return
        with self.update_lock():
            self.update_socket_list(self.inputs, 'GeometrySocket')

    def update(self):
        if self.is_updating:
            return
        with self.update_lock():
            self.update_socket_list(self.inputs, 'GeometrySocket')

    def insert_link(self, link):
        if self.is_updating:
            return
        with self.update_lock():
            self.update_socket_list(self.inputs, 'GeometrySocket', insert=link.to_socket)

    def compile(self, compiler):
        result = self.compile_socket_list(compiler, self.inputs, "PASS_MESH", "MESH_COMBINE", "VALUE_MESH")
        compiler.link(result, compiler.graph_output("mesh"))


class GeometryElementInfoNode(GeometryNodeBase, ObjectNode):
    '''Properties of the current geometry element'''
    bl_idname = 'GeometryElementInfoNode'
    bl_label = 'Element Info'

    def init(self, context):
        self.outputs.new('NodeSocketInt', "Index")
        self.outputs.new('NodeSocketVector', "Location")

    def compile(self, compiler):
        compiler.map_output(0, compiler.graph_input("element.index"))
        compiler.map_output(1, compiler.graph_input("element.location"))


class GeometryMeshLoadNode(GeometryNodeBase, ObjectNode):
    '''Mesh object data'''
    bl_idname = 'GeometryMeshLoadNode'
    bl_label = 'Mesh'

    def init(self, context):
        self.outputs.new('GeometrySocket', "")

    def compile(self, compiler):
        node = compiler.add_node("MESH_LOAD")
        compiler.link(compiler.graph_input("modifier.base_mesh"), node.inputs[0])
        compiler.map_output(0, node.outputs[0])


class GeometryObjectFinalMeshNode(GeometryNodeBase, ObjectNode):
    '''Load the final mesh of an object'''
    bl_idname = 'GeometryObjectFinalMeshNode'
    bl_label = 'Object Mesh'

    bl_id_property_type = 'OBJECT'
    def bl_id_property_poll(self, ob):
        return ob.type == 'MESH'

    def draw_buttons(self, context, layout):
        layout.template_ID(self, "id")

    def eval_dependencies(self, depsnode):
        ob = self.id
        if ob:
            depsnode.add_object_relation(ob, 'GEOMETRY')

    def init(self, context):
        self.outputs.new('GeometrySocket', "")

    def compile(self, compiler):
        if self.id is None:
            return
        ob, tfm, itfm = compile_modifier_inputs(compiler, self.id)

        node = compiler.add_node("OBJECT_FINAL_MESH")
        compiler.link(ob, node.inputs[0])
        compiler.map_output(0, node.outputs[0])


class GeometryMeshCombineNode(GeometryNodeBase, ObjectNode, DynamicSocketListNode):
    '''Combine multiple meshes into one'''
    bl_idname = 'GeometryMeshCombineNode'
    bl_label = 'Combine Meshes'

    def init(self, context):
        if self.is_updating:
            return
        with self.update_lock():
            self.update_socket_list(self.inputs, 'GeometrySocket')
            self.outputs.new('GeometrySocket', "")

    def update(self):
        if self.is_updating:
            return
        with self.update_lock():
            self.update_socket_list(self.inputs, 'GeometrySocket')

    def insert_link(self, link):
        if self.is_updating:
            return
        with self.update_lock():
            self.update_socket_list(self.inputs, 'GeometrySocket', insert=link.to_socket)

    def compile(self, compiler):
        result = self.compile_socket_list(compiler, self.inputs, "PASS_MESH", "MESH_COMBINE", "VALUE_MESH")
        compiler.map_output(0, result)


class GeometryMeshArrayNode(GeometryNodeBase, ObjectNode):
    '''Make a number of transformed copies of a mesh'''
    bl_idname = 'GeometryMeshArrayNode'
    bl_label = 'Array'

    def init(self, context):
        self.inputs.new('GeometrySocket', "")
        self.inputs.new('NodeSocketInt', "Count")
        self.inputs.new('TransformSocket', "Transform")
        self.outputs.new('GeometrySocket', "")

    def compile(self, compiler):
        node = compiler.add_node("MESH_ARRAY")
        compiler.map_input(0, node.inputs["mesh_in"])
        compiler.map_input(1, node.inputs["count"])
        compiler.map_input(2, node.inputs["transform"])
        compiler.map_output(0, node.outputs["mesh_out"])


class GeometryMeshDisplaceNode(GeometryNodeBase, ObjectNode):
    '''Add an offset vector to each vertex location'''
    bl_idname = 'GeometryMeshDisplaceNode'
    bl_label = 'Displace'

    def init(self, context):
        self.inputs.new('GeometrySocket', "")
        self.inputs.new('NodeSocketVector', "Vector")
        self.outputs.new('GeometrySocket', "")

    def compile(self, compiler):
        node = compiler.add_node("MESH_DISPLACE")
        compiler.map_input(0, node.inputs["mesh_in"])
        compiler.map_input(1, node.inputs["vector"])
        compiler.map_output(0, node.outputs["mesh_out"])


class GeometryBooleanNode(GeometryNodeBase, ObjectNode):
    '''Boolean operation with another mesh'''
    bl_idname = 'GeometryBooleanNode'
    bl_label = 'Boolean'

    bl_id_property_type = 'OBJECT'
    def bl_id_property_poll(self, ob):
        return ob.type == 'MESH'

    operation = enum_property_copy(bpy.types.BooleanModifier, "operation")
    operation_value = enum_property_value_prop("operation")

    use_separate = BoolProperty(name="Separate",
                                description="Keep edges separate",
                                default=False)
    use_dissolve = BoolProperty(name="Dissolve",
                                description="Dissolve verts created from tessellated intersection",
                                default=True)
    use_connect_regions = BoolProperty(name="Calculate Holes",
                                       description="Connect regions (needed for hole filling)",
                                       default=True)
    threshold = FloatProperty(name="Threshold",
                              default=0.0,
                              min=0.0,
                              max=1.0)

    def draw_buttons(self, context, layout):
        layout.template_ID(self, "id")
        layout.prop(self, "operation")
        layout.prop(self, "use_separate")
        layout.prop(self, "use_dissolve")
        layout.prop(self, "use_connect_regions")
        layout.prop(self, "threshold")

    def eval_dependencies(self, depsnode):
        curveob = self.id
        if curveob:
            depsnode.add_object_relation(curveob, 'TRANSFORM')
            depsnode.add_object_relation(curveob, 'GEOMETRY')

    def init(self, context):
        self.inputs.new('GeometrySocket', "")
        self.outputs.new('GeometrySocket', "")

    def compile(self, compiler):
        if self.id is None:
            return
        ob, tfm, itfm = compile_modifier_inputs(compiler, self.id)

        node = compiler.add_node("MESH_BOOLEAN")
        compiler.map_input(0, node.inputs[0])
        compiler.link(ob, node.inputs[1])
        compiler.link(tfm, node.inputs[2])
        compiler.link(itfm, node.inputs[3])
        node.inputs[4].set_value(self.operation_value)
        node.inputs[5].set_value(self.use_separate)
        node.inputs[6].set_value(self.use_dissolve)
        node.inputs[7].set_value(self.use_connect_regions)
        node.inputs[8].set_value(self.threshold)
        
        compiler.map_output(0, node.outputs[0])


class GeometryClosestPointNode(GeometryNodeBase, ObjectNode):
    '''Closest point on the a mesh'''
    bl_idname = 'GeometryClosestPointNode'
    bl_label = 'Closest Point'

    def init(self, context):
        self.inputs.new('GeometrySocket', "Mesh")
        self.inputs.new('NodeSocketVector', "Vector")
        self.outputs.new('NodeSocketVector', "Position")
        self.outputs.new('NodeSocketVector', "Normal")
        self.outputs.new('NodeSocketVector', "Tangent")

    def compile(self, compiler):
        node = compiler.add_node("MESH_CLOSEST_POINT")
        
        compiler.map_input(0, node.inputs["mesh"])
        compiler.map_input(1, node.inputs["vector"])
        compiler.map_output(0, node.outputs["position"])
        compiler.map_output(1, node.outputs["normal"])
        compiler.map_output(2, node.outputs["tangent"])

###############################################################################

class CurveNodeBase(NodeBase):
    @classmethod
    def poll(cls, ntree):
        return isinstance(ntree, NodeTreeBase)

class CurvePathNode(CurveNodeBase, ObjectNode):
    '''Get curve geometry values'''
    bl_idname = 'CurvePathNode'
    bl_label = 'Curve Path'

    bl_id_property_type = 'OBJECT'
    def bl_id_property_poll(self, ob):
        return ob.type == 'CURVE'

    def draw_buttons(self, context, layout):
        layout.template_ID(self, "id")

    def eval_dependencies(self, depsnode):
        curveob = self.id
        if curveob:
            depsnode.add_object_relation(curveob, 'TRANSFORM')
            depsnode.add_object_relation(curveob, 'GEOMETRY')

    def init(self, context):
        self.inputs.new('NodeSocketFloat', "Parameter")
        self.outputs.new('NodeSocketVector', "Location")
        self.outputs.new('NodeSocketVector', "Direction")
        self.outputs.new('NodeSocketVector', "Normal")
        self.outputs.new('TransformSocket', "Rotation")
        self.outputs.new('NodeSocketFloat', "Radius")
        self.outputs.new('NodeSocketFloat', "Weight")
        self.outputs.new('NodeSocketFloat', "Tilt")

    def compile(self, compiler):
        if self.id is None:
            return
        ob, tfm, itfm = compile_modifier_inputs(compiler, self.id)
        
        node = compiler.add_node("CURVE_PATH")
        compiler.link(ob, node.inputs[0])
        compiler.link(tfm, node.inputs[1])
        compiler.link(itfm, node.inputs[2])
        compiler.map_input(0, node.inputs[3])
        
        compiler.map_output(0, node.outputs[0])
        compiler.map_output(1, node.outputs[1])
        compiler.map_output(2, node.outputs[2])
        compiler.map_output(3, node.outputs[3])
        compiler.map_output(4, node.outputs[4])
        compiler.map_output(5, node.outputs[5])
        compiler.map_output(6, node.outputs[6])

###############################################################################

class GeometryNodesNew(Operator):
    """Create new geometry node tree"""
    bl_idname = "object_nodes.geometry_nodes_new"
    bl_label = "New"
    bl_options = {'REGISTER', 'UNDO'}

    name = StringProperty(
            name="Name",
            default="GeometryNodes",
            )

    @classmethod
    def make_node_tree(cls, name="GeometryNodes"):
        ntree = bpy.data.node_groups.new(name, GeometryNodeTree.bl_idname)
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
        "Geometry", GeometryNodeTree, GeometryNodeBase)
    bpy.utils.register_module(__name__)

    node_categories = [
        GeometryNodeCategory("GEO_INPUT", "Input", items=[
            NodeItem("ObjectRangeNode"),
            NodeItem("GeometryMeshLoadNode"),
            NodeItem("GeometryObjectFinalMeshNode"),
            NodeItem(ginput.bl_idname),
            NodeItem("GeometryElementInfoNode"),
            NodeItem("ObjectValueFloatNode"),
            NodeItem("ObjectValueIntNode"),
            NodeItem("ObjectValueVectorNode"),
            NodeItem("ObjectValueColorNode"),
            ]),
        GeometryNodeCategory("GEO_OUTPUT", "Output", items=[
            NodeItem("GeometryOutputNode"),
            NodeItem(goutput.bl_idname),
            ]),
        GeometryNodeCategory("GEO_MODIFIER", "Modifier", items=[
            NodeItem("GeometryMeshArrayNode"),
            NodeItem("GeometryMeshDisplaceNode"),
            NodeItem("GeometryBooleanNode"),
            ]),
        GeometryNodeCategory("GEO_CONVERTER", "Converter", items=[
            NodeItem("ObjectSeparateVectorNode"),
            NodeItem("ObjectCombineVectorNode"),
            NodeItem("GeometryMeshCombineNode"),
            ]),
        GeometryNodeCategory("GEO_MATH", "Math", items=[
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
            NodeItem("GeometryClosestPointNode"),
            ]),
        GeometryNodeCategory("GEO_TEXTURE", "Texture", items=[
            NodeItem("ImageSampleNode"),
            NodeItem("ObjectTextureCloudsNode"),
            NodeItem("ObjectTextureDistNoiseNode"),
            NodeItem("ObjectTextureMagicNode"),
            NodeItem("ObjectTextureMarbleNode"),
            NodeItem("ObjectTextureMusgraveNode"),
            NodeItem("ObjectTextureStucciNode"),
            NodeItem("ObjectTextureVoronoiNode"),
            NodeItem("ObjectTextureWoodNode"),
            ]),
        GeometryNodeCategory("GEO_CURVE", "Curve", items=[
            NodeItem("CurvePathNode"),
            ]),
        group_nodes.GroupNodeCategory("GEO", gnode, ginput, goutput),
        ]
    nodeitems_utils.register_node_categories("GEOMETRY_NODES", node_categories)

def unregister():
    nodeitems_utils.unregister_node_categories("GEOMETRY_NODES")

    bpy.utils.unregister_module(__name__)

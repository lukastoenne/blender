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
from common_nodes import NodeTreeBase, NodeBase, DynamicSocketListNode, enum_property_copy, enum_property_value_prop
import group_nodes

###############################################################################


# our own base class with an appropriate poll function,
# so the categories only show up in our own tree type
class TextureNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        tree = context.space_data.edit_tree
        return tree and tree.bl_idname == 'TextureNodeTree'


###############################################################################

class TextureNodeTree(NodeTreeBase, NodeTree):
    '''Texture nodes'''
    bl_idname = 'TextureNodeTree'
    bl_label = 'Texture Nodes'
    bl_icon = 'TEXTURE'

    @classmethod
    def get_from_context(cls, context):
        space = context.space_data

        ntree = None
        id_owner = None
        id_from = None

        if (space.texture_type == 'OBJECT'):
            ob = context.active_object
            if ob:
                if ob.type == 'LAMP':
                    id_from = ob.data
                    id_owner = id_from.active_texture
                else:
                    id_from = ob.active_material
                    ma = id_from.active_node_material if id_from else None
                    if not ma:
                        ma = id_from
                    id_owner = ma.active_texture if ma else None
                ntree = id_owner.node_tree if id_owner else None
        
        elif (space.texture_type == 'WORLD'):
            scene = context.scene
            id_from = scene.world
            id_owner = id_from.active_texture if id_from else None
            ntree = id_owner.node_tree if id_owner else None
        
        elif (space.texture_type == 'BRUSH'):
            ob = context.active_object
            if ob and ob.mode == 'SCULPT':
                id_from = scene.tool_settings.sculpt.brush
            else:
                id_from = scene.tool_settings.image_paint.brush
            id_owner = id_from.texture if id_from else None
            ntree = id_owner.node_tree if id_owner else None
        
        elif (space.texture_type == 'LINESTYLE'):
            rlayer = context.scene.render.layers.active
            lineset = rlayer.freestyle.linesets.active if rlayer else None
            id_from = lineset.linestyle if lineset else None
            id_owner = id_from.active_texture if id_from else None
            ntree = id_owner.node_tree if id_owner else None

        return ntree, id_owner, id_from

    def init_default(self):
        out = self.nodes.new(TextureOutputNode.bl_idname)
        out.location = (0, 20)

###############################################################################

class TextureNodeBase(NodeBase):
    @classmethod
    def poll(cls, ntree):
        return ntree.bl_idname == 'TextureNodeTree'


class TextureOutputNode(TextureNodeBase, ObjectNode, DynamicSocketListNode):
    '''Texture output'''
    bl_idname = 'TextureOutputNode'
    bl_label = 'Output'

    def init(self, context):
        s = self.inputs.new('NodeSocketColor', "Color")
        s.default_value = (1, 1, 1, 1)
        s = self.inputs.new('NodeSocketVector', "Normal")
        s.default_value = (0, 0, 1)

    def compile(self, compiler):
        compiler.map_input(0, compiler.graph_output("color"))
        compiler.map_input(1, compiler.graph_output("normal"))

###############################################################################

def register():
    gnode, ginput, goutput = group_nodes.make_node_group_types(
        "Texture", TextureNodeTree, TextureNodeBase)
    bpy.utils.register_module(__name__)

    node_categories = [
        TextureNodeCategory("TEX_INPUT", "Input", items=[
            NodeItem(ginput.bl_idname),
            NodeItem("ObjectValueFloatNode"),
            NodeItem("ObjectValueIntNode"),
            NodeItem("ObjectValueVectorNode"),
            NodeItem("ObjectValueColorNode"),
            ]),
        TextureNodeCategory("TEX_OUTPUT", "Output", items=[
            NodeItem("TextureOutputNode"),
            NodeItem(goutput.bl_idname),
            ]),
        TextureNodeCategory("TEX_CONVERTER", "Converter", items=[
            NodeItem("ObjectSeparateVectorNode"),
            NodeItem("ObjectCombineVectorNode"),
            ]),
        TextureNodeCategory("TEX_MATH", "Math", items=[
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
        TextureNodeCategory("TEX_TEXTURE", "Texture", items=[
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
        group_nodes.GroupNodeCategory("TEX", gnode, ginput, goutput),
        ]
    nodeitems_utils.register_node_categories("TEXTURE_NODES", node_categories)

def unregister():
    nodeitems_utils.unregister_node_categories("TEXTURE_NODES")

    bpy.utils.unregister_module(__name__)

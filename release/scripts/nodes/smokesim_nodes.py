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
from bpy.types import PropertyGroup, Operator, ObjectNode, NodeTree, Node
from bpy.props import *
from nodeitems_utils import NodeCategory, NodeItem
from mathutils import *
from common_nodes import NodeTreeBase, NodeBase, DynamicSocketListNode, enum_property_copy, enum_property_value_prop
import group_nodes

###############################################################################


# our own base class with an appropriate poll function,
# so the categories only show up in our own tree type
class SmokeSimNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        tree = context.space_data.edit_tree
        return tree and tree.bl_idname == 'SmokeSimNodeTree'

###############################################################################

class SmokeSimNodeTree(NodeTreeBase, NodeTree):
    '''Smoke simulation nodes'''
    bl_idname = 'SmokeSimNodeTree'
    bl_label = 'SmokeSim Nodes'
    bl_icon = 'MOD_SMOKE'

    # does not show up in the editor header
    @classmethod
    def poll(cls, context):
        return False

    def init_default(self):
        #out = self.nodes.new(OutputNode.bl_idname)
        #out.location = (100, 20)
        pass


class SmokeSimNodeBase(NodeBase):
    @classmethod
    def poll(cls, ntree):
        return ntree.bl_idname == 'SmokeSimNodeTree'

###############################################################################

# class SmokeStepNode(SmokeSimNodeBase, ObjectNode):
#     '''Evolve the velocity field and marker particles of a smoke volume'''
#     bl_idname = 'SmokeSimNode_Step'
#     bl_label = 'Step'

#     _advection_scheme_items = [
#         ('SEMI_LAGRANGE', "Semi-Lagrange", "Semi-Lagrangian advection scheme"),
#         ('MACCORMACK', "MacCormack", "MacCormack advection scheme"),
#         ]
#     advection_scheme = EnumProperty(name="Advection Scheme",
#                                     description="Method of transport through the velocity field",
#                                     items=_advection_scheme_items,
#                                     default='MACCORMACK')

#     def draw_buttons(self, context, layout):
#         layout.prop(self, "advection_scheme")

#     def init(self, context):
#         s = self.inputs.new('NodeSocketInt', "Substeps")
#         s.default_value = 1
#         s = self.inputs.new('NodeSocketFloat', "Time Scale")
#         s.default_value = 1.0
#         self.inputs.new('NodeSocketFloat', "Density")
#         self.inputs.new('NodeSocketVector', "Velocity")
#         self.outputs.new('NodeSocketVector', "Velocity")

#     def compile(self, compiler):
#         pass


class TimeStepNode(SmokeSimNodeBase, ObjectNode):
    '''Perform a smoke simulation time step'''
    bl_idname = 'SmokeSimNode_TimeStep'
    bl_label = 'Time Step'

    def init(self, context):
        s = self.inputs.new('NodeSocketInt', "Substeps")
        s.default_value = 1
        s = self.inputs.new('NodeSocketFloat', "Time Scale")
        s.default_value = 1.0
        self.inputs.new('NodeSocketFloat', "Density")
        self.inputs.new('NodeSocketVector', "Velocity")

    def compile(self, compiler):
        pass


class StateNode(SmokeSimNodeBase, ObjectNode):
    '''State of the smoke simulation data'''
    bl_idname = 'SmokeSimNode_State'
    bl_label = 'State'

    def init(self, context):
        self.outputs.new('NodeSocketFloat', "Density")
        self.outputs.new('NodeSocketVector', "Velocity")

    def compile(self, compiler):
        pass


class ExtForcesNode(SmokeSimNodeBase, ObjectNode):
    '''Apply external forces to the velocity field'''
    bl_idname = 'SmokeSimNode_ExtForces'
    bl_label = 'External Forces'

    bl_id_property_type = 'GROUP'

    def draw_buttons(self, context, layout):
        layout.template_ID(self, "id")

    def init(self, context):
        self.inputs.new('NodeSocketVector', "Velocity")
        s = self.inputs.new('NodeSocketFloat', "Influence")
        s.default_value = 1.0
        self.outputs.new('NodeSocketVector', "Velocity")

    def compile(self, compiler):
        pass


class AdvectNode(SmokeSimNodeBase, ObjectNode):
    '''Advect grid data based on velocity'''
    bl_idname = 'SmokeSimNode_Advect'
    bl_label = 'Advect'

    _mode_items = [
        ('SEMI_LAGRANGE', "Semi-Lagrange", "Semi-Lagrangian advection scheme"),
        ('MACCORMACK', "MacCormack", "MacCormack advection scheme"),
        ]
    mode = EnumProperty(name="Mode",
                        description="Advection scheme",
                        items=_mode_items,
                        default='MACCORMACK')

    def draw_buttons(self, context, layout):
        layout.prop(self, "mode")

    def init(self, context):
        self.inputs.new('AnySocket', "Data")
        self.inputs.new('NodeSocketVector', "Velocity")
        self.outputs.new('AnySocket', "Advected Data")

    def compile(self, compiler):
        pass


class DiffusionNode(SmokeSimNodeBase, ObjectNode):
    '''Apply a viscous diffusion step to a velocity grid'''
    bl_idname = 'SmokeSimNode_Diffusion'
    bl_label = 'Diffusion'

    def init(self, context):
        self.inputs.new('NodeSocketVector', "Velocity")
        self.inputs.new('NodeSocketFloat', "Viscosity")
        self.outputs.new('NodeSocketVector', "Velocity")

    def compile(self, compiler):
        pass


class ProjectionNode(SmokeSimNodeBase, ObjectNode):
    '''Make a velocity field divergence-free by projection'''
    bl_idname = 'SmokeSimNode_Projection'
    bl_label = 'Projection'

    def init(self, context):
        self.inputs.new('NodeSocketVector', "Velocity")
        self.outputs.new('NodeSocketFloat', "Pressure")
        self.outputs.new('NodeSocketVector', "Velocity")

    def compile(self, compiler):
        pass


class ExtrapolateCellsNode(SmokeSimNodeBase, ObjectNode):
    '''Extrapolate velocity grid into inactive cells'''
    bl_idname = 'SmokeSimNode_ExtrapolateCells'
    bl_label = 'Extrapolate Cells'

    def init(self, context):
        self.inputs.new('NodeSocketVector', "Velocity")
        self.outputs.new('NodeSocketVector', "Velocity")

    def compile(self, compiler):
        pass


class ParticleGridsNode(SmokeSimNodeBase, ObjectNode):
    '''Calculate grid values from particles'''
    bl_idname = 'SmokeSimNode_ParticleGrids'
    bl_label = 'Particle Grids'

    def init(self, context):
        self.inputs.new('GeometrySocket', "Particles")
        self.outputs.new('NodeSocketFloat', "Density")
        self.outputs.new('NodeSocketVector', "Velocity")

    def compile(self, compiler):
        pass


class MoveMarkerParticlesNode(SmokeSimNodeBase, ObjectNode):
    '''Move marker particles through the velocity grid'''
    bl_idname = 'SmokeSimNode_MoveMarkerParticles'
    bl_label = 'Move Marker Particles'

    def init(self, context):
        self.inputs.new('GeometrySocket', "Particles")
        self.inputs.new('NodeSocketVector', "Velocity")
        self.outputs.new('GeometrySocket', "Particles")

    def compile(self, compiler):
        pass

###############################################################################

class SmokeSimNodesNew(Operator):
    """Create new smoke simulation node tree"""
    bl_idname = "object_nodes.smokesim_nodes_new"
    bl_label = "New"
    bl_options = {'REGISTER', 'UNDO'}

    name = StringProperty(
            name="Name",
            default="SmokeSimNodes",
            )

    @classmethod
    def make_node_tree(cls, name="SmokeSimNodes"):
        ntree = bpy.data.node_groups.new(name, SmokeSimNodeTree.bl_idname)
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
        "SmokeSim", SmokeSimNodeTree, SmokeSimNodeBase)
    bpy.utils.register_module(__name__)

    node_categories = [
        SmokeSimNodeCategory("SMO_INPUT", "Input", items=[
            NodeItem("SmokeSimNode_State"),
            NodeItem("ObjectIterationNode"),
            NodeItem(ginput.bl_idname),
            NodeItem("ObjectValueFloatNode"),
            NodeItem("ObjectValueIntNode"),
            NodeItem("ObjectValueVectorNode"),
            NodeItem("ObjectValueColorNode"),
            ]),
        SmokeSimNodeCategory("SMO_OUTPUT", "Output", items=[
            NodeItem("SmokeSimNode_TimeStep"),
            NodeItem(goutput.bl_idname),
            ]),
        SmokeSimNodeCategory("SMO_VOLUME", "Volume", items=[
            NodeItem("SmokeSimNode_ExtForces"),
            NodeItem("SmokeSimNode_Advect"),
            NodeItem("SmokeSimNode_Diffusion"),
            NodeItem("SmokeSimNode_Projection"),
            NodeItem("SmokeSimNode_ExtrapolateCells"),
            NodeItem("SmokeSimNode_ParticleGrids"),
            NodeItem("SmokeSimNode_MoveMarkerParticles"),
            ]),
        SmokeSimNodeCategory("SMO_CONVERTER", "Converter", items=[
            NodeItem("ObjectSeparateVectorNode"),
            NodeItem("ObjectCombineVectorNode"),
            ]),
        SmokeSimNodeCategory("SMO_MATH", "Math", items=[
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
        SmokeSimNodeCategory("SMO_TEXTURE", "Texture", items=[
            NodeItem("ObjectTextureCloudsNode"),
            NodeItem("ObjectTextureDistNoiseNode"),
            NodeItem("ObjectTextureMagicNode"),
            NodeItem("ObjectTextureMarbleNode"),
            NodeItem("ObjectTextureMusgraveNode"),
            NodeItem("ObjectTextureStucciNode"),
            NodeItem("ObjectTextureVoronoiNode"),
            NodeItem("ObjectTextureWoodNode"),
            ]),
        group_nodes.GroupNodeCategory("SMO", gnode, ginput, goutput),
        ]
    nodeitems_utils.register_node_categories("SMOKESIM_NODES", node_categories)

def unregister():
    nodeitems_utils.unregister_node_categories("SMOKESIM_NODES")

    bpy.utils.unregister_module(__name__)

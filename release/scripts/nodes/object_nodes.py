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
from smokesim_nodes import SmokeSimNodesNew
from rigidbody_nodes import RigidBodyNodesNew

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
        layout.template_ID(self, "id", new=InstancingNodesNew.bl_idname)

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
        layout.template_ID(self, "id", new=ForceFieldNodesNew.bl_idname)

    def compile(self, compiler):
        pass


class SmokeSimNode(ObjectNodeBase, ObjectNode):
    '''Smoke simulation'''
    bl_idname = 'SmokeSimNode'
    bl_label = 'Smoke Simulation'
    bl_icon = 'MOD_SMOKE'

    bl_id_property_type = 'NODETREE'
    def bl_id_property_poll(self, ntree):
        return ntree.bl_idname == 'SmokeSimNodeTree'

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
        layout.template_ID(self, "id", new=SmokeSimNodesNew.bl_idname)

    def compile(self, compiler):
        pass


class RigidBodyNode(ObjectNodeBase, ObjectNode):
    '''Rigid body simulation'''
    bl_idname = 'RigidBodyNode'
    bl_label = 'Rigid Body Simulation'
    bl_icon = 'PHYSICS'

    bl_id_property_type = 'NODETREE'
    def bl_id_property_poll(self, ntree):
        return ntree.bl_idname == 'RigidBodyNodeTree'

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
        layout.template_ID(self, "id", new=RigidBodyNodesNew.bl_idname)

    def compile(self, compiler):
        pass

###############################################################################

class ObjectComponentSocket(NodeSocket):
    '''Object data component'''
    bl_idname = 'ObjectComponentSocket'
    bl_label = 'Component'

    is_readonly = BoolProperty(name="Is Placeholder",
                               default=False)

    def draw(self, context, layout, node, text):
        layout.label(text)

    def draw_color(self, context, node):
        alpha = 0.4 if self.is_readonly else 1.0
        return (1.0, 0.4, 0.216, alpha)

class ComponentsNode(ObjectNodeBase, ObjectNode):
    '''Object data components'''
    bl_idname = 'ObjectComponentsNode'
    bl_label = 'Components'

    def init(self, context):
        self.outputs.new('ObjectComponentSocket', "Particles")
        self.outputs.new('ObjectComponentSocket', "Fracture Mesh")

    def compile(self, compiler):
        pass

class ApplyIslandTransformsNode(ObjectNodeBase, ObjectNode):
    '''Apply mesh island transforms from particles'''
    bl_idname = 'ObjectApplyMeshIslandsTransformNode'
    bl_label = 'Apply Island Transforms'

    def init(self, context):
        self.inputs.new('ObjectComponentSocket', "Particles").is_readonly = True
        self.inputs.new('ObjectComponentSocket', "Fracture Mesh").is_readonly = True
        self.outputs.new('ObjectComponentSocket', "Mesh")

    def compile(self, compiler):
        pass

class ParticleRigidBodySimNode(ObjectNodeBase, ObjectNode):
    '''Define particles as rigid bodies in the scene'''
    bl_idname = 'ObjectParticleRigidBodySimNodeNode'
    bl_label = 'Particle Rigid Body Simulation'

    def init(self, context):
        self.inputs.new('ObjectComponentSocket', "Particles")
        self.inputs.new('ObjectComponentSocket', "Fracture Mesh").is_readonly = True
        self.outputs.new('ObjectComponentSocket', "RB Particles")

    def compile(self, compiler):
        pass

class DynamicFractureNode(ObjectNodeBase, ObjectNode):
    '''Fracture shards based on collision impacts'''
    bl_idname = 'ObjectDynamicFractureNodeNode'
    bl_label = 'Dynamic Fracture'

    def init(self, context):
        self.inputs.new('ObjectComponentSocket', "Particles")
        self.inputs.new('ObjectComponentSocket', "Fracture Mesh")
        self.outputs.new('ObjectComponentSocket', "Particles")
        self.outputs.new('ObjectComponentSocket', "Fracture Mesh")

    def compile(self, compiler):
        pass

def make_attribute_nodes(attribute_set, attr_default, data_name, data_type):
    def attribute_items(self, context):
        return [(attr[0], attr[0], "", 'NONE', 2**i) for i, attr in enumerate(attribute_set)]

    def update_attributes(self, context):
        for socket in self.outputs:
            socket.enabled = (socket.name in self.attributes)

    class get_node(ObjectNodeBase, ObjectNode):
        __doc__ = "Get attribute from %s" % data_name
        bl_idname = "Get%sAttributeNode" % data_name
        bl_label = "Get %s Attribute" % data_name

        attributes = EnumProperty(name="Attribute",
                                  items=attribute_items,
                                  update=update_attributes,
                                  options={'ENUM_FLAG'})

        def draw_buttons(self, context, layout):
            layout.prop_menu_enum(self, "attributes")

        def init(self, context):
            self.inputs.new(data_type, data_name).is_readonly = True
            for attr in attribute_set:
                self.outputs.new(attr[1], attr[0])

            # set default
            self.attributes = { attr_default }

        def compile(self, compiler):
            pass

    class set_node(ObjectNodeBase, ObjectNode):
        __doc__ = "Set attribute of %s" % data_name
        bl_idname = "Set%sAttributeNode" % data_name
        bl_label = "Set %s Attribute" % data_name

        def update_attributes(self, context):
            for socket in self.inputs[1:]:
                socket.enabled = (socket.name in self.attributes)
        attributes = EnumProperty(name="Attribute",
                                  items=attribute_items,
                                  update=update_attributes,
                                  options={'ENUM_FLAG'})

        def draw_buttons(self, context, layout):
            layout.prop_menu_enum(self, "attributes")

        def init(self, context):
            self.inputs.new(data_type, data_name)
            for attr in attribute_set:
                self.inputs.new(attr[1], attr[0])
            self.outputs.new(data_type, data_name)

            # set default
            self.attributes = { attr_default }

        def compile(self, compiler):
            pass

    return get_node, set_node

_mesh_attribute_set = [
    ("vertex.location", 'NodeSocketVector'),
    ("vertex.shard", 'NodeSocketInt'),
    ]
GetMeshAttributeNode, SetMeshAttributeNode = \
    make_attribute_nodes(_mesh_attribute_set, 'vertex.location',
                         "Mesh", 'ObjectComponentSocket')

_particle_attribute_set = [
    ("id", 'NodeSocketInt'),
    ("location", 'NodeSocketVector'),
    ("velocity", 'NodeSocketVector'),
    ]
GetParticlesAttributeNode, SetParticlesAttributeNode = \
    make_attribute_nodes(_particle_attribute_set, 'location',
                         "Particles", 'ObjectComponentSocket')

class DefineRigidBodyNode(ObjectNodeBase, ObjectNode):
    '''Define rigid bodies for simulation'''
    bl_idname = 'DefineRigidBodyNode'
    bl_label = 'Define Rigid Body'

    def init(self, context):
        self.inputs.new('NodeSocketInt', "ID")
        self.inputs.new('TransformSocket', "transform")
        self.inputs.new('NodeSocketVector', "velocity")
        self.inputs.new('NodeSocketVector', "angular velocity")
        self.outputs.new('TransformSocket', "transform")
        self.outputs.new('NodeSocketVector', "velocity")
        self.outputs.new('NodeSocketVector', "angular velocity")

    def compile(self, compiler):
        pass

class CacheRigidBodyContactsNode(ObjectNodeBase, ObjectNode):
    '''Contacts of rigid bodies from the collision step'''
    bl_idname = 'CacheRigidBodyContactsNode'
    bl_label = 'Cache Rigid Body Contacts'

    def init(self, context):
        self.inputs.new('NodeSocketInt', "ID")
        self.outputs.new('ObjectComponentSocket', "contacts")

    def compile(self, compiler):
        pass

class FindMaxImpactNode(ObjectNodeBase, ObjectNode):
    '''Find the contact with maximum impact force'''
    bl_idname = 'FindMaxImpactNode'
    bl_label = 'Find Maximum Impact'

    def init(self, context):
        self.inputs.new('NodeSocketInt', "ID")
        self.inputs.new('ObjectComponentSocket', "contacts")
        self.outputs.new('NodeSocketVector', "max impact")

    def compile(self, compiler):
        pass

class SingleImpactFractureNode(ObjectNodeBase, ObjectNode):
    '''Fracture shards based on a single impact'''
    bl_idname = 'SingleImpactFractureNode'
    bl_label = 'Single Impact Fracture'

    def init(self, context):
        self.inputs.new('NodeSocketVector', "impact")
        self.inputs.new('NodeSocketFloat', "threshold")
        self.inputs.new('ObjectComponentSocket', "particles")
        self.inputs.new('ObjectComponentSocket', "shards")
        self.outputs.new('ObjectComponentSocket', "particles")
        self.outputs.new('ObjectComponentSocket', "shards")

    def compile(self, compiler):
        pass

class MapValueNode(ObjectNodeBase, ObjectNode):
    '''Use an index to map values to a new index domain'''
    bl_idname = 'MapValueNode'
    bl_label = 'Map Value'

    def init(self, context):
        self.inputs.new('NodeSocketInt', "index")
        self.inputs.new('TransformSocket', "values")
        self.outputs.new('TransformSocket', "mapped values")

    def compile(self, compiler):
        pass

class ApplyTransformNode(ObjectNodeBase, ObjectNode):
    '''Apply transform to a vector'''
    bl_idname = 'ApplyTransformNode'
    bl_label = 'Apply Transform'

    def init(self, context):
        self.inputs.new('TransformSocket', "transform")
        self.inputs.new('NodeSocketVector', "vector")
        self.outputs.new('NodeSocketVector', "vector")

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
        ObjectNodeCategory("OBJ_COMPONENTS", "Components", items=[
            NodeItem("GeometryNode",
                     settings={"id": "bpy.types.OBJECT_NODES_OT_geometry_nodes_new.make_node_tree()"}),
            NodeItem("ForceFieldNode",
                     settings={"id": "bpy.types.OBJECT_NODES_OT_forcefield_nodes_new.make_node_tree()"}),
            NodeItem("InstancingNode",
                     settings={"id": "bpy.types.OBJECT_NODES_OT_instancing_nodes_new.make_node_tree()"}),
            NodeItem("SmokeSimNode",
                     settings={"id": "bpy.types.OBJECT_NODES_OT_smokesim_nodes_new.make_node_tree()"}),
            NodeItem("RigidBodyNode",
                     settings={"id": "bpy.types.OBJECT_NODES_OT_rigidbody_nodes_new.make_node_tree()"}),
            NodeItem("ObjectComponentsNode"),
            NodeItem("ObjectApplyMeshIslandsTransformNode"),
            NodeItem("ObjectParticleRigidBodySimNodeNode"),
            NodeItem("ObjectDynamicFractureNodeNode"),
            NodeItem("GetMeshAttributeNode"),
            NodeItem("SetMeshAttributeNode"),
            NodeItem("GetParticlesAttributeNode"),
            NodeItem("SetParticlesAttributeNode"),
            NodeItem("DefineRigidBodyNode"),
            NodeItem("CacheRigidBodyContactsNode"),
            NodeItem("FindMaxImpactNode"),
            NodeItem("SingleImpactFractureNode"),
            NodeItem("MapValueNode"),
            NodeItem("ApplyTransformNode"),
            ]),
        ObjectNodeCategory("OBJ_CONVERTER", "Converter", items=[
            NodeItem("ObjectSeparateVectorNode"),
            NodeItem("ObjectCombineVectorNode"),
            ]),
        ObjectNodeCategory("OBJ_MATH", "Math", items=[
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

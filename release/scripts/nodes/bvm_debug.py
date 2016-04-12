### BEGIN GPL LICENSE BLOCK #####
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

# <pep8 compliant>

import os, subprocess
import bpy
from bpy.types import Operator, Panel
from bpy.props import EnumProperty

txtfile = '/tmp/bvm_nodes.txt'
dotfile = '/tmp/bvm_nodes.dot'
imgfile = '/tmp/bvm_nodes.svg'
dotformat = 'svg'

def enum_property_copy(prop):
    items = [(i.identifier, i.name, i.description, i.icon, i.value) for i in prop.enum_items]
    return EnumProperty(name=prop.name,
                        description=prop.description,
                        default=prop.default,
                        items=items)

class BVMNodeGraphvizOperator(Operator):
    bl_idname = "node.bvm_graphviz_show"
    bl_label = "Show Debug Graphviz Nodes"
    bl_options = {'REGISTER', 'UNDO'}

    function_type = enum_property_copy(bpy.types.NodeTree.bl_rna.functions['bvm_debug_graphviz'].parameters['function_type'])
    debug_mode = enum_property_copy(bpy.types.NodeTree.bl_rna.functions['bvm_debug_graphviz'].parameters['debug_mode'])

    def execute(self, context):
        if not hasattr(context, "debug_nodetree"):
            return {'CANCELLED'}

        ntree = context.debug_nodetree

        if (self.debug_mode in {'NODES', 'NODES_UNOPTIMIZED', 'BVM_CODE'}):
            ntree.bvm_debug_graphviz(dotfile, self.function_type, self.debug_mode, label=ntree.name)

            process = subprocess.Popen(['dot', '-T'+dotformat, '-o', imgfile, dotfile])
            process.wait()
        
            subprocess.Popen(['xdg-open', imgfile])
        else:
            ntree.bvm_debug_graphviz(txtfile, self.function_type, self.debug_mode, label=ntree.name)
            
            subprocess.Popen(['xdg-open', txtfile])
        
        return {'FINISHED'}

def draw_depshow_op(layout, ntree):
    if isinstance(ntree, bpy.types.GeometryNodeTree):
        funtype = 'GEOMETRY'
    elif isinstance(ntree, bpy.types.InstancingNodeTree):
        funtype = 'INSTANCING'
    elif isinstance(ntree, bpy.types.TextureNodeTree):
        funtype = 'TEXTURE'
    elif isinstance(ntree, bpy.types.ForceFieldNodeTree):
        funtype = 'FORCEFIELD'
    else:
        return

    layout.context_pointer_set("debug_nodetree", ntree)

    col = layout.column(align=True)
    props = col.operator(BVMNodeGraphvizOperator.bl_idname, text="Nodes")
    props.function_type = funtype
    props.debug_mode = 'NODES'
    props = col.operator(BVMNodeGraphvizOperator.bl_idname, text="Nodes (unoptimized)")
    props.function_type = funtype
    props.debug_mode = 'NODES_UNOPTIMIZED'
    props = col.operator(BVMNodeGraphvizOperator.bl_idname, text="BVM Code")
    props.function_type = funtype
    props.debug_mode = 'BVM_CODE'
    props = col.operator(BVMNodeGraphvizOperator.bl_idname, text="LLVM Code")
    props.function_type = funtype
    props.debug_mode = 'LLVM_CODE'
    props = col.operator(BVMNodeGraphvizOperator.bl_idname, text="LLVM Code (unoptimized)")
    props.function_type = funtype
    props.debug_mode = 'LLVM_CODE_UNOPTIMIZED'

class BVMNodeGraphvizPanel(Panel):
    bl_idname = "node.bvm_graphviz_panel"
    bl_label = "Debug Nodes"
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'

    @classmethod
    def poll(cls, context):
        # XXX should use a bpy.app.debug_*** option to enable this

        if context.space_data.edit_tree is None:
            return False
        return True

    def draw(self, context):
        space = context.space_data
        ntree = space.edit_tree
        layout = self.layout
        draw_depshow_op(layout, ntree)

def register():
    bpy.utils.register_class(BVMNodeGraphvizOperator)
    bpy.utils.register_class(BVMNodeGraphvizPanel)

def unregister():
    bpy.utils.unregister_class(BVMNodeGraphvizPanel)
    bpy.utils.unregister_class(BVMNodeGraphvizOperator)

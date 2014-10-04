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

# <pep8 compliant>
import bpy
from bpy.types import Panel

from bl_ui.properties_physics_common import (point_cache_ui,
                                             effector_weights_ui)


class PhysicButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    @classmethod
    def poll(cls, context):
        ob = context.object
        rd = context.scene.render
        return (ob and ob.type == 'MESH') and (not rd.use_game_engine) and (context.hair)


class PHYSICS_PT_hair(PhysicButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        col = layout.column()
        col.operator("hair.reset to rest location")
        col.operator("hair.copy_from_particles")


class PHYSICS_PT_hair_simulation(PhysicButtonsPanel, Panel):
    bl_label = "Hair Simulation"

    def draw(self, context):
        layout = self.layout
        md = context.hair
        hsys = md.hair_system
        params = hsys.params

        split = layout.split()

        split.prop(params, "substeps")

        split = layout.split()

        col = split.column()
        col.prop(params, "stretch_stiffness")
        col.prop(params, "bend_stiffness")
        col.prop(params, "bend_smoothing")

        col = split.column()
        col.prop(params, "stretch_damping")
        col.prop(params, "bend_damping")

class PHYSICS_PT_hair_field_weights(PhysicButtonsPanel, Panel):
    bl_label = "Field Weights"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout
        md = context.hair
        hsys = md.hair_system
        params = hsys.params

        effector_weights_ui(self, context, params.effector_weights, 'HAIR')


class PHYSICS_PT_hair_collision(PhysicButtonsPanel, Panel):
    bl_label = "Hair Collision"

    def draw(self, context):
        layout = self.layout
        md = context.hair
        hsys = md.hair_system
        params = hsys.params

        split = layout.split()

        col = split.column()
        col.prop(params, "restitution")
        col.prop(params, "friction")
        col.prop(params, "margin")

        col = split.column()
        col.prop(params, "drag")


class PHYSICS_PT_hair_render(PhysicButtonsPanel, Panel):
    bl_label = "Hair Render"

    def draw(self, context):
        layout = self.layout
        md = context.hair
        hsys = md.hair_system
        params = hsys.params
        render = params.render

        col = layout.column()

        col.prop(render, "material_slot")

        col.separator()

        col.prop(render, "render_hairs")
        col.prop(render, "interpolation_steps")

        col.label("Curl:")
        col.prop(render, "curl_smoothing")

        col.label("Strands:")
        col.prop(render, "shape", text="Shape")
        row = col.row()
        row.prop(render, "root_width", text="Root")
        row.prop(render, "tip_width", text="Tip")
        row = col.row()
        row.prop(render, "radius_scale", text="Scaling")
        row.prop(render, "use_closetip", text="Close tip")


class PHYSICS_PT_hair_display(PhysicButtonsPanel, Panel):
    bl_label = "Hair Display"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        md = context.hair
        hsys = md.hair_system
        display = hsys.display

        col = layout.column()
        row = col.row()
        row.prop(display, "mode", expand=True)


class PHYSICS_PT_hair_debug(PhysicButtonsPanel, Panel):
    bl_label = "Hair Debugging"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        self.layout.prop(context.hair, "show_debug", text="")

    def draw(self, context):
        layout = self.layout
        md = context.hair

        col = layout.column()
        col.active = md.show_debug
        col.prop(md, "show_debug_contacts")
        col.prop(md, "show_debug_size")
        col.prop(md, "show_debug_roots")
        col.prop(md, "show_debug_frames")
        col.prop(md, "show_debug_bending")
        col.prop(md, "show_debug_smoothing")
        
        box = col.box()
        box.prop(md, "show_debug_forces")
        sub = box.column(align=True)
        sub.prop(md, "show_debug_force_bending", toggle=True)


#class PHYSICS_PT_hair_cache(PhysicButtonsPanel, Panel):
    #bl_label = "Hair Cache"
    #bl_options = {'DEFAULT_CLOSED'}
#
    #@classmethod
    #def poll(cls, context):
        #md = context.hair
        #rd = context.scene.render
        #return md and (not rd.use_game_engine)
#
    #def draw(self, context):
        #layout = self.layout
#
        #md = context.hair
        #hsys = md.hair_system
        #cache = hsys.point_cache
#
        #layout.label(text="Compression:")
        #layout.prop(md, "point_cache_compress_type", expand=True)
#
        #point_cache_ui(self, context, cache, (cache.is_baked is False), 'SMOKE')


#class PHYSICS_PT_hair_field_weights(PhysicButtonsPanel, Panel):
    #bl_label = "Hair Field Weights"
    #bl_options = {'DEFAULT_CLOSED'}
#
    #@classmethod
    #def poll(cls, context):
        #md = context.hair
        #rd = context.scene.render
        #return md and (not rd.use_game_engine)
#
    #def draw(self, context):
        #md = context.hair
        #hsys = md.hair_system
        #effector_weights_ui(self, context, hsys.effector_weights, 'HAIR')

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)

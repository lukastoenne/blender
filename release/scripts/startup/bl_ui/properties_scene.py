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
from bpy.types import Panel, UIList
from rna_prop_ui import PropertyPanel

from bl_ui.properties_physics_common import (
        point_cache_ui,
        effector_weights_ui,
        )


class SCENE_UL_keying_set_paths(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        # assert(isinstance(item, bpy.types.KeyingSetPath)
        kspath = item
        icon = layout.enum_item_icon(kspath, "id_type", kspath.id_type)
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            # Do not make this one editable in uiList for now...
            layout.label(text=kspath.data_path, translate=False, icon_value=icon)
        elif self.layout_type in {'GRID'}:
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class SceneButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "scene"

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return context.scene and (rd.engine in cls.COMPAT_ENGINES)


class SCENE_PT_scene(SceneButtonsPanel, Panel):
    bl_label = "Scene"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene

        layout.prop(scene, "camera")
        layout.prop(scene, "background_set", text="Background")
        if context.scene.render.engine != 'BLENDER_GAME':
            layout.prop(scene, "active_clip", text="Active Clip")


class SCENE_PT_unit(SceneButtonsPanel, Panel):
    bl_label = "Units"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        unit = context.scene.unit_settings

        col = layout.column()
        col.row().prop(unit, "system", expand=True)
        col.row().prop(unit, "system_rotation", expand=True)

        if unit.system != 'NONE':
            row = layout.row()
            row.prop(unit, "scale_length", text="Scale")
            row.prop(unit, "use_separate")


class SceneKeyingSetsPanel:
    def draw_keyframing_settings(self, context, layout, ks, ksp):
        self.draw_keyframing_setting(context, layout, ks, ksp, "Needed",
                                     "use_insertkey_override_needed", "use_insertkey_needed",
                                     userpref_fallback="use_keyframe_insert_needed")

        self.draw_keyframing_setting(context, layout, ks, ksp, "Visual",
                                     "use_insertkey_override_visual", "use_insertkey_visual",
                                     userpref_fallback="use_visual_keying")

        self.draw_keyframing_setting(context, layout, ks, ksp, "XYZ to RGB",
                                     "use_insertkey_override_xyz_to_rgb", "use_insertkey_xyz_to_rgb")

    def draw_keyframing_setting(self, context, layout, ks, ksp, label, toggle_prop, prop, userpref_fallback=None):
        if ksp:
            item = ksp

            if getattr(ks, toggle_prop):
                owner = ks
                propname = prop
            else:
                owner = context.user_preferences.edit
                if userpref_fallback:
                    propname = userpref_fallback
                else:
                    propname = prop
        else:
            item = ks

            owner = context.user_preferences.edit
            if userpref_fallback:
                propname = userpref_fallback
            else:
                propname = prop

        row = layout.row(align=True)
        row.prop(item, toggle_prop, text="", icon='STYLUS_PRESSURE', toggle=True)  # XXX: needs dedicated icon

        subrow = row.row()
        subrow.active = getattr(item, toggle_prop)
        if subrow.active:
            subrow.prop(item, prop, text=label)
        else:
            subrow.prop(owner, propname, text=label)


class SCENE_PT_keying_sets(SceneButtonsPanel, SceneKeyingSetsPanel, Panel):
    bl_label = "Keying Sets"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        row = layout.row()

        col = row.column()
        col.template_list("UI_UL_list", "keying_sets", scene, "keying_sets", scene.keying_sets, "active_index", rows=1)

        col = row.column(align=True)
        col.operator("anim.keying_set_add", icon='ZOOMIN', text="")
        col.operator("anim.keying_set_remove", icon='ZOOMOUT', text="")

        ks = scene.keying_sets.active
        if ks and ks.is_path_absolute:
            row = layout.row()

            col = row.column()
            col.prop(ks, "bl_description")

            subcol = col.column()
            subcol.operator_context = 'INVOKE_DEFAULT'
            subcol.operator("anim.keying_set_export", text="Export to File").filepath = "keyingset.py"

            col = row.column()
            col.label(text="Keyframing Settings:")
            self.draw_keyframing_settings(context, col, ks, None)


class SCENE_PT_keying_set_paths(SceneButtonsPanel, SceneKeyingSetsPanel, Panel):
    bl_label = "Active Keying Set"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        ks = context.scene.keying_sets.active
        return (ks and ks.is_path_absolute)

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        ks = scene.keying_sets.active

        row = layout.row()
        row.label(text="Paths:")

        row = layout.row()

        col = row.column()
        col.template_list("SCENE_UL_keying_set_paths", "", ks, "paths", ks.paths, "active_index", rows=1)

        col = row.column(align=True)
        col.operator("anim.keying_set_path_add", icon='ZOOMIN', text="")
        col.operator("anim.keying_set_path_remove", icon='ZOOMOUT', text="")

        ksp = ks.paths.active
        if ksp:
            col = layout.column()
            col.label(text="Target:")
            col.template_any_ID(ksp, "id", "id_type")
            col.template_path_builder(ksp, "data_path", ksp.id)

            row = col.row(align=True)
            row.label(text="Array Target:")
            row.prop(ksp, "use_entire_array", text="All Items")
            if ksp.use_entire_array:
                row.label(text=" ")  # padding
            else:
                row.prop(ksp, "array_index", text="Index")

            layout.separator()

            row = layout.row()
            col = row.column()
            col.label(text="F-Curve Grouping:")
            col.prop(ksp, "group_method", text="")
            if ksp.group_method == 'NAMED':
                col.prop(ksp, "group")

            col = row.column()
            col.label(text="Keyframing Settings:")
            self.draw_keyframing_settings(context, col, ks, ksp)


class SCENE_PT_color_management(SceneButtonsPanel, Panel):
    bl_label = "Color Management"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene

        col = layout.column()
        col.label(text="Display:")
        col.prop(scene.display_settings, "display_device")

        col = layout.column()
        col.separator()
        col.label(text="Render:")
        col.template_colormanaged_view_settings(scene, "view_settings")

        col = layout.column()
        col.separator()
        col.label(text="Sequencer:")
        col.prop(scene.sequencer_colorspace_settings, "name")


class SCENE_PT_audio(SceneButtonsPanel, Panel):
    bl_label = "Audio"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = context.scene.render
        ffmpeg = rd.ffmpeg

        layout.prop(scene, "audio_volume")
        layout.operator("sound.bake_animation")

        split = layout.split()

        col = split.column()
        col.label("Distance Model:")
        col.prop(scene, "audio_distance_model", text="")
        sub = col.column(align=True)
        sub.prop(scene, "audio_doppler_speed", text="Speed")
        sub.prop(scene, "audio_doppler_factor", text="Doppler")

        col = split.column()
        col.label("Format:")
        col.prop(ffmpeg, "audio_channels", text="")
        col.prop(ffmpeg, "audio_mixrate", text="Rate")


class SCENE_PT_physics(SceneButtonsPanel, Panel):
    bl_label = "Gravity"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        self.layout.prop(context.scene, "use_gravity", text="")

    def draw(self, context):
        layout = self.layout

        scene = context.scene

        layout.active = scene.use_gravity

        layout.prop(scene, "gravity", text="")


class SCENE_PT_rigid_body_world(SceneButtonsPanel, Panel):
    bl_label = "Rigid Body World"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        rd = scene.render
        return scene and (rd.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        scene = context.scene
        rbw = scene.rigidbody_world
        if rbw is not None:
            self.layout.prop(rbw, "enabled", text="")

    def draw(self, context):
        layout = self.layout

        scene = context.scene

        rbw = scene.rigidbody_world

        if rbw is None:
            layout.operator("rigidbody.world_add")
        else:
            layout.operator("rigidbody.world_remove")

            col = layout.column()
            col.active = rbw.enabled

            col = col.column()
            col.prop(rbw, "group")
            col.prop(rbw, "constraints")

            split = col.split()

            col = split.column()
            col.prop(rbw, "time_scale", text="Speed")
            col.prop(rbw, "use_split_impulse")

            col = split.column()
            col.prop(rbw, "steps_per_second", text="Steps Per Second")
            col.prop(rbw, "solver_iterations", text="Solver Iterations")


class SCENE_PT_rigid_body_cache(SceneButtonsPanel, Panel):
    bl_label = "Rigid Body Cache"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        scene = context.scene
        return scene and scene.rigidbody_world and (rd.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        scene = context.scene
        rbw = scene.rigidbody_world

        point_cache_ui(self, context, rbw.point_cache, rbw.point_cache.is_baked is False and rbw.enabled, 'RIGID_BODY')


class SCENE_PT_rigid_body_field_weights(SceneButtonsPanel, Panel):
    bl_label = "Rigid Body Field Weights"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        scene = context.scene
        return scene and scene.rigidbody_world and (rd.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        scene = context.scene
        rbw = scene.rigidbody_world

        effector_weights_ui(self, context, rbw.effector_weights, 'RIGID_BODY')


class SCENE_PT_simplify(SceneButtonsPanel, Panel):
    bl_label = "Simplify"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        rd = context.scene.render
        self.layout.prop(rd, "use_simplify", text="")

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        layout.active = rd.use_simplify

        split = layout.split()

        col = split.column()
        col.prop(rd, "simplify_subdivision", text="Subdivision")
        col.prop(rd, "simplify_child_particles", text="Child Particles")

        col.prop(rd, "use_simplify_triangulate")

        col = split.column()
        col.prop(rd, "simplify_shadow_samples", text="Shadow Samples")
        col.prop(rd, "simplify_ao_sss", text="AO and SSS")


# XXX temporary solution
bpy.types.CacheLibrary.filter_string = \
    bpy.props.StringProperty(
        name="Filter Object Name",
        description="Filter cache library objects by name",
        )
bpy.types.CacheLibrary.filter_types = \
    bpy.props.EnumProperty(
        name="Filter Item Type",
        description="Filter cache library items by type",
        options={'ENUM_FLAG'},
        items=[ (e.identifier, e.name, e.description, e.icon, 2**i) for i, e in enumerate(bpy.types.CacheItem.bl_rna.properties['type'].enum_items) ],
        default=set( e.identifier for e in bpy.types.CacheItem.bl_rna.properties['type'].enum_items ),
        )

def cachelib_objects(cachelib, filter_string = ""):
    filter_string = filter_string.lower()

    if not cachelib.group:
        return []
    elif filter_string:
        return filter(lambda ob: filter_string in ob.name.lower(), cachelib.group.objects)
    else:
        return cachelib.group.objects

# Yields (item, type, index, enabled)
# Note that item can be None when not included in the cache yet
def cachelib_object_items(cachelib, ob, filter_types = []):
    def items_desc():
        yield 'OBJECT', -1
        
        if (ob.type == 'MESH'):
            yield 'DERIVED_MESH', -1

        for index, psys in enumerate(ob.particle_systems):
            if psys.settings.type == 'EMITTER':
                yield 'PARTICLES', index
            if psys.settings.type == 'HAIR':
                yield 'HAIR', index
                yield 'HAIR_PATHS', index

    for item_type, item_index in items_desc():
        item = cachelib.cache_item_find(ob, item_type, item_index)
        
        show = False
        enable = False
        # always show existing items
        if item:
            show = True
            enable = True
        # always show selected types
        elif item_type in filter_types:
            show = True
            enable = True
        # special case: OBJECT type used as top level, show but disable
        elif item_type == 'OBJECT':
            show = True
            enable = False
        
        if show:
            yield item, item_type, item_index, enable

class SCENE_PT_cache_manager(SceneButtonsPanel, Panel):
    bl_label = "Cache Manager"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    item_type_icon = { e.identifier : e.icon for e in bpy.types.CacheItem.bl_rna.properties['type'].enum_items }

    @classmethod
    def poll(cls, context):
        return True

    def draw_cachelib(self, context, layout, cachelib):
        # imitate template_ID without an actual pointer property
        row = layout.row(align=True)
        row.alignment = 'EXPAND'
        row.prop(cachelib, "name", text="")
        sub = row.row(align=True)
        sub.alignment = 'RIGHT'
        sub.prop(cachelib, "use_fake_user", text="F", toggle=True)
        sub.operator("cachelibrary.delete", text="", icon='X')

        row = layout.row(align=True)
        row.alignment = 'LEFT'
        row.label("Group:")
        row.template_ID(cachelib, "group")

        col = layout.column(align=True)
        colrow = col.row(align=True)
        colrow.label("Archive:")
        props = colrow.operator("cachelibrary.archive_info", text="", icon='QUESTION')
        props.use_stdout = True
        props.use_popup = True
        props.use_clipboard = True
        col.prop(cachelib, "filepath", text="")

        colrow = col.row(align=True)
        colrow.prop(cachelib, "read", text="Read", toggle=True)
        colrow.operator("cachelibrary.bake")
        col.prop(cachelib, "eval_mode", expand=False)

        row = layout.row(align=True)
        row.label("Filter:")
        row.prop(cachelib, "filter_types", icon_only=True, toggle=True)
        row.prop(cachelib, "filter_string", icon='VIEWZOOM', text="")

        objects = cachelib_objects(cachelib, cachelib.filter_string)
        first = True
        for ob in objects:
            if not any(cachelib_object_items(cachelib, ob, cachelib.filter_types)):
                continue

            if first:
                layout.separator()
                first = False

            for item, item_type, item_index, enable in cachelib_object_items(cachelib, ob, cachelib.filter_types):
                row = layout.row(align=True)
                row.alignment = 'LEFT'
                row.template_cache_library_item(cachelib, ob, item_type, item_index, enable)


    def draw(self, context):
        layout = self.layout

        layout.operator("cachelibrary.new")
        for cachelib in context.blend_data.cache_libraries:
            box = layout.box()
            box.context_pointer_set("cache_library", cachelib)
            
            self.draw_cachelib(context, box, cachelib)


class SCENE_PT_custom_props(SceneButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}
    _context_path = "scene"
    _property_type = bpy.types.Scene

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)

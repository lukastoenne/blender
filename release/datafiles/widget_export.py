import bpy
from bpy.types import Operator
from bpy.props import StringProperty
from bpy_extras.io_utils import ExportHelper

def mesh_triangulate(me):
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    

class ExportWidget(Operator, ExportHelper):
    """Export a widget mesh as a C file"""
    bl_idname = "export_scene.widget"
    bl_label = "Export Widget"
    bl_options = {'PRESET', 'UNDO'}

    filename_ext = ".c"
    filter_glob = StringProperty(
            default="*.c;",
            options={'HIDDEN'},
            )
    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH')

    def execute(self, context):
        ob = context.active_object
        scene = context.scene

        try:
            me = ob.to_mesh(scene, True, 'PREVIEW', calc_tessface=False)
        except RuntimeError:
            me = None

        if me is None:
            return {'CANCELLED'}

        mesh_triangulate(me)

        name = ob.name
        f = open(self.filepath, 'w')
        f.write("int _WIDGET_nverts_%s = %d;\n" % (name, len(me.vertices)))
        f.write("int _WIDGET_ntris_%s = %d;\n\n" % (name, len(me.polygons)))
        f.write("float _WIDGET_verts_%s[][3] = {\n" % name)
        for v in me.vertices:
            f.write("    {%.6f, %.6f, %.6f},\n" % v.co[:])            
        f.write("};\n\n")
        f.write("float _WIDGET_normals_%s[][3] = {\n" % name)
        for v in me.vertices:
            f.write("    {%.6f, %.6f, %.6f},\n" % v.normal[:])            
        f.write("};\n\n")
        f.write("unsigned short _WIDGET_indices_%s[] = {\n" % name)
        for p in me.polygons:
            f.write("    %d, %d, %d,\n" % p.vertices[:])            
        f.write("};\n")
        f.close()
        
        return {'FINISHED'}

def menu_func_export(self, context):
    self.layout.operator(ExportWidget.bl_idname, text="Widget (.c)")

        
def register():
   bpy.utils.register_module(__name__)
   bpy.types.INFO_MT_file_export.append(menu_func_export)


def unregister():
    bpy.utils.unregister_module(__name__)
    bpy.types.INFO_MT_file_export.remove(menu_func_export)

if __name__ == "__main__":
    register()

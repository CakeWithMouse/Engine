"""blender -b --factory-startup --python make-blend-fixtures.py -- <output-directory>"""
import os
import sys
import bpy

folder = os.path.abspath(sys.argv[sys.argv.index("--") + 1])
os.makedirs(folder, exist_ok=True)
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
bpy.ops.mesh.primitive_cube_add()
cube = bpy.context.object
material = bpy.data.materials.new("Green Base Color")
material.use_nodes = True
principled = material.node_tree.nodes.get("Principled BSDF")
image = bpy.data.images.new("Green", width=2, height=2)
image.pixels[:] = [0.05, 0.9, 0.02, 1.0] * 4
image.filepath_raw = os.path.join(folder, "green.png")
image.file_format = "PNG"
image.save()
image.source = "FILE"
texture = material.node_tree.nodes.new("ShaderNodeTexImage")
texture.image = image
material.node_tree.links.new(texture.outputs["Color"], principled.inputs["Base Color"])
cube.data.materials.append(material)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, "blend-external.blend"))
bpy.ops.export_scene.fbx(filepath=os.path.join(folder, "blender-external.fbx"), path_mode="RELATIVE")
image.pack()
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, "blend-packed.blend"))
bpy.ops.export_scene.fbx(filepath=os.path.join(folder, "blender-packed.fbx"), path_mode="COPY", embed_textures=True)
image.unpack(method="REMOVE")
image.filepath = "//missing.png"
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, "blend-missing.blend"))

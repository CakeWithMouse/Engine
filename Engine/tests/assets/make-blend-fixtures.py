"""blender -b --factory-startup --python make-blend-fixtures.py -- <output-directory>"""
import os
import sys
import bpy

if bpy.app.version < (4, 2, 0):
    raise RuntimeError("BLEND integration fixtures require Blender 4.2 or newer")

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

# A real Blender model, plus a UV chart whose four corners expose vertical flips.
# Self-contained fixtures: Suzanne ships with Blender; no downloaded assets/license required.
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
chart = bpy.data.images.new("UV corners", width=64, height=64)
pixels = []
for y in range(64):
    for x in range(64):
        pixels.extend(((0, 0, 1, 1) if x < 32 else (1, 1, 0, 1)) if y < 32
                      else ((1, 0, 0, 1) if x < 32 else (0, 1, 0, 1)))
chart.pixels[:] = pixels
chart.filepath_raw = os.path.join(folder, "uv-corners.png")
chart.file_format = "PNG"
chart.save()
chart.pack()
chart_mat = bpy.data.materials.new("UV chart")
chart_mat.use_nodes = True
texture = chart_mat.node_tree.nodes.new("ShaderNodeTexImage")
texture.image = chart
chart_mat.node_tree.links.new(texture.outputs["Color"],
                            chart_mat.node_tree.nodes.get("Principled BSDF").inputs["Base Color"])
mesh = bpy.data.meshes.new("UV plane")
mesh.from_pydata([(-1, 0, -1), (1, 0, -1), (1, 0, 1), (-1, 0, 1)], [], [(0, 1, 2, 3)])
uv = mesh.uv_layers.new()
for loop, coord in zip(uv.data, [(0, 0), (1, 0), (1, 1), (0, 1)]):
    loop.uv = coord
plane = bpy.data.objects.new("UV plane", mesh)
bpy.context.collection.objects.link(plane)
mesh.materials.append(chart_mat)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, "blend-uv.blend"))
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
bpy.ops.mesh.primitive_monkey_add()
suzanne = bpy.context.object
modifier = suzanne.modifiers.new("Subdivision", "SUBSURF")
modifier.levels = 1
bpy.ops.object.modifier_apply(modifier=modifier.name)
bpy.ops.object.shade_smooth()
suzanne.data.materials.append(chart_mat)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, "blend-suzanne.blend"))
second = bpy.data.materials.new("Second material magenta")
second.use_nodes = True
second.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value = (1, 0, 1, 1)
suzanne.data.materials.append(second)
for polygon in suzanne.data.polygons:
    polygon.material_index = int(polygon.center.x > 0)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, "blend-suzanne-multiple.blend"))
